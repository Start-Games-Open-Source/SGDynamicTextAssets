// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDynamicTextAssetCookManifest.h"

#include "Management/SGDynamicTextAssetCookManifestBinaryHeader.h"
#include "Management/SGDynamicTextAssetCookManifestStringTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SGDynamicTextAssetsRuntimeModule.h"

const TCHAR* FSGDynamicTextAssetCookManifest::MANIFEST_FILENAME = TEXT("dta_manifest.bin");

FSGDynamicTextAssetCookManifestEntry::FSGDynamicTextAssetCookManifestEntry(const FSGDynamicTextAssetId& InID,
	const FString& InClassName, const FString& InUserFacingId, const FSGDynamicTextAssetTypeId& InAssetTypeId)
	: Id(InID)
	, ClassName(InClassName)
	, UserFacingId(InUserFacingId)
	, AssetTypeId(InAssetTypeId)
{

}

void FSGDynamicTextAssetCookManifest::AddEntry(const FSGDynamicTextAssetId& Id, const FString& ClassName,
	const FString& UserFacingId, const FSGDynamicTextAssetTypeId& AssetTypeId)
{
	int32 index = Entries.Emplace(Id, ClassName, UserFacingId, AssetTypeId);
	IdIndex.Add(Id, index);

	TArray<int32>& classIndices = ClassNameIndex.FindOrAdd(ClassName);
	classIndices.Add(index);

	if (AssetTypeId.IsValid())
	{
		TArray<int32>& typeIdIndices = AssetTypeIdIndex.FindOrAdd(AssetTypeId);
		typeIdIndices.Add(index);
	}
}

bool FSGDynamicTextAssetCookManifest::SaveToFileBinary(const FString& DirectoryPath) const
{
	FString filePath = FPaths::Combine(DirectoryPath,
		FSGDynamicTextAssetCookManifestBinaryHeader::BINARY_MANIFEST_FILENAME);

	// Step 1: Build deduplicated string table from all entries
	FSGDynamicTextAssetCookManifestStringTable stringTable;

	struct FBinaryEntryIndices
	{
		uint32 ClassNameIndex;
		uint32 UserFacingIdIndex;
	};

	TArray<FBinaryEntryIndices> entryIndices;
	entryIndices.Reserve(Entries.Num());

	for (const FSGDynamicTextAssetCookManifestEntry& entry : Entries)
	{
		FBinaryEntryIndices indices;
		indices.ClassNameIndex = stringTable.AddString(entry.ClassName);
		indices.UserFacingIdIndex = entry.UserFacingId.IsEmpty()
			? FSGDynamicTextAssetCookManifestStringTable::INVALID_STRING_INDEX
			: stringTable.AddString(entry.UserFacingId);
		entryIndices.Add(indices);
	}

	// Step 2: Create memory writer
	TArray<uint8> buffer;
	buffer.Reserve(FSGDynamicTextAssetCookManifestBinaryHeader::HEADER_SIZE + (Entries.Num() * 40) + 4096);
	FMemoryWriter writer(buffer);

	// Step 3: Write header with placeholder ContentHash
	FSGDynamicTextAssetCookManifestBinaryHeader header;
	header.EntryCount = static_cast<uint32>(Entries.Num());
	// StringTableOffset and ContentHash will be patched later
	header.Serialize(writer);

	// Step 4: Record entries start position (immediately after header)
	int64 entriesStartOffset = writer.Tell();

	// Step 5: Write each entry (40 bytes each)
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FSGDynamicTextAssetCookManifestEntry& entry = Entries[i];
		FBinaryEntryIndices& indices = entryIndices[i];

		// Write FGuid for asset Id (16 bytes)
		FGuid assetGuid = entry.Id.GetGuid();
		writer << assetGuid;

		// Write FGuid for AssetTypeId (16 bytes)
		FGuid typeGuid = entry.AssetTypeId.GetGuid();
		writer << typeGuid;

		// Write string table indices (4 + 4 = 8 bytes)
		writer << indices.ClassNameIndex;
		writer << indices.UserFacingIdIndex;
	}

	// Step 6: Record StringTableOffset and patch the header field
	uint32 stringTableOffset = static_cast<uint32>(writer.Tell());
	{
		// Seek back to patch StringTableOffset in the header
		// StringTableOffset is at offset: 4 (magic) + 4 (version) + 4 (entry count) = 12
		static constexpr int64 STRING_TABLE_OFFSET_POSITION = 12;
		writer.Seek(STRING_TABLE_OFFSET_POSITION);
		writer << stringTableOffset;
		writer.Seek(buffer.Num());
	}

	// Step 7: Write string table
	stringTable.SerializeTo(writer);

	// Step 8: Compute SHA-1 hash over entries + string table bytes (everything after header)
	int64 contentEndOffset = writer.Tell();
	int64 contentSize = contentEndOffset - entriesStartOffset;

	uint8 fullHash[20];
	FSHA1 sha1;
	sha1.Update(buffer.GetData() + entriesStartOffset, static_cast<uint32>(contentSize));
	sha1.Final();
	sha1.GetHash(fullHash);

	// Step 9: Truncate to 12 bytes and patch ContentHash in the header
	{
		// ContentHash is at offset: 4 (magic) + 4 (version) + 4 (entry count) + 4 (string table offset) + 4 (reserved) = 20
		static constexpr int64 CONTENT_HASH_POSITION = 20;
		writer.Seek(CONTENT_HASH_POSITION);
		writer.Serialize(fullHash, FSGDynamicTextAssetCookManifestBinaryHeader::CONTENT_HASH_SIZE);
		writer.Seek(buffer.Num());
	}

	// Step 10: Write buffer to file
	if (!FFileHelper::SaveArrayToFile(buffer, *filePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetCookManifest: Failed to write binary manifest to '%s'"), *filePath);
		return false;
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDynamicTextAssetCookManifest: Saved binary manifest with %d entries (%d unique strings) to '%s'"),
		Entries.Num(), stringTable.Num(), *filePath);
	return true;
}

bool FSGDynamicTextAssetCookManifest::LoadFromFileBinary(const FString& DirectoryPath)
{
	Clear();

	FString filePath = FPaths::Combine(DirectoryPath,
		FSGDynamicTextAssetCookManifestBinaryHeader::BINARY_MANIFEST_FILENAME);

	// Step 1: Load entire file into memory
	TArray<uint8> fileData;
	if (!FFileHelper::LoadFileToArray(fileData, *filePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetCookManifest: Binary manifest not found at '%s'"), *filePath);
		return false;
	}

	FMemoryReader reader(fileData);

	// Step 2: Read and validate header
	FSGDynamicTextAssetCookManifestBinaryHeader header;
	header.Serialize(reader);

	if (reader.IsError())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetCookManifest: Failed to read header from '%s'"), *filePath);
		return false;
	}

	if (!header.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetCookManifest: Invalid magic number %s in '%s' (expected %s)"),
			*header.GetMagicNumberString(), *filePath,
			*FSGDynamicTextAssetCookManifestBinaryHeader::GetExpectedMagicNumberString());
		return false;
	}

	if (header.Version > FSGDynamicTextAssetCookManifestBinaryHeader::CURRENT_VERSION)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetCookManifest: Unsupported manifest version %u in '%s' (max supported: %u)"),
			header.Version, *filePath, FSGDynamicTextAssetCookManifestBinaryHeader::CURRENT_VERSION);
		return false;
	}

	// Step 3: Record entries start position (immediately after header)
	int64 entriesStartOffset = reader.Tell();

	// Step 4: Read raw binary entries into temporary storage
	struct FRawBinaryEntry
	{
		FGuid AssetGuid;
		FGuid TypeGuid;
		uint32 ClassNameIndex;
		uint32 UserFacingIdIndex;
	};

	TArray<FRawBinaryEntry> rawEntries;
	rawEntries.Reserve(header.EntryCount);

	for (uint32 i = 0; i < header.EntryCount; ++i)
	{
		FRawBinaryEntry rawEntry;
		reader << rawEntry.AssetGuid;
		reader << rawEntry.TypeGuid;
		reader << rawEntry.ClassNameIndex;
		reader << rawEntry.UserFacingIdIndex;

		if (reader.IsError())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDynamicTextAssetCookManifest: Failed to read entry %u of %u from '%s'"),
				i, header.EntryCount, *filePath);
			return false;
		}

		rawEntries.Add(rawEntry);
	}

	// Step 5: Seek to string table and deserialize
	reader.Seek(header.StringTableOffset);

	FSGDynamicTextAssetCookManifestStringTable stringTable;
	if (!stringTable.DeserializeFrom(reader))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetCookManifest: Failed to deserialize string table from '%s'"), *filePath);
		return false;
	}

	// Step 6: Verify content hash (entries + string table bytes)
	int64 contentEndOffset = reader.Tell();

	if (header.HasContentHash())
	{
		int64 contentSize = contentEndOffset - entriesStartOffset;

		uint8 fullHash[20];
		FSHA1 sha1;
		sha1.Update(fileData.GetData() + entriesStartOffset, static_cast<uint32>(contentSize));
		sha1.Final();
		sha1.GetHash(fullHash);

		if (FMemory::Memcmp(fullHash, header.ContentHash,
			FSGDynamicTextAssetCookManifestBinaryHeader::CONTENT_HASH_SIZE) != 0)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDynamicTextAssetCookManifest: Content hash mismatch in '%s' — file may be corrupt"),
				*filePath);
			return false;
		}
	}

	// Step 7: Build manifest entries from raw data + resolved strings
	Entries.Reserve(rawEntries.Num());

	for (const FRawBinaryEntry& rawEntry : rawEntries)
	{
		FSGDynamicTextAssetId id = FSGDynamicTextAssetId::FromGuid(rawEntry.AssetGuid);
		FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::FromGuid(rawEntry.TypeGuid);
		const FString& className = stringTable.GetString(rawEntry.ClassNameIndex);
		const FString& userFacingId = stringTable.GetString(rawEntry.UserFacingIdIndex);

		AddEntry(id, className, userFacingId, typeId);
	}

	bIsLoaded = 1;

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDynamicTextAssetCookManifest: Loaded binary manifest with %d entries from '%s'"),
		Entries.Num(), *filePath);
	return true;
}

bool FSGDynamicTextAssetCookManifest::IsLoaded() const
{
	return bIsLoaded;
}

const FSGDynamicTextAssetCookManifestEntry* FSGDynamicTextAssetCookManifest::FindById(const FSGDynamicTextAssetId& Id) const
{
	const int32* indexPtr = IdIndex.Find(Id);
	if (indexPtr == nullptr)
	{
		return nullptr;
	}

	return &Entries[*indexPtr];
}

void FSGDynamicTextAssetCookManifest::FindByClassName(const FString& ClassName, TArray<const FSGDynamicTextAssetCookManifestEntry*>& OutEntries) const
{
	const TArray<int32>* indicesPtr = ClassNameIndex.Find(ClassName);
	if (indicesPtr == nullptr)
	{
		return;
	}

	OutEntries.Reserve(OutEntries.Num() + indicesPtr->Num());
	for (int32 index : *indicesPtr)
	{
		OutEntries.Add(&Entries[index]);
	}
}

void FSGDynamicTextAssetCookManifest::FindByAssetTypeId(const FSGDynamicTextAssetTypeId& AssetTypeId, TArray<const FSGDynamicTextAssetCookManifestEntry*>& OutEntries) const
{
	const TArray<int32>* indicesPtr = AssetTypeIdIndex.Find(AssetTypeId);
	if (indicesPtr == nullptr)
	{
		return;
	}

	OutEntries.Reserve(OutEntries.Num() + indicesPtr->Num());
	for (int32 index : *indicesPtr)
	{
		OutEntries.Add(&Entries[index]);
	}
}

const TArray<FSGDynamicTextAssetCookManifestEntry>& FSGDynamicTextAssetCookManifest::GetEntries() const
{
	return Entries;
}

int32 FSGDynamicTextAssetCookManifest::Num() const
{
	return Entries.Num();
}

void FSGDynamicTextAssetCookManifest::Clear()
{
	Entries.Empty();
	IdIndex.Empty();
	ClassNameIndex.Empty();
	AssetTypeIdIndex.Empty();
	bIsLoaded = 0;
}

void FSGDynamicTextAssetCookManifest::RebuildIndices()
{
	IdIndex.Empty(Entries.Num());
	ClassNameIndex.Empty();
	AssetTypeIdIndex.Empty();

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FSGDynamicTextAssetCookManifestEntry& entry = Entries[i];
		IdIndex.Add(entry.Id, i);

		TArray<int32>& classIndices = ClassNameIndex.FindOrAdd(entry.ClassName);
		classIndices.Add(i);

		if (entry.AssetTypeId.IsValid())
		{
			TArray<int32>& typeIdIndices = AssetTypeIdIndex.FindOrAdd(entry.AssetTypeId);
			typeIdIndices.Add(i);
		}
	}
}

