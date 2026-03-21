// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Settings/SGDynamicTextAssetSettings.h"
#include "SGBinaryDynamicTextAssetHeader.h"

struct FSGBinaryEncodeParams;
struct FSGDynamicTextAssetId;

/**
 * Static utility class for binary serialization of dynamic text assets.
 *
 * Converts Text data to compressed binary format for cooked builds.
 * Uses Unreal's FCompression API for compression/decompression.
 *
 * Binary format:
 * [Header - 80 bytes]
 * ├── Magic Number (8 bytes): 0x5347445441 ("SGDTA") + 3 bytes padding
 * ├── Plugin Schema Version (4 bytes)
 * ├── Compression Type (4 bytes)
 * ├── Uncompressed Size (4 bytes)
 * ├── Compressed Size (4 bytes)
 * ├── SerializerTypeId (4 bytes): integer ID of the serializer that produced the payload
 * ├── GUID (16 bytes): asset instance identity
 * ├── AssetTypeGuid (16 bytes): asset type (class) identity
 * ├── Content Hash (20 bytes): SHA-1 of compressed payload
 * [Compressed Data  - always begins at byte offset HEADER_SIZE (80)]
 * └── Compressed payload bytes (format determined by SerializerTypeId)
 *
 * @see FSGDynamicTextAssetJsonSerializer
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetBinarySerializer
{
public:

	/**
	 * Converts a payload string to compressed binary format.
	 *
	 * @param PayloadString The serialized payload to compress (e.g. JSON, XML)
	 * @param Params Encoding parameters (Id, SerializerTypeId, AssetTypeId, CompressionMethod)
	 * @param OutBinaryData Output array containing the binary data
	 * @return True if conversion succeeded
	 */
	static bool StringToBinary(
		const FString& PayloadString,
		const FSGBinaryEncodeParams& Params,
		TArray<uint8>& OutBinaryData);

	/**
	 * Extracts payload string and serializer type ID from compressed binary data.
	 *
	 * @param BinaryData The binary data to decompress
	 * @param OutPayloadString Output string containing the decompressed payload
	 * @param OutSerializerTypeId The serializer type ID stored in the header  - pass to FSGDynamicTextAssetFileManager::FindSerializerForTypeId() to get the deserializer
	 * @return True if extraction succeeded
	 */
	static bool BinaryToString(
		const TArray<uint8>& BinaryData,
		FString& OutPayloadString,
		uint32& OutSerializerTypeId);

	/**
	 * Reads the header from binary data without decompressing the payload.
	 * Useful for quick metadata access.
	 * 
	 * @param BinaryData The binary data to read
	 * @param OutHeader Output header structure
	 * @return True if header was read successfully
	 */
	static bool ReadHeader(
		const TArray<uint8>& BinaryData,
		FSGBinaryDynamicTextAssetHeader& OutHeader);

	/**
	 * Reads binary data from a file.
	 * 
	 * @param FilePath Full path to the binary file
	 * @param OutBinaryData Output array containing the file contents
	 * @return True if file was read successfully
	 */
	static bool ReadBinaryFile(
		const FString& FilePath,
		TArray<uint8>& OutBinaryData);

	/**
	 * Writes binary data to a file.
	 * 
	 * @param FilePath Full path to the output file
	 * @param BinaryData The binary data to write
	 * @return True if file was written successfully
	 */
	static bool WriteBinaryFile(
		const FString& FilePath,
		const TArray<uint8>& BinaryData);

	/**
	 * Converts a JSON file to binary format and writes to disk.
	 * 
	 * @param JsonFilePath Full path to the source JSON file
	 * @param BinaryFilePath Full path to the output binary file
	 * @param CompressionMethod Compression algorithm to use
	 * @return True if conversion and write succeeded
	 */
	static bool ConvertJsonFileToBinary(
		const FString& JsonFilePath,
		const FString& BinaryFilePath,
		ESGDynamicTextAssetCompressionMethod CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib);

	/**
	 * Reads the serializer type ID from binary data without decompressing the payload.
	 * Use this when you only need to identify the format without fully loading the file.
	 * Pass the returned ID to FSGDynamicTextAssetFileManager::FindSerializerForTypeId() to resolve the serializer.
	 *
	 * @param BinaryData The binary data to read
	 * @param OutSerializerTypeId The serializer type ID stored in the header
	 * @return True if the header was valid and the read succeeded
	 */
	static bool BinaryReadSerializerTypeId(
		const TArray<uint8>& BinaryData,
		uint32& OutSerializerTypeId);

	/**
	 * Extracts the dynamic text asset ID from binary data without full decompression.
	 *
	 * @param BinaryData The binary data to read
	 * @param OutId Output dynamic text asset ID
	 * @return True if the ID was extracted successfully
	 */
	static bool ExtractId(
		const TArray<uint8>& BinaryData,
		FSGDynamicTextAssetId& OutId);

	/**
	 * Checks if binary data has a valid header.
	 * 
	 * @param BinaryData The binary data to validate
	 * @return True if the header is valid
	 */
	static bool IsValidBinaryData(const TArray<uint8>& BinaryData);

	/**
	 * Gets the file extension for binary dynamic text asset files.
	 *
	 * @return The file extension (without dot): "dta.bin"
	 */
	static FString GetFileExtension();

	/** Maximum allowed uncompressed payload size (64 MB). Prevents OOM from crafted headers. */
	static constexpr uint32 MAX_UNCOMPRESSED_SIZE = FSGBinaryDynamicTextAssetHeader::HEADER_SIZE * 1024 * 1024;

private:

	/**
	 * Gets the FName for the compression format.
	 *
	 * @param CompressionMethod The compression method enum
	 * @return The FName used by FCompression API
	 */
	static FName GetCompressionFormatName(ESGDynamicTextAssetCompressionMethod CompressionMethod);

	/**
	 * Gets the compression method from format name.
	 *
	 * @param FormatName The FName from FCompression API
	 * @return The corresponding compression method enum
	 */
	static ESGDynamicTextAssetCompressionMethod GetCompressionMethodFromFormatName(FName FormatName);
};
