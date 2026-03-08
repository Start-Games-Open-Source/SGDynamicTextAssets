// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAssetId.h"
#include "UObject/GCObject.h"
#include "UObject/ScriptInterface.h"

/**
 * Editor-only singleton cache for loading and caching dynamic text assets
 * when no GameInstance (and therefore no USGDynamicTextAssetSubsystem) is available.
 *
 * This enables FSGDynamicTextAssetRef::Get() and LoadAsync() to work in editor
 * non-PIE contexts (e.g., Actor OnConstruction during level editing).
 *
 * Design:
 * - FGCObject-based singleton holding UObject references via AddReferencedObjects
 * - Objects are outed to GetTransientPackage() (transient, not saved to disk)
 * - Cache is cleared on PIE start/end to avoid stale data
 * - Get() is cache-only (no implicit loading); LoadDynamicTextAsset() triggers loading
 *
 * Performance:
 * - First load per unique DTA: one-time file I/O + deserialization
 * - All subsequent lookups: O(1) TMap cache hit
 * - Safe for thousands of actors referencing a smaller set of unique DTAs
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetEditorCache : public FGCObject
{
public:

	FSGDynamicTextAssetEditorCache();
	~FSGDynamicTextAssetEditorCache();

	/** Returns the singleton instance, creating it on first access. */
	static FSGDynamicTextAssetEditorCache& Get();

	/** Destroys the singleton. Called by module shutdown. */
	static void TearDown();

	/**
	 * Synchronously loads a dynamic text asset by ID from disk and caches the result.
	 * If already cached, returns the cached provider.
	 * This is the only entry point that triggers file I/O in the editor cache.
	 *
	 * @param Id The unique identifier of the dynamic text asset to load
	 * @return The loaded provider, or empty TScriptInterface on failure
	 */
	TScriptInterface<ISGDynamicTextAssetProvider> LoadDynamicTextAsset(const FSGDynamicTextAssetId& Id);

	/**
	 * Cache lookup only. Does NOT trigger loading on cache miss.
	 *
	 * @param Id The unique identifier of the dynamic text asset
	 * @return The cached provider, or empty TScriptInterface if not cached
	 */
	TScriptInterface<ISGDynamicTextAssetProvider> GetDynamicTextAsset(const FSGDynamicTextAssetId& Id) const;

	/**
	 * Checks if a dynamic text asset is currently in the editor cache.
	 *
	 * @param Id The unique identifier to check
	 * @return True if the asset is cached
	 */
	bool IsCached(const FSGDynamicTextAssetId& Id) const;

	/** Clears all cached dynamic text assets. */
	void ClearCache();

	/**
	 * Removes a specific dynamic text asset from the cache.
	 *
	 * @param Id The unique identifier of the asset to remove
	 * @return True if an asset was removed
	 */
	bool RemoveFromCache(const FSGDynamicTextAssetId& Id);

	/** Returns the number of cached dynamic text assets. */
	int32 GetCachedObjectCount() const;

	// FGCObject overrides
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// ~FGCObject overrides

private:

	/**
	 * Internal loading pipeline: finds the file on disk, reads and deserializes it,
	 * then caches the resulting provider.
	 *
	 * @param Id The unique identifier to load
	 * @return The loaded provider, or empty TScriptInterface on failure
	 */
	TScriptInterface<ISGDynamicTextAssetProvider> Internal_LoadFromFile(const FSGDynamicTextAssetId& Id);

	/**
	 * Constructs a TScriptInterface from a UObject that implements ISGDynamicTextAssetProvider.
	 *
	 * @param Object The UObject to wrap
	 * @return A populated TScriptInterface, or empty if Object is null or doesn't implement the interface
	 */
	static TScriptInterface<ISGDynamicTextAssetProvider> MakeProvider(UObject* Object);

	/** Cached dynamic text asset providers keyed by Id. */
	TMap<FSGDynamicTextAssetId, TScriptInterface<ISGDynamicTextAssetProvider>> CachedObjects;

	/** Delegate handle for PIE start callback. */
	FDelegateHandle PrePIEHandle;

	/** Delegate handle for PIE end callback. */
	FDelegateHandle EndPIEHandle;

	/** Called when PIE is about to start. Clears cache so subsystem takes over. */
	void OnPreBeginPIE(const bool bIsSimulating);

	/** Called when PIE ends. Clears cache since files may have changed during PIE. */
	void OnEndPIE(const bool bIsSimulating);

	/** Singleton instance pointer. */
	static FSGDynamicTextAssetEditorCache* Instance;
};

#endif // WITH_EDITOR
