// Copyright Start Games, Inc. All Rights Reserved.

#include "Subsystem/SGDynamicTextAssetSubsystem.h"

#include "SGDynamicTextAssetLogs.h"
#include "Async/Async.h"
#include "Core/SGDynamicTextAsset.h"
#include "Engine/AssetManager.h"
#include "Core/SGDynamicTextAssetValidationResult.h"
#include "Management/SGDynamicTextAssetCookManifest.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "UObject/Package.h"
#include "Serialization/SGDynamicTextAssetSerializerBase.h"
#include "Server/SGDynamicTextAssetServerNullInterface.h"
#include "Templates/SubclassOf.h"

void USGDynamicTextAssetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Always create a valid server interface (null pattern)
    ServerInterface = NewObject<USGDynamicTextAssetServerNullInterface>(this);

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("SGDynamicTextAssetSubsystem initialized ServerInterface(%s)"),
        *GetNameSafe(ServerInterface));
}

void USGDynamicTextAssetSubsystem::Deinitialize()
{
    ClearCache();
    ServerInterface = nullptr;
    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("SGDynamicTextAssetSubsystem deinitialized"));
    Super::Deinitialize();
}

TScriptInterface<ISGDynamicTextAssetProvider> USGDynamicTextAssetSubsystem::MakeProvider(UObject* Object)
{
    TScriptInterface<ISGDynamicTextAssetProvider> provider;
    if (Object)
    {
        provider.SetObject(Object);
        provider.SetInterface(Cast<ISGDynamicTextAssetProvider>(Object));
    }
    return provider;
}

TScriptInterface<ISGDynamicTextAssetProvider> USGDynamicTextAssetSubsystem::GetDynamicTextAsset(const FSGDynamicTextAssetId& Id) const
{
    if (!Id.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Id"));
        return TScriptInterface<ISGDynamicTextAssetProvider>();
    }

    const TScriptInterface<ISGDynamicTextAssetProvider>* found = LoadedObjects.Find(Id);
    return found ? *found : TScriptInterface<ISGDynamicTextAssetProvider>();
}

bool USGDynamicTextAssetSubsystem::IsDynamicTextAssetCached(const FSGDynamicTextAssetId& Id) const
{
    if (!Id.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Id"));
        return false;
    }

    return LoadedObjects.Contains(Id);
}

void USGDynamicTextAssetSubsystem::ClearCache()
{
    const int32 count = LoadedObjects.Num();
    LoadedObjects.Empty();
    TrackedInstancedClasses.Empty();
    InstancedClassRefCounts.Empty();
    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Cleared %d dynamic text assets from cache"), count);
}

bool USGDynamicTextAssetSubsystem::RemoveFromCache(const FSGDynamicTextAssetId& Id)
{
    if (!Id.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Id"));
        return false;
    }

    const int32 removed = LoadedObjects.Remove(Id);
    if (removed > 0)
    {
        // Decrement ref counts and collect classes that drop to zero
        TArray<UClass*> releasedClasses;
        if (const FSGTrackedInstancedClasses* trackedEntry = TrackedInstancedClasses.Find(Id))
        {
            for (const TObjectPtr<UClass>& trackedClass : trackedEntry->Classes)
            {
                if (!trackedClass)
                {
                    continue;
                }

                if (int32* refCount = InstancedClassRefCounts.Find(trackedClass))
                {
                    (*refCount)--;
                    if (*refCount <= 0)
                    {
                        InstancedClassRefCounts.Remove(trackedClass);
                        releasedClasses.Add(trackedClass);
                    }
                }
            }
        }

        TrackedInstancedClasses.Remove(Id);

        // Broadcast if any classes were fully released
        if (!releasedClasses.IsEmpty())
        {
            FSGInstancedClassReleaseContext context;
            context.RemovedAssetId = Id;
            context.ReleasedClasses = MoveTemp(releasedClasses);
            context.RemainingTrackedClassCount = InstancedClassRefCounts.Num();

            UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
                TEXT("Released %d instanced object class(es) for DTA Id(%s), %d class(es) still tracked"),
                context.ReleasedClasses.Num(), *Id.ToString(), context.RemainingTrackedClassCount);

            OnInstancedClassesReleased.Broadcast(context);
        }

        UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Removed dynamic text asset from cache: Id(%s)"), *Id.ToString());
        return true;
    }

    return false;
}

bool USGDynamicTextAssetSubsystem::AddToCache(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider)
{
    UObject* providerObject = Provider.GetObject();
    if (!providerObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL Provider"));
        return false;
    }

    ISGDynamicTextAssetProvider* providerInterface = Provider.GetInterface();
    if (!providerInterface)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("Provider UObject(%s) does not implement ISGDynamicTextAssetProvider"),
            *GetNameSafe(providerObject));
        return false;
    }

    const FSGDynamicTextAssetId& id = providerInterface->GetDynamicTextAssetId();
    if (!id.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("Provider(%s) has INVALID id"),
            *GetNameSafe(providerObject));
        return false;
    }

    // Check if already cached
    if (LoadedObjects.Contains(id))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Provider already in cache: id(%s)"), *id.ToString());
        return false;
    }

    LoadedObjects.Add(id, Provider);

    // Track instanced sub-object classes as a GC safety net
    TrackInstancedClassesForProvider(id, providerObject);

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Added provider to cache: id(%s) Class(%s)"),
        *id.ToString(), *GetNameSafe(providerObject->GetClass()));

    OnDynamicTextAssetCached.Broadcast(Provider);
    return true;
}

void USGDynamicTextAssetSubsystem::Internal_LoadDynamicTextAssetFromFileAsync_GameThread(const FString& FilePath,
    const UClass* ClassPtr, const FString& TextPayload, uint32 SerializerTypeId,
    const bool& bReadSuccess, const FOnDynamicTextAssetLoaded& OnComplete)
{
    // Decrement pending counter
    PendingAsyncLoads.Decrement();

    TScriptInterface<ISGDynamicTextAssetProvider> emptyProvider;

    if (!bReadSuccess)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Async load failed to read FilePath(%s)"), *FilePath);
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(emptyProvider, false);
        }
        return;
    }

    // For binary files SerializerTypeId is non-zero (read from header); for text files use extension lookup
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = (SerializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
        ? FSGDynamicTextAssetFileManager::FindSerializerForTypeId(SerializerTypeId)
        : FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
    if (!serializer.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Async load: no serializer found for FilePath(%s)"), *FilePath);
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(emptyProvider, false);
        }
        return;
    }

    // Check cache first (in case it was loaded synchronously while we were reading)
    FSGDynamicTextAssetFileMetadata extractedMeta;
    if (serializer->ExtractMetadata(TextPayload, extractedMeta))
    {
        TScriptInterface<ISGDynamicTextAssetProvider> cached = GetDynamicTextAsset(extractedMeta.Id);
        if (cached.GetObject())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose, TEXT("Async load: returning cached dynamic text asset Id(%s)"), *extractedMeta.Id.ToString());
            if (OnComplete.IsBound())
            {
                OnComplete.Execute(cached, true);
            }
            return;
        }
    }

    // Create and deserialize on game thread
    TScriptInterface<ISGDynamicTextAssetProvider> dataObject(NewObject<UObject>(this, ClassPtr));
    if (!dataObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Async load: failed to create instance of class(%s)"), *GetNameSafe(ClassPtr));
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(emptyProvider, false);
        }
        return;
    }

    bool bMigrated = false;
    if (!serializer->DeserializeProvider(TextPayload, dataObject.GetInterface(), bMigrated))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Async load: failed to deserialize FilePath(%s)"), *FilePath);
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(emptyProvider, false);
        }
        return;
    }

    // Extract asset bundle data from the serialized text payload.
    // In non-editor builds, property metadata is stripped so ExtractFromObject is a no-op.
    // The serializer parses the bundle block directly from the text instead.
    serializer->ExtractSGDTAssetBundles(TextPayload, dataObject->GetMutableSGDTAssetBundleData());

#if WITH_EDITOR
    // If migration occurred, re-save the file with the updated version
    if (bMigrated)
    {
        FString updatedJson;
        if (serializer->SerializeProvider(dataObject.GetInterface(), updatedJson))
        {
            if (FSGDynamicTextAssetFileManager::WriteRawFileContents(FilePath, updatedJson))
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Async load: re-saved migrated dynamic text asset to FilePath(%s)"), *FilePath);
            }
            else
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Async load: migration succeeded but failed to re-save FilePath(%s)"), *FilePath);
            }
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Async load: migration succeeded but failed to re-serialize DynamicTextAsset(%s)"), *dataObject->GetDynamicTextAssetId().ToString());
        }
    }
#endif

#if !UE_BUILD_SHIPPING
    // Never populate on shipping builds
    // Populate UserFacingId if not set by deserialization
    if (dataObject->GetUserFacingId().IsEmpty())
    {
        // Try manifest lookup (cooked builds)
        const FSGDynamicTextAssetCookManifest* manifest = FSGDynamicTextAssetFileManager::GetCookManifest();
        if (manifest != nullptr && manifest->IsLoaded())
        {
            const FSGDynamicTextAssetCookManifestEntry* entry = manifest->FindById(dataObject->GetDynamicTextAssetId());
            if (entry != nullptr && !entry->UserFacingId.IsEmpty())
            {
                dataObject->SetUserFacingId(entry->UserFacingId);
            }
        }

        // Fallback: extract from filename
        if (dataObject->GetUserFacingId().IsEmpty())
        {
            FString fromPath = FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(FilePath);
            if (!fromPath.IsEmpty())
            {
                dataObject->SetUserFacingId(fromPath);
            }
        }
    }
#endif

    AddToCache(dataObject);

    // Call post-load hook via interface
    dataObject->PostDynamicTextAssetLoaded();

    // Run validation via interface
    FSGDynamicTextAssetValidationResult validationResult;
    if (!dataObject->Native_ValidateDynamicTextAsset(validationResult))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("Async loaded dynamic text asset has validation errors: Id(%s) - %s"),
            *dataObject->GetDynamicTextAssetId().ToString(),
            *validationResult.ToFormattedString());
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Async loaded dynamic text asset: Id(%s) class(%s) FilePath(%s)"),
        *dataObject->GetDynamicTextAssetId().ToString(), *GetNameSafe(dataObject.GetObject()->GetClass()), *FilePath);

    if (OnComplete.IsBound())
    {
        OnComplete.Execute(dataObject, true);
    }
}

TScriptInterface<ISGDynamicTextAssetProvider> USGDynamicTextAssetSubsystem::LoadDynamicTextAssetFromFile(const FString& FilePath, UClass* DynamicTextAssetClass)
{
    TScriptInterface<ISGDynamicTextAssetProvider> emptyProvider;

    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted EMPTY FilePath"));
        return emptyProvider;
    }

    if (!DynamicTextAssetClass)
    {
        // Try extracting metadata first to resolve the class implicitly.
        FString jsonContents;
        uint32 classResolveTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
        if (FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, jsonContents, &classResolveTypeId))
        {
            TSharedPtr<ISGDynamicTextAssetSerializer> serializer = (classResolveTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
                ? FSGDynamicTextAssetFileManager::FindSerializerForTypeId(classResolveTypeId)
                : FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
            if (serializer.IsValid())
            {
                FSGDynamicTextAssetFileMetadata fileMeta;
                if (serializer->ExtractMetadata(jsonContents, fileMeta))
                {
                    // Try AssetTypeId-based resolution first (reliable in cooked builds)
                    if (fileMeta.AssetTypeId.IsValid())
                    {
                        if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
                        {
                            DynamicTextAssetClass = registry->ResolveClassForTypeId(fileMeta.AssetTypeId);
                        }
                    }

                    // Fallback to class name lookup
                    if (!DynamicTextAssetClass)
                    {
                        DynamicTextAssetClass = FindFirstObject<UClass>(*fileMeta.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
                    }
                }
            }
        }
    }

    if (!DynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetSubsystem::LoadDynamicTextAssetFromFile - DynamicTextAssetClass was nullptr and could not be resolved from file FilePath(%s)"), *FilePath);
        return emptyProvider;
    }

    // Read file contents for binary files, OutSerializerTypeId identifies the deserializer
    FString jsonContents;
    uint32 serializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
    if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, jsonContents, &serializerTypeId))
    {
        return emptyProvider;
    }

    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = (serializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
        ? FSGDynamicTextAssetFileManager::FindSerializerForTypeId(serializerTypeId)
        : FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
    if (!serializer.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("No serializer found for FilePath(%s)"), *FilePath);
        return emptyProvider;
    }

    // Try to extract Id first to check cache
    FSGDynamicTextAssetFileMetadata cacheLookupMeta;
    if (serializer->ExtractMetadata(jsonContents, cacheLookupMeta))
    {
        // Check if already cached
        TScriptInterface<ISGDynamicTextAssetProvider> cached = GetDynamicTextAsset(cacheLookupMeta.Id);
        if (cached.GetObject())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose, TEXT("Returning cached dynamic text asset: Id(%s)"), *cacheLookupMeta.Id.ToString());
            return cached;
        }
    }

    // Create new dynamic text asset instance
    TScriptInterface<ISGDynamicTextAssetProvider> dataObject(NewObject<UObject>(this, DynamicTextAssetClass));
    if (!dataObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Failed to create instance of DynamicTextAssetClass(%s)"), *GetNameSafe(DynamicTextAssetClass));
        return emptyProvider;
    }

    // Deserialize JSON into object
    bool bMigrated = false;
    if (!serializer->DeserializeProvider(jsonContents, dataObject.GetInterface(), bMigrated))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Failed to deserialize dynamic text asset from FilePath(%s)"), *FilePath);
        return emptyProvider;
    }

    // Extract asset bundle data from the serialized text payload.
    // In non-editor builds, property metadata is stripped so ExtractFromObject is a no-op.
    // The serializer parses the bundle block directly from the text instead.
    serializer->ExtractSGDTAssetBundles(jsonContents, dataObject->GetMutableSGDTAssetBundleData());

#if WITH_EDITOR
    // If migration occurred, re-save the file with the updated version
    if (bMigrated)
    {
        FString updatedJson;
        if (serializer->SerializeProvider(dataObject.GetInterface(), updatedJson))
        {
            if (FSGDynamicTextAssetFileManager::WriteRawFileContents(FilePath, updatedJson))
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Re-saved migrated dynamic text asset to FilePath(%s)"), *FilePath);
            }
            else
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Migration succeeded but failed to re-save FilePath(%s)"), *FilePath);
            }
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Migration succeeded but failed to re-serialize DynamicTextAsset(%s)"), *dataObject->GetDynamicTextAssetId().ToString());
        }
    }
#endif

#if !UE_BUILD_SHIPPING
    // Never populate on shipping builds
    // Populate UserFacingId if not set by deserialization
    if (dataObject->GetUserFacingId().IsEmpty())
    {
        // Try manifest lookup (cooked builds)
        const FSGDynamicTextAssetCookManifest* manifest = FSGDynamicTextAssetFileManager::GetCookManifest();
        if (manifest != nullptr && manifest->IsLoaded())
        {
            const FSGDynamicTextAssetCookManifestEntry* entry = manifest->FindById(dataObject->GetDynamicTextAssetId());
            if (entry != nullptr && !entry->UserFacingId.IsEmpty())
            {
                dataObject->SetUserFacingId(entry->UserFacingId);
            }
        }

        // Fallback: extract from filename
        if (dataObject->GetUserFacingId().IsEmpty())
        {
            FString fromPath = FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(FilePath);
            if (!fromPath.IsEmpty())
            {
                dataObject->SetUserFacingId(fromPath);
            }
        }
    }
#endif

    if (!AddToCache(dataObject))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Dynamic text asset loaded but failed to cache: FilePath(%s)"), *FilePath);
    }

    // Call post-load hook via interface
    dataObject->PostDynamicTextAssetLoaded();

    // Run validation via interface
    FSGDynamicTextAssetValidationResult validationResult;
    if (!dataObject->Native_ValidateDynamicTextAsset(validationResult))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("Loaded dynamic text asset has validation errors: Id(%s) - %s"),
            *dataObject->GetDynamicTextAssetId().ToString(),
            *validationResult.ToFormattedString());
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Loaded dynamic text asset: Id(%s) Class(%s) FilePath(%s)"),
        *dataObject->GetDynamicTextAssetId().ToString(), *GetNameSafe(dataObject.GetObject()->GetClass()), *FilePath);

    return dataObject;
}

TScriptInterface<ISGDynamicTextAssetProvider> USGDynamicTextAssetSubsystem::GetOrLoadDynamicTextAsset(const FSGDynamicTextAssetId& Id, const FString& FilePath, UClass* DynamicTextAssetClass)
{
    if (!Id.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Id"));
        return TScriptInterface<ISGDynamicTextAssetProvider>();
    }

    // Check cache first
    TScriptInterface<ISGDynamicTextAssetProvider> cached = GetDynamicTextAsset(Id);
    if (cached.GetObject())
    {
        return cached;
    }

    // Load from file
    return LoadDynamicTextAssetFromFile(FilePath, DynamicTextAssetClass);
}

int32 USGDynamicTextAssetSubsystem::LoadAllDynamicTextAssetsOfClass(UClass* DynamicTextAssetClass, bool bIncludeSubclasses)
{
    if (!DynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
        return 0;
    }

    // Find all files for this class
    TArray<FString> filePaths;
    FSGDynamicTextAssetFileManager::FindAllFilesForClass(DynamicTextAssetClass, filePaths, bIncludeSubclasses);

    int32 loadedCount = 0;
    for (const FString& filePath : filePaths)
    {
        // Extract class name from JSON to determine actual class
        FString jsonContents;
        uint32 fileSerializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
        if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(filePath, jsonContents, &fileSerializerTypeId))
        {
            continue;
        }

        TSharedPtr<ISGDynamicTextAssetSerializer> serializer = (fileSerializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
            ? FSGDynamicTextAssetFileManager::FindSerializerForTypeId(fileSerializerTypeId)
            : FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
        if (!serializer.IsValid())
        {
            continue;
        }

        FSGDynamicTextAssetFileMetadata classResolveMeta;
        if (!serializer->ExtractMetadata(jsonContents, classResolveMeta))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Could not extract class name from FilePath(%s)"), *filePath);
            continue;
        }

        // Try AssetTypeId-based resolution first (reliable in cooked builds)
        UClass* actualClass = nullptr;
        if (classResolveMeta.AssetTypeId.IsValid())
        {
            if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                actualClass = registry->ResolveClassForTypeId(classResolveMeta.AssetTypeId);
            }
        }

        // Fallback to class name lookup
        if (!actualClass)
        {
            actualClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/%s.%s"),
                *DynamicTextAssetClass->GetOuterUPackage()->GetName(), *classResolveMeta.ClassName));
        }

        if (!actualClass)
        {
            actualClass = FindFirstObject<UClass>(*classResolveMeta.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
        }

        if (!actualClass || !actualClass->IsChildOf(DynamicTextAssetClass))
        {
            // Fall back to using the provided class
            actualClass = DynamicTextAssetClass;
        }

        TScriptInterface<ISGDynamicTextAssetProvider> loaded = LoadDynamicTextAssetFromFile(filePath, actualClass);
        if (loaded.GetObject())
        {
            loadedCount++;
        }
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Loaded %d dynamic text assets of class(%s)"), loadedCount, *GetNameSafe(DynamicTextAssetClass));
    return loadedCount;
}

void USGDynamicTextAssetSubsystem::LoadDynamicTextAssetFromFileAsync(const FString& FilePath, UClass* DynamicTextAssetClass, FOnDynamicTextAssetLoaded OnComplete)
{
    TScriptInterface<ISGDynamicTextAssetProvider> emptyProvider;

    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted EMPTY FilePath"));
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(emptyProvider, false);
        }
        return;
    }

    if (!DynamicTextAssetClass)
    {
        // Class can be null, it will be resolved on the background thread during file search / read.
    }

    // Increment pending counter
    PendingAsyncLoads.Increment();

    // Capture weak reference to avoid issues if subsystem is destroyed
    TWeakObjectPtr<USGDynamicTextAssetSubsystem> weakThis(this);
    UClass* classPtr = DynamicTextAssetClass;

    // Execute file I/O on background thread
    Async(EAsyncExecution::ThreadPool, [weakThis, FilePath, classPtr, OnComplete]()
    {
        // Read file on background thread, binary files are decompressed and yield a non-zero TypeId
        FString jsonContents;
        uint32 serializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
        bool readSuccess = FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, jsonContents, &serializerTypeId);

        // Return to game thread for object creation and callback
        AsyncTask(ENamedThreads::GameThread, [weakThis, FilePath, classPtr, jsonContents, serializerTypeId, readSuccess, OnComplete]()
        {
            USGDynamicTextAssetSubsystem* subsystem = weakThis.Get();
            if (!subsystem)
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Async load callback: subsystem no longer valid"));
                if (OnComplete.IsBound())
                {
                    TScriptInterface<ISGDynamicTextAssetProvider> emptyProvider;
                    OnComplete.Execute(emptyProvider, false);
                }
                return;
            }

            subsystem->Internal_LoadDynamicTextAssetFromFileAsync_GameThread(FilePath, classPtr, jsonContents, serializerTypeId, readSuccess, OnComplete);
        });
    });
}

void USGDynamicTextAssetSubsystem::LoadDynamicTextAssetAsync(const FSGDynamicTextAssetId& Id, const UClass* ClassHint, FOnDynamicTextAssetLoaded OnComplete)
{
    TScriptInterface<ISGDynamicTextAssetProvider> emptyProvider;

    if (!Id.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Id"));
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(emptyProvider, false);
        }
        return;
    }

    // Check cache first
    TScriptInterface<ISGDynamicTextAssetProvider> cached = GetDynamicTextAsset(Id);
    if (cached.GetObject())
    {
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(cached, true);
        }
        return;
    }

    // Increment pending counter
    PendingAsyncLoads.Increment();

    // Capture weak reference
    TWeakObjectPtr<USGDynamicTextAssetSubsystem> weakThis(this);
    // Explicitly copy ClassHint to avoid issues if it's destroyed, though UClasses usually aren't.
    const UClass* safeClassHint = ClassHint;

    // Execute file search on background thread
    Async(EAsyncExecution::ThreadPool, [weakThis, Id, safeClassHint, OnComplete]()
    {
        // Find file path
        FString foundPath;
        bool bFound = FSGDynamicTextAssetFileManager::FindFileForId(Id, foundPath, safeClassHint);

        if (!bFound)
        {
            // Failed to find file
            AsyncTask(ENamedThreads::GameThread, [weakThis, Id, OnComplete]()
            {
                if (USGDynamicTextAssetSubsystem* subsystem = weakThis.Get())
                {
                    subsystem->PendingAsyncLoads.Decrement(); // Manually decrement since we aren't calling Internal_...
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Async load failed: Could not find file for Id(%s)"), *Id.ToString());
                    if (OnComplete.IsBound())
                    {
                        TScriptInterface<ISGDynamicTextAssetProvider> emptyProvider;
                        OnComplete.Execute(emptyProvider, false);
                    }
                }
            });
            return;
        }

        // Found file, now proceed with standard load logic and binary files yield a non-zero TypeId
        FString textContents;
        uint32 asyncSerializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
        bool readSuccess = FSGDynamicTextAssetFileManager::ReadRawFileContents(foundPath, textContents, &asyncSerializerTypeId);

        // Return to game thread
        AsyncTask(ENamedThreads::GameThread, [weakThis, foundPath, safeClassHint, textContents, asyncSerializerTypeId, readSuccess, OnComplete]()
        {
            USGDynamicTextAssetSubsystem* subsystem = weakThis.Get();
            if (!subsystem)
            {
                return;
            }

            // Determine class from file content if hint is not specific enough
            const UClass* classToUse = safeClassHint;

            if (!classToUse && readSuccess)
            {
                 TSharedPtr<ISGDynamicTextAssetSerializer> serializer = (asyncSerializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
                     ? FSGDynamicTextAssetFileManager::FindSerializerForTypeId(asyncSerializerTypeId)
                     : FSGDynamicTextAssetFileManager::FindSerializerForFile(foundPath);
                 if (serializer.IsValid())
                 {
                     FSGDynamicTextAssetFileMetadata asyncMeta;
                     if (serializer->ExtractMetadata(textContents, asyncMeta))
                     {
                          // Try AssetTypeId-based resolution first (reliable in cooked builds)
                          if (asyncMeta.AssetTypeId.IsValid())
                          {
                              if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
                              {
                                  classToUse = registry->ResolveClassForTypeId(asyncMeta.AssetTypeId);
                              }
                          }

                          // Fallback to class name lookup
                          if (!classToUse)
                          {
                              classToUse = FindFirstObject<UClass>(*asyncMeta.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
                          }

                          // Validate resolved class implements ISGDynamicTextAssetProvider
                          if (classToUse && !classToUse->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
                          {
                              UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                  TEXT("LoadDynamicTextAssetAsync: Resolved class '%s' does not implement ISGDynamicTextAssetProvider, ignoring"),
                                  *asyncMeta.ClassName);
                              classToUse = nullptr;
                          }
                     }
                 }
            }

            if (!classToUse)
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Could not resolve proper class to spawn for file '%s'. Ensure the type manifest contains a valid classPath."), *foundPath);
                if (OnComplete.IsBound())
                {
                    TScriptInterface<ISGDynamicTextAssetProvider> emptyResult;
                    OnComplete.Execute(emptyResult, false);
                }
                return;
            }

            subsystem->Internal_LoadDynamicTextAssetFromFileAsync_GameThread(foundPath, classToUse, textContents, asyncSerializerTypeId, readSuccess, OnComplete);
        });
    });
}

bool USGDynamicTextAssetSubsystem::LoadSGDTAssetBundle(const FSGDynamicTextAssetId& Id, FName BundleName, FStreamableDelegate OnComplete)
{
	if (!Id.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("LoadSGDTAssetBundle: Invalid Id provided"));
		return false;
	}

	if (BundleName.IsNone())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("LoadSGDTAssetBundle: BundleName is NAME_None"));
		return false;
	}

	TScriptInterface<ISGDynamicTextAssetProvider> provider = GetDynamicTextAsset(Id);
	if (!provider.GetInterface())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("LoadSGDTAssetBundle: DTA Id(%s) is not cached"), *Id.ToString());
		return false;
	}

	TArray<FSoftObjectPath> paths;
	if (!provider->GetSGDTAssetBundleData().GetPathsForBundle(BundleName, paths) || paths.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
			TEXT("LoadSGDTAssetBundle: No paths found for bundle '%s' on DTA Id(%s)"),
			*BundleName.ToString(), *Id.ToString());
		return false;
	}

	FStreamableManager& streamableManager = UAssetManager::Get().GetStreamableManager();
	streamableManager.RequestAsyncLoad(paths, OnComplete);

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("LoadSGDTAssetBundle: Initiated async load of %d asset(s) for bundle '%s' on DTA Id(%s)"),
		paths.Num(), *BundleName.ToString(), *Id.ToString());

	return true;
}

int32 USGDynamicTextAssetSubsystem::LoadSGDTAssetBundleForAll(FName BundleName, FStreamableDelegate OnComplete)
{
	if (BundleName.IsNone())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("LoadSGDTAssetBundleForAll: BundleName is NAME_None"));
		return 0;
	}

	TArray<FSoftObjectPath> allPaths;
	int32 matchingDTACount = 0;

	for (const TPair<FSGDynamicTextAssetId, TScriptInterface<ISGDynamicTextAssetProvider>>& pair : LoadedObjects)
	{
		ISGDynamicTextAssetProvider* provider = pair.Value.GetInterface();
		if (!provider)
		{
			continue;
		}

		const int32 pathCountBefore = allPaths.Num();
		provider->GetSGDTAssetBundleData().GetPathsForBundle(BundleName, allPaths);

		if (allPaths.Num() > pathCountBefore)
		{
			++matchingDTACount;
		}
	}

	if (!allPaths.IsEmpty())
	{
		FStreamableManager& streamableManager = UAssetManager::Get().GetStreamableManager();
		streamableManager.RequestAsyncLoad(allPaths, OnComplete);

		UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
			TEXT("LoadSGDTAssetBundleForAll: Initiated async load of %d asset(s) for bundle '%s' across %d DTA(s)"),
			allPaths.Num(), *BundleName.ToString(), matchingDTACount);
	}
	else if (OnComplete.IsBound())
	{
		// No paths to load, invoke callback immediately
		OnComplete.Execute();
	}

	return matchingDTACount;
}

const FSGDynamicTextAssetBundleData* USGDynamicTextAssetSubsystem::GetSGDTAssetBundleData(const FSGDynamicTextAssetId& Id) const
{
	if (!Id.IsValid())
	{
		return nullptr;
	}

	const TScriptInterface<ISGDynamicTextAssetProvider>* found = LoadedObjects.Find(Id);
	if (!found || !found->GetInterface())
	{
		return nullptr;
	}

	return &found->GetInterface()->GetSGDTAssetBundleData();
}

bool USGDynamicTextAssetSubsystem::GetSGDTAssetBundleDataCopy(const FSGDynamicTextAssetId& Id, FSGDynamicTextAssetBundleData& OutBundleData) const
{
	const FSGDynamicTextAssetBundleData* bundleData = GetSGDTAssetBundleData(Id);
	if (!bundleData)
	{
		return false;
	}

	OutBundleData = *bundleData;
	return true;
}

void USGDynamicTextAssetSubsystem::GetAllPathsForSGDTBundle(FName BundleName, TArray<FSoftObjectPath>& OutPaths) const
{
	if (BundleName.IsNone())
	{
		return;
	}

	for (const TPair<FSGDynamicTextAssetId, TScriptInterface<ISGDynamicTextAssetProvider>>& pair : LoadedObjects)
	{
		ISGDynamicTextAssetProvider* provider = pair.Value.GetInterface();
		if (!provider)
		{
			continue;
		}

		provider->GetSGDTAssetBundleData().GetPathsForBundle(BundleName, OutPaths);
	}
}

void USGDynamicTextAssetSubsystem::ApplyServerTypeOverrides(const TSharedPtr<FJsonObject>& ServerData)
{
#if WITH_EDITOR
    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("ApplyServerTypeOverrides: Skipped in editor builds"));
#else
    USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
    if (!registry)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("ApplyServerTypeOverrides: Registry is not available"));
        return;
    }

    registry->ApplyServerTypeOverrides(ServerData);
#endif
}

void USGDynamicTextAssetSubsystem::ClearServerTypeOverrides()
{
    USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
    if (!registry)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("ClearServerTypeOverrides: Registry is not available"));
        return;
    }

    registry->ClearServerTypeOverrides();
}

void USGDynamicTextAssetSubsystem::FetchAndApplyServerTypeOverrides(FOnServerTypeOverridesComplete OnComplete)
{
#if WITH_EDITOR
    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("FetchAndApplyServerTypeOverrides: Skipped in editor builds"));
    OnComplete.ExecuteIfBound(true, nullptr);
    return;
#else
    if (!ServerInterface)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("FetchAndApplyServerTypeOverrides: No server interface available"));
        OnComplete.ExecuteIfBound(false, nullptr);
        return;
    }

    TWeakObjectPtr<USGDynamicTextAssetSubsystem> weakThis(this);

    ServerInterface->FetchTypeManifestOverrides(
        FOnServerTypeOverridesComplete::CreateLambda(
            [weakThis, OnComplete](bool bSuccess, TSharedPtr<FJsonObject> OverrideData)
            {
                USGDynamicTextAssetSubsystem* subsystem = weakThis.Get();
                if (!subsystem)
                {
                    OnComplete.ExecuteIfBound(false, nullptr);
                    return;
                }

                if (bSuccess && OverrideData.IsValid())
                {
                    subsystem->ApplyServerTypeOverrides(OverrideData);
                }

                OnComplete.ExecuteIfBound(bSuccess, OverrideData);
            }));
#endif
}

void USGDynamicTextAssetSubsystem::TrackInstancedClassesForProvider(
    const FSGDynamicTextAssetId& Id,
    const UObject* ProviderObject)
{
    if (!ProviderObject || !Id.IsValid())
    {
        return;
    }

    FSGTrackedInstancedClasses& entry = TrackedInstancedClasses.FindOrAdd(Id);
    FSGDynamicTextAssetSerializerBase::CollectInstancedObjectClasses(ProviderObject, entry.Classes);

    // Increment ref counts for each tracked class
    for (const TObjectPtr<UClass>& trackedClass : entry.Classes)
    {
        if (trackedClass)
        {
            int32& refCount = InstancedClassRefCounts.FindOrAdd(trackedClass, 0);
            refCount++;
        }
    }

    if (!entry.Classes.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
            TEXT("Tracked %d instanced object class(es) for DTA Id(%s)"),
            entry.Classes.Num(), *Id.ToString());
    }
}

int32 USGDynamicTextAssetSubsystem::GetTrackedInstancedClassCount() const
{
    int32 total = 0;
    for (const TPair<FSGDynamicTextAssetId, FSGTrackedInstancedClasses>& pair : TrackedInstancedClasses)
    {
        total += pair.Value.Classes.Num();
    }
    return total;
}

TSet<UClass*> USGDynamicTextAssetSubsystem::GetAllTrackedInstancedClasses() const
{
    TSet<UClass*> result;
    for (const TPair<FSGDynamicTextAssetId, FSGTrackedInstancedClasses>& pair : TrackedInstancedClasses)
    {
        result.Reserve(result.Num() + pair.Value.Classes.Num());
        for (UClass* classPtr : pair.Value.Classes)
        {
            if (classPtr)
            {
                result.Add(classPtr);
            }
        }
    }
    return result;
}

TArray<UClass*> USGDynamicTextAssetSubsystem::GetTrackedInstancedClassesForId(const FSGDynamicTextAssetId& Id) const
{
    TArray<UClass*> result;
    if (const FSGTrackedInstancedClasses* entry = TrackedInstancedClasses.Find(Id))
    {
        result.Reserve(result.Num() + entry->Classes.Num());
        for (UClass* classPtr : entry->Classes)
        {
            if (classPtr)
            {
                result.AddUnique(classPtr);
            }
        }
    }
    return result;
}
