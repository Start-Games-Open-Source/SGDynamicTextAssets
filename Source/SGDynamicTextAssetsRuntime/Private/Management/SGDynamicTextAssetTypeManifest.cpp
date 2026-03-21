// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDynamicTextAssetTypeManifest.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SGDynamicTextAssetLogs.h"

const FString FSGDynamicTextAssetTypeManifest::KEY_SCHEMA = TEXT("schema");
const FString FSGDynamicTextAssetTypeManifest::VALUE_SCHEMA = TEXT("dta_type_manifest");
const FString FSGDynamicTextAssetTypeManifest::KEY_VERSION = TEXT("version");
const FString FSGDynamicTextAssetTypeManifest::KEY_ROOT_TYPE_ID = TEXT("rootTypeId");
const FString FSGDynamicTextAssetTypeManifest::KEY_TYPES = TEXT("types");
const FString FSGDynamicTextAssetTypeManifest::KEY_TYPE_ID = TEXT("typeId");
const FString FSGDynamicTextAssetTypeManifest::KEY_CLASS_NAME = TEXT("className");
const FString FSGDynamicTextAssetTypeManifest::KEY_CLASS_PATH = TEXT("classPath");
const FString FSGDynamicTextAssetTypeManifest::KEY_PARENT_TYPE_ID = TEXT("parentTypeId");

FSGDynamicTextAssetTypeManifestEntry::FSGDynamicTextAssetTypeManifestEntry(
	const FSGDynamicTextAssetTypeId& InTypeId,
	const TSoftClassPtr<UObject>& InClass,
	const FSGDynamicTextAssetTypeId& InParentTypeId)
	: TypeId(InTypeId)
	, Class(InClass)
	, ParentTypeId(InParentTypeId)
{
	// Extract class name from the soft class path for fast lookup
	FString classPath = InClass.ToString();
	if (!classPath.IsEmpty())
	{
		// The class name is the last segment after the last dot or slash
		int32 lastDot = INDEX_NONE;
		if (classPath.FindLastChar(TEXT('.'), lastDot))
		{
			ClassName = classPath.Mid(lastDot + 1);
		}
		else
		{
			ClassName = classPath;
		}
	}
}

bool FSGDynamicTextAssetTypeManifest::LoadFromFile(const FString& FilePath)
{
	Clear();

	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetTypeManifest: Manifest not found at '%s'"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetTypeManifest: Failed to parse manifest JSON from '%s'"), *FilePath);
		return false;
	}

	// Validate schema
	FString schema;
	if (!rootObject->TryGetStringField(KEY_SCHEMA, schema) || schema != VALUE_SCHEMA)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetTypeManifest: Invalid schema in manifest '%s'"), *FilePath);
		return false;
	}

	// Validate version
	int32 version = 0;
	if (!rootObject->TryGetNumberField(KEY_VERSION, version) || version > MANIFEST_VERSION)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetTypeManifest: Unsupported manifest version %d (max supported: %d)"),
			version, MANIFEST_VERSION);
		return false;
	}

	// Parse root type ID
	FString rootTypeIdString;
	if (rootObject->TryGetStringField(KEY_ROOT_TYPE_ID, rootTypeIdString))
	{
		RootTypeId.ParseString(rootTypeIdString);
	}

	// Parse types array
	const TArray<TSharedPtr<FJsonValue>>* typesArray = nullptr;
	if (!rootObject->TryGetArrayField(KEY_TYPES, typesArray) || typesArray == nullptr)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetTypeManifest: No '%s' array in manifest '%s'"), *KEY_TYPES, *FilePath);
		return false;
	}

	Types.Reserve(typesArray->Num());

	for (const TSharedPtr<FJsonValue>& typeValue : *typesArray)
	{
		const TSharedPtr<FJsonObject>* typeObjectPtr = nullptr;
		if (!typeValue.IsValid() || !typeValue->TryGetObject(typeObjectPtr) || typeObjectPtr == nullptr)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDynamicTextAssetTypeManifest: Skipping invalid type entry in manifest"));
			continue;
		}

		FSGDynamicTextAssetTypeManifestEntry entry;
		if (ParseTypeEntry(*typeObjectPtr, entry))
		{
			Types.Add(MoveTemp(entry));
		}
	}

	RebuildIndices();
	bIsDirty = false;

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDynamicTextAssetTypeManifest: Loaded manifest with %d types from '%s'"), Types.Num(), *FilePath);
	return true;
}

bool FSGDynamicTextAssetTypeManifest::SaveToFile(const FString& FilePath) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(KEY_SCHEMA, VALUE_SCHEMA);
	rootObject->SetNumberField(KEY_VERSION, MANIFEST_VERSION);
	rootObject->SetStringField(KEY_ROOT_TYPE_ID, RootTypeId.ToString());

	TArray<TSharedPtr<FJsonValue>> typesArray;
	typesArray.Reserve(Types.Num());

	for (const FSGDynamicTextAssetTypeManifestEntry& entry : Types)
	{
		TSharedRef<FJsonObject> typeObject = MakeShared<FJsonObject>();
		typeObject->SetStringField(KEY_TYPE_ID, entry.TypeId.ToString());
		typeObject->SetStringField(KEY_CLASS_NAME, entry.ClassName);
		typeObject->SetStringField(KEY_CLASS_PATH, entry.Class.ToString());
		typeObject->SetStringField(KEY_PARENT_TYPE_ID, entry.ParentTypeId.ToString());
		typesArray.Add(MakeShared<FJsonValueObject>(typeObject));
	}

	rootObject->SetArrayField(KEY_TYPES, typesArray);

	FString outputString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outputString);

	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetTypeManifest: Failed to serialize manifest JSON"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(outputString, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetTypeManifest: Failed to write manifest to '%s'"), *FilePath);
		return false;
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDynamicTextAssetTypeManifest: Saved manifest with %d types to '%s'"), Types.Num(), *FilePath);
	return true;
}

const FSGDynamicTextAssetTypeManifestEntry* FSGDynamicTextAssetTypeManifest::FindByTypeId(
	const FSGDynamicTextAssetTypeId& TypeId) const
{
	const int32* indexPtr = TypeIdIndex.Find(TypeId);
	if (indexPtr == nullptr)
	{
		return nullptr;
	}
	return &Types[*indexPtr];
}

const FSGDynamicTextAssetTypeManifestEntry* FSGDynamicTextAssetTypeManifest::FindByClassName(
	const FString& ClassName) const
{
	const int32* indexPtr = ClassNameIndex.Find(ClassName);
	if (indexPtr == nullptr)
	{
		return nullptr;
	}
	return &Types[*indexPtr];
}

void FSGDynamicTextAssetTypeManifest::AddType(const FSGDynamicTextAssetTypeId& TypeId,
	const TSoftClassPtr<UObject>& InClass, const FSGDynamicTextAssetTypeId& ParentTypeId)
{
	if (!TypeId.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetTypeManifest::AddType: Ignoring entry with invalid TypeId"));
		return;
	}

	// Check if type already exists  - replace it
	const int32* existingIndex = TypeIdIndex.Find(TypeId);
	if (existingIndex != nullptr)
	{
		FSGDynamicTextAssetTypeManifestEntry& existing = Types[*existingIndex];

		// Remove old class name index entry
		ClassNameIndex.Remove(existing.ClassName);

		// Update in place
		existing = FSGDynamicTextAssetTypeManifestEntry(TypeId, InClass, ParentTypeId);

		// Re-add class name index
		ClassNameIndex.Add(existing.ClassName, *existingIndex);
	}
	else
	{
		int32 index = Types.Emplace(TypeId, InClass, ParentTypeId);
		TypeIdIndex.Add(TypeId, index);
		ClassNameIndex.Add(Types[index].ClassName, index);
	}

	bIsDirty = true;
}

bool FSGDynamicTextAssetTypeManifest::RemoveType(const FSGDynamicTextAssetTypeId& TypeId)
{
	const int32* indexPtr = TypeIdIndex.Find(TypeId);
	if (indexPtr == nullptr)
	{
		return false;
	}

	int32 removeIndex = *indexPtr;
	const FString& className = Types[removeIndex].ClassName;

	// Remove from indices
	TypeIdIndex.Remove(TypeId);
	ClassNameIndex.Remove(className);

	// Swap-remove from array and fix up indices
	Types.RemoveAtSwap(removeIndex);

	// If we swapped the last element into this position, update its index entries
	if (removeIndex < Types.Num())
	{
		const FSGDynamicTextAssetTypeManifestEntry& swapped = Types[removeIndex];
		TypeIdIndex.Add(swapped.TypeId, removeIndex);
		ClassNameIndex.Add(swapped.ClassName, removeIndex);
	}

	bIsDirty = true;
	return true;
}

const TArray<FSGDynamicTextAssetTypeManifestEntry>& FSGDynamicTextAssetTypeManifest::GetAllTypes() const
{
	return Types;
}

void FSGDynamicTextAssetTypeManifest::GetAllEffectiveTypes(
	TArray<FSGDynamicTextAssetTypeManifestEntry>& OutEntries) const
{
	OutEntries.Reset();

	if (!HasServerOverrides())
	{
		// Fast path: no overlays, just return local entries directly
		OutEntries = Types;
		return;
	}

	OutEntries.Reserve(Types.Num() + ServerOverlayEntries.Num());
	TSet<FSGDynamicTextAssetTypeId> processedIds;

	// Process local entries with server overlay consideration
	for (const FSGDynamicTextAssetTypeManifestEntry& entry : Types)
	{
		const FSGDynamicTextAssetTypeManifestEntry* effective = GetEffectiveEntry(entry.TypeId);
		if (effective) // nullptr means disabled by server
		{
			OutEntries.Add(*effective);
		}
		processedIds.Add(entry.TypeId);
	}

	// Add server-only entries (new types not in local manifest)
	for (const TPair<FSGDynamicTextAssetTypeId, FSGDynamicTextAssetTypeManifestEntry>& pair : ServerOverlayEntries)
	{
		if (!processedIds.Contains(pair.Key) && !pair.Value.ClassName.IsEmpty())
		{
			OutEntries.Add(pair.Value);
		}
	}
}

TSoftClassPtr<UObject> FSGDynamicTextAssetTypeManifest::GetSoftClassPtr(const FSGDynamicTextAssetTypeId& TypeId) const
{
	const FSGDynamicTextAssetTypeManifestEntry* effectiveEntry = GetEffectiveEntry(TypeId);
	if (effectiveEntry == nullptr)
	{
		return TSoftClassPtr<UObject>();
	}

	return effectiveEntry->Class;
}

TSoftClassPtr<UObject> FSGDynamicTextAssetTypeManifest::GetSoftClassPtrByClassName(const FString& ClassName) const
{
	const FSGDynamicTextAssetTypeManifestEntry* entry = FindByClassName(ClassName);
	if (entry == nullptr)
	{
		return TSoftClassPtr<UObject>();
	}

	return entry->Class;
}

const FSGDynamicTextAssetTypeId& FSGDynamicTextAssetTypeManifest::GetRootTypeId() const
{
	return RootTypeId;
}

void FSGDynamicTextAssetTypeManifest::SetRootTypeId(const FSGDynamicTextAssetTypeId& InRootTypeId)
{
	RootTypeId = InRootTypeId;
	bIsDirty = true;
}

int32 FSGDynamicTextAssetTypeManifest::Num() const
{
	return Types.Num();
}

bool FSGDynamicTextAssetTypeManifest::IsDirty() const
{
	return bIsDirty;
}

void FSGDynamicTextAssetTypeManifest::Clear()
{
	Types.Empty();
	TypeIdIndex.Empty();
	ClassNameIndex.Empty();
	RootTypeId.Invalidate();
	bIsDirty = false;
}

void FSGDynamicTextAssetTypeManifest::ApplyServerOverrides(const TSharedPtr<FJsonObject>& ServerData)
{
	if (!ServerData.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetTypeManifest::ApplyServerOverrides: Invalid server data"));
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* typesArray = nullptr;
	if (!ServerData->TryGetArrayField(KEY_TYPES, typesArray) || typesArray == nullptr)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetTypeManifest::ApplyServerOverrides: No '%s' array in server data"), *KEY_TYPES);
		return;
	}

	for (const TSharedPtr<FJsonValue>& typeValue : *typesArray)
	{
		const TSharedPtr<FJsonObject>* typeObjectPtr = nullptr;
		if (!typeValue.IsValid() || !typeValue->TryGetObject(typeObjectPtr) || typeObjectPtr == nullptr)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDynamicTextAssetTypeManifest::ApplyServerOverrides: Skipping invalid server type entry"));
			continue;
		}

		const TSharedPtr<FJsonObject>& typeObject = *typeObjectPtr;

		// TypeId is always required
		FString typeIdString;
		if (!typeObject->TryGetStringField(KEY_TYPE_ID, typeIdString))
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDynamicTextAssetTypeManifest::ApplyServerOverrides: Skipping entry with missing typeId"));
			continue;
		}

		FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::FromString(typeIdString);
		if (!typeId.IsValid())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDynamicTextAssetTypeManifest::ApplyServerOverrides: Skipping entry with invalid typeId '%s'"),
				*typeIdString);
			continue;
		}

		// ClassName  - empty means "disable this type"
		FString className;
		typeObject->TryGetStringField(KEY_CLASS_NAME, className);

		if (className.IsEmpty())
		{
			// Empty className signals removal/disable  - store a sentinel entry
			FSGDynamicTextAssetTypeManifestEntry disabledEntry;
			disabledEntry.TypeId = typeId;
			ServerOverlayEntries.Add(typeId, MoveTemp(disabledEntry));
			continue;
		}

		// Build a soft class path from the class name
		FString classPath = FString::Printf(TEXT("/Script/%s"), *className);

		FSGDynamicTextAssetTypeManifestEntry overlayEntry;
		overlayEntry.TypeId = typeId;
		overlayEntry.ClassName = className;
		overlayEntry.Class = TSoftClassPtr<UObject>(FSoftObjectPath(classPath));

		FString parentTypeIdString;
		if (typeObject->TryGetStringField(KEY_PARENT_TYPE_ID, parentTypeIdString))
		{
			overlayEntry.ParentTypeId.ParseString(parentTypeIdString);
		}

		ServerOverlayEntries.Add(typeId, MoveTemp(overlayEntry));
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDynamicTextAssetTypeManifest::ApplyServerOverrides: Applied %d server overlay entries"),
		ServerOverlayEntries.Num());
}

const FSGDynamicTextAssetTypeManifestEntry* FSGDynamicTextAssetTypeManifest::GetEffectiveEntry(
	const FSGDynamicTextAssetTypeId& TypeId) const
{
	// Server overlay takes precedence
	const FSGDynamicTextAssetTypeManifestEntry* overlayEntry = ServerOverlayEntries.Find(TypeId);
	if (overlayEntry != nullptr)
	{
		// Empty ClassName means the type is disabled by the server
		if (overlayEntry->ClassName.IsEmpty())
		{
			return nullptr;
		}
		return overlayEntry;
	}

	// Fall through to local entry
	return FindByTypeId(TypeId);
}

void FSGDynamicTextAssetTypeManifest::ClearServerOverrides()
{
	ServerOverlayEntries.Empty();
}

bool FSGDynamicTextAssetTypeManifest::HasServerOverrides() const
{
	return ServerOverlayEntries.Num() > 0;
}

void FSGDynamicTextAssetTypeManifest::RebuildIndices()
{
	TypeIdIndex.Empty(Types.Num());
	ClassNameIndex.Empty(Types.Num());

	for (int32 i = 0; i < Types.Num(); ++i)
	{
		const FSGDynamicTextAssetTypeManifestEntry& entry = Types[i];
		TypeIdIndex.Add(entry.TypeId, i);
		if (!entry.ClassName.IsEmpty())
		{
			ClassNameIndex.Add(entry.ClassName, i);
		}
	}
}

bool FSGDynamicTextAssetTypeManifest::ParseTypeEntry(const TSharedPtr<FJsonObject>& EntryObject,
	FSGDynamicTextAssetTypeManifestEntry& OutEntry)
{
	FString typeIdString;
	FString className;

	if (!EntryObject->TryGetStringField(KEY_TYPE_ID, typeIdString)
		|| !EntryObject->TryGetStringField(KEY_CLASS_NAME, className))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetTypeManifest: Skipping type entry with missing required fields"));
		return false;
	}

	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::FromString(typeIdString);
	if (!typeId.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetTypeManifest: Skipping type entry with invalid typeId '%s'"), *typeIdString);
		return false;
	}

	OutEntry.TypeId = typeId;
	OutEntry.ClassName = className;

	// Use the full class path for reliable TSoftClassPtr resolution in packaged builds.
	// Bare class names (e.g., "SGDynamicTextAsset") are not valid FSoftObjectPath values  -
	// the full path (e.g., "/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset") is required.
	FString classPath;
	if (EntryObject->TryGetStringField(KEY_CLASS_PATH, classPath) && !classPath.IsEmpty())
	{
		OutEntry.Class = TSoftClassPtr<UObject>(FSoftObjectPath(classPath));
	}
	else
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetTypeManifest: Entry '%s' has no classPath, class resolution may fail in packaged builds"),
			*className);
		OutEntry.Class = TSoftClassPtr<UObject>(FSoftObjectPath(className));
	}

	// Parse optional parent type ID
	FString parentTypeIdString;
	if (EntryObject->TryGetStringField(KEY_PARENT_TYPE_ID, parentTypeIdString))
	{
		OutEntry.ParentTypeId.ParseString(parentTypeIdString);
	}

	return true;
}
