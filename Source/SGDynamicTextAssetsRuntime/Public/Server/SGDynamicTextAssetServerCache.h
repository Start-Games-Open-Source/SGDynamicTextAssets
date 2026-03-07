// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "GameFramework/SaveGame.h"

#include "SGDynamicTextAssetServerCache.generated.h"

/**
 * A single entry in the server cache.
 * Stores the serialized data and version for a dynamic text asset.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGServerCacheEntry
{
	GENERATED_BODY()
public:

	FSGServerCacheEntry() = default;

	FSGServerCacheEntry(const FString& InData,
		const FSGDynamicTextAssetVersion& InServerVersion)
		: Data(InData)
		, ServerVersion(InServerVersion)
	{ }

	/**
	 * Custom serialization for optimized binary format.
	 * Writes/reads the string data directly without property tags.
	 *
	 * @param Ar The archive to serialize to/from
	 * @return True (serialization always succeeds for this simple struct)
	 */
	bool Serialize(FArchive& Ar);

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		Serialize(Ar);
		bOutSuccess = true;
		return true;
	}

	/** The serialized dynamic text asset data (typically JSON) */
	UPROPERTY(SaveGame, BlueprintReadOnly)
	FString Data;

	/** The server-side version of this dynamic text asset */
	UPROPERTY(SaveGame, BlueprintReadOnly)
	FSGDynamicTextAssetVersion ServerVersion;
};

template<>
struct TStructOpsTypeTraits<FSGServerCacheEntry> : public TStructOpsTypeTraitsBase2<FSGServerCacheEntry>
{
	enum
	{
		WithNetSerializer = true,
		WithSerializer = true
	};
};

/**
 * Persistent cache for server-provided dynamic text asset data.
 *
 * This SaveGame object stores dynamic text assets that have been fetched from
 * a game server, allowing the client to use cached data when offline
 * or to reduce server requests on subsequent launches.
 *
 * The cache is stored in the Saved folder using Unreal's SaveGame system.
 * The filename is configurable via USGDynamicTextAssetSettings.
 *
 * @see USGDynamicTextAssetServerInterface
 * @see USGDynamicTextAssetSettings::GetServerCacheFilename
 */
UCLASS(BlueprintType, ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetServerCache : public USaveGame
{
	GENERATED_BODY()

public:

	USGDynamicTextAssetServerCache();

	/**
	 * Loads the server cache from disk for a specific user.
	 * Creates a new cache if one doesn't exist.
	 * 
	 * @param UserIndex The platform user index (from local player) that identifies the user
	 * @return The loaded or newly created cache instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	static USGDynamicTextAssetServerCache* LoadCache(int32 UserIndex = 0);

	/**
	 * Saves the cache to disk for a specific user.
	 * 
	 * @param UserIndex The platform user index (from local player) that identifies the user
	 * @return True if save was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	bool SaveCache(int32 UserIndex);

	/**
	 * Gets cached data for a specific ID.
	 *
	 * @param Id The dynamic text asset ID to look up
	 * @param OutEntry The cache entry if found
	 * @return True if the ID was found in cache
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	bool GetCachedData(const FSGDynamicTextAssetId& Id, FSGServerCacheEntry& OutEntry) const;

	/**
	 * Gets just the cached data string for a specific ID.
	 *
	 * @param Id The dynamic text asset ID to look up
	 * @return The cached data string, or empty string if not found
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Server Cache")
	FString GetCachedDataString(const FSGDynamicTextAssetId& Id) const;

	/**
	 * Sets or updates cached data for an ID.
	 *
	 * @param Id The dynamic text asset ID to cache
	 * @param Data The serialized data to cache
	 * @param ServerVersion The server version of this data
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	void SetCachedData(const FSGDynamicTextAssetId& Id, const FString& Data, const FSGDynamicTextAssetVersion& ServerVersion);

	/**
	 * Removes a specific ID from the cache.
	 *
	 * @param Id The dynamic text asset ID to remove
	 * @return True if the ID was found and removed
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	bool RemoveCachedData(const FSGDynamicTextAssetId& Id);

	/**
	 * Gets all IDs currently in the cache.
	 *
	 * @param OutIds Array to populate with cached IDs
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	void GetAllCachedIds(TArray<FSGDynamicTextAssetId>& OutIds) const;

	/**
	 * Clears all entries from the cache.
	 * Does not automatically save - call SaveCache() after if persistence is needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	void ClearCache();

	/**
	 * Checks if an ID exists in the cache.
	 *
	 * @param Id The dynamic text asset ID to check
	 * @return True if the ID is cached
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Server Cache")
	bool IsCached(const FSGDynamicTextAssetId& Id) const;

	/**
	 * Gets the number of entries in the cache.
	 * 
	 * @return Number of cached entries
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Server Cache")
	int32 GetCacheCount() const;

	/**
	 * Gets the timestamp of the last successful server fetch.
	 * 
	 * @return The last fetch timestamp, or FDateTime() if never fetched
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Server Cache")
	FDateTime GetLastFetchTimestamp() const { return LastFetchTimestamp; }

	/**
	 * Sets the last fetch timestamp to the current time.
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	void UpdateLastFetchTimestamp();

	/**
	 * Sets the last fetch timestamp to a specific time.
	 * 
	 * @param Timestamp The timestamp to set
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Server Cache")
	void SetLastFetchTimestamp(const FDateTime& Timestamp);

	/**
	 * Gets the cache format version.
	 * Used for migration when the cache format changes.
	 * 
	 * @return The cache version number
	 */
	UFUNCTION(BlueprintPure, Category = "SG Dynamic Text Assets|Server Cache")
	int32 GetCacheVersion() const { return CacheVersion; }

protected:

	/** Version of the cache format for migration purposes */
	UPROPERTY(SaveGame)
	int32 CacheVersion = 1;

	/** Timestamp of the last successful server fetch */
	UPROPERTY(SaveGame)
	FDateTime LastFetchTimestamp;

	/** Map of dynamic text asset ID to cached data entry */
	UPROPERTY(SaveGame)
	TMap<FSGDynamicTextAssetId, FSGServerCacheEntry> CacheEntries;

private:

	/** Current cache format version */
	static constexpr int32 CURRENT_CACHE_VERSION = 1;

	/** Slot name used for SaveGame operations */
	static const FString SAVE_SLOT_NAME;
};
