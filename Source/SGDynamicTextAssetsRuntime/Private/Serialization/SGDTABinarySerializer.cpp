// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDTABinarySerializer.h"

#include "Management/SGDTAFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Misc/Compression.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/SGDTABinaryEncodeParams.h"
#include "Serialization/SGDTASerializer.h"
#include "SGDynamicTextAssetLogs.h"
#include "Statics/SGDTAConstants.h"

bool FSGDynamicTextAssetBinarySerializer::StringToBinary(
	const FString& PayloadString,
	const FSGDTABinaryEncodeParams& Params,
	TArray<uint8>& OutBinaryData)
{
	if (PayloadString.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Inputted EMPTY PayloadString"));
		return false;
	}
	if (!Params.Id.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Inputted INVALID Id"));
		return false;
	}
	if (!Params.SerializerFormat.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Inputted INVALID SerializerFormat"));
		return false;
	}

	// Convert payload string to UTF-8 bytes
	FTCHARToUTF8 utf8Converter(*PayloadString);
	const int32 uncompressedSize = utf8Converter.Length();

	if (uncompressedSize == 0)
	{
		return false;
	}

	// Prepare header  - SerializerFormat is stored here so the loader can route
	// the decompressed payload to the correct deserializer without a string lookup.
	// AssetTypeGuid enables class resolution directly from the header without decompression.
	FSGDTABinaryHeader header;
	header.CompressionType = static_cast<uint32>(Params.CompressionMethod);
	header.UncompressedSize = uncompressedSize;
	header.SerializerTypeId = Params.SerializerFormat.GetTypeId();
	header.Guid = Params.Id.GetGuid();
	header.AssetTypeGuid = Params.AssetTypeId.GetGuid();

	TArray<uint8> compressedData;

	if (Params.CompressionMethod == ESGDynamicTextAssetCompressionMethod::None)
	{
		// No compression - just copy the data
		compressedData.SetNumUninitialized(uncompressedSize);
		FMemory::Memcpy(compressedData.GetData(), utf8Converter.Get(), uncompressedSize);
		header.CompressedSize = uncompressedSize;
	}
	else
	{
		// Get compression format name
		FName formatName = GetCompressionFormatName(Params.CompressionMethod);
		if (formatName == NAME_None)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Custom compression configured but no custom compression name set in settings"));
			return false;
		}

		// Calculate maximum compressed size and allocate buffer
		int32 maxCompressedSize = FCompression::CompressMemoryBound(formatName, uncompressedSize);
		compressedData.SetNumUninitialized(maxCompressedSize);

		// Compress the data
		int32 actualCompressedSize = maxCompressedSize;
		bool bCompressed = FCompression::CompressMemory(
			formatName,
			compressedData.GetData(),
			actualCompressedSize,
			utf8Converter.Get(),
			uncompressedSize
		);

		if (!bCompressed)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Failed to compress data for ID %s"), *Params.Id.ToString());
			return false;
		}

		// Shrink to actual size
		compressedData.SetNum(actualCompressedSize);
		header.CompressedSize = actualCompressedSize;
	}

	// Compute SHA-1 hash of the compressed payload for integrity verification
	FSHA1 sha1;
	sha1.Update(compressedData.GetData(), compressedData.Num());
	sha1.Final();
	sha1.GetHash(header.ContentHash);

	// Write header then compressed payload directly  - no variable-length block
	OutBinaryData.Empty();
	OutBinaryData.Reserve(FSGDTABinaryHeader::HEADER_SIZE + compressedData.Num());

	// Serialize the fixed 64-byte header to memory
	FMemoryWriter headerWriter(OutBinaryData);
	header.Serialize(headerWriter);

	// Verify header serialized to exactly HEADER_SIZE bytes
	check(OutBinaryData.Num() == FSGDTABinaryHeader::HEADER_SIZE);

	// Append compressed payload  - always starts at byte offset HEADER_SIZE
	OutBinaryData.Append(compressedData);

	return true;
}

bool FSGDynamicTextAssetBinarySerializer::BinaryToString(
	const TArray<uint8>& BinaryData,
	FString& OutPayloadString,
	FSGDTASerializerFormat& OutSerializerFormat)
{
	OutPayloadString.Empty();
	OutSerializerFormat = FSGDTASerializerFormat();

	if (BinaryData.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Inputted Empty BinaryData"));
		return false;
	}

	// Read and validate the fixed 64-byte header
	FSGDTABinaryHeader header;
	if (!ReadHeader(BinaryData, header))
	{
		return false;
	}

	// Validate uncompressed size against maximum to prevent OOM from crafted headers
	if (header.UncompressedSize > MAX_UNCOMPRESSED_SIZE)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: UncompressedSize(%u) exceeds MAX_UNCOMPRESSED_SIZE(%u) for ID(%s)"),
			header.UncompressedSize, MAX_UNCOMPRESSED_SIZE, *header.Guid.ToString());
		return false;
	}

	// Validate the compressed payload fits within the remaining data.
	// BinaryData.Num() - HEADER_SIZE is the count of bytes available after the header (a size, not a location).
	const int32 totalDataAfterHeader = BinaryData.Num() - FSGDTABinaryHeader::HEADER_SIZE;
	if (header.CompressedSize > static_cast<uint32>(totalDataAfterHeader))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: CompressedSize(%u) exceeds available data(%d) for ID(%s)"),
			header.CompressedSize, totalDataAfterHeader, *header.Guid.ToString());
		return false;
	}

	// Validate total expected size
	const int32 expectedTotalSize = FSGDTABinaryHeader::HEADER_SIZE + header.CompressedSize;
	if (BinaryData.Num() < expectedTotalSize)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Binary data too small. expectedTotalSize(%d), BinaryData size(%d)"),
			expectedTotalSize, BinaryData.Num());
		return false;
	}

	// BinaryData.GetData() returns a uint8* to the first byte of the array buffer.
	// Adding HEADER_SIZE advances that pointer exactly 64 bytes forward  - past the fixed
	// header  - to the first byte of the compressed payload. This is a memory location,
	// not a size count. Compare with BinaryData.Num() - HEADER_SIZE above, which is a count.
	//
	// reinterpret_cast is needed because FUTF8ToTCHAR expects ANSICHAR* but GetData()
	// returns uint8*. Both are 1-byte types, so no data is actually reinterpreted  - this
	// is purely a type-system cast to satisfy the API signature.
	const uint8* compressedDataPtr = BinaryData.GetData() + FSGDTABinaryHeader::HEADER_SIZE;

	// Verify content hash
	if (!header.HasContentHash())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Binary data is missing hash for ID(%s)"),
			*header.Guid.ToString());
		return false;
	}
	uint8 computedHash[FSGDTABinaryHeader::CONTENT_HASH_SIZE];
	FSHA1 sha1;
	sha1.Update(compressedDataPtr, header.CompressedSize);
	sha1.Final();
	sha1.GetHash(computedHash);

	if (FMemory::Memcmp(computedHash, header.ContentHash, FSGDTABinaryHeader::CONTENT_HASH_SIZE) != 0)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Content hash mismatch for ID(%s) - file may be corrupted or tampered with"),
			*header.Guid.ToString());
		return false;
	}

	TArray<uint8> uncompressedData;
	uncompressedData.SetNumUninitialized(header.UncompressedSize);

	ESGDynamicTextAssetCompressionMethod compressionMethod = static_cast<ESGDynamicTextAssetCompressionMethod>(header.CompressionType);

	if (compressionMethod == ESGDynamicTextAssetCompressionMethod::None)
	{
		// No compression - just copy
		if (header.CompressedSize != header.UncompressedSize)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Size mismatch for uncompressed data"));
			return false;
		}
		FMemory::Memcpy(uncompressedData.GetData(), compressedDataPtr, header.UncompressedSize);
	}
	else
	{
		// Decompress
		FName formatName = GetCompressionFormatName(compressionMethod);
		if (formatName == NAME_None)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Binary file uses custom compression but no custom compression name configured in settings for ID(%s)"),
				*header.Guid.ToString());
			return false;
		}

		bool bDecompressed = FCompression::UncompressMemory(
			formatName,
			uncompressedData.GetData(),
			header.UncompressedSize,
			compressedDataPtr,
			header.CompressedSize
		);

		if (!bDecompressed)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Failed to decompress data for ID(%s)"),
				*header.Guid.ToString());
			return false;
		}
	}

	// Convert UTF-8 bytes back to FString
	FUTF8ToTCHAR utf8Converter(reinterpret_cast<const ANSICHAR*>(uncompressedData.GetData()), uncompressedData.Num());
	OutPayloadString = FString(utf8Converter.Length(), utf8Converter.Get());

	// Return the serializer format from the header - caller passes this to
	// FSGDynamicTextAssetFileManager::FindSerializerForFormat() to get the right deserializer
	OutSerializerFormat = FSGDTASerializerFormat(header.SerializerTypeId);

	return true;
}

bool FSGDynamicTextAssetBinarySerializer::ReadHeader(
	const TArray<uint8>& BinaryData,
	FSGDTABinaryHeader& OutHeader)
{
	if (BinaryData.Num() < FSGDTABinaryHeader::HEADER_SIZE)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Binary data too small for header"));
		return false;
	}

	// Read header from memory
	FMemoryReader headerReader(BinaryData);
	OutHeader.Serialize(headerReader);

	// Validate magic number
	if (!OutHeader.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Invalid magic number(%s) (expected %s)"),
			*OutHeader.GetMagicNumberString(), *FSGDTABinaryHeader::GetExpectedMagicNumberString());
		return false;
	}

	// Check schema version compatibility
	if (OutHeader.PluginSchemaVersion > FSGDTABinaryHeader::CURRENT_SCHEMA_VERSION)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Unsupported PluginSchemaVersion(%d) (max supported: %d)"),
			OutHeader.PluginSchemaVersion, FSGDTABinaryHeader::CURRENT_SCHEMA_VERSION);
		return false;
	}

	// Validate size fields are non-zero
	if (OutHeader.UncompressedSize == 0)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Header has zero UncompressedSize"));
		return false;
	}

	if (OutHeader.CompressedSize == 0)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Header has zero CompressedSize"));
		return false;
	}

	return true;
}

bool FSGDynamicTextAssetBinarySerializer::ReadBinaryFile(
	const FString& FilePath,
	TArray<uint8>& OutBinaryData)
{
	if (!FFileHelper::LoadFileToArray(OutBinaryData, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Failed to read file: %s"), *FilePath);
		return false;
	}
	return true;
}

bool FSGDynamicTextAssetBinarySerializer::WriteBinaryFile(
	const FString& FilePath,
	const TArray<uint8>& BinaryData)
{
	if (!FFileHelper::SaveArrayToFile(BinaryData, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Failed to write file: %s"), *FilePath);
		return false;
	}
	return true;
}

bool FSGDynamicTextAssetBinarySerializer::BinaryReadSerializerFormat(
	const TArray<uint8>& BinaryData,
	FSGDTASerializerFormat& OutSerializerFormat)
{
	OutSerializerFormat = FSGDTASerializerFormat();

	// Read and validate the fixed 64-byte header at the start of the binary data.
	// This gives us access to SerializerFormat without touching the compressed payload at all.
	FSGDTABinaryHeader header;
	if (!ReadHeader(BinaryData, header))
	{
		return false;
	}

	// SerializerFormat is a fixed uint32 inside the header - no variable-length data to skip.
	// The compressed payload always starts at exactly HEADER_SIZE bytes into the file,
	// regardless of which serializer produced it.
	// Pass this format to FSGDynamicTextAssetFileManager::FindSerializerForFormat() to get the
	// serializer instance that knows how to deserialize the payload.
	OutSerializerFormat = FSGDTASerializerFormat(header.SerializerTypeId);
	return true;
}

bool FSGDynamicTextAssetBinarySerializer::BinaryReadSerializerTypeId(
	const TArray<uint8>& BinaryData,
	uint32& OutSerializerTypeId)
{
	FSGDTASerializerFormat format;
	bool bResult = BinaryReadSerializerFormat(BinaryData, format);
	OutSerializerTypeId = format.GetTypeId();
	return bResult;
}

bool FSGDynamicTextAssetBinarySerializer::ConvertJsonFileToBinary(
	const FString& JsonFilePath,
	const FString& BinaryFilePath,
	ESGDynamicTextAssetCompressionMethod CompressionMethod)
{
	// Find serializer for the source file
	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(JsonFilePath);
	if (!serializer.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: No serializer found for %s"), *JsonFilePath);
		return false;
	}

	// Read source file
	FString fileContents;
	if (!FFileHelper::LoadFileToString(fileContents, *JsonFilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Failed to read file(%s)"), *JsonFilePath);
		return false;
	}

	// Extract ID and Asset Type ID
	FSGDynamicTextAssetFileInfo binMeta;
	if (!serializer->ExtractFileInfo(fileContents, binMeta) || !binMeta.Id.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Failed to extract valid ID from (%s)"), *JsonFilePath);
		return false;
	}

	// Convert to binary, storing the serializer's type ID and asset type ID for routing on load
	FSGDTABinaryEncodeParams params;
	params.Id = binMeta.Id;
	params.SerializerFormat = serializer->GetSerializerFormat();
	params.AssetTypeId = binMeta.AssetTypeId;
	params.CompressionMethod = CompressionMethod;

	TArray<uint8> binaryData;
	if (!StringToBinary(fileContents, params, binaryData))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetBinarySerializer: Failed to convert to binary from (%s)"), *JsonFilePath);
		return false;
	}

	// Write binary file
	if (!WriteBinaryFile(BinaryFilePath, binaryData))
	{
		return false;
	}

	return true;
}

bool FSGDynamicTextAssetBinarySerializer::ExtractId(
	const TArray<uint8>& BinaryData,
	FSGDynamicTextAssetId& OutId)
{
	FSGDTABinaryHeader header;
	if (!ReadHeader(BinaryData, header))
	{
		return false;
	}
	OutId = FSGDynamicTextAssetId::FromGuid(header.Guid);
	return OutId.IsValid();
}

bool FSGDynamicTextAssetBinarySerializer::IsValidBinaryData(const TArray<uint8>& BinaryData)
{
	FSGDTABinaryHeader header;
	return ReadHeader(BinaryData, header);
}

FString FSGDynamicTextAssetBinarySerializer::GetFileExtension()
{
	return SGDynamicTextAssetConstants::BINARY_FILE_EXTENSION;
}

FName FSGDynamicTextAssetBinarySerializer::GetCompressionFormatName(ESGDynamicTextAssetCompressionMethod CompressionMethod)
{
	switch (CompressionMethod)
	{
		case ESGDynamicTextAssetCompressionMethod::None:
		{
			return NAME_None;
		}
		case ESGDynamicTextAssetCompressionMethod::Zlib:
		{
			return NAME_Zlib;
		}
		case ESGDynamicTextAssetCompressionMethod::Gzip:
		{
			return NAME_Gzip;
		}
		case ESGDynamicTextAssetCompressionMethod::LZ4:
		{
			return NAME_LZ4;
		}
		case ESGDynamicTextAssetCompressionMethod::Custom:
		{
			return USGDynamicTextAssetSettings::Get()->GetCustomCompressionName();
		}
		default:
		{
			return NAME_Zlib;
		}
	}
}

ESGDynamicTextAssetCompressionMethod FSGDynamicTextAssetBinarySerializer::GetCompressionMethodFromFormatName(FName FormatName)
{
	if (FormatName == NAME_None)
	{
		return ESGDynamicTextAssetCompressionMethod::None;
	}
	if (FormatName == NAME_Zlib)
	{
		return ESGDynamicTextAssetCompressionMethod::Zlib;
	}
	if (FormatName == NAME_Gzip)
	{
		return ESGDynamicTextAssetCompressionMethod::Gzip;
	}
	if (FormatName == NAME_LZ4)
	{
		return ESGDynamicTextAssetCompressionMethod::LZ4;
	}

	// Default to Zlib
	return ESGDynamicTextAssetCompressionMethod::Zlib;
}
