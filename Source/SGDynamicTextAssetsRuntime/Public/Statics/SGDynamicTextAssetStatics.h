// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAssetBundleData.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "SGDynamicTextAssetDelegates.h"

#include "SGDynamicTextAssetStatics.generated.h"

struct FSGDynamicTextAssetValidationResult;

class USGDynamicTextAsset;

/**
 * Blueprint function library for working with FSGDynamicTextAssetRef.
 * 
 * Provides static utility functions for validity checks, getters, setters,
 * loading operations, and collection queries on dynamic text asset references.
 */
UCLASS(ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Returns true if the reference has a valid ID.
	 * Does not check if the dynamic text asset actually exists or is loaded.
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference")
	static bool IsValidDynamicTextAssetRef(const FSGDynamicTextAssetRef& Ref);

	/**
	 * Returns true if the referenced dynamic text asset is currently loaded in memory.
	 * 
	 * @param WorldContextObject Object used to get the game instance
	 * @param Ref The dynamic text asset reference to check
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference", meta = (WorldContext = "WorldContextObject"))
	static bool IsDynamicTextAssetRefLoaded(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref);

	/**
	 * Sets the reference to point to the specified ID.
	 * 
	 * @param Ref The reference to modify
	 * @param Id The ID to set
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Reference")
	static void SetDynamicTextAssetRefById(UPARAM(ref) FSGDynamicTextAssetRef& Ref, const FSGDynamicTextAssetId& Id);

	/**
	 * Sets the reference by looking up a User Facing ID.
	 * Returns true if the ID was found and the reference was set.
	 * 
	 * @param Ref The reference to modify
	 * @param UserFacingId The user-facing ID to look up
	 * @return True if the ID was found and reference was set
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Reference")
	static bool SetDynamicTextAssetRefByUserFacingId(UPARAM(ref) FSGDynamicTextAssetRef& Ref, const FString& UserFacingId);

	/**
	 * Clears the reference, resetting it to an invalid state.
	 * 
	 * @param Ref The reference to clear
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Reference")
	static void ClearDynamicTextAssetRef(UPARAM(ref) FSGDynamicTextAssetRef& Ref);

	/**
	 * Gets the ID from the reference.
	 * 
	 * @param Ref The reference to get the ID from
	 * @return The ID (may be invalid if reference is not set)
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference")
	static FSGDynamicTextAssetId GetDynamicTextAssetRefId(const FSGDynamicTextAssetRef& Ref);

	/**
	 * Returns the user-facing ID for the referenced dynamic text asset by resolving its file information.
	 * Works in any context editor, runtime, without a world or game instance.
	 * Returns an empty string if the ID is invalid or no matching file is found.
	 *
	 * @param Ref The reference to resolve the user-facing ID from
	 * @return The user-facing ID, or empty string if not found
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference")
	static FString GetDynamicTextAssetRefUserFacingId(const FSGDynamicTextAssetRef& Ref);

	/**
	 * Gets the loaded dynamic text asset from the reference.
	 * Returns an empty TScriptInterface if not loaded or ID is invalid.
	 * 
	 * @param WorldContextObject Object used to get the game instance (allowed to be null in editor).
	 * @param Ref The reference to resolve
	 * @return The loaded provider, or empty TScriptInterface
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference", meta = (WorldContext = "WorldContextObject"))
	static TScriptInterface<ISGDynamicTextAssetProvider> GetDynamicTextAsset(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref);

	/**
	 * Loads the dynamic text asset asynchronously.
	 * The callback will be called when loading completes (or fails).
	 *
	 * When BundleNames is non-empty, the referenced assets for those bundles
	 * are also async-loaded before the callback fires. The callback is only
	 * invoked once both the DTA and all requested bundle assets are ready.
	 *
	 * @param WorldContextObject Object used to get the game instance
	 * @param Ref The reference to load
	 * @param OnLoaded Callback when loading completes
	 * @param BundleNames [Optional] Bundle names to async-load after the DTA is cached. Empty loads no bundles.
	 * @param FilePath [Optional] The file path to load from. If empty, the system will search for the file using the ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Reference", meta = (WorldContext = "WorldContextObject",
		AutoCreateRefTerm = "BundleNames, FilePath", AdvancedDisplay = "BundleNames, FilePath"))
	static void LoadDynamicTextAssetRefAsync(const UObject* WorldContextObject,
		const FSGDynamicTextAssetRef& Ref, FOnDynamicTextAssetRefLoaded OnLoaded,
		const TArray<FName>& BundleNames, const FString& FilePath = TEXT(""));

	/** Shorthand version of ::LoadDynamicTextAssetRefAsync with empty BundleNames and FilePath. */
	static void LoadDynamicTextAssetRefAsync(const UObject* WorldContextObject,
		const FSGDynamicTextAssetRef& Ref, FOnDynamicTextAssetRefLoaded OnLoaded)
	{
		LoadDynamicTextAssetRefAsync(WorldContextObject, Ref, MoveTemp(OnLoaded),
			TArray<FName>(), FString());
	}

	/**
	 * Removes the referenced dynamic text asset from the cache (unloads it).
	 * The object will be garbage collected if no other references exist.
	 * 
	 * @param WorldContextObject Object used to get the game instance
	 * @param Ref The reference to unload
	 * @return True if the object was found and removed from cache
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Reference", meta = (WorldContext = "WorldContextObject"))
	static bool UnloadDynamicTextAssetRef(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref);

	/** Returns true if the two dynamic text asset refs are equal (A == B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference", meta = (DisplayName = "Equal (Dynamic Text Asset Ref)",
		CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_DynamicTextAssetRef(const FSGDynamicTextAssetRef& A, const FSGDynamicTextAssetRef& B);

	/** Returns true if the two dynamic text asset refs are not equal (A != B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference", meta = (DisplayName = "Not Equal (Dynamic Text Asset Ref)",
		CompactNodeTitle = "!=", Keywords = "!= not equal"))
	static bool NotEqual_DynamicTextAssetRef(const FSGDynamicTextAssetRef& A, const FSGDynamicTextAssetRef& B);

	/** Returns true if the dynamic text asset refs is equal to the dynamic text asset Id (A == B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets", meta = (DisplayName = "Equal (Ref == Id)",
		CompactNodeTitle = "==", Keywords = "== equal"))
	static bool Equals_DynamicTextAssetRefDynamicTextAssetId(const FSGDynamicTextAssetRef& Ref, const FSGDynamicTextAssetId& Id);

	/** Returns true if the dynamic text asset refs is not equal to the dynamic text asset Id (A != B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets", meta = (DisplayName = "Not Equal (Ref != Id)",
		CompactNodeTitle = "!=", Keywords = "!= not equal"))
	static bool NotEquals_DynamicTextAssetRefDynamicTextAssetId(const FSGDynamicTextAssetRef& Ref, const FSGDynamicTextAssetId& Id);

	/** Returns true if the two dynamic text asset ID's are equal (A == B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|ID", meta = (DisplayName = "Equal (Dynamic Text Asset ID)",
		CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_DynamicTextAssetId(const FSGDynamicTextAssetId& A, const FSGDynamicTextAssetId& B);

	/** Returns true if the two dynamic text asset ID's are not equal (A != B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|ID", meta = (DisplayName = "Not Equal (Dynamic Text Asset ID)",
		CompactNodeTitle = "!=", Keywords = "!= not equal"))
	static bool NotEqual_DynamicTextAssetId(const FSGDynamicTextAssetId& A, const FSGDynamicTextAssetId& B);

	/**
	 * Gets all Ids of dynamic text assets of the specified class (from file system).
	 * This scans the file system, not the loaded cache.
	 * 
	 * @param DynamicTextAssetClass The class to filter by
	 * @param bIncludeSubclasses Whether to include subclasses
	 * @param OutIds Array to populate with found Ids
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Collection")
	static void GetAllDynamicTextAssetIdsByClass(UClass* DynamicTextAssetClass,
		bool bIncludeSubclasses, TArray<FSGDynamicTextAssetId>& OutIds);

	/**
	 * Gets all User Facing IDs of dynamic text assets of the specified class (from file system).
	 * This scans the file system, not the loaded cache.
	 * 
	 * @param DynamicTextAssetClass The class to filter by
	 * @param bIncludeSubclasses Whether to include subclasses
	 * @param OutUserFacingIds Array to populate with found IDs
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Collection")
	static void GetAllDynamicTextAssetUserFacingIdsByClass(UClass* DynamicTextAssetClass,
		bool bIncludeSubclasses, TArray<FString>& OutUserFacingIds);

	/**
	 * Gets all currently loaded dynamic text assets of the specified class.
	 * Only returns objects that are already in the cache.
	 * 
	 * @param WorldContextObject Object used to get the game instance
	 * @param DynamicTextAssetClass The class to filter by
	 * @param bIncludeSubclasses Whether to include subclasses
	 * @param OutDynamicTextAssets Array to populate with loaded objects
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Collection", meta = (WorldContext = "WorldContextObject"))
	static void GetAllLoadedDynamicTextAssetsOfClass(const UObject* WorldContextObject, UClass* DynamicTextAssetClass,
		bool bIncludeSubclasses, TArray<TScriptInterface<ISGDynamicTextAssetProvider>>& OutDynamicTextAssets);

	/**
	 * Creates a dynamic text asset reference from a ID.
	 * 
	 * @param Id The ID to create a reference for
	 * @return A new dynamic text asset reference
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Reference")
	static FSGDynamicTextAssetRef MakeDynamicTextAssetRef(const FSGDynamicTextAssetId& Id);

	/**
 	 * Attempts to retrieve the dynamic text asset's type via the inputted ID.
 	 * *WARNING* Can be a heavy operation depending on over usage.
 	 *
 	 * @param Id The ID to Query the type for.
 	 * @return The found dynamic text asset type.
 	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Utilities")
	static UClass* GetDynamicTextAssetTypeFromId(const FSGDynamicTextAssetId& Id);

	/**
	 * Converts a ID to its string representation.
	 * 
	 * @param Id The ID to convert
	 * @return The ID as a string
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Utilities")
	static FString IdToString(const FSGDynamicTextAssetId& Id);

	/**
	 * Parses a string to a ID.
	 *
	 * @param IdString The string to parse
	 * @param OutId The parsed ID
	 * @return True if parsing succeeded
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Utilities")
	static bool StringToId(const FString& IdString, FSGDynamicTextAssetId& OutId);

	/** Returns true if the dynamic text asset type ID has a valid (non-zero) GUID. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DisplayName = "Is Valid (DTA Type ID)"))
	static bool IsValid_DTA_TypeId(const FSGDynamicTextAssetTypeId& AssetTypeId);

	/** Returns true if the two dynamic text asset type IDs are equal (A == B). */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DisplayName = "Equal (DTA Type ID)",
		CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_DTA_TypeId(const FSGDynamicTextAssetTypeId& A, const FSGDynamicTextAssetTypeId& B);

	/** Returns true if the two asset type IDs are not equal (A != B). */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DisplayName = "Not Equal (DTA Type ID)",
		CompactNodeTitle = "!=", Keywords = "!= not equal"))
	static bool NotEqual_DTA_TypeId(const FSGDynamicTextAssetTypeId& A, const FSGDynamicTextAssetTypeId& B);

	/**
	 * Converts an dynamic text asset type ID to its string representation.
	 *
	 * @param AssetTypeId The asset type ID to convert
	 * @return The ID as a hyphenated GUID string
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DisplayName = "To String (DTA Type ID)"))
	static FString ToString_DTA_TypeId(const FSGDynamicTextAssetTypeId& AssetTypeId);

	/**
	 * Parses a string to an dynamic text asset type ID.
	 *
	 * @param AssetTypeIdString The string to parse (hyphenated GUID format)
	 * @param OutAssetTypeId The parsed asset type ID
	 * @return True if parsing succeeded
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DisplayName = "From String (DTA Type ID)"))
	static bool FromString_DTA_TypeId(const FString& AssetTypeIdString, FSGDynamicTextAssetTypeId& OutAssetTypeId);

	/** Creates a dynamic text asset type ID from the inputted GUID. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DisplayName = "From GUID (DTA Type ID)"))
	static FSGDynamicTextAssetTypeId FromGuid_DTA_TypeId(const FGuid& Guid);

	/** Returns a copy of the dynamic text asset type Id's GUID. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DisplayName = "Get GUID (DTA Type ID)"))
	static FGuid GetGuid_DTA_TypeId(const FSGDynamicTextAssetTypeId& AssetTypeId);

	/**
	 * Creates a new randomly generated asset type ID.
	 * Primarily for editor/tooling use, runtime code should use IDs from manifests.
	 *
	 * @return A new unique asset type ID
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Asset Type ID", meta = (DevelopmentOnly))
	static FSGDynamicTextAssetTypeId GenerateDynamicAssetTypeId();

	/** Returns true if version fields represent a usable version (Major >= 1) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Version")
	static bool IsVersionValid(const FSGDynamicTextAssetVersion& Version);

	/** Returns true if major versions match (compatible for loading) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Version")
	static bool IsVersionCompatibleWith(const FSGDynamicTextAssetVersion& A, const FSGDynamicTextAssetVersion& B);

	/** Returns true if the two dynamic text asset versions are equal (A == B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Version", meta = (DisplayName = "Equal (Dynamic Text Asset Version)",
		CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_DynamicTextAssetVersionDynamicTextAssetVersion(const FSGDynamicTextAssetVersion& A, const FSGDynamicTextAssetVersion& B);

	/** Returns true if the two dynamic text asset versions are not equal (A != B) */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Version", meta = (DisplayName = "Not Equal (Dynamic Text Asset Version)",
		CompactNodeTitle = "!=", Keywords = "!= not equal"))
	static bool NotEqual_DynamicTextAssetVersionDynamicTextAssetVersion(const FSGDynamicTextAssetVersion& A, const FSGDynamicTextAssetVersion& B);

	/**
	 * Returns true if Version falls within the range [Min, Max] (inclusive).
	 * Compares Major, Minor, and Patch components using standard ordering.
	 *
	 * Min <= Version <= Max
	 *
	 * @param Version The version to evaluate.
	 * @param Min The minimum version (inclusive).
	 * @param Max The maximum version (inclusive).
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Version", meta = (DisplayName = "Is Version In Range"))
	static bool IsVersionInRange(const FSGDynamicTextAssetVersion& Version, const FSGDynamicTextAssetVersion& Min, const FSGDynamicTextAssetVersion& Max);

	/** Returns the unique dynamic text asset ID for this provider. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Provider", meta = (DisplayName = "Get Dynamic Text Asset ID (DTA Provider)"))
	static FSGDynamicTextAssetId GetDynamicTextAssetId_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider);

	/** Returns the human-readable identifier for this dynamic text asset. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Provider", meta = (DisplayName = "Get User Facing ID (DTA Provider)"))
	static FString GetUserFacingId_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider);

	/**
	 * Returns the current major version declared by this class.
	 * Used to determine if migration is needed when loading older data.
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Provider", meta = (DisplayName = "Get Current Major Version (DTA Provider)"))
	static int32 GetCurrentMajorVersion_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider);

	/** Returns the version of this dynamic text asset instance (from file). */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Provider", meta = (DisplayName = "Get Version (DTA Provider)"))
	static FSGDynamicTextAssetVersion GetVersion_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider);

	/**
	 * Returns the full current version declared by this class.
	 * Subclasses can override to declare minor/patch versions
	 * (e.g., FSGDynamicTextAssetVersion(2, 1, 0)).
	 *
	 * Default implementation returns (GetCurrentMajorVersion(), 0, 0).
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Provider", meta = (DisplayName = "Get Current Version (DTA Provider)"))
	static FSGDynamicTextAssetVersion GetCurrentVersion_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider);

	/**
	 * Returns true if the validation result contains any error-severity entries.
	 *
	 * @param Result The validation result to query
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static bool ValidationResultHasErrors(const FSGDynamicTextAssetValidationResult& Result);

	/**
	 * Returns true if the validation result contains any warning-severity entries.
	 *
	 * @param Result The validation result to query
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static bool ValidationResultHasWarnings(const FSGDynamicTextAssetValidationResult& Result);

	/**
	 * Returns true if the validation result contains any informational entries.
	 *
	 * @param Result The validation result to query
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static bool ValidationResultHasInfos(const FSGDynamicTextAssetValidationResult& Result);

	/**
	 * Returns true if the validation result has no entries of any severity.
	 *
	 * @param Result The validation result to query
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static bool IsValidationResultEmpty(const FSGDynamicTextAssetValidationResult& Result);

	/**
	 * Returns true if validation passed (no errors).
	 * Warnings and informational entries do not cause failure.
	 *
	 * @param Result The validation result to query
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static bool IsValidationResultValid(const FSGDynamicTextAssetValidationResult& Result);

	/**
	 * Returns the total number of entries across all severities.
	 *
	 * @param Result The validation result to query
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static int32 GetValidationResultTotalCount(const FSGDynamicTextAssetValidationResult& Result);

	/**
	 * Returns the error entries from the validation result.
	 *
	 * @param Result The validation result to query
	 * @param OutErrors Array populated with error entries
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static void GetValidationResultErrors(const FSGDynamicTextAssetValidationResult& Result, TArray<FSGDynamicTextAssetValidationEntry>& OutErrors);

	/**
	 * Returns the warning entries from the validation result.
	 *
	 * @param Result The validation result to query
	 * @param OutWarnings Array populated with warning entries
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static void GetValidationResultWarnings(const FSGDynamicTextAssetValidationResult& Result, TArray<FSGDynamicTextAssetValidationEntry>& OutWarnings);

	/**
	 * Returns the informational entries from the validation result.
	 *
	 * @param Result The validation result to query
	 * @param OutInfos Array populated with informational entries
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static void GetValidationResultInfos(const FSGDynamicTextAssetValidationResult& Result, TArray<FSGDynamicTextAssetValidationEntry>& OutInfos);

	/**
	 * Builds a formatted string of all entries for display.
	 * Each entry is prefixed with its severity and includes
	 * property path and suggested fix when available.
	 *
	 * @param Result The validation result to format
	 * @return Formatted multi-line string
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Validation")
	static FString ValidationResultToString(const FSGDynamicTextAssetValidationResult& Result);

	/**
	 * Retrieves the asset bundle data for the dynamic text asset referenced by the given ref.
	 * Resolves through the subsystem when available, falls back to the editor cache
	 * outside of PIE so this works in editor tools and utilities.
	 * Returns false if the ref is invalid or the asset is not loaded.
	 *
	 * @param WorldContextObject Object used to resolve the game instance subsystem (allowed to be null in editor).
	 * @param Ref The dynamic text asset reference to look up.
	 * @param OutBundleData Populated with the bundle data if found.
	 * @return True if the asset was found and bundle data was copied.
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Bundle", meta = (WorldContext = "WorldContextObject"))
	static bool GetBundleDataFromRef(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref, FSGDynamicTextAssetBundleData& OutBundleData);

	/**
	 * Retrieves the asset bundle data directly from a dynamic text asset provider.
	 * Returns false if the provider is null or has no bundle data.
	 *
	 * @param Provider The provider to extract bundle data from.
	 * @param OutBundleData Populated with the provider's bundle data.
	 * @return True if the provider was valid and had bundle data.
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Bundle")
	static bool GetBundleDataFromProvider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, FSGDynamicTextAssetBundleData& OutBundleData);

	/** Returns true if the bundle data contains any bundles. */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Bundle")
	static bool HasBundles(const FSGDynamicTextAssetBundleData& BundleData);

	/**
	 * Returns the number of bundles in the bundle data.
	 *
	 * @param BundleData The bundle data to query
	 * @return The number of bundles
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Bundle")
	static int32 GetBundleCount(const FSGDynamicTextAssetBundleData& BundleData);

	/**
	 * Populates the output array with all bundle names.
	 *
	 * @param BundleData The bundle data to query
	 * @param OutBundleNames Array populated with bundle names
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Bundle")
	static void GetBundleNames(const FSGDynamicTextAssetBundleData& BundleData, TArray<FName>& OutBundleNames);

	/**
	 * Collects all soft object paths for a specific bundle.
	 * Appends to OutPaths without clearing it first.
	 *
	 * @param BundleData The bundle data to query
	 * @param BundleName The bundle to get paths for
	 * @param OutPaths Array to append soft object paths to
	 * @return True if the bundle was found and had entries
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Bundle")
	static bool GetPathsForBundle(const FSGDynamicTextAssetBundleData& BundleData, FName BundleName, TArray<FSoftObjectPath>& OutPaths);

	/**
	 * Returns all entries for a specific bundle.
	 *
	 * @param BundleData The bundle data to query
	 * @param BundleName The bundle to get entries for
	 * @param OutEntries Array populated with bundle entries
	 * @return True if the bundle was found
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Bundle")
	static bool GetBundleEntries(const FSGDynamicTextAssetBundleData& BundleData, FName BundleName, TArray<FSGDynamicTextAssetBundleEntry>& OutEntries);

	/**
	 * Extracts bundle metadata from a UObject by walking its UClass properties.
	 *
	 * Iterates all UPROPERTY fields using TFieldIterator. For each
	 * FSoftObjectProperty or FSoftClassProperty that has
	 * meta=(AssetBundles="..."), parses the comma-separated bundle
	 * names and collects the current FSoftObjectPath value into
	 * the corresponding bundles.
	 *
	 * Recursively handles FStructProperty, FArrayProperty,
	 * FMapProperty, FSetProperty, and instanced sub-objects
	 * (CPF_InstancedReference).
	 *
	 * @param Object The UObject to extract bundle data from.
	 * @param BundleData The extracted bundle data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Bundle")
	static void ExtractBundleDataFromObject(UObject* Object, UPARAM(ref) FSGDynamicTextAssetBundleData& BundleData);

	/** Resets the bundle data by clearing all bundle data in it. */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Bundle")
	static void ResetBundleData(UPARAM(ref) FSGDynamicTextAssetBundleData& BundleData);

	/**
	 * Logs all registered serializer types and their IDs to the runtime log.
	 * Useful for diagnosing registration issues or verifying plugin serializer load order.
	 * Output appears in the Output Log under the SGDynamicTextAssetsRuntime log category.
	 *
	 * By default does not execute on shipping builds.
	 * @see SG_DYNAMIC_TEXT_ASSETS_LOG_SERIALIZERS_SHIPPING regarding enabling this function on Shipping builds.
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Diagnostics")
	static void LogRegisteredSerializers();

	/**
	 * Returns a diagnostic description string for each registered serializer.
	 * Each entry is formatted as: "TypeId=N | Extension='ext' | Format='name'"
	 *
	 * @param OutDescriptions Array populated with one entry per registered serializer
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Diagnostics")
	static void GetRegisteredSerializerDescriptions(TArray<FString>& OutDescriptions);

	/**
	 * Finds a registered serializer by its integer type ID.
	 * C++ only  - not Blueprint exposed (serializer instances are not UObjects).
	 * Use this to get the serializer for a payload extracted from a binary file.
	 *
	 * @param TypeId The serializer type ID to look up
	 * @return The serializer, or nullptr if not found
	 */
	static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForTypeId(uint32 TypeId);

	/**
	 * Finds the serializer that can handle the file associated with the given dynamic text asset ID.
	 *
	 * Locates the on-disk file for the specified ID, then resolves the appropriate serializer
	 * based on that file's format (e.g. JSON, XML, or any registered third-party format).
	 *
	 * @param Id  The dynamic text asset ID to look up.
	 * @return    The matching serializer, or nullptr if no file was found for the ID
	 *            or no registered serializer supports its format.
	 */
	static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForDynamicTextAssetId(const FSGDynamicTextAssetId& Id);

	/**
	 * Returns the integer TypeId for the serializer registered under the given file extension.
	 * Returns 0 if no serializer is registered for that extension.
	 * C++ only  - use FindSerializerForTypeId to go the other direction.
	 *
	 * @param Extension File extension without leading dot (e.g., "dta.json")
	 */
	static uint32 GetTypeIdForExtension(const FString& Extension);

	/**
	 * Handles validating soft paths (soft objects and soft classes) properties on this dynamic text asset
	 * with the goal of confirming if they are pointing to a real asset and the path isn't invalid.
	 */
	static void ValidateSoftPathsInProperty(const FProperty* Property,
		const void* ContainerPtr,
		const FString& PropertyPath,
		FSGDynamicTextAssetValidationResult& OutResult);
};
