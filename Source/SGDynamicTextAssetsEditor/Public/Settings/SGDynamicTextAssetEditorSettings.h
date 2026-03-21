// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DeveloperSettings.h"

#include "SGDynamicTextAssetEditorSettings.generated.h"

/**
 * Editor settings for SGDynamicTextAssets plugin.
 * 
 * Appears in Project Settings > Plugins > SG Dynamic Text Assets Editor.
 * Contains editor-only configuration options.
 */
UCLASS(Config = "EditorPerProjectUserSettings", DefaultConfig, ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

	// UDeveloperSettings overrides
	virtual FName GetCategoryName() const override { return FName("Start Games"); }
	virtual FName GetSectionName() const override { return FName("SG Dynamic Text Assets Editor"); }
	// ~UDeveloperSettings overrides

	/** Gets the singleton settings instance. */
	static USGDynamicTextAssetEditorSettings* Get();

	/** Returns the root folder path where cache files and folders are stored relative to the project's Saved folder. */
	FString GetCacheRootFolder() const;

	/**
	 * The folder path where cache files and folders are stored.
	 * Relative to the project's Saved folder.
	 *
	 * IE:
	 * [SAVED FOLDER]/CacheRootFolderPath
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Cache")
	FString CacheRootFolderPath = TEXT("SGDynamicTextAssets");

	/**
	 * Whether to scan Engine content for dynamic text asset references.
	 * Engine Blueprints and Levels rarely reference game dynamic text assets,
	 * so this is disabled by default to speed up scanning.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Reference Scanning")
	uint8 bScanEngineContent : 1 = 0;

	/**
	 * Whether to scan Plugin content for dynamic text asset references.
	 * Enabled by default since plugins often contain game-specific Blueprints.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Reference Scanning")
	uint8 bScanPluginContent : 1 = 1;

	/**
	 * The folder path where reference cache files are stored.
	 * Relative to the project's Saved folder.
	 *
	 * IE:
	 * [SAVED FOLDER]/[ROOT CACHE FOLDER]/ReferenceCacheFolderPath
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Reference Scanning", AdvancedDisplay)
	FString ReferenceCacheFolderPath = TEXT("ReferenceCache");

	/**
	 * Whether to display asset bundle icons next to soft reference properties
	 * that have meta=(AssetBundles="...") tags in the DTA editor details panel.
	 *
	 * *NOTE*
	 * You will have to close and reopen the open DTA's your editing to see the change applied.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor Display")
	uint8 bShowAssetBundleIcons : 1 = 1;
};
