// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDTAExtenderManifest.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Management/SGDTAExtenderManifestBinaryHeader.h"
#include "Management/SGDTACookManifestStringTable.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SGDynamicTextAssetLogs.h"

const FString FSGDTAExtenderManifest::KEY_SCHEMA = TEXT("schema");
const FString FSGDTAExtenderManifest::VALUE_SCHEMA = TEXT("dta_extender_manifest");
const FString FSGDTAExtenderManifest::KEY_VERSION = TEXT("version");
const FString FSGDTAExtenderManifest::KEY_FRAMEWORK = TEXT("framework");
const FString FSGDTAExtenderManifest::KEY_EXTENDERS = TEXT("extenders");
const FString FSGDTAExtenderManifest::KEY_EXTENDER_ID = TEXT("extenderId");
const FString FSGDTAExtenderManifest::KEY_CLASS_NAME = TEXT("className");
const FString FSGDTAExtenderManifest::KEY_CLASS_PATH = TEXT("classPath");

FSGDTAExtenderManifest::FSGDTAExtenderManifest()
	: FrameworkKey(NAME_None)
{
}

FSGDTAExtenderManifest::FSGDTAExtenderManifest(FName InFrameworkKey)
	: FrameworkKey(InFrameworkKey)
{
}

bool FSGDTAExtenderManifest::LoadFromFile(const FString& FilePath)
{
	Clear();

	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest [%s]: Manifest not found at '%s'"),
			*FrameworkKey.ToString(), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Failed to parse manifest JSON from '%s'"),
			*FrameworkKey.ToString(), *FilePath);
		return false;
	}

	// Validate schema
	FString schema;
	if (!rootObject->TryGetStringField(KEY_SCHEMA, schema) || schema != VALUE_SCHEMA)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Invalid schema in manifest '%s'"),
			*FrameworkKey.ToString(), *FilePath);
		return false;
	}

	// Validate version
	int32 version = 0;
	if (!rootObject->TryGetNumberField(KEY_VERSION, version) || version > MANIFEST_VERSION)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Unsupported manifest version %d (max supported: %d)"),
			*FrameworkKey.ToString(), version, MANIFEST_VERSION);
		return false;
	}

	// Read framework key from file
	FString frameworkString;
	if (rootObject->TryGetStringField(KEY_FRAMEWORK, frameworkString))
	{
		FrameworkKey = FName(*frameworkString);
	}

	// Parse extenders array
	const TArray<TSharedPtr<FJsonValue>>* extendersArray = nullptr;
	if (!rootObject->TryGetArrayField(KEY_EXTENDERS, extendersArray) || extendersArray == nullptr)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: No '%s' array in manifest '%s'"),
			*FrameworkKey.ToString(), *KEY_EXTENDERS, *FilePath);
		return false;
	}

	Entries.Reserve(extendersArray->Num());

	for (const TSharedPtr<FJsonValue>& extenderValue : *extendersArray)
	{
		const TSharedPtr<FJsonObject>* extenderObjectPtr = nullptr;
		if (!extenderValue.IsValid() || !extenderValue->TryGetObject(extenderObjectPtr) || extenderObjectPtr == nullptr)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTAExtenderManifest [%s]: Skipping invalid extender entry in manifest"),
				*FrameworkKey.ToString());
			continue;
		}

		FSGDTASerializerExtenderRegistryEntry entry;
		if (ParseExtenderEntry(*extenderObjectPtr, entry))
		{
			Entries.Add(MoveTemp(entry));
		}
	}

	bIsDirty = false;

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDTAExtenderManifest [%s]: Loaded manifest with %d extenders from '%s'"),
		*FrameworkKey.ToString(), Entries.Num(), *FilePath);
	return true;
}

bool FSGDTAExtenderManifest::SaveToBinaryFile(const FString& FilePath) const
{
	// Step 1: Build deduplicated string table from all entries
	FSGDynamicTextAssetCookManifestStringTable stringTable;

	struct FBinaryEntryIndices
	{
		uint32 ClassNameIndex;
		uint32 ClassPathIndex;
	};

	TArray<FBinaryEntryIndices> entryIndices;
	entryIndices.Reserve(Entries.Num());

	for (const FSGDTASerializerExtenderRegistryEntry& entry : Entries)
	{
		FBinaryEntryIndices indices;
		indices.ClassNameIndex = stringTable.AddString(entry.ClassName);

		FString classPath = entry.Class.ToString();
		indices.ClassPathIndex = classPath.IsEmpty()
			? FSGDynamicTextAssetCookManifestStringTable::INVALID_STRING_INDEX
			: stringTable.AddString(classPath);

		entryIndices.Add(indices);
	}

	// Step 2: Create memory writer with reserved buffer
	// 24 bytes per entry: 16 (FGuid) + 4 (ClassNameIndex) + 4 (ClassPathIndex)
	TArray<uint8> buffer;
	buffer.Reserve(FSGDTAExtenderManifestBinaryHeader::HEADER_SIZE + (Entries.Num() * 24) + 4096);
	FMemoryWriter writer(buffer);

	// Step 3: Write header with placeholder ContentHash
	FSGDTAExtenderManifestBinaryHeader header;
	header.EntryCount = static_cast<uint32>(Entries.Num());
	// StringTableOffset and ContentHash will be patched later
	header.Serialize(writer);

	// Step 4: Record entries start position (immediately after header)
	int64 entriesStartOffset = writer.Tell();

	// Step 5: Write each entry (24 bytes each)
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FSGDTASerializerExtenderRegistryEntry& entry = Entries[i];
		FBinaryEntryIndices& indices = entryIndices[i];

		// Write FGuid for ExtenderId (16 bytes)
		FGuid extenderGuid = entry.ExtenderId.GetGuid();
		writer << extenderGuid;

		// Write string table indices (4 + 4 = 8 bytes)
		writer << indices.ClassNameIndex;
		writer << indices.ClassPathIndex;
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
		writer.Serialize(fullHash, FSGDTAExtenderManifestBinaryHeader::CONTENT_HASH_SIZE);
		writer.Seek(buffer.Num());
	}

	// Step 10: Write buffer to file
	if (!FFileHelper::SaveArrayToFile(buffer, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Failed to write binary manifest to '%s'"),
			*FrameworkKey.ToString(), *FilePath);
		return false;
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDTAExtenderManifest [%s]: Saved binary manifest with %d entries (%d unique strings) to '%s'"),
		*FrameworkKey.ToString(), Entries.Num(), stringTable.Num(), *FilePath);
	return true;
}

bool FSGDTAExtenderManifest::LoadFromBinaryFile(const FString& FilePath)
{
	Clear();

	// Step 1: Load entire file into memory
	TArray<uint8> fileData;
	if (!FFileHelper::LoadFileToArray(fileData, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest [%s]: Binary manifest not found at '%s'"),
			*FrameworkKey.ToString(), *FilePath);
		return false;
	}

	FMemoryReader reader(fileData);

	// Step 2: Read and validate header
	FSGDTAExtenderManifestBinaryHeader header;
	header.Serialize(reader);

	if (reader.IsError())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Failed to read header from '%s'"),
			*FrameworkKey.ToString(), *FilePath);
		return false;
	}

	if (!header.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Invalid magic number %s in '%s' (expected %s)"),
			*FrameworkKey.ToString(),
			*header.GetMagicNumberString(), *FilePath,
			*FSGDTAExtenderManifestBinaryHeader::GetExpectedMagicNumberString());
		return false;
	}

	if (header.Version > FSGDTAExtenderManifestBinaryHeader::CURRENT_VERSION)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Unsupported manifest version %u in '%s' (max supported: %u)"),
			*FrameworkKey.ToString(),
			header.Version, *FilePath, FSGDTAExtenderManifestBinaryHeader::CURRENT_VERSION);
		return false;
	}

	// Step 3: Record entries start position (immediately after header)
	int64 entriesStartOffset = reader.Tell();

	// Step 4: Read raw binary entries into temporary storage
	struct FRawBinaryEntry
	{
		FGuid ExtenderGuid;
		uint32 ClassNameIndex;
		uint32 ClassPathIndex;
	};

	TArray<FRawBinaryEntry> rawEntries;
	rawEntries.Reserve(header.EntryCount);

	for (uint32 i = 0; i < header.EntryCount; ++i)
	{
		FRawBinaryEntry rawEntry;
		reader << rawEntry.ExtenderGuid;
		reader << rawEntry.ClassNameIndex;
		reader << rawEntry.ClassPathIndex;

		if (reader.IsError())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDTAExtenderManifest [%s]: Failed to read entry %u of %u from '%s'"),
				*FrameworkKey.ToString(), i, header.EntryCount, *FilePath);
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
			TEXT("FSGDTAExtenderManifest [%s]: Failed to deserialize string table from '%s'"),
			*FrameworkKey.ToString(), *FilePath);
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
			FSGDTAExtenderManifestBinaryHeader::CONTENT_HASH_SIZE) != 0)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDTAExtenderManifest [%s]: Content hash mismatch in '%s' - file may be corrupt"),
				*FrameworkKey.ToString(), *FilePath);
			return false;
		}
	}

	// Step 7: Build manifest entries from raw data + resolved strings
	Entries.Reserve(rawEntries.Num());

	for (const FRawBinaryEntry& rawEntry : rawEntries)
	{
		FSGDTASerializerExtenderRegistryEntry entry;
		entry.ExtenderId = FSGDTAClassId::FromGuid(rawEntry.ExtenderGuid);
		entry.ClassName = stringTable.GetString(rawEntry.ClassNameIndex);

		const FString& classPath = stringTable.GetString(rawEntry.ClassPathIndex);
		if (!classPath.IsEmpty())
		{
			entry.Class = TSoftClassPtr<UObject>(FSoftObjectPath(classPath));
		}

		Entries.Add(MoveTemp(entry));
	}

	bIsDirty = false;

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDTAExtenderManifest [%s]: Loaded binary manifest with %d entries from '%s'"),
		*FrameworkKey.ToString(), Entries.Num(), *FilePath);
	return true;
}

bool FSGDTAExtenderManifest::SaveToFile(const FString& FilePath) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(KEY_SCHEMA, VALUE_SCHEMA);
	rootObject->SetNumberField(KEY_VERSION, MANIFEST_VERSION);
	rootObject->SetStringField(KEY_FRAMEWORK, FrameworkKey.ToString());

	TArray<TSharedPtr<FJsonValue>> extendersArray;
	extendersArray.Reserve(Entries.Num());

	for (const FSGDTASerializerExtenderRegistryEntry& entry : Entries)
	{
		TSharedRef<FJsonObject> extenderObject = MakeShared<FJsonObject>();
		extenderObject->SetStringField(KEY_EXTENDER_ID, entry.ExtenderId.ToString());
		extenderObject->SetStringField(KEY_CLASS_NAME, entry.ClassName);
		extenderObject->SetStringField(KEY_CLASS_PATH, entry.Class.ToString());
		extendersArray.Add(MakeShared<FJsonValueObject>(extenderObject));
	}

	rootObject->SetArrayField(KEY_EXTENDERS, extendersArray);

	FString outputString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outputString);

	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Failed to serialize manifest JSON"),
			*FrameworkKey.ToString());
		return false;
	}

	if (!FFileHelper::SaveStringToFile(outputString, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDTAExtenderManifest [%s]: Failed to write manifest to '%s'"),
			*FrameworkKey.ToString(), *FilePath);
		return false;
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDTAExtenderManifest [%s]: Saved manifest with %d extenders to '%s'"),
		*FrameworkKey.ToString(), Entries.Num(), *FilePath);
	return true;
}

const FSGDTASerializerExtenderRegistryEntry* FSGDTAExtenderManifest::FindByExtenderId(
	const FSGDTAClassId& ExtenderId) const
{
	for (const FSGDTASerializerExtenderRegistryEntry& entry : Entries)
	{
		if (entry.ExtenderId == ExtenderId)
		{
			return &entry;
		}
	}
	return nullptr;
}

const FSGDTASerializerExtenderRegistryEntry* FSGDTAExtenderManifest::FindByClassName(
	const FString& ClassName) const
{
	for (const FSGDTASerializerExtenderRegistryEntry& entry : Entries)
	{
		if (entry.ClassName == ClassName)
		{
			return &entry;
		}
	}
	return nullptr;
}

void FSGDTAExtenderManifest::AddExtender(const FSGDTAClassId& ExtenderId,
	const TSoftClassPtr<UObject>& InClass)
{
	if (!ExtenderId.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest [%s]: Ignoring entry with invalid ExtenderId"),
			*FrameworkKey.ToString());
		return;
	}

	// Check if extender already exists - replace it
	for (FSGDTASerializerExtenderRegistryEntry& entry : Entries)
	{
		if (entry.ExtenderId == ExtenderId)
		{
			entry = FSGDTASerializerExtenderRegistryEntry(ExtenderId, InClass);
			bIsDirty = true;
			return;
		}
	}

	// New entry
	Entries.Emplace(ExtenderId, InClass);
	bIsDirty = true;
}

bool FSGDTAExtenderManifest::RemoveExtender(const FSGDTAClassId& ExtenderId)
{
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].ExtenderId == ExtenderId)
		{
			Entries.RemoveAtSwap(i);
			bIsDirty = true;
			return true;
		}
	}
	return false;
}

const TArray<FSGDTASerializerExtenderRegistryEntry>& FSGDTAExtenderManifest::GetAllExtenders() const
{
	return Entries;
}

void FSGDTAExtenderManifest::GetAllEffectiveExtenders(
	TArray<FSGDTASerializerExtenderRegistryEntry>& OutEntries) const
{
	OutEntries.Reset();

	if (!HasServerOverrides())
	{
		OutEntries = Entries;
		return;
	}

	OutEntries.Reserve(Entries.Num() + ServerOverlayEntries.Num());
	TSet<FSGDTAClassId> processedIds;

	// Process local entries with server overlay consideration
	for (const FSGDTASerializerExtenderRegistryEntry& entry : Entries)
	{
		if (const FSGDTASerializerExtenderRegistryEntry* effective = GetEffectiveEntry(entry.ExtenderId))
		{
			OutEntries.Add(*effective);
		}
		processedIds.Add(entry.ExtenderId);
	}

	// Add server-only entries (new extenders not in local manifest)
	for (const TPair<FSGDTAClassId, FSGDTASerializerExtenderRegistryEntry>& pair : ServerOverlayEntries)
	{
		if (!processedIds.Contains(pair.Key) && !pair.Value.ClassName.IsEmpty())
		{
			OutEntries.Add(pair.Value);
		}
	}
}

TSoftClassPtr<UObject> FSGDTAExtenderManifest::GetSoftClassPtr(const FSGDTAClassId& ExtenderId) const
{
	const FSGDTASerializerExtenderRegistryEntry* effectiveEntry = GetEffectiveEntry(ExtenderId);
	if (effectiveEntry == nullptr)
	{
		return TSoftClassPtr<UObject>();
	}
	return effectiveEntry->Class;
}

TSoftClassPtr<UObject> FSGDTAExtenderManifest::GetSoftClassPtrByClassName(const FString& ClassName) const
{
	const FSGDTASerializerExtenderRegistryEntry* entry = FindByClassName(ClassName);
	if (entry == nullptr)
	{
		return TSoftClassPtr<UObject>();
	}
	return entry->Class;
}

const FSGDTASerializerExtenderRegistryEntry* FSGDTAExtenderManifest::GetEffectiveEntry(
	const FSGDTAClassId& ExtenderId) const
{
	// Server overlay takes precedence
	const FSGDTASerializerExtenderRegistryEntry* overlayEntry = ServerOverlayEntries.Find(ExtenderId);
	if (overlayEntry != nullptr)
	{
		// Empty ClassName means the extender is disabled by the server
		if (overlayEntry->ClassName.IsEmpty())
		{
			return nullptr;
		}
		return overlayEntry;
	}

	// Fall through to local entry
	return FindByExtenderId(ExtenderId);
}

int32 FSGDTAExtenderManifest::Num() const
{
	return Entries.Num();
}

bool FSGDTAExtenderManifest::IsDirty() const
{
	return bIsDirty;
}

void FSGDTAExtenderManifest::Clear()
{
	Entries.Empty();
	bIsDirty = false;
}

FName FSGDTAExtenderManifest::GetFrameworkKey() const
{
	return FrameworkKey;
}

void FSGDTAExtenderManifest::ApplyServerOverrides(const TSharedPtr<FJsonObject>& ServerData)
{
	if (!ServerData.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest [%s]: Invalid server data"),
			*FrameworkKey.ToString());
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* extendersArray = nullptr;
	if (!ServerData->TryGetArrayField(KEY_EXTENDERS, extendersArray) || extendersArray == nullptr)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest [%s]: No '%s' array in server data"),
			*FrameworkKey.ToString(), *KEY_EXTENDERS);
		return;
	}

	for (const TSharedPtr<FJsonValue>& extenderValue : *extendersArray)
	{
		const TSharedPtr<FJsonObject>* extenderObjectPtr = nullptr;
		if (!extenderValue.IsValid() || !extenderValue->TryGetObject(extenderObjectPtr) || extenderObjectPtr == nullptr)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTAExtenderManifest [%s]: Skipping invalid server extender entry"),
				*FrameworkKey.ToString());
			continue;
		}

		const TSharedPtr<FJsonObject>& extenderObject = *extenderObjectPtr;

		// ExtenderId is always required
		FString extenderIdString;
		if (!extenderObject->TryGetStringField(KEY_EXTENDER_ID, extenderIdString))
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTAExtenderManifest [%s]: Skipping entry with missing extenderId"),
				*FrameworkKey.ToString());
			continue;
		}

		FSGDTAClassId extenderId = FSGDTAClassId::FromString(extenderIdString);
		if (!extenderId.IsValid())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTAExtenderManifest [%s]: Skipping entry with invalid extenderId '%s'"),
				*FrameworkKey.ToString(), *extenderIdString);
			continue;
		}

		// ClassName - empty means "disable this extender"
		FString className;
		extenderObject->TryGetStringField(KEY_CLASS_NAME, className);

		if (className.IsEmpty())
		{
			// Empty className signals removal/disable - store a sentinel entry
			FSGDTASerializerExtenderRegistryEntry disabledEntry;
			disabledEntry.ExtenderId = extenderId;
			ServerOverlayEntries.Add(extenderId, MoveTemp(disabledEntry));
			continue;
		}

		// Build a soft class path from the class name
		FString classPath = FString::Printf(TEXT("/Script/%s"), *className);

		FSGDTASerializerExtenderRegistryEntry overlayEntry;
		overlayEntry.ExtenderId = extenderId;
		overlayEntry.ClassName = className;
		overlayEntry.Class = TSoftClassPtr<UObject>(FSoftObjectPath(classPath));

		ServerOverlayEntries.Add(extenderId, MoveTemp(overlayEntry));
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDTAExtenderManifest [%s]: Applied %d server overlay entries"),
		*FrameworkKey.ToString(), ServerOverlayEntries.Num());
}

void FSGDTAExtenderManifest::ClearServerOverrides()
{
	ServerOverlayEntries.Empty();
}

bool FSGDTAExtenderManifest::HasServerOverrides() const
{
	return !ServerOverlayEntries.IsEmpty();
}

bool FSGDTAExtenderManifest::ParseExtenderEntry(const TSharedPtr<FJsonObject>& EntryObject,
	FSGDTASerializerExtenderRegistryEntry& OutEntry)
{
	FString extenderIdString;
	FString className;

	if (!EntryObject->TryGetStringField(KEY_EXTENDER_ID, extenderIdString)
		|| !EntryObject->TryGetStringField(KEY_CLASS_NAME, className))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest: Skipping extender entry with missing required fields"));
		return false;
	}

	FSGDTAClassId extenderId = FSGDTAClassId::FromString(extenderIdString);
	if (!extenderId.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest: Skipping extender entry with invalid extenderId '%s'"),
			*extenderIdString);
		return false;
	}

	OutEntry.ExtenderId = extenderId;
	OutEntry.ClassName = className;

	// Use the full class path for reliable TSoftClassPtr resolution in packaged builds.
	FString classPath;
	if (EntryObject->TryGetStringField(KEY_CLASS_PATH, classPath) && !classPath.IsEmpty())
	{
		OutEntry.Class = TSoftClassPtr<UObject>(FSoftObjectPath(classPath));
	}
	else
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDTAExtenderManifest: Entry '%s' has no classPath, class resolution may fail in packaged builds"),
			*className);
		OutEntry.Class = TSoftClassPtr<UObject>(FSoftObjectPath(className));
	}

	return true;
}
