// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGBinaryDynamicTextAssetHeader.h"

// Validate the struct is properly defined.
static_assert(sizeof(FSGBinaryDynamicTextAssetHeader::MAGIC_NUMBER) == 5,
	"Magic number must be exactly 5 bytes");
static_assert(sizeof(FSGBinaryDynamicTextAssetHeader) == FSGBinaryDynamicTextAssetHeader::HEADER_SIZE,
	"FSGBinaryDynamicTextAssetHeader size mismatch - check member alignment");

FSGBinaryDynamicTextAssetHeader::FSGBinaryDynamicTextAssetHeader()
{
	FMemory::Memcpy(MagicNumber, MAGIC_NUMBER, sizeof(MagicNumber));
}

bool FSGBinaryDynamicTextAssetHeader::IsValid() const
{
	return FMemory::Memcmp(MagicNumber, MAGIC_NUMBER, sizeof(MagicNumber)) == 0;
}

bool FSGBinaryDynamicTextAssetHeader::HasContentHash() const
{
	for (uint32 index = 0; index < CONTENT_HASH_SIZE; ++index)
	{
		if (ContentHash[index] != 0)
		{
			return true;
		}
	}
	return false;
}

bool FSGBinaryDynamicTextAssetHeader::HasAssetTypeGuid() const
{
	return AssetTypeGuid.IsValid();
}

FString FSGBinaryDynamicTextAssetHeader::GetMagicNumberString() const
{
	FString result = TEXT("0x");
	for (int32 index = 0; index < sizeof(MagicNumber); ++index)
	{
		result += FString::Printf(TEXT("%02X"), MagicNumber[index]);
	}
	return result;
}

FString FSGBinaryDynamicTextAssetHeader::GetExpectedMagicNumberString()
{
	FString result = TEXT("0x");
	static constexpr uint8 size = sizeof(MAGIC_NUMBER);
	for (int32 index = 0; index < size; ++index)
	{
		result += FString::Printf(TEXT("%02X"), MAGIC_NUMBER[index]);
	}
	return result;
}

void FSGBinaryDynamicTextAssetHeader::Serialize(FArchive& Ar)
{
	Ar.Serialize(MagicNumber, sizeof(MagicNumber));
	Ar.Serialize(MagicNumberPadding, sizeof(MagicNumberPadding));
	Ar << PluginSchemaVersion;
	Ar << CompressionType;
	Ar << UncompressedSize;
	Ar << CompressedSize;
	Ar << SerializerTypeId;
	Ar << Guid;
	Ar << AssetTypeGuid;
	Ar.Serialize(ContentHash, sizeof(ContentHash));
}
