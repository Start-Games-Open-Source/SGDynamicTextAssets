// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDynamicTextAssetEditorCache.h"

#if WITH_EDITOR

#include "Core/ISGDynamicTextAssetProvider.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "SGDynamicTextAssetsRuntimeModule.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Editor.h"

FSGDynamicTextAssetEditorCache* FSGDynamicTextAssetEditorCache::Instance = nullptr;

FSGDynamicTextAssetEditorCache& FSGDynamicTextAssetEditorCache::Get()
{
	if (!Instance)
	{
		Instance = new FSGDynamicTextAssetEditorCache();
	}
	return *Instance;
}

void FSGDynamicTextAssetEditorCache::TearDown()
{
	if (Instance)
	{
		delete Instance;
		Instance = nullptr;
	}
}

FSGDynamicTextAssetEditorCache::FSGDynamicTextAssetEditorCache()
{
	if (GEditor)
	{
		PrePIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &FSGDynamicTextAssetEditorCache::OnPreBeginPIE);
		EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FSGDynamicTextAssetEditorCache::OnEndPIE);
	}
}

FSGDynamicTextAssetEditorCache::~FSGDynamicTextAssetEditorCache()
{
	FEditorDelegates::PreBeginPIE.Remove(PrePIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
	CachedObjects.Empty();
}

TScriptInterface<ISGDynamicTextAssetProvider> FSGDynamicTextAssetEditorCache::LoadDynamicTextAsset(const FSGDynamicTextAssetId& Id)
{
	if (!Id.IsValid())
	{
		return TScriptInterface<ISGDynamicTextAssetProvider>();
	}

	// Check cache first
	if (const TScriptInterface<ISGDynamicTextAssetProvider>* found = CachedObjects.Find(Id))
	{
		if (found->GetObject())
		{
			return *found;
		}
		// Stale entry, remove it
		CachedObjects.Remove(Id);
	}

	return Internal_LoadFromFile(Id);
}

TScriptInterface<ISGDynamicTextAssetProvider> FSGDynamicTextAssetEditorCache::GetDynamicTextAsset(const FSGDynamicTextAssetId& Id) const
{
	if (!Id.IsValid())
	{
		return TScriptInterface<ISGDynamicTextAssetProvider>();
	}

	if (const TScriptInterface<ISGDynamicTextAssetProvider>* found = CachedObjects.Find(Id))
	{
		if (found->GetObject())
		{
			return *found;
		}
	}

	return TScriptInterface<ISGDynamicTextAssetProvider>();
}

bool FSGDynamicTextAssetEditorCache::IsCached(const FSGDynamicTextAssetId& Id) const
{
	if (const TScriptInterface<ISGDynamicTextAssetProvider>* found = CachedObjects.Find(Id))
	{
		return found->GetObject() != nullptr;
	}
	return false;
}

void FSGDynamicTextAssetEditorCache::ClearCache()
{
	UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetEditorCache: Clearing %d cached object(s)"), CachedObjects.Num());
	CachedObjects.Empty();
}

bool FSGDynamicTextAssetEditorCache::RemoveFromCache(const FSGDynamicTextAssetId& Id)
{
	return CachedObjects.Remove(Id) > 0;
}

int32 FSGDynamicTextAssetEditorCache::GetCachedObjectCount() const
{
	return CachedObjects.Num();
}

void FSGDynamicTextAssetEditorCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<FSGDynamicTextAssetId, TScriptInterface<ISGDynamicTextAssetProvider>>& pair : CachedObjects)
	{
		if (TObjectPtr<UObject> obj = pair.Value.GetObject())
		{
			Collector.AddReferencedObject(obj);
		}
	}
}

FString FSGDynamicTextAssetEditorCache::GetReferencerName() const
{
	return TEXT("FSGDynamicTextAssetEditorCache");
}

void FSGDynamicTextAssetEditorCache::OnPreBeginPIE(const bool bIsSimulating)
{
	UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetEditorCache: PIE starting, clearing editor cache"));
	ClearCache();
}

void FSGDynamicTextAssetEditorCache::OnEndPIE(const bool bIsSimulating)
{
	UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetEditorCache: PIE ended, clearing editor cache"));
	ClearCache();
}

TScriptInterface<ISGDynamicTextAssetProvider> FSGDynamicTextAssetEditorCache::Internal_LoadFromFile(const FSGDynamicTextAssetId& Id)
{
	TScriptInterface<ISGDynamicTextAssetProvider> emptyProvider;

	// Step 1: Find the file on disk
	FString filePath;
	if (!FSGDynamicTextAssetFileManager::FindFileForId(Id, filePath))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetEditorCache: Could not find file for Id(%s)"), *Id.ToString());
		return emptyProvider;
	}

	// Step 2: Read raw file contents
	FString textPayload;
	uint32 serializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
	if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(filePath, textPayload, &serializerTypeId))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetEditorCache: Failed to read file FilePath(%s)"), *filePath);
		return emptyProvider;
	}

	// Step 3: Find the appropriate serializer
	TSharedPtr<ISGDynamicTextAssetSerializer> serializer =
		(serializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
		? FSGDynamicTextAssetFileManager::FindSerializerForTypeId(serializerTypeId)
		: FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
	if (!serializer.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetEditorCache: No serializer found for FilePath(%s)"), *filePath);
		return emptyProvider;
	}

	// Step 4: Extract metadata to resolve the class
	FSGDynamicTextAssetId metadataId;
	FString metadataClassName, metadataUserId, metadataVersion;
	FSGDynamicTextAssetTypeId metadataAssetTypeId;
	if (!serializer->ExtractMetadata(textPayload, metadataId, metadataClassName, metadataUserId, metadataVersion, metadataAssetTypeId))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetEditorCache: Failed to extract metadata from FilePath(%s)"), *filePath);
		return emptyProvider;
	}

	// Step 5: Resolve the class (TypeId first, fallback to className)
	UClass* dynamicTextAssetClass = nullptr;
	if (metadataAssetTypeId.IsValid())
	{
		if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
		{
			dynamicTextAssetClass = registry->ResolveClassForTypeId(metadataAssetTypeId);
		}
	}
	if (!dynamicTextAssetClass && !metadataClassName.IsEmpty())
	{
		dynamicTextAssetClass = FindFirstObject<UClass>(*metadataClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	}
	if (!dynamicTextAssetClass)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetEditorCache: Could not resolve class for Id(%s) ClassName(%s) TypeId(%s)"),
			*Id.ToString(), *metadataClassName, *metadataAssetTypeId.ToString());
		return emptyProvider;
	}

	// Step 6: Create new instance outed to GetTransientPackage()
	TScriptInterface<ISGDynamicTextAssetProvider> dataObject = MakeProvider(
		NewObject<UObject>(GetTransientPackage(), dynamicTextAssetClass));
	if (!dataObject)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetEditorCache: Failed to create instance of Class(%s)"), *GetNameSafe(dynamicTextAssetClass));
		return emptyProvider;
	}

	// Step 7: Deserialize
	bool bMigrated = false;
	if (!serializer->DeserializeProvider(textPayload, dataObject.GetInterface(), bMigrated))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetEditorCache: Failed to deserialize FilePath(%s)"), *filePath);
		return emptyProvider;
	}

	// Re-save if migration occurred
	if (bMigrated)
	{
		FString updatedContents;
		if (serializer->SerializeProvider(dataObject.GetInterface(), updatedContents))
		{
			if (FSGDynamicTextAssetFileManager::WriteRawFileContents(filePath, updatedContents))
			{
				UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
					TEXT("FSGDynamicTextAssetEditorCache: Re-saved migrated dynamic text asset to FilePath(%s)"), *filePath);
			}
			else
			{
				UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
					TEXT("FSGDynamicTextAssetEditorCache: Migration succeeded but failed to re-save FilePath(%s)"), *filePath);
			}
		}
	}

	// Populate UserFacingId if not set by deserialization
	if (dataObject->GetUserFacingId().IsEmpty())
	{
		FString fromPath = FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(filePath);
		if (!fromPath.IsEmpty())
		{
			dataObject->SetUserFacingId(fromPath);
		}
	}

	// Step 8: Cache the result
	CachedObjects.Add(dataObject->GetDynamicTextAssetId(), dataObject);

	// Post-load hook and validation
	dataObject->PostDynamicTextAssetLoaded();

	FSGDynamicTextAssetValidationResult validationResult;
	if (!dataObject->Native_ValidateDynamicTextAsset(validationResult))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetEditorCache: Loaded dynamic text asset has validation errors: Id(%s) - %s"),
			*dataObject->GetDynamicTextAssetId().ToString(),
			*validationResult.ToFormattedString());
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDynamicTextAssetEditorCache: Loaded Id(%s) Class(%s) FilePath(%s)"),
		*dataObject->GetDynamicTextAssetId().ToString(), *GetNameSafe(dataObject.GetObject()->GetClass()), *filePath);

	return dataObject;
}

TScriptInterface<ISGDynamicTextAssetProvider> FSGDynamicTextAssetEditorCache::MakeProvider(UObject* Object)
{
	if (!Object)
	{
		return TScriptInterface<ISGDynamicTextAssetProvider>();
	}

	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(Object);
	if (!provider)
	{
		return TScriptInterface<ISGDynamicTextAssetProvider>();
	}

	TScriptInterface<ISGDynamicTextAssetProvider> result;
	result.SetObject(Object);
	result.SetInterface(provider);
	return result;
}

#endif // WITH_EDITOR
