// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetRef.h"

#include "Core/SGDynamicTextAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Subsystem/SGDynamicTextAssetSubsystem.h"

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
    if (!world)
    {
        return TScriptInterface<ISGDynamicTextAssetProvider>();
    }

    const UGameInstance* gameInstance = world->GetGameInstance();
    if (!gameInstance)
    {
        return TScriptInterface<ISGDynamicTextAssetProvider>();
    }

    return Get(gameInstance);
}

void FSGDynamicTextAssetRef::LoadAsync(const UGameInstance* GameInstance, const FString& FilePath, TFunction<void(TScriptInterface<ISGDynamicTextAssetProvider>, bool)> OnComplete) const
{
    if (!Id.IsValid() || !GameInstance || FilePath.IsEmpty())
    {
        if (OnComplete)
        {
            OnComplete(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
        }
        return;
    }

    USGDynamicTextAssetSubsystem* subsystem = GameInstance->GetSubsystem<USGDynamicTextAssetSubsystem>();
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

    subsystem->LoadDynamicTextAssetFromFileAsync(FilePath, USGDynamicTextAsset::StaticClass(),
        FOnDynamicTextAssetLoaded::CreateLambda([OnComplete](const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess)
        {
            if (OnComplete)
            {
                OnComplete(Provider, bSuccess);
            }
        }));
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
