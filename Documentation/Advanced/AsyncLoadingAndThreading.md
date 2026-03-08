# Async Loading and Threading

[Back to Table of Contents](../TableOfContents.md)

## Threading Model

The subsystem uses a two-thread model:

| Thread | Operations |
|--------|-----------|
| **Background (ThreadPool)** | File I/O only: `ReadRawFileContents()`, `FindFileForId()` |
| **Game Thread** | Object creation (`NewObject`), deserialization, cache management, callback execution |

All callbacks are marshaled to the game thread via `AsyncTask(ENamedThreads::GameThread, ...)`.

## Async Loading Pipeline

### LoadDynamicTextAssetFromFileAsync

Step-by-step flow when calling `LoadDynamicTextAssetFromFileAsync()`:

```
Game Thread                          Background Thread
    |                                       |
    | 1. Increment PendingAsyncLoads        |
    |    (FThreadSafeCounter)               |
    |                                       |
    | 2. Capture TWeakObjectPtr<Subsystem>  |
    |                                       |
    | 3. Dispatch to ThreadPool ----------->|
    |                                       | 4. ReadRawFileContents()
    |                                       |    (decompress if .dta.bin)
    |                                       |
    | 5. AsyncTask(GameThread) <------------|
    |                                       |
    | 6. Check weakThis.Get() validity      |
    | 7. Find serializer (by TypeId or ext) |
    | 8. ExtractMetadata → check cache      |
    |    (may have loaded during async)     |
    | 9. NewObject<UObject>(this, ClassPtr) |
    | 10. DeserializeProvider()             |
    | 11. Handle migration (editor: resave) |
    | 12. Populate UserFacingId             |
    | 13. AddToCache()                      |
    |     → broadcasts OnDynamicTextAssetCached
    | 14. PostDynamicTextAssetLoaded()      |
    | 15. Native_ValidateDynamicTextAsset() |
    | 16. Execute OnComplete callback       |
    | 17. Decrement PendingAsyncLoads       |
```

### LoadDynamicTextAssetAsync (by ID)

When loading by `FSGDynamicTextAssetId` instead of file path, the background thread has an additional step:

1. Check cache first on game thread (return immediately if cached)
2. Background thread calls `FindFileForId()` to locate the file
3. If not found, return to game thread with failure
4. If found, proceed with `ReadRawFileContents()` and the standard pipeline above

### Thread Safety

The `PendingAsyncLoads` counter is an `FThreadSafeCounter` (atomic) that can be safely read from any thread:

```cpp
FThreadSafeCounter PendingAsyncLoads;

// Safe to call from any thread
bool IsAsyncLoadInProgress() const { return PendingAsyncLoads.GetValue() > 0; }
int32 GetPendingAsyncLoadCount() const { return PendingAsyncLoads.GetValue(); }
```

Async lambdas capture a `TWeakObjectPtr<USGDynamicTextAssetSubsystem>` to prevent use-after-free if the subsystem is destroyed before the background work completes.

## Cache Architecture

### Data Structure

```cpp
// Subsystem member (UPROPERTY ensures GC visibility)
UPROPERTY(Transient)
TMap<FSGDynamicTextAssetId, TScriptInterface<ISGDynamicTextAssetProvider>> LoadedObjects;
```

### Ownership

Objects are created with the subsystem as their `Outer`:

```cpp
NewObject<UObject>(this, DynamicTextAssetClass)
```

This means cached objects are owned by the subsystem and will be garbage-collected when the subsystem is destroyed.

### Cache Operations

| Method | Behavior |
|--------|----------|
| `GetDynamicTextAsset(Id)` | O(1) lookup, returns empty interface if not found |
| `IsDynamicTextAssetCached(Id)` | O(1) existence check |
| `AddToCache(Provider)` | Adds to map, broadcasts `OnDynamicTextAssetCached`, fails if already cached |
| `RemoveFromCache(Id)` | Removes single entry, object becomes eligible for GC |
| `ClearCache()` | Empties entire map |
| `GetCachedObjectCount()` | Returns `LoadedObjects.Num()` |

### Cache Events

```cpp
// Broadcast when any provider is added to cache
UPROPERTY(BlueprintAssignable, Category = "Dynamic Text Asset|Events")
FOnDynamicTextAssetCached OnDynamicTextAssetCached;
```

Subscribe to react to cache additions:

```cpp
Subsystem->OnDynamicTextAssetCached.AddDynamic(this, &UMyComponent::HandleAssetCached);
```

### Race Condition Prevention

During async loads, the game thread callback checks the cache before creating a new object. If another load (sync or async) cached the same asset while the background I/O was in progress, the cached version is returned instead of creating a duplicate.

## Memory Management

### Object Lifecycle

1. **Creation**: `NewObject<UObject>(Subsystem, Class)` where the subsystem is the outer
2. **Caching**: Added to `LoadedObjects` map (UPROPERTY keeps GC reference)
3. **Usage**: Game code holds references via `TScriptInterface` or typed pointers
4. **Eviction**: `RemoveFromCache(Id)` removes from map, object becomes GC-eligible when no other references exist
5. **Shutdown**: `Deinitialize()` calls `ClearCache()`, subsystem destruction triggers GC of all owned objects

### UPROPERTY(Transient)

The `Transient` flag on `LoadedObjects` means the cache is not serialized to disk. It exists only in memory during the session.

### Manual Eviction

For memory-sensitive scenarios (large datasets, streaming levels), explicitly evict assets:

```cpp
Subsystem->RemoveFromCache(LargeAssetId);
// Object will be GC'd once no other UPROPERTY or strong references hold it
```

## PIE Behavior

Play In Editor works without special handling:

- The cache is `UPROPERTY(Transient)` so it is automatically cleared between PIE sessions
- The subsystem (`UGameInstanceSubsystem`) is destroyed when PIE ends
- All objects with the subsystem as outer are garbage-collected
- File changes during PIE are **not** auto-detected (manual reload required via `ClearCache()` + re-load)

## Hot Reload

- Module startup registers serializers; shutdown unregisters them
- Cache contents may become invalid after hot reload if class layouts change (properties added/removed/retyped)
- Best practice: clear cache and reload after significant C++ code changes
- The registry's class cache is invalidated on registration changes, but the subsystem's object cache is independent

## Network Replication

### What Replicates

Both `FSGDynamicTextAssetId` and `FSGDynamicTextAssetRef` support `NetSerialize`:

```cpp
// 16 bytes per GUID - compact network representation
bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
```

**Only the ID replicates, not the object data.** Clients resolve the ID locally via their own subsystem cache. Both server and clients must have the same files available.

### Text Serialization

Both structs support `ExportTextItem` / `ImportTextItem` for clipboard round-tripping (copy/paste in editor, config files).

### Replication Pattern

```cpp
// Server: replicate the reference
UPROPERTY(Replicated)
FSGDynamicTextAssetRef WeaponRef;

// Client: resolve locally
void OnRep_WeaponRef()
{
    UWeaponData* Weapon = WeaponRef.Get<UWeaponData>(GetGameInstance());
    if (Weapon)
    {
        ApplyWeaponVisuals(Weapon);
    }
}
```

[Back to Table of Contents](../TableOfContents.md)
