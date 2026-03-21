// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Engine/StreamableManager.h"
#include "Server/ISGDynamicTextAssetServerInterface.h"
#include "UObject/ScriptInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SGDynamicTextAssetDelegates.h"

#include "SGDynamicTextAssetSubsystem.generated.h"

class FJsonObject;

class USGDynamicTextAsset;

struct FSGDynamicTextAssetBundleData;

/**
 * Wrapper for a set of tracked UClass references used by instanced sub-objects.
 * Needed because UPROPERTY does not support nested containers
 * (e.g., TMap<Id, TSet<UClass*>>).
 */
USTRUCT()
struct FSGTrackedInstancedClasses
{
    GENERATED_BODY()
public:

    /** Set of UClass references used by instanced sub-objects of a single DTA. */
    UPROPERTY(Transient)
    TSet<TObjectPtr<UClass>> Classes;
};

/**
 * Context passed when instanced object classes drop to zero references
 * after a DTA is removed from cache. External systems can use this to
 * trigger GC, log diagnostics, or pre-load replacements.
 */
USTRUCT(BlueprintType)
struct FSGInstancedClassReleaseContext
{
    GENERATED_BODY()
public:

    /** The DTA Id whose removal caused these classes to become unreferenced. */
    UPROPERTY(BlueprintReadOnly, Category = "Dynamic Text Asset")
    FSGDynamicTextAssetId RemovedAssetId;

    /** Classes that dropped to zero references after this removal. */
    UPROPERTY(BlueprintReadOnly, Category = "Dynamic Text Asset")
    TArray<TObjectPtr<UClass>> ReleasedClasses;

    /** Number of unique classes still tracked after this removal. */
    UPROPERTY(BlueprintReadOnly, Category = "Dynamic Text Asset")
    int32 RemainingTrackedClassCount = 0;
};

/**
 * Broadcast when one or more instanced object classes drop to zero
 * references after a DTA is removed from cache via RemoveFromCache.
 * Not broadcast during ClearCache (bulk/shutdown operation).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnInstancedClassesReleased,
    const FSGInstancedClassReleaseContext&, Context);

/**
 * Game Instance Subsystem for loading and caching dynamic text assets at runtime.
 *
 * This subsystem manages the lifecycle of dynamic text assets, providing:
 * - Synchronous and asynchronous loading from JSON files
 * - In-memory caching with ID-based lookup
 * - Automatic cache management
 *
 * All public APIs operate on TScriptInterface<ISGDynamicTextAssetProvider>,
 * allowing any provider implementation to participate in the cache.
 *
 * Usage:
 *   USGDynamicTextAssetSubsystem* Subsystem = UGameInstance::GetSubsystem<USGDynamicTextAssetSubsystem>(GetGameInstance());
 *   TScriptInterface<ISGDynamicTextAssetProvider> Provider = Subsystem->GetDynamicTextAsset(WeaponId);
 *   UWeaponData* WeaponData = Subsystem->GetDynamicTextAsset<UWeaponData>(WeaponId);
 */
UCLASS()
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // USubsystem overrides
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
    // ~USubsystem overrides

    /**
     * Gets a dynamic text asset provider from cache by Id.
     * Returns an empty TScriptInterface if not loaded.
     *
     * @param Id The unique identifier of the dynamic text asset
     * @return The cached provider, or empty TScriptInterface if not found
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    TScriptInterface<ISGDynamicTextAssetProvider> GetDynamicTextAsset(const FSGDynamicTextAssetId& Id) const;

    /**
     * Templated version for type-safe access.
     * Casts the provider's underlying UObject to the requested type.
     *
     * @param Id The unique identifier of the dynamic text asset
     * @return The cached object cast to T, or nullptr if not found or wrong type
     */
    template<typename T>
    T* GetDynamicTextAsset(const FSGDynamicTextAssetId& Id) const
    {
        return Cast<T>(GetDynamicTextAsset(Id).GetObject());
    }

    /**
     * Checks if a dynamic text asset is currently cached.
     *
     * @param Id The unique identifier to check
     * @return True if the dynamic text asset is in cache
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    bool IsDynamicTextAssetCached(const FSGDynamicTextAssetId& Id) const;

    /**
     * Returns the number of dynamic text assets currently in cache.
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    int32 GetCachedObjectCount() const { return LoadedObjects.Num(); }

    /**
     * Clears all cached dynamic text assets.
     * Use with caution - may invalidate existing references.
     */
    UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset")
    void ClearCache();

    /**
     * Removes a specific dynamic text asset from the cache.
     *
     * @param Id The unique identifier of the object to remove
     * @return True if an object was removed
     */
    UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset")
    bool RemoveFromCache(const FSGDynamicTextAssetId& Id);

    /**
     * Loads a dynamic text asset from a file path synchronously.
     * If already cached, returns the cached provider.
     *
     * @param FilePath Absolute path to the JSON file
     * @param DynamicTextAssetClass The expected class of the dynamic text asset
     * @return The loaded provider, or empty TScriptInterface on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|Loading")
    TScriptInterface<ISGDynamicTextAssetProvider> LoadDynamicTextAssetFromFile(const FString& FilePath, UClass* DynamicTextAssetClass = nullptr);

    /**
     * Templated version for type-safe loading from file.
     */
    template<typename T>
    T* LoadDynamicTextAssetFromFile(const FString& FilePath)
    {
        return Cast<T>(LoadDynamicTextAssetFromFile(FilePath, T::StaticClass()).GetObject());
    }

    /**
     * Gets a dynamic text asset from cache, or loads it if not cached.
     *
     * @param Id The unique identifier of the dynamic text asset
     * @param FilePath The file path to load from if not cached
     * @param DynamicTextAssetClass The expected class of the dynamic text asset
     * @return The loaded provider, or empty TScriptInterface on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|Loading")
    TScriptInterface<ISGDynamicTextAssetProvider> GetOrLoadDynamicTextAsset(const FSGDynamicTextAssetId& Id, const FString& FilePath, UClass* DynamicTextAssetClass = nullptr);

    /**
     * Loads all dynamic text asset files from a directory into cache.
     *
     * @param DynamicTextAssetClass The class to load (determines folder path)
     * @param bIncludeSubclasses If true, also loads subclass files
     * @return Number of dynamic text assets successfully loaded
     */
    UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|Loading")
    int32 LoadAllDynamicTextAssetsOfClass(UClass* DynamicTextAssetClass, bool bIncludeSubclasses = true);

    /**
     * Loads a dynamic text asset from a file path asynchronously.
     * The callback is invoked on the game thread when loading completes.
     *
     * @param FilePath Absolute path to the JSON file
     * @param DynamicTextAssetClass The expected class of the dynamic text asset
     * @param OnComplete Callback invoked with the loaded provider (or empty on failure)
     */
    void LoadDynamicTextAssetFromFileAsync(const FString& FilePath, UClass* DynamicTextAssetClass, FOnDynamicTextAssetLoaded OnComplete);

    /**
     * Templated version for type-safe async loading from file.
     */
    template<typename T>
    void LoadDynamicTextAssetFromFileAsync(const FString& FilePath, TFunction<void(T*, bool)> OnComplete)
    {
        LoadDynamicTextAssetFromFileAsync(FilePath, T::StaticClass(), FOnDynamicTextAssetLoaded::CreateLambda(
            [OnComplete](const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess)
            {
                if (OnComplete)
                {
                    OnComplete(Cast<T>(Provider.GetObject()), bSuccess);
                }
            }));
    }

    /**
     * Loads a dynamic text asset asynchronously using its Id.
     * Performs a file system search to find the file path first.
     *
     * @param Id The unique identifier of the dynamic text asset
     * @param ClassHint Optional class hint to speed up file search
     * @param OnComplete Callback invoked with the loaded provider (or empty on failure)
     */
    void LoadDynamicTextAssetAsync(const FSGDynamicTextAssetId& Id, const UClass* ClassHint, FOnDynamicTextAssetLoaded OnComplete);

    /**
     * Checks if any async loads are currently in progress.
     *
     * @return True if async loads are pending
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Loading")
    bool IsAsyncLoadInProgress() const { return PendingAsyncLoads.GetValue() > 0; }

    /**
     * Returns the number of pending async loads.
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Loading")
    int32 GetPendingAsyncLoadCount() const { return PendingAsyncLoads.GetValue(); }

    /**
     * Async-loads all referenced assets for a specific bundle on a single DTA.
     * Retrieves the cached provider via GetDynamicTextAsset(Id), extracts paths
     * from GetSGDTAssetBundleData().GetPathsForBundle(), and uses
     * FStreamableManager to async-load them.
     *
     * @param Id The unique identifier of the DTA to load bundles for
     * @param BundleName The bundle name to load (e.g., "Visual", "Audio")
     * @param OnComplete Callback invoked when all assets in the bundle are loaded
     * @return True if an async load was initiated
     */
    bool LoadSGDTAssetBundle(const FSGDynamicTextAssetId& Id,
        FName BundleName,
        FStreamableDelegate OnComplete = FStreamableDelegate());

    /**
     * Async-loads all referenced assets for a named bundle across every cached DTA.
     * Iterates all cached providers in LoadedObjects, collects paths for the
     * named bundle, and batch-loads them via FStreamableManager.
     *
     * @param BundleName The bundle name to load across all cached DTAs
     * @param OnComplete Callback invoked when all assets are loaded
     * @return Number of DTAs that had matching bundles
     */
    int32 LoadSGDTAssetBundleForAll(FName BundleName,
        FStreamableDelegate OnComplete = FStreamableDelegate());

    /**
     * Returns a pointer to the cached provider's bundle data for a given DTA Id.
     * Returns nullptr if the DTA is not cached.
     *
     * @param Id The unique identifier of the DTA
     * @return Pointer to the bundle data, or nullptr if not cached
     */
    const FSGDynamicTextAssetBundleData* GetSGDTAssetBundleData(const FSGDynamicTextAssetId& Id) const;

    /**
     * Blueprint-accessible version that copies the bundle data into an out parameter.
     *
     * @param Id The unique identifier of the DTA
     * @param OutBundleData The bundle data for this DTA (only valid if return is true)
     * @return True if the DTA was cached and bundle data was retrieved
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Bundles")
    bool GetSGDTAssetBundleDataCopy(const FSGDynamicTextAssetId& Id,
        FSGDynamicTextAssetBundleData& OutBundleData) const;

    /**
     * Gathers all soft object paths for a named bundle across all cached providers.
     * Appends to OutPaths without clearing it first.
     *
     * @param BundleName The bundle name to collect paths for
     * @param OutPaths Array to append soft object paths to
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Bundles")
    void GetAllPathsForSGDTBundle(FName BundleName,
        TArray<FSoftObjectPath>& OutPaths) const;

    /**
     * Applies server-provided type overrides to the type registry.
     * Routes the JSON data to the appropriate manifests and rebuilds type lookup maps.
     * No-op in editor builds (server overlay is runtime-only).
     *
     * Expected JSON format:
     * {
     *   "manifests": {
     *     "<rootTypeId>": { "types": [{ "typeId": "...", "className": "...", "parentTypeId": "..." }] },
     *     ...
     *   }
     * }
     *
     * @param ServerData JSON object containing server type overrides
     */
    void ApplyServerTypeOverrides(const TSharedPtr<FJsonObject>& ServerData);

    /**
     * Clears all server type overrides, reverting to local-only type resolution.
     */
    void ClearServerTypeOverrides();

    /**
     * Fetches type manifest overrides from the server and applies them.
     * Uses the registered server interface to request overrides, then applies
     * non-null responses to the type registry.
     *
     * @param OnComplete Optional callback invoked when the operation finishes
     */
    void FetchAndApplyServerTypeOverrides(FOnServerTypeOverridesComplete OnComplete = FOnServerTypeOverridesComplete());

    /**
     * Returns the total number of unique instanced object UClass references
     * currently tracked across all cached DTAs.
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Diagnostics")
    int32 GetTrackedInstancedClassCount() const;

    /**
     * Returns the deduplicated set of all instanced object UClass references
     * currently tracked across all cached DTAs.
     * Useful for diagnostics and verifying that GC is correctly retaining
     * classes used by instanced sub-objects.
     *
     * @return Set of unique UClass pointers from all tracked DTAs
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Diagnostics")
    TSet<UClass*> GetAllTrackedInstancedClasses() const;

    /**
     * Returns the tracked UClass references for a specific DTA by Id.
     * Returns an empty array if the Id is not tracked.
     *
     * @param Id The unique identifier of the DTA
     * @return Array of UClass pointers tracked for this DTA
     */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Diagnostics")
    TArray<UClass*> GetTrackedInstancedClassesForId(const FSGDynamicTextAssetId& Id) const;

    /** Broadcast when a dynamic text asset provider is added to cache. */
    UPROPERTY(BlueprintAssignable, Category = "Dynamic Text Asset|Events")
    FOnDynamicTextAssetProvider_Dynamic_Multi OnDynamicTextAssetCached;

    /**
     * Broadcast when instanced object classes drop to zero references
     * after a DTA is removed via RemoveFromCache. The subsystem removes
     * its UPROPERTY tracking (making classes GC-eligible) but never
     * force-unloads packages. External systems can listen to this
     * delegate to trigger GC, log, or take other cleanup actions.
     * Not broadcast during ClearCache (bulk/shutdown operation).
     */
    UPROPERTY(BlueprintAssignable, Category = "Dynamic Text Asset|Events")
    FOnInstancedClassesReleased OnInstancedClassesReleased;

protected:

    /**
     * Adds a dynamic text asset provider to the cache.
     * Called internally after successful load.
     *
     * @param Provider The provider to cache (must have valid Id)
     * @return True if successfully added
     */
    bool AddToCache(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider);

    /** Map of loaded dynamic text asset providers by Id */
    UPROPERTY(Transient)
    TMap<FSGDynamicTextAssetId, TScriptInterface<ISGDynamicTextAssetProvider>> LoadedObjects;

    /** Counter for pending async load operations */
    FThreadSafeCounter PendingAsyncLoads;

    /**
     * Server interface for remote dynamic text asset operations.
     * Always valid  - either a real implementation or USGDynamicTextAssetServerNullInterface.
     */
    UPROPERTY(Transient)
    TObjectPtr<USGDynamicTextAssetServerInterface> ServerInterface = nullptr;

private:

    /**
     * Scans a provider's UObject for instanced sub-object classes and adds
     * them to the tracking map under the given Id.
     *
     * @param Id The DTA identifier to track under
     * @param ProviderObject The UObject to scan for instanced sub-objects
     */
    void TrackInstancedClassesForProvider(const FSGDynamicTextAssetId& Id, const UObject* ProviderObject);

    /**
     * Per-DTA tracking of UClass references used by instanced sub-objects.
     * Acts as a GC safety net: holding a UPROPERTY reference to each UClass
     * ensures GC will not collect a class while any cached DTA uses it.
     * Populated after deserialization, cleaned up on cache removal.
     */
    UPROPERTY(Transient)
    TMap<FSGDynamicTextAssetId, FSGTrackedInstancedClasses> TrackedInstancedClasses;

    /**
     * Reference count per instanced object UClass across all cached DTAs.
     * When a class's count drops to zero during RemoveFromCache, it is
     * included in the OnInstancedClassesReleased broadcast and removed
     * from this map, allowing GC to collect the class.
     */
    UPROPERTY(Transient)
    TMap<TObjectPtr<UClass>, int32> InstancedClassRefCounts;

    /**
     * Constructs a TScriptInterface from a UObject that implements ISGDynamicTextAssetProvider.
     *
     * @param Object The UObject to wrap (must implement ISGDynamicTextAssetProvider)
     * @return A populated TScriptInterface, or empty if Object is null or doesn't implement the interface
     */
    static TScriptInterface<ISGDynamicTextAssetProvider> MakeProvider(UObject* Object);

    /**
     * Completes an async dynamic text asset load on the game thread.
     * Creates/deserializes the object, adds it to cache, and invokes the completion delegate.
     *
     * @param FilePath          Absolute path to the source file
     * @param ClassPtr          Class to instantiate for the dynamic text asset
     * @param TextPayload      Text payload read on the background thread (decompressed if binary)
     * @param SerializerTypeId  TypeId from binary header (non-zero) or 0 for plain text files
     * @param bReadSuccess      True if the file read succeeded
     * @param OnComplete        Delegate invoked with the loaded provider (or empty on failure)
     */
    void Internal_LoadDynamicTextAssetFromFileAsync_GameThread(const FString& FilePath,
        const UClass* ClassPtr,
        const FString& TextPayload,
        uint32 SerializerTypeId,
        const bool& bReadSuccess,
        const FOnDynamicTextAssetLoaded& OnComplete);
};
