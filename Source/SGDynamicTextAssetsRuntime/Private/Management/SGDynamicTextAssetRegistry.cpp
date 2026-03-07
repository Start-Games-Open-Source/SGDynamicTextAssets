// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDynamicTextAssetRegistry.h"

#include "Components/ActorComponent.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAsset.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetTypeManifest.h"
#include "Misc/Paths.h"
#include "SGDynamicTextAssetsRuntimeModule.h"
#include "UObject/UObjectIterator.h"

USGDynamicTextAssetRegistry* USGDynamicTextAssetRegistry::Get()
{
    return GEngine ? GEngine->GetEngineSubsystem<USGDynamicTextAssetRegistry>() : nullptr;
}

USGDynamicTextAssetRegistry& USGDynamicTextAssetRegistry::GetChecked()
{
    USGDynamicTextAssetRegistry* registry = Get();
    check(registry);
    return *registry;
}

void USGDynamicTextAssetRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Register the base USGDynamicTextAsset class with the registry
    RegisterDynamicTextAssetClass(USGDynamicTextAsset::StaticClass());

    if (FSGDynamicTextAssetFileManager::ShouldUseCookedDirectory())
    {
        // Packaged builds: load pre-cooked type manifests from _TypeManifests/
        LoadCookedManifests();
    }
    else
    {
        // Editor: discover classes via reflection and sync manifests
        SyncManifests();
    }
}

void USGDynamicTextAssetRegistry::Deinitialize()
{
    RootClassManifests.Empty();
    TypeIdToSoftClassMap.Empty();
    ClassToTypeIdMap.Empty();

    Super::Deinitialize();
}

bool USGDynamicTextAssetRegistry::RegisterDynamicTextAssetClass(UClass* DynamicTextAssetClass)
{
    if (!DynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
        return false;
    }
    // Guard: AActor subclasses cannot be dynamic text asset providers
    if (DynamicTextAssetClass->IsChildOf(AActor::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DynamicTextAssetClass(%s): AActor subclasses cannot be dynamic text asset providers"),
            *GetNameSafe(DynamicTextAssetClass));
        return false;
    }
    // Guard: UActorComponent subclasses cannot be dynamic text asset providers
    if (DynamicTextAssetClass->IsChildOf(UActorComponent::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DynamicTextAssetClass(%s): UActorComponent subclasses cannot be dynamic text asset providers"),
            *GetNameSafe(DynamicTextAssetClass));
        return false;
    }
    // Guard: Must implement ISGDynamicTextAssetProvider
    if (!DynamicTextAssetClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DynamicTextAssetClass(%s) does not implement ISGDynamicTextAssetProvider"),
            *GetNameSafe(DynamicTextAssetClass));
        return false;
    }
    if (RegisteredBaseClasses.Contains(DynamicTextAssetClass))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
            TEXT("DynamicTextAssetClass(%s) is already registered"),
            *GetNameSafe(DynamicTextAssetClass));
        return true;
    }

    RegisteredBaseClasses.Add(DynamicTextAssetClass);
    bCacheInvalid = true;

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("Registered DynamicTextAssetClass(%s)"),
        *GetNameSafe(DynamicTextAssetClass));

    OnRegistryChanged.Broadcast();
    return true;
}

bool USGDynamicTextAssetRegistry::UnregisterDynamicTextAssetClass(UClass* DynamicTextAssetClass)
{
    if (!DynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
        return false;
    }

    if (RegisteredBaseClasses.Remove(DynamicTextAssetClass) > 0)
    {
        bCacheInvalid = true;
        UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Unregistered DynamicTextAssetClass(%s)"), *GetNameSafe(DynamicTextAssetClass));
        OnRegistryChanged.Broadcast();
        return true;
    }

    return false;
}

void USGDynamicTextAssetRegistry::GetAllRegisteredClasses(TArray<UClass*>& OutClasses) const
{
    if (bCacheInvalid)
    {
        RebuildClassCache();
    }

    OutClasses = CachedAllClasses;
}

void USGDynamicTextAssetRegistry::GetAllRegisteredBaseClasses(TArray<UClass*>& OutClasses) const
{
    OutClasses.Reset();
    OutClasses.Reserve(RegisteredBaseClasses.Num());

    for (const TObjectPtr<UClass>& baseClass : RegisteredBaseClasses)
    {
        if (baseClass)
        {
            OutClasses.Add(baseClass.Get());
        }
    }
}

bool USGDynamicTextAssetRegistry::IsRegisteredClass(UClass* DynamicTextAssetClass) const
{
    if (!DynamicTextAssetClass)
    {
        return false;
    }

    TArray<UClass*> allClasses;
    GetAllRegisteredClasses(allClasses);
    return allClasses.Contains(DynamicTextAssetClass);
}

void USGDynamicTextAssetRegistry::GetAllInstantiableClasses(TArray<UClass*>& OutClasses) const
{
    TArray<UClass*> allClasses;
    GetAllRegisteredClasses(allClasses);

    OutClasses.Reset();
    OutClasses.Reserve(allClasses.Num());

    for (UClass* dataClass : allClasses)
    {
        if (dataClass && !dataClass->HasAnyClassFlags(CLASS_Abstract))
        {
            OutClasses.Add(dataClass);
        }
    }
}

bool USGDynamicTextAssetRegistry::IsInstantiableClass(UClass* DynamicTextAssetClass) const
{
    if (!DynamicTextAssetClass)
    {
        return false;
    }
    if (DynamicTextAssetClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }
    TArray<UClass*> allClasses;
    GetAllRegisteredClasses(allClasses);
    if (!allClasses.Contains(DynamicTextAssetClass))
    {
        return false;
    }
    return true;
}

FString USGDynamicTextAssetRegistry::GetFolderPathForClass(UClass* DynamicTextAssetClass) const
{
    if (!DynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
        return FString();
    }

    // Build path from inheritance hierarchy
    // Each segment is formatted as {ClassName}_{TypeId} when a valid TypeId is available
    TArray<FString> PathParts;
    UClass* currentClass = DynamicTextAssetClass;

    while (currentClass && currentClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        FString folderName = currentClass->GetName();

        const FSGDynamicTextAssetTypeId typeId = GetTypeIdForClass(currentClass);
        if (typeId.IsValid())
        {
            folderName += TEXT("_") + typeId.ToString();
        }

        PathParts.Insert(folderName, 0);
        currentClass = currentClass->GetSuperClass();
    }

    return FString::Join(PathParts, TEXT("/"));
}

bool USGDynamicTextAssetRegistry::IsValidDynamicTextAssetClass(UClass* TestClass) const
{
    if (!TestClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL TestClass"));
        return false;
    }
    if (!TestClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("TestClass(%s) does not implement ISGDynamicTextAssetProvider"),
            *GetNameSafe(TestClass));
        return false;
    }

    // Check if it's a registered base or child of a registered base
    for (const TObjectPtr<UClass>& baseClass : RegisteredBaseClasses)
    {
        if (baseClass && TestClass->IsChildOf(baseClass.Get()))
        {
            return true;
        }
    }

    return false;
}

void USGDynamicTextAssetRegistry::RebuildClassCache() const
{
    CachedAllClasses.Reset();

    // For each registered base class, find all derived classes
    for (const TObjectPtr<UClass>& baseClass : RegisteredBaseClasses)
    {
        if (!baseClass)
        {
            continue;
        }

        // Add the base class itself
        CachedAllClasses.AddUnique(baseClass.Get());
        TArray<UClass*> childClasses;
        GetDerivedClasses(baseClass.Get(), childClasses);

        for (UClass* childClass : childClasses)
        {
            if (!childClass)
            {
                continue;
            }
            // Don't consider these as registered
            if (childClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated))
            {
                continue;
            }
            // Only include children that implement ISGDynamicTextAssetProvider
            if (!childClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
            {
                continue;
            }
            CachedAllClasses.AddUnique(childClass);
        }
    }

    bCacheInvalid = false;

    UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose, TEXT("Rebuilt dynamic text asset class cache: %d classes"), CachedAllClasses.Num());
}

FSGDynamicTextAssetTypeId USGDynamicTextAssetRegistry::GetTypeIdForClass(const UClass* DynamicTextAssetClass) const
{
    if (!DynamicTextAssetClass)
    {
        return FSGDynamicTextAssetTypeId::INVALID_TYPE_ID;
    }

    if (const FSGDynamicTextAssetTypeId* found = ClassToTypeIdMap.Find(
        TWeakObjectPtr<const UClass>(DynamicTextAssetClass)))
    {
        return *found;
    }

    return FSGDynamicTextAssetTypeId::INVALID_TYPE_ID;
}

TSoftClassPtr<UObject> USGDynamicTextAssetRegistry::GetSoftClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const
{
    if (const TSoftClassPtr<UObject>* found = TypeIdToSoftClassMap.Find(TypeId))
    {
        return *found;
    }

    return TSoftClassPtr<UObject>();
}

UClass* USGDynamicTextAssetRegistry::ResolveClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const
{
    return GetSoftClassForTypeId(TypeId).Get();
}

const FSGDynamicTextAssetTypeManifest* USGDynamicTextAssetRegistry::GetManifestForRootClass(const UClass* RootClass) const
{
    if (!RootClass)
    {
        return nullptr;
    }

    const TSharedPtr<FSGDynamicTextAssetTypeManifest>* found = RootClassManifests.Find(
        TWeakObjectPtr<const UClass>(RootClass));
    if (found && found->IsValid())
    {
        return found->Get();
    }

    return nullptr;
}

void USGDynamicTextAssetRegistry::LoadCookedManifests()
{
    FString typeManifestsDir = FSGDynamicTextAssetFileManager::GetCookedTypeManifestsPath();

    // Find all .json files in _TypeManifests/
    TArray<FString> manifestFilenames;
    IFileManager::Get().FindFiles(manifestFilenames, *FPaths::Combine(typeManifestsDir, TEXT("*.json")), true, false);

    for (const FString& filename : manifestFilenames)
    {
        FString fullPath = FPaths::Combine(typeManifestsDir, filename);

        TSharedPtr<FSGDynamicTextAssetTypeManifest> manifest = MakeShared<FSGDynamicTextAssetTypeManifest>();
        if (!manifest->LoadFromFile(fullPath))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("LoadCookedManifests: Failed to load type manifest from '%s'"), *fullPath);
            continue;
        }

        // Identify root class from the manifest's root type ID
        UClass* rootClass = nullptr;
        const FSGDynamicTextAssetTypeManifestEntry* rootEntry = manifest->FindByTypeId(manifest->GetRootTypeId());
        if (rootEntry)
        {
            rootClass = rootEntry->Class.LoadSynchronous();
        }

        // Store manifest keyed by root class for GetManifestForRootClass()
        if (rootClass)
        {
            RootClassManifests.Add(rootClass, manifest);
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("LoadCookedManifests: Could not resolve root class for manifest '%s'"), *fullPath);
        }
    }

    // Build flat TypeId/Class maps from all manifests
    RebuildTypeIdMaps();

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("LoadCookedManifests: Loaded %d type IDs across %d manifests"),
        TypeIdToSoftClassMap.Num(), RootClassManifests.Num());

    OnTypeManifestUpdated.Broadcast();
}

void USGDynamicTextAssetRegistry::SyncManifests()
{
    TypeIdToSoftClassMap.Empty();
    ClassToTypeIdMap.Empty();

    for (const TObjectPtr<UClass>& rootClass : RegisteredBaseClasses)
    {
        if (!rootClass)
        {
            continue;
        }

        // Get or create manifest for this root class
        TSharedPtr<FSGDynamicTextAssetTypeManifest>& manifest = RootClassManifests.FindOrAdd(rootClass);
        if (!manifest.IsValid())
        {
            manifest = MakeShared<FSGDynamicTextAssetTypeManifest>();
        }

        // Load existing manifest from disk (non-fatal if not found)
        FString manifestPath = GetRootClassManifestFilePath(rootClass);
        manifest->LoadFromFile(manifestPath);

        // When the manifest is empty (file didn't exist or was invalid), attempt to
        // recover TypeIds from existing folder names in the content directory.
        // Folder naming convention: {StrippedClassName}_{TypeId}
        // (e.g., "TestDataAssetBase_AE064875-49CB-FDF6-8437-FA9285804FFE")
        TMap<FString, FSGDynamicTextAssetTypeId> recoveredTypeIds;
        if (manifest->Num() == 0)
        {
            FString contentRoot = FSGDynamicTextAssetFileManager::GetDynamicTextAssetsRootPath();

            IFileManager::Get().IterateDirectoryRecursively(*contentRoot,
                [&recoveredTypeIds](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
            {
                if (!bIsDirectory)
                {
                    return true; // Skip files, only interested in directories
                }

                FString dirName = FPaths::GetCleanFilename(FilenameOrDirectory);

                // GUID string format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX (36 chars)
                // Folder name format: {ClassName}_{GUID}
                // Minimum length: 1 (class name) + 1 (underscore) + 36 (GUID) = 38
                static constexpr int32 GUID_STRING_LENGTH = 36;
                static constexpr int32 MIN_FOLDER_NAME_LENGTH = GUID_STRING_LENGTH + 2;

                if (dirName.Len() < MIN_FOLDER_NAME_LENGTH)
                {
                    return true;
                }

                // Check for underscore separator at the expected position before the GUID suffix
                int32 separatorIndex = dirName.Len() - GUID_STRING_LENGTH - 1;
                if (dirName[separatorIndex] != TEXT('_'))
                {
                    return true;
                }

                // Extract potential GUID string and try to parse it
                FString guidString = dirName.Right(GUID_STRING_LENGTH);
                FGuid parsedGuid;
                if (!FGuid::Parse(guidString, parsedGuid) || !parsedGuid.IsValid())
                {
                    return true;
                }

                // Extract the class name portion (everything before the separator underscore)
                FString folderClassName = dirName.Left(separatorIndex);
                if (!folderClassName.IsEmpty() && !recoveredTypeIds.Contains(folderClassName))
                {
                    recoveredTypeIds.Add(folderClassName, FSGDynamicTextAssetTypeId::FromGuid(parsedGuid));

                    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
                        TEXT("SyncManifests: Recovered typeId(%s) for class '%s' from folder name"),
                        *guidString, *folderClassName);
                }

                return true; // Continue iteration
            });

            if (recoveredTypeIds.Num() > 0)
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
                    TEXT("SyncManifests: Recovered %d typeId(s) from existing folder names"),
                    recoveredTypeIds.Num());
            }
        }

        // Gather all classes in this hierarchy, sorted by depth (parents first)
        TArray<UClass*> allClasses;
        allClasses.Add(rootClass);

        TArray<UClass*> derivedClasses;
        GetDerivedClasses(rootClass, derivedClasses);

        for (UClass* derivedClass : derivedClasses)
        {
            if (!derivedClass)
            {
                continue;
            }
            if (derivedClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated))
            {
                continue;
            }
            if (!derivedClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
            {
                continue;
            }
            allClasses.Add(derivedClass);
        }

        // Sort by class hierarchy depth so parents are processed before children
        allClasses.Sort([](const UClass& A, const UClass& B)
        {
            int32 depthA = 0;
            for (const UClass* c = &A; c; c = c->GetSuperClass())
            {
                ++depthA;
            }
            int32 depthB = 0;
            for (const UClass* c = &B; c; c = c->GetSuperClass())
            {
                ++depthB;
            }
            return depthA < depthB;
        });

        // Ensure every class has an entry in the manifest
        for (UClass* classPtr : allClasses)
        {
            FString className = classPtr->GetName();
            const FSGDynamicTextAssetTypeManifestEntry* existingEntry = manifest->FindByClassName(className);

            FSGDynamicTextAssetTypeId typeId;

            if (existingEntry)
            {
                typeId = existingEntry->TypeId;

                // Ensure the entry's Class soft pointer has the full path.
                // Entries loaded from older manifests may have bare class names
                // (e.g., "SGDynamicTextAsset") instead of full paths
                // (e.g., "/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset").
                FSoftObjectPath correctPath(classPtr);
                FString existingPath = existingEntry->Class.ToString();
                FString correctPathStr = correctPath.ToString();

                if (existingPath != correctPathStr)
                {
                    // AddType replaces the entry in-place, invalidating existingEntry
                    TSoftClassPtr<UObject> correctClassPtr(correctPath);
                    manifest->AddType(typeId, correctClassPtr, existingEntry->ParentTypeId);
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
                        TEXT("SyncManifests: Updated classPath for '%s': '%s' -> '%s'"),
                        *className, *existingPath, *correctPathStr);
                }
            }
            else
            {
                // Attempt to recover the TypeId from an existing folder name before generating a new one.
                // StripClassPrefix is applied to match the folder naming convention used by BuildClassHierarchyPath.
                FString strippedClassName = FSGDynamicTextAssetFileManager::StripClassPrefix(className);
                const FSGDynamicTextAssetTypeId* recoveredId = recoveredTypeIds.Find(strippedClassName);

                if (recoveredId != nullptr)
                {
                    typeId = *recoveredId;

                    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
                        TEXT("SyncManifests: Recovered typeId(%s) for className(%s) from existing folder name"),
                        *typeId.ToString(), *className);
                }
                else
                {
                    typeId = FSGDynamicTextAssetTypeId::NewGeneratedId();

                    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
                        TEXT("SyncManifests: Assigned new typeId(%s) to className(%s)"),
                        *typeId.ToString(), *className);
                }

                // Determine parent type ID from the manifest
                FSGDynamicTextAssetTypeId parentTypeId;
                if (UClass* parentClass = classPtr->GetSuperClass())
                {
                    if (const FSGDynamicTextAssetTypeManifestEntry* parentEntry =
                        manifest->FindByClassName(parentClass->GetName()))
                    {
                        parentTypeId = parentEntry->TypeId;
                    }
                }

                manifest->AddType(typeId, TSoftClassPtr<UObject>(FSoftObjectPath(classPtr)), parentTypeId);
            }

            // Set root type ID from the base class
            if (classPtr == rootClass && !manifest->GetRootTypeId().IsValid())
            {
                manifest->SetRootTypeId(typeId);
            }

            // Build lookup maps using the actual UClass soft path
            TypeIdToSoftClassMap.Add(typeId, TSoftClassPtr<UObject>(FSoftObjectPath(classPtr)));
            ClassToTypeIdMap.Add(classPtr, typeId);
        }

        // Save manifest if it was modified
        if (manifest->IsDirty())
        {
            IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
            platformFile.CreateDirectoryTree(*FPaths::GetPath(manifestPath));

            if (manifest->SaveToFile(manifestPath))
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
                    TEXT("SyncManifests: Saved manifest for root class '%s'"),
                    *GetNameSafe(rootClass));
            }
        }
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("SyncManifests: Complete. %d type IDs across %d manifests"),
        TypeIdToSoftClassMap.Num(), RootClassManifests.Num());

    OnTypeManifestUpdated.Broadcast();
}

FString USGDynamicTextAssetRegistry::GetRootClassManifestFilePath(const UClass* RootClass)
{
    const FString rootFolder = FSGDynamicTextAssetFileManager::GetFolderPathForClass(RootClass);
    const FString manifestFileName = RootClass->GetName() + TEXT("_TypeManifest.json");
    return FPaths::Combine(rootFolder, manifestFileName);
}

void USGDynamicTextAssetRegistry::ApplyServerTypeOverrides(const TSharedPtr<FJsonObject>& ServerData)
{
#if WITH_EDITOR
    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("ApplyServerTypeOverrides: Skipped in editor builds"));
    return;
#else
    if (!ServerData.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("ApplyServerTypeOverrides: Invalid server data"));
        return;
    }

    // Server data format:
    // {
    //   "manifests": {
    //     "<rootTypeId>": { "types": [...] },
    //     ...
    //   }
    // }
    const TSharedPtr<FJsonObject>* manifestsObject = nullptr;
    if (!ServerData->TryGetObjectField(TEXT("manifests"), manifestsObject)
        || !manifestsObject || !(*manifestsObject).IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("ApplyServerTypeOverrides: No 'manifests' object in server data"));
        return;
    }

    int32 appliedCount = 0;
    for (const auto& [rootTypeIdString, value] : (*manifestsObject)->Values)
    {
        FSGDynamicTextAssetTypeId rootTypeId = FSGDynamicTextAssetTypeId::FromString(rootTypeIdString);
        if (!rootTypeId.IsValid())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("ApplyServerTypeOverrides: Invalid root type ID '%s'"), *rootTypeIdString);
            continue;
        }

        // Find the manifest matching this root type ID
        for (auto& [classWeak, manifest] : RootClassManifests)
        {
            if (manifest.IsValid() && manifest->GetRootTypeId() == rootTypeId)
            {
                TSharedPtr<FJsonObject> rootData = value->AsObject();
                if (rootData.IsValid())
                {
                    manifest->ApplyServerOverrides(rootData);
                    appliedCount++;
                }
                break;
            }
        }
    }

    // Rebuild flat maps to reflect overlay
    RebuildTypeIdMaps();

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("ApplyServerTypeOverrides: Applied overrides to %d manifests"), appliedCount);

    OnTypeManifestUpdated.Broadcast();
#endif
}

void USGDynamicTextAssetRegistry::ClearServerTypeOverrides()
{
    bool bHadOverrides = false;
    for (auto& [classWeak, manifest] : RootClassManifests)
    {
        if (manifest.IsValid() && manifest->HasServerOverrides())
        {
            manifest->ClearServerOverrides();
            bHadOverrides = true;
        }
    }

    if (bHadOverrides)
    {
        RebuildTypeIdMaps();

        UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
            TEXT("ClearServerTypeOverrides: Cleared all server type overrides"));

        OnTypeManifestUpdated.Broadcast();
    }
}

bool USGDynamicTextAssetRegistry::HasServerTypeOverrides() const
{
    for (const auto& [classWeak, manifest] : RootClassManifests)
    {
        if (manifest.IsValid() && manifest->HasServerOverrides())
        {
            return true;
        }
    }
    return false;
}

void USGDynamicTextAssetRegistry::RebuildTypeIdMaps()
{
    TypeIdToSoftClassMap.Empty();
    ClassToTypeIdMap.Empty();

    for (const auto& [classWeak, manifest] : RootClassManifests)
    {
        if (!manifest.IsValid())
        {
            continue;
        }

        TArray<FSGDynamicTextAssetTypeManifestEntry> effectiveEntries;
        manifest->GetAllEffectiveTypes(effectiveEntries);

        for (const FSGDynamicTextAssetTypeManifestEntry& entry : effectiveEntries)
        {
            TypeIdToSoftClassMap.Add(entry.TypeId, entry.Class);

            UClass* resolvedClass = entry.Class.LoadSynchronous();
            if (resolvedClass)
            {
                ClassToTypeIdMap.Add(resolvedClass, entry.TypeId);
            }
        }
    }
}
