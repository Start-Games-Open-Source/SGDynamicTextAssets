// Copyright Start Games, Inc. All Rights Reserved.

#include "Server/SGDynamicTextAssetServerCache.h"

#include "Kismet/GameplayStatics.h"
#include "Settings/SGDynamicTextAssetSettings.h"

const FString USGDynamicTextAssetServerCache::SAVE_SLOT_NAME = TEXT("SGDynamicTextAssetServerCache");

bool FSGServerCacheEntry::Serialize(FArchive& Ar)
{
	Ar << Data;
	ServerVersion.Serialize(Ar);
	return true;
}

USGDynamicTextAssetServerCache::USGDynamicTextAssetServerCache()
	: CacheVersion(CURRENT_CACHE_VERSION)
	, LastFetchTimestamp(0)
{
}

USGDynamicTextAssetServerCache* USGDynamicTextAssetServerCache::LoadCache(int32 UserIndex)
{
	// Get the cache filename from settings
	FString slotName = SAVE_SLOT_NAME;
	if (USGDynamicTextAssetSettingsAsset* settings = USGDynamicTextAssetSettings::GetSettings())
	{
		FString configuredName = settings->GetServerCacheFilename();
		if (!configuredName.IsEmpty())
		{
			slotName = configuredName;
		}
	}

	// Try to load existing cache
	if (UGameplayStatics::DoesSaveGameExist(slotName, UserIndex))
	{
		USaveGame* loadedGame = UGameplayStatics::LoadGameFromSlot(slotName, UserIndex);
		if (USGDynamicTextAssetServerCache* cache = Cast<USGDynamicTextAssetServerCache>(loadedGame))
		{
			// Check if migration is needed
			if (cache->CacheVersion < CURRENT_CACHE_VERSION)
			{
				// Future: Handle cache format migration here
				cache->CacheVersion = CURRENT_CACHE_VERSION;
			}
			
			return cache;
		}
	}

	// Create new cache if none exists or load failed
	USGDynamicTextAssetServerCache* newCache = Cast<USGDynamicTextAssetServerCache>(
		UGameplayStatics::CreateSaveGameObject(USGDynamicTextAssetServerCache::StaticClass())
	);

	return newCache;
}

bool USGDynamicTextAssetServerCache::SaveCache(int32 UserIndex)
{
	// Get the cache filename from settings
	FString slotName = SAVE_SLOT_NAME;
	if (USGDynamicTextAssetSettingsAsset* settings = USGDynamicTextAssetSettings::GetSettings())
	{
		FString configuredName = settings->GetServerCacheFilename();
		if (!configuredName.IsEmpty())
		{
			slotName = configuredName;
		}
	}

	return UGameplayStatics::SaveGameToSlot(this, slotName, UserIndex);
}

bool USGDynamicTextAssetServerCache::GetCachedData(const FSGDynamicTextAssetId& Id, FSGServerCacheEntry& OutEntry) const
{
	if (const FSGServerCacheEntry* entry = CacheEntries.Find(Id))
	{
		OutEntry = *entry;
		return true;
	}

	return false;
}

FString USGDynamicTextAssetServerCache::GetCachedDataString(const FSGDynamicTextAssetId& Id) const
{
	if (const FSGServerCacheEntry* entry = CacheEntries.Find(Id))
	{
		return entry->Data;
	}

	return FString();
}

void USGDynamicTextAssetServerCache::SetCachedData(const FSGDynamicTextAssetId& Id, const FString& Data, const FSGDynamicTextAssetVersion& ServerVersion)
{
	CacheEntries.Add(Id, FSGServerCacheEntry(Data, ServerVersion));
}

bool USGDynamicTextAssetServerCache::RemoveCachedData(const FSGDynamicTextAssetId& Id)
{
	return CacheEntries.Remove(Id) > 0;
}

void USGDynamicTextAssetServerCache::GetAllCachedIds(TArray<FSGDynamicTextAssetId>& OutIds) const
{
	CacheEntries.GetKeys(OutIds);
}

void USGDynamicTextAssetServerCache::ClearCache()
{
	CacheEntries.Empty();
	LastFetchTimestamp = FDateTime(0);
}

bool USGDynamicTextAssetServerCache::IsCached(const FSGDynamicTextAssetId& Id) const
{
	return CacheEntries.Contains(Id);
}

int32 USGDynamicTextAssetServerCache::GetCacheCount() const
{
	return CacheEntries.Num();
}

void USGDynamicTextAssetServerCache::UpdateLastFetchTimestamp()
{
	LastFetchTimestamp = FDateTime::UtcNow();
}

void USGDynamicTextAssetServerCache::SetLastFetchTimestamp(const FDateTime& Timestamp)
{
	LastFetchTimestamp = Timestamp;
}
