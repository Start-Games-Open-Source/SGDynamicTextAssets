// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"

#include "SGDynamicTextAssetSettings.generated.h"

/**
 * Compression methods available for cooked dynamic text asset files.
 */
UENUM(BlueprintType)
enum class ESGDynamicTextAssetCompressionMethod : uint8
{
	/** No compression. Fastest loading but largest file size. Use for debugging or very small dynamic text assets. */
	None     = 0 UMETA(DisplayName = "None"),

	/** Zlib compression (Unreal's default). Good balance of compression ratio and speed. Recommended for most projects. */
	Zlib     = 1 UMETA(DisplayName = "Zlib (Default)"),

	/** Gzip compression. Similar to Zlib with slightly better compression ratio but slower decompression. */
	Gzip     = 2 UMETA(DisplayName = "Gzip"),

	/** LZ4 compression. Very fast decompression with moderate compression ratio. Best for projects prioritizing load times. */
	LZ4      = 3 UMETA(DisplayName = "LZ4 (Fast)"),

	/** Custom compression format resolved via settings. Requires a valid FName registered with Unreal's compression system. */
	Custom   = 255 UMETA(DisplayName = "Custom")
};

/**
 * Data Asset containing SGDynamicTextAssets plugin configuration.
 *
 * Using a Data Asset instead of INI files prevents players from
 * modifying settings in shipped builds. Create an instance of this
 * asset and assign it in Project Settings > Game > SG Dynamic Text Assets.
 *
 * Access via USGDynamicTextAssetSettings::GetSettings().
 */
UCLASS(BlueprintType, NotBlueprintable, ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetSettingsAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	/**
	 * Returns the FName of the custom compression format to use.
	 * Override in settings asset subclasses for dynamic resolution.
	 * Default implementation returns the CustomCompressionName property.
	 */
	virtual FName GetCustomCompressionName() const;

	/** Returns the server request timeout in seconds. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	float GetServerRequestTimeoutSeconds() const { return ServerRequestTimeoutSeconds; }

	/** Returns whether server override is enabled. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	bool IsServerOverrideEnabled() const { return bEnableServerOverride; }

	/** Returns the server cache filename. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	FString GetServerCacheFilename() const { return ServerCacheFilename; }

	/** Returns whether to delete local data on server mismatch. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	bool ShouldDeleteLocalOnServerMismatch() const { return bDeleteLocalOnServerMismatch; }

	/** Returns the default compression method for cooking. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	ESGDynamicTextAssetCompressionMethod GetDefaultCompressionMethod() const { return DefaultCompressionMethod; }

	/** Returns whether UserFacingId should be stripped from the cook manifest. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	bool ShouldStripUserFacingIdFromCookedManifest() const { return bStripUserFacingIdFromCookedManifest; }

	/** Returns whether the cooked directory should be cleaned before cooking. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	bool ShouldCleanCookedDirectoryBeforeCook() const { return bCleanCookedDirectoryBeforeCook; }

	/** Returns whether cooked assets should be deleted after packaging completes. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	bool ShouldDeleteCookedAssetsAfterPackaging() const { return bDeleteCookedAssetsAfterPackaging; }

	/** Whether to enable server-side data override for live games. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server")
	uint8 bEnableServerOverride : 1 = 0;

	/** Whether to delete local dynamic text assets that don't exist on the server. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server")
	uint8 bDeleteLocalOnServerMismatch : 1 = 0;

	/** Timeout in seconds for server requests. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float ServerRequestTimeoutSeconds = 5.0f;

	/** Filename for the server cache (stored in Saved folder). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server")
	FString ServerCacheFilename = "SGDynamicTextAssetServerCache";

	/** Compression method used when cooking dynamic text assets to binary format. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking")
	ESGDynamicTextAssetCompressionMethod DefaultCompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	/** The FName of a custom compression format registered with Unreal's compression system. Only used when DefaultCompressionMethod is Custom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking",
		meta = (EditCondition = "DefaultCompressionMethod == ESGDynamicTextAssetCompressionMethod::Custom"))
	FName CustomCompressionName = NAME_None;

	/**
	 * When enabled, UserFacingId is omitted from the cook manifest.
	 * Useful for removing debug ID's from the manifest which reduces the manifest size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking")
	uint8 bStripUserFacingIdFromCookedManifest : 1 = 0;

	/**
	 * When enabled, deletes all files in the cooked directory before cooking.
	 * Prevents stale .dta.bin files from deleted dynamic text assets persisting across cooks.
	 * Disable for build machines that reuse cooked output for faster iterative builds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking")
	uint8 bCleanCookedDirectoryBeforeCook : 1 = 1;

	/**
	 * When enabled, deletes cooked .dta.bin files after the packaging process completes (success or failure).
	 * Prevents generated binary files from cluttering the Content Browser between packaging runs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking")
	uint8 bDeleteCookedAssetsAfterPackaging : 1 = 1;
};

/**
 * Project settings for SGDynamicTextAssets plugin.
 *
 * Appears in Project Settings > Game > SG Dynamic Text Assets.
 * Stores a soft reference to the settings data asset.
 */
UCLASS(Config = "Game", DefaultConfig, ClassGroup = "Start Games", meta = (DisplayName = "SG Dynamic Text Assets"))
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

	// UDeveloperSettings overrides
	virtual FName GetCategoryName() const override { return FName("Start Games"); }
	// ~UDeveloperSettings overrides

	/**
	 * Gets the singleton settings instance.
	 * @return The settings instance
	 */
	static USGDynamicTextAssetSettings* Get();

	/**
	 * Gets the settings asset, loading it if necessary.
	 * Shorthand for USGDynamicTextAssetSettings::Get()->GetSettingsAsset().
	 *
	 * @return The settings asset, or nullptr if not configured
	 */
	static USGDynamicTextAssetSettingsAsset* GetSettings();

	/**
	 * Gets the settings asset instance, loading it if necessary.
	 * @return The settings asset, or nullptr if not configured
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Settings")
	USGDynamicTextAssetSettingsAsset* GetSettingsAsset() const;

protected:

	/**
	 * Soft reference to the settings data asset.
	 * Using a soft reference allows the asset to be in any location.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Settings", meta = (DisplayName = "Settings Asset"))
	TSoftObjectPtr<USGDynamicTextAssetSettingsAsset> SettingsAsset;

private:

	/** Cached pointer to the loaded settings asset */
	mutable TWeakObjectPtr<USGDynamicTextAssetSettingsAsset> CachedSettingsAsset = nullptr;
};
