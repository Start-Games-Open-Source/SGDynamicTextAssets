// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDynamicTextAssetEditorProxy.h"

#include "Management/SGDynamicTextAssetFileManager.h"
#include "UObject/Package.h"

USGDynamicTextAssetEditorProxy* USGDynamicTextAssetEditorProxy::Create(const FString& InFilePath,
    UClass* InDynamicTextAssetClass, const FSGDynamicTextAssetTypeId& InAssetTypeId)
{
    // Using user facing ID for the name since this is used with the editor tab information
    USGDynamicTextAssetEditorProxy* proxy = NewObject<USGDynamicTextAssetEditorProxy>(GetTransientPackage(),
        FName(FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(InFilePath)), RF_Transient);
    proxy->FilePath = InFilePath;
    proxy->DynamicTextAssetClass = InDynamicTextAssetClass;
    proxy->AssetTypeId = InAssetTypeId;
    return proxy;
}
