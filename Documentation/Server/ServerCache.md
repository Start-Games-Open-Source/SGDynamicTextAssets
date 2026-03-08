# Server Cache

[Back to Table of Contents](../TableOfContents.md)

## Overview

`USGDynamicTextAssetServerCache` is a `USaveGame` subclass that provides persistent local caching of server provided data.
It stores server responses in the local `Saved/` folder so the game can use cached server data on subsequent launches without refetching.

## API

### Loading and Saving

```cpp
// Load cache from disk (uses UGameplayStatics::LoadGameFromSlot)
bool LoadCache(int32 UserIndex = 0);

// Save cache to disk
bool SaveCache(int32 UserIndex = 0);
```

### Data Operations

```cpp
// Retrieve cached data for a dynamic text asset
FString GetCachedData(const FSGDynamicTextAssetId& Id) const;

// Store server provided data
void SetCachedData(const FSGDynamicTextAssetId& Id, const FString& JsonString);

// List all IDs in cache
TArray<FSGDynamicTextAssetId> GetAllCachedIds() const;

// Remove all cached entries
void ClearCache();
```

## Storage

The cache is stored as a SaveGame slot using the filename configured in `USGDynamicTextAssetSettingsAsset::ServerCacheFilename` (default: `"SGDynamicTextAssetServerCache"`). The file is located in the project's `Saved/SaveGames/` directory.

## Usage Flow

1. **Server responds**: The server returns updated data for one or more dynamic text assets.
2. **Cache stores data**: `SetCachedData()` stores each response's data string keyed by FSGDynamicTextAssetId.
3. **Save to disk**: `SaveCache()` persists the cache for future sessions.
4. **Next launch**: `LoadCache()` restores the cache from disk.
5. **Check before request**: The subsystem checks the cache before making a server request, reducing unnecessary network round trips for data that hasn't changed.

This caching layer ensures that the game has fast access to the most recent server data even when offline or during startup before server connections are established.

[Back to Table of Contents](../TableOfContents.md)
