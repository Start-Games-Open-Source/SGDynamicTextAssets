// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetRef.h"

#include "Core/SGDynamicTextAssetBundleData.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "SGDynamicTextAssetLogs.h"
#include "Subsystem/SGDynamicTextAssetSubsystem.h"

#if WITH_EDITOR
#include "Management/SGDynamicTextAssetEditorCache.h"
#endif

bool FSGDynamicTextAssetRef::IsNull() const
{
    return !Id.IsValid();
}

bool FSGDynamicTextAssetRef::IsValid() const
{
    return Id.IsValid();
}

const FSGDynamicTextAssetId& FSGDynamicTextAssetRef::GetId() const
{
    return Id;
}

FString FSGDynamicTextAssetRef::GetUserFacingId() const
{
    if (!Id.IsValid())
    {
        return FString();
    }

    FString filePath;
    if (!FSGDynamicTextAssetFileManager::FindFileForId(Id, filePath))
    {
        return FString();
    }

    const FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);
    return fileInfo.bIsValid ? fileInfo.UserFacingId : FString();
}

void FSGDynamicTextAssetRef::SetId(const FSGDynamicTextAssetId& InId)
{
    Id = InId;
}

void FSGDynamicTextAssetRef::Reset()
{
    Id.Invalidate();
}

bool FSGDynamicTextAssetRef::Serialize(FArchive& Ar)
{
    Id.Serialize(Ar);
    return true;
}

bool FSGDynamicTextAssetRef::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    Id.NetSerialize(Ar, Map, bOutSuccess);
    bOutSuccess = true;
    return true;
}

TScriptInterface<ISGDynamicTextAssetProvider> FSGDynamicTextAssetRef::Get(const UGameInstance* GameInstance) const
{
    if (!Id.IsValid() || !GameInstance)
    {
        return TScriptInterface<ISGDynamicTextAssetProvider>();
    }

    if (USGDynamicTextAssetSubsystem* subsystem = GameInstance->GetSubsystem<USGDynamicTextAssetSubsystem>())
    {
        return subsystem->GetDynamicTextAsset(Id);
    }

    return TScriptInterface<ISGDynamicTextAssetProvider>();
}

TScriptInterface<ISGDynamicTextAssetProvider> FSGDynamicTextAssetRef::Get(const UObject* WorldContextObject) const
{
    if (!Id.IsValid() || !WorldContextObject)
    {
        return TScriptInterface<ISGDynamicTextAssetProvider>();
    }

    const UWorld* world = WorldContextObject->GetWorld();
    const UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
    if (gameInstance)
    {
        return Get(gameInstance);
    }

#if WITH_EDITOR
    // No GameInstance available (editor non-PIE context)  - check editor cache (cache-only, no loading)
    return FSGDynamicTextAssetEditorCache::Get().GetDynamicTextAsset(Id);
#else
    return TScriptInterface<ISGDynamicTextAssetProvider>();
#endif
}

void FSGDynamicTextAssetRef::LoadAsync(const UObject* WorldContextObject,
    TFunction<void(TScriptInterface<ISGDynamicTextAssetProvider>, bool)> OnComplete,
    const TArray<FName>& BundleNames, const FString& FilePath) const
{
    if (!Id.IsValid())
    {
        if (OnComplete)
        {
            OnComplete(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
        }
        return;
    }

    // After the DTA loads, optionally async-load any requested bundle assets
    // before invoking the caller's callback.
    auto loadBundlesThenCallback = [](const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess,
        TFunction<void(TScriptInterface<ISGDynamicTextAssetProvider>, bool)> Callback,
        const TArray<FName>& InBundleNames)
    {
        if (!bSuccess || InBundleNames.IsEmpty() || !Provider.GetInterface())
        {
            if (Callback)
            {
                Callback(Provider, bSuccess);
            }
            return;
        }

        // Gather paths for all requested bundles
        const FSGDynamicTextAssetBundleData& bundleData = Provider->GetSGDTAssetBundleData();

        TArray<FSoftObjectPath> paths;
        for (const FName& bundleName : InBundleNames)
        {
            bundleData.GetPathsForBundle(bundleName, paths);
        }

        if (paths.IsEmpty())
        {
            if (Callback)
            {
                Callback(Provider, bSuccess);
            }
            return;
        }

        UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
            TEXT("FSGDynamicTextAssetRef::LoadAsync: Async-loading %d bundle asset(s) for DTA '%s'"),
            paths.Num(), *GetNameSafe(Provider.GetObject()));

        FStreamableManager& streamableManager = UAssetManager::Get().GetStreamableManager();
        streamableManager.RequestAsyncLoad(paths,
            FStreamableDelegate::CreateLambda([Provider, Callback = MoveTemp(Callback), paths]()
            {
#if !UE_BUILD_SHIPPING
                // Validate each loaded object, only doing this on non-shipping builds
                // so shipping builds can strip out unnecessary validation.
                for (int32 i = 0; i < paths.Num(); ++i)
                {
                    UObject* resolved = paths[i].ResolveObject();
                    if (!resolved)
                    {
                        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                            TEXT("FSGDynamicTextAssetRef::LoadAsync: Bundle asset failed to resolve after async load - Path='%s'. "
                                "Verify the asset is not a Blueprint (UBlueprint objects are stripped in packaged builds) "
                                "and that it is referenced by at least one cooked UE asset."),
                            *paths[i].ToString());
                    }
                }
#endif
                if (Callback)
                {
                    Callback(Provider, true);
                }
            }));
    };

    // Try to get the GameInstance for the normal subsystem path
    if (const UWorld* world = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
    {
        if (const UGameInstance* gameInstance = world->GetGameInstance())
        {
            USGDynamicTextAssetSubsystem* subsystem = gameInstance->GetSubsystem<USGDynamicTextAssetSubsystem>();
            if (!subsystem)
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetRef::LoadAsync: "
                    "NULL USGDynamicTextAssetSubsystem from gameInstance(%s)|world(%s)"),
                    *GetNameSafe(gameInstance),
                    *GetNameSafe(world));
                if (OnComplete)
                {
                    OnComplete(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
                }
                return;
            }

            // Check if already cached
            TScriptInterface<ISGDynamicTextAssetProvider> cached = subsystem->GetDynamicTextAsset(Id);
            if (cached.GetObject())
            {
                loadBundlesThenCallback(cached, true, MoveTemp(OnComplete), BundleNames);
                return;
            }

            // Load asynchronously via the subsystem
            if (FilePath.IsEmpty())
            {
                // ID-based search
                subsystem->LoadDynamicTextAssetAsync(Id, nullptr,
                    FOnDynamicTextAssetLoaded::CreateLambda(
                        [OnComplete = MoveTemp(OnComplete), BundleNames, loadBundlesThenCallback]
                        (const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess) mutable
                    {
                        loadBundlesThenCallback(Provider, bSuccess, MoveTemp(OnComplete), BundleNames);
                    }));
            }
            else
            {
                // Load from explicit file path
                subsystem->LoadDynamicTextAssetFromFileAsync(FilePath, nullptr,
                    FOnDynamicTextAssetLoaded::CreateLambda(
                        [OnComplete = MoveTemp(OnComplete), BundleNames, loadBundlesThenCallback]
                        (const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess) mutable
                    {
                        loadBundlesThenCallback(Provider, bSuccess, MoveTemp(OnComplete), BundleNames);
                    }));
            }
            return;
        }
    }

    // No GameInstance
#if WITH_EDITOR
    // Editor non-PIE fallback for tooling (synchronous)
    TScriptInterface<ISGDynamicTextAssetProvider> result = FSGDynamicTextAssetEditorCache::Get().LoadDynamicTextAsset(Id);
    loadBundlesThenCallback(result, result.GetObject() != nullptr, MoveTemp(OnComplete), BundleNames);
#else
    UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
        TEXT("FSGDynamicTextAssetRef::LoadAsync: No GameInstance available for Id(%s)"), *Id.ToString());
    if (OnComplete)
    {
        OnComplete(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
    }
#endif
}

void FSGDynamicTextAssetRef::LoadAsync(const UObject* WorldContextObject,
    FOnDynamicTextAssetRefLoaded OnComplete,
    const TArray<FName>& BundleNames, const FString& FilePath) const
{
    LoadAsync(WorldContextObject, [OnComplete](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        OnComplete.ExecuteIfBound(Provider, bSuccess);
    }, BundleNames, FilePath);
}

void FSGDynamicTextAssetRef::LoadAsync(const UObject* WorldContextObject,
    FOnDynamicTextAssetLoaded OnComplete,
    const TArray<FName>& BundleNames, const FString& FilePath) const
{
    LoadAsync(WorldContextObject, [OnComplete](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        OnComplete.ExecuteIfBound(Provider, bSuccess);
    }, BundleNames, FilePath);
}

bool FSGDynamicTextAssetRef::operator==(const FSGDynamicTextAssetRef& Other) const
{
    return Id == Other.Id;
}

bool FSGDynamicTextAssetRef::operator!=(const FSGDynamicTextAssetRef& Other) const
{
    return Id != Other.Id;
}

bool FSGDynamicTextAssetRef::operator==(const FSGDynamicTextAssetId& Other) const
{
    return Id == Other;
}

bool FSGDynamicTextAssetRef::operator!=(const FSGDynamicTextAssetId& Other) const
{
    return Id != Other;
}

bool FSGDynamicTextAssetRef::ExportTextItem(FString& ValueStr, const FSGDynamicTextAssetRef& DefaultValue, UObject* Parent,
                                      int32 PortFlags, UObject* ExportRootScope) const
{
    ValueStr += Id.ToString();
    return true;
}

bool FSGDynamicTextAssetRef::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
    return Id.ImportTextItem(Buffer, PortFlags, Parent, ErrorText);
}
