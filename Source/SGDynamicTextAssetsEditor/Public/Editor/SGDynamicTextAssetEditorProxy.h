// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Core/SGDynamicTextAssetTypeId.h"

#include "SGDynamicTextAssetEditorProxy.generated.h"

/**
 * Transient proxy UObject used as an asset handle for FSGDynamicTextAssetEditorToolkit.
 *
 * FAssetEditorToolkit requires a UObject to identify the asset being edited.
 * Because dynamic text assets are plain JSON files (not UAssets), this lightweight
 * proxy bridges the gap — it is created transiently and carries the file path
 * and class needed to initialise the editor.
 *
 * Never saved to disk. One proxy instance is created per opened editor window.
 */
UCLASS(Transient)
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetEditorProxy : public UObject
{
    GENERATED_BODY()

public:

    /**
     * Creates a new transient proxy for the given dynamic text asset file.
     *
     * @param FilePath              Absolute path to the .dta.json file
     * @param DynamicTextAssetClass The USGDynamicTextAsset subclass stored in the file
     * @param AssetTypeId           The asset type ID for the class
     * @return                      A new proxy owned by the transient package
     */
    static USGDynamicTextAssetEditorProxy* Create(const FString& FilePath, UClass* DynamicTextAssetClass,
        const FSGDynamicTextAssetTypeId& AssetTypeId);

    /** Absolute path to the dynamic text asset JSON file */
    FString FilePath;

    /** The USGDynamicTextAsset subclass this file contains */
    TWeakObjectPtr<UClass> DynamicTextAssetClass = nullptr;

    /** The asset type ID for this file's class */
    FSGDynamicTextAssetTypeId AssetTypeId;
};
