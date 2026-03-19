// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EditorSubsystem.h"
#include "Core/SGDynamicTextAssetEnums.h"
#include "Core/SGDynamicTextAssetId.h"

#include "SGDynamicTextAssetReferenceSubsystem.generated.h"

class USGDynamicTextAsset;
class USGDynamicTextAssetScanSubsystem;

/**
 * Describes a single reference link between an asset and a dynamic text asset.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetReferenceEntry
{
	GENERATED_BODY()

public:

	FSGDynamicTextAssetReferenceEntry() = default;

	FSGDynamicTextAssetReferenceEntry(const FSGDynamicTextAssetId& InReferencedId,
	                            const FSoftObjectPath& InSourceAsset,
	                            const FString& InPropertyPath,
	                            ESGReferenceType InReferenceType)
		: ReferencedId(InReferencedId)
		, SourceAsset(InSourceAsset)
		, PropertyPath(InPropertyPath)
		, ReferenceType(InReferenceType)
	{ }

	/** The ID of the dynamic text asset being referenced */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSGDynamicTextAssetId ReferencedId;

	/** The asset that contains the reference (Blueprint, Level, DynamicTextAsset file, etc.) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSoftObjectPath SourceAsset = nullptr;

	/** The property path where the reference was found (e.g. "WeaponRef", "Loadout.Weapons[0]") */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString PropertyPath;

	/** The type of asset containing the reference */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ESGReferenceType ReferenceType = ESGReferenceType::Other;

	/** Display name of the source asset for UI */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SourceDisplayName;

	/** For DynamicTextAsset-type entries only: raw file system path to the source .ddo file. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SourceFilePath;
};

/**
 * Content category for reference cache organization.
 */
enum class ESGReferenceCacheContentType : uint8
{
	/** Content from the project's Content folder */
	GameContent,
	/** Content from plugin folders */
	PluginContent,
	/** Content from the Engine folder */
	EngineContent,
	/** Dynamic text asset files (always scanned) */
	DynamicTextAssets
};

/**
 * Cached entry for a single scanned asset.
 * Stores the asset's timestamp and found references for incremental scanning.
 */
struct SGDYNAMICTEXTASSETSEDITOR_API FSGReferenceCacheEntry
{
	/** Full path to the asset or file */
	FString AssetPath;

	/** Last modified timestamp when this entry was scanned */
	FDateTime LastModifiedTime;

	/** Type of content this asset belongs to */
	ESGReferenceCacheContentType ContentType = ESGReferenceCacheContentType::GameContent;

	/** References found in this asset (ID -> property paths) */
	TMap<FSGDynamicTextAssetId, TArray<FString>> FoundReferences;

	/** Display name for the source asset */
	FString SourceDisplayName;

	/** Reference type for all entries from this asset */
	ESGReferenceType ReferenceType = ESGReferenceType::Other;
};

/**
 * Describes a dependency from a dynamic text asset to another dynamic text asset.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetDependencyEntry
{
	GENERATED_BODY()

public:

	FSGDynamicTextAssetDependencyEntry() = default;

	FSGDynamicTextAssetDependencyEntry(const FSGDynamicTextAssetId& InDependencyId,
	                             const FString& InPropertyPath,
	                             const FString& InUserFacingId)
		: DependencyId(InDependencyId)
		, PropertyPath(InPropertyPath)
		, UserFacingId(InUserFacingId)
		, ReferenceType(ESGReferenceType::DynamicTextAsset)
	{ }

	FSGDynamicTextAssetDependencyEntry(const FSoftObjectPath& InAssetPath,
	                             const FString& InPropertyPath,
	                             const FName& InAssetClass,
	                             ESGReferenceType InReferenceType = ESGReferenceType::Blueprint)
		: PropertyPath(InPropertyPath)
		, bIsAssetReference(true)
		, AssetPath(InAssetPath)
		, AssetClass(InAssetClass)
		, ReferenceType(InReferenceType)
	{ }

	/** The ID of the dynamic text asset being depended upon */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency")
	FSGDynamicTextAssetId DependencyId;

	/** The property path that holds the reference (e.g. "MaterialSetRef") */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency")
	FString PropertyPath;

	/** User-facing ID of the dependency for display */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency")
	FString UserFacingId;

	/** True if this is an asset reference instead of a dynamic text asset reference */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency")
	uint8 bIsAssetReference : 1 = 0;

	/** The path to the referenced asset */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency")
	FSoftObjectPath AssetPath;

	/** The class name of the referenced asset */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency")
	FName AssetClass;

	/** The type of reference (DynamicTextAsset, Blueprint, Level, etc.) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependency")
	ESGReferenceType ReferenceType = ESGReferenceType::Other;
};

/**
 * Editor subsystem that tracks references between assets and dynamic text assets.
 *
 * Provides two directions of reference scanning:
 * - FindReferencers: What assets reference a given dynamic text asset?
 * - FindDependencies: What dynamic text assets does a given dynamic text asset reference?
 *
 * Results are cached and refreshed on demand or when dynamic text assets change.
 */
UCLASS(ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetReferenceSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:

	// UEditorSubsystem overrides
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~UEditorSubsystem overrides

	/**
	 * Finds all assets that reference a specific dynamic text asset by ID.
	 * Scans Blueprint CDOs, level actors, and other dynamic text assets for
	 * FSGDynamicTextAssetRef properties containing the target ID.
	 *
	 * @param Id The dynamic text asset ID to find referencers for
	 * @param OutReferencers Output array of reference entries
	 * @param bForceRescan If true, ignores cache and rescans all assets
	 */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|References")
	void FindReferencers(const FSGDynamicTextAssetId& Id, TArray<FSGDynamicTextAssetReferenceEntry>& OutReferencers, bool bForceRescan = false);

	/**
	 * Finds all dynamic text assets that a specific dynamic text asset depends on.
	 * Inspects the dynamic text asset's FSGDynamicTextAssetRef properties to find
	 * outgoing references.
	 *
	 * @param Id The dynamic text asset IO to find dependencies for
	 * @param OutDependencies Output array of dependency entries
	 */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|References")
	void FindDependencies(const FSGDynamicTextAssetId& Id, TArray<FSGDynamicTextAssetDependencyEntry>& OutDependencies);

	/**
	 * Returns the total number of referencers for a dynamic text asset.
	 * Uses cached data if available.
	 *
	 * @param Id The dynamic text asset ID
	 * @return Number of assets referencing this dynamic text asset
	 */
	UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|References")
	int32 GetReferencerCount(const FSGDynamicTextAssetId& Id);

	/**
	 * Invalidates the cached reference data, forcing a full rescan
	 * on the next FindReferencers call.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|References")
	void InvalidateCache();

	/**
	 * Performs a full scan of all assets and builds the complete
	 * reference map. Call this once to populate the cache, then
	 * use FindReferencers for individual lookups.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|References")
	void RebuildReferenceCache();

	/**
	 * Starts an asynchronous rebuild of the reference cache.
	 * Uses incremental scanning - only rescans files that have changed since last scan.
	 */
	void RebuildReferenceCacheAsync();

	/**
	 * Clears all cached data and performs a full rescan.
	 * Use this when you want to force a complete rebuild of the cache.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|References")
	void ClearCacheAndRescan();

	/**
	 * Saves the current reference cache to disk.
	 * Called automatically after scans complete.
	 */
	void SaveCacheToDisk();

	/**
	 * Loads the reference cache from disk.
	 * Called during subsystem initialization.
	 */
	void LoadCacheFromDisk();

	/** Returns true if the cache is currently being rebuilt */
	UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|References")
	bool IsScanningInProgress() const;

	/** Returns true if the cache has been populated at least once */
	UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|References")
	bool IsCacheReady() const;

	/** Delegate broadcast when async scanning completes */
	DECLARE_MULTICAST_DELEGATE(FOnReferenceScanComplete);
	FOnReferenceScanComplete OnReferenceScanComplete;

	/** Delegate broadcast for progress updates during async scan */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnReferenceScanProgress, int32 /*Current*/, int32 /*Total*/, const FText& /*StatusText*/);
	FOnReferenceScanProgress OnReferenceScanProgress;

	/** Registers scan phases with the scan subsystem for Blueprint, Level, and DTA reference scanning. */
	void RegisterScanPhases();

private:

	/** Phase setup callbacks: populate pending work queues for each phase. */
	void SetupBlueprintScanPhase();
	void SetupLevelScanPhase();
	void SetupDTAReferenceScanPhase();

	/** Phase process callbacks: process one item per call. Returns true if more remain. */
	bool ProcessOneBlueprintItem();
	bool ProcessOneLevelItem();
	bool ProcessOneDTAReferenceItem();

	/** Phase completion callbacks. */
	void OnBlueprintPhaseComplete();
	void OnLevelPhaseComplete();
	void OnDTAReferencePhaseComplete();

	/** Processes a single Blueprint asset for references */
	void ProcessBlueprintAsset(const FAssetData& AssetData);

	/** Processes a single Level asset for references */
	void ProcessLevelAsset(const FAssetData& AssetData);

	/** Processes a single Dynamic Text Asset file for references */
	void ProcessDynamicTextAssetFile(const FString& FilePath);

	/**
	 * Scans all Blueprint assets for FSGDynamicTextAssetRef properties
	 * and populates the reference cache.
	 */
	void ScanBlueprintAssets();

	/**
	 * Scans all dynamic text asset JSON files for FSGDynamicTextAssetRef properties
	 * within their serialized data.
	 */
	void ScanDynamicTextAssetFiles();

	/**
	 * Scans all Level (World) assets for actors and components
	 * that contain FSGDynamicTextAssetRef properties.
	 */
	void ScanLevelAssets();

	/**
	 * Inspects a UClass for FSGDynamicTextAssetRef properties and extracts
	 * ID values from the given object instance.
	 *
	 * @param ObjectClass The class to inspect for struct properties
	 * @param ObjectInstance The object instance to read values from (can be CDO)
	 * @param SourceAsset The asset path for reporting
	 * @param SourceDisplayName Display name for the source asset
	 * @param ReferenceType The type of source asset
	 * @param OutEntries Output array that receives all found reference entries
	 */
	void ExtractRefsFromObject(const UClass* ObjectClass,
	                           const UObject* ObjectInstance,
	                           const FSoftObjectPath& SourceAsset,
	                           const FString& SourceDisplayName,
	                           ESGReferenceType ReferenceType,
	                           TArray<FSGDynamicTextAssetReferenceEntry>& OutEntries);

	/**
	 * Recursively inspects a property for FSGDynamicTextAssetRef values.
	 * Handles direct struct properties, arrays of structs, and maps.
	 *
	 * @param Property The property to inspect
	 * @param ContainerPtr Pointer to the containing object's data
	 * @param PropertyPath Dot-separated path built during recursion
	 * @param SourceAsset The asset path for reporting
	 * @param SourceDisplayName Display name for the source asset
	 * @param ReferenceType The type of source asset
	 * @param OutEntries Output array that receives all found reference entries
	 */
	void ExtractRefsFromProperty(const FProperty* Property,
	                             const void* ContainerPtr,
	                             const FString& PropertyPath,
	                             const FSoftObjectPath& SourceAsset,
	                             const FString& SourceDisplayName,
	                             ESGReferenceType ReferenceType,
	                             TArray<FSGDynamicTextAssetReferenceEntry>& OutEntries);

	/**
	 * Inspects a loaded data object's properties for outgoing
	 * FSGDynamicTextAssetRef dependencies.
	 *
	 * @param DataObject The data object (ISGDynamicTextAssetProvider implementor) to inspect
	 * @param OutDependencies Output array of found dependencies
	 */
	void ExtractDependenciesFromObject(const UObject* DataObject,
	                                   TArray<FSGDynamicTextAssetDependencyEntry>& OutDependencies) const;

	/**
	 * Recursively inspects a property for FSGDynamicTextAssetRef dependencies.
	 *
	 * @param Property The property to inspect
	 * @param ContainerPtr Pointer to the containing object's data
	 * @param PropertyPath Dot-separated path built during recursion
	 * @param OutDependencies Output array of found dependencies
	 */
	void ExtractDepsFromProperty(const FProperty* Property,
	                             const void* ContainerPtr,
	                             const FString& PropertyPath,
	                             TArray<FSGDynamicTextAssetDependencyEntry>& OutDependencies) const;

	/**
	 * Cached reference map: Dynamic text asset ID -> list of referencers.
	 * Populated by RebuildReferenceCache or lazily by FindReferencers.
	 */
	TMap<FSGDynamicTextAssetId, TArray<FSGDynamicTextAssetReferenceEntry>> ReferencerCache;

	/**
	 * Persistent cache of scanned assets: Asset path -> cache entry.
	 * Used for incremental scanning to skip unchanged files.
	 */
	TMap<FString, FSGReferenceCacheEntry> PersistentAssetCache;

	/** Whether the cache has been populated at least once */
	uint8 bCachePopulated : 1;

	/** Whether to perform a full rescan (ignore timestamps) */
	uint8 bForceFullRescan : 1;

	/** Pending Blueprint assets to scan during async operation */
	TArray<FAssetData> PendingBlueprintAssets;

	/** Pending Level assets to scan during async operation */
	TArray<FAssetData> PendingLevelAssets;

	/** Pending Dynamic Text Asset files to scan during async operation */
	TArray<FString> PendingDynamicTextAssetFiles;

	/** Determines the content type for an asset path */
	ESGReferenceCacheContentType GetContentTypeForPath(const FString& AssetPath) const;

	/** Gets the full path to the cache folder in Saved directory */
	FString GetCacheFolderPath() const;

	/** Gets the cache file path for a specific content type */
	FString GetCacheFilePath(ESGReferenceCacheContentType ContentType) const;

	/** Checks if an asset needs to be rescanned based on its timestamp */
	bool DoesAssetNeedRescan(const FString& AssetPath, const FDateTime& CurrentTimestamp) const;

	/** Rebuilds the ReferencerCache from the PersistentAssetCache */
	void RebuildReferencerCacheFromPersistent();
};
