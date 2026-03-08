// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetRef.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "SGDynamicTextAssetsRuntimeModule.h"
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

    const FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
    return metadata.bIsValid ? metadata.UserFacingId : FString();
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
    // No GameInstance available (editor non-PIE context) — check editor cache (cache-only, no loading)
    return FSGDynamicTextAssetEditorCache::Get().GetDynamicTextAsset(Id);
#else
    return TScriptInterface<ISGDynamicTextAssetProvider>();
#endif
}

void FSGDynamicTextAssetRef::LoadAsync(const UObject* WorldContextObject, TFunction<void(TScriptInterface<ISGDynamicTextAssetProvider>, bool)> OnComplete) const
{
    if (!Id.IsValid())
    {
        if (OnComplete)
        {
            OnComplete(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
        }
        return;
    }

    // Try to get the GameInstance for the normal subsystem path
    const UWorld* world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    const UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;

    if (gameInstance)
    {
        USGDynamicTextAssetSubsystem* subsystem = gameInstance->GetSubsystem<USGDynamicTextAssetSubsystem>();
        if (!subsystem)
        {
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
            if (OnComplete)
            {
                OnComplete(cached, true);
            }
            return;
        }

        // Load asynchronously via the subsystem (resolves file path from ID internally)
        subsystem->LoadDynamicTextAssetAsync(Id, nullptr,
            FOnDynamicTextAssetLoaded::CreateLambda(
                [OnComplete](const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess)
            {
                if (OnComplete)
                {
                    OnComplete(Provider, bSuccess);
                }
            }));
        return;
    }

    // No GameInstance
#if WITH_EDITOR
    // Editor non-PIE fallback
    TScriptInterface<ISGDynamicTextAssetProvider> result = FSGDynamicTextAssetEditorCache::Get().LoadDynamicTextAsset(Id);
    if (OnComplete)
    {
        OnComplete(result, result.GetObject() != nullptr);
    }
#else
    UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
        TEXT("FSGDynamicTextAssetRef::LoadAsync: No GameInstance available for Id(%s)"), *Id.ToString());
    if (OnComplete)
    {
        OnComplete(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
    }
#endif
}

void FSGDynamicTextAssetRef::LoadAsync(const UObject* WorldContextObject, FOnDynamicTextAssetRefLoaded OnComplete) const
{
    LoadAsync(WorldContextObject, [OnComplete](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        OnComplete.ExecuteIfBound(Provider, bSuccess);
    });
}

void FSGDynamicTextAssetRef::LoadAsync(const UObject* WorldContextObject, FOnDynamicTextAssetLoaded OnComplete) const
{
    LoadAsync(WorldContextObject, [OnComplete](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        OnComplete.ExecuteIfBound(Provider, bSuccess);
    });
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
