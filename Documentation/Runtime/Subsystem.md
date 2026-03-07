# Subsystem

[Back to Table of Contents](../TableOfContents.md)

## USGDynamicTextAssetSubsystem

The subsystem is a `UGameInstanceSubsystem` that manages the lifecycle of dynamic text assets at runtime. 
It provides synchronous and asynchronous loading from files, `FSGDynamicTextAssetId`based in memory caching, and cache management.

```cpp
UCLASS()
class USGDynamicTextAssetSubsystem : public UGameInstanceSubsystem
```

The subsystem is created automatically by the engine when the game instance initializes. 
Access it from any gameplay code:

```cpp
USGDynamicTextAssetSubsystem* Subsystem = GetGameInstance()->GetSubsystem<USGDynamicTextAssetSubsystem>();
```

## Cache

The subsystem maintains an in-memory cache mapping dynamic text asset IDs to loaded providers:

```cpp
UPROPERTY(Transient)
TMap<FSGDynamicTextAssetId, TScriptInterface<ISGDynamicTextAssetProvider>> LoadedObjects;
```

Once loaded, dynamic text assets remain in cache until explicitly removed or the subsystem is deinitialized. 
The `FOnDynamicTextAssetCached` delegate broadcasts when a new provider is added to cache.

## Synchronous API

### GetDynamicTextAsset

Retrieves a dynamic text asset from cache by ID. Returns an empty `TScriptInterface` if not loaded. Does not trigger loading.

```cpp
UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
TScriptInterface<ISGDynamicTextAssetProvider> GetDynamicTextAsset(const FSGDynamicTextAssetId& Id) const;

// Type-safe version
template<typename T>
T* GetDynamicTextAsset(const FSGDynamicTextAssetId& Id) const;
```

### LoadDynamicTextAssetFromFile

Loads a dynamic text asset from a file path synchronously. 
If an object with the same ID is already cached, returns the cached version.

```cpp
UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|Loading")
TScriptInterface<ISGDynamicTextAssetProvider> LoadDynamicTextAssetFromFile(
    const FString& FilePath,
    UClass* DynamicTextAssetClass = nullptr);

// Type-safe version
template<typename T>
T* LoadDynamicTextAssetFromFile(const FString& FilePath);
```

The `DynamicTextAssetClass` parameter is optional. 
When provided, it determines the UClass to instantiate. 
When `nullptr`, the class is inferred from the file's metadata.

### GetOrLoadDynamicTextAsset

Gets from cache if available, otherwise loads from the specified file path.

```cpp
UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|Loading")
TScriptInterface<ISGDynamicTextAssetProvider> GetOrLoadDynamicTextAsset(
    const FSGDynamicTextAssetId& Id,
    const FString& FilePath,
    UClass* DynamicTextAssetClass = nullptr);
```

### LoadAllDynamicTextAssetsOfClass

Scans the file system for all dynamic text assets of a given class and loads them into cache. 
Returns the number of assets loaded.

```cpp
UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset|Loading")
int32 LoadAllDynamicTextAssetsOfClass(UClass* DynamicTextAssetClass, bool bIncludeSubclasses = true);
```

## Asynchronous API

### LoadDynamicTextAssetFromFileAsync

Loads a dynamic text asset from file on a background thread. 
File I/O and decompression (for binary files) happen off the game thread. 
Object creation and deserialization happen on the game thread. 
The callback is invoked on the game thread when complete.

```cpp
void LoadDynamicTextAssetFromFileAsync(
    const FString& FilePath,
    UClass* DynamicTextAssetClass,
    FOnDynamicTextAssetLoaded OnComplete);

// Type-safe version
template<typename T>
void LoadDynamicTextAssetFromFileAsync(
    const FString& FilePath,
    TFunction<void(T*, bool)> OnComplete);
```

### LoadDynamicTextAssetAsync

Loads by ID, automatically searching the file system to find the correct file. 
An optional class hint speeds up the file search.

```cpp
void LoadDynamicTextAssetAsync(const FSGDynamicTextAssetId& Id, const UClass* ClassHint,
    FOnDynamicTextAssetLoaded OnComplete);
```

### Async Status

```cpp
UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Loading")
bool IsAsyncLoadInProgress() const;

UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset|Loading")
int32 GetPendingAsyncLoadCount() const;
```

## Cache Management

```cpp
UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
bool IsDynamicTextAssetCached(const FSGDynamicTextAssetId& Id) const;

UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
int32 GetCachedObjectCount() const;

UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset")
bool RemoveFromCache(const FSGDynamicTextAssetId& Id);

UFUNCTION(BlueprintCallable, Category = "Dynamic Text Asset")
void ClearCache();
```

Use `ClearCache()` with caution or sparingly, existing `FSGDynamicTextAssetRef` resolutions will return `nullptr` until the objects are reloaded.

## Delegates

| Delegate | Signature | Description |
|----------|-----------|-------------|
| `FOnDynamicTextAssetLoaded` | `(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess)` | Callback for async load completion |
| `FOnDynamicTextAssetCached` | `(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider)` | Broadcast when a provider is added to cache. Dynamic multicast, bindable in Blueprints via `OnDynamicTextAssetCached`. |

## Server Interface Integration

The subsystem always maintains a valid `ServerInterface` member (never null). 
On initialization, it creates a `USGDynamicTextAssetServerNullInterface` that provides immediate no-op responses. 
This **null object pattern** eliminates null checks throughout the loading flow if no real server backend is configured, 
the null interface causes loading to fall through to local files.

```cpp
void USGDynamicTextAssetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ServerInterface = NewObject<USGDynamicTextAssetServerNullInterface>(this);
}
```

See [Server Interface](../Server/ServerInterface.md) for implementation details.

## Loading Flow

1. **Check cache**: If the ID is already cached, return immediately.
2. **Check server**: Query the server interface for updates (no-op if using null interface).
3. **Apply server data**: If the server provides updated data, use it instead of the local file.
4. **Locate file**: Find the file path (provided directly or via file system scan).
5. **Read file**: Read raw contents via `ReadRawFileContents` (handles binary decompression transparently, returns `SerializerTypeId` for binary files).
6. **Resolve serializer**: For binary files, use `FindSerializerForTypeId` with the embedded TypeId. For text files, use `FindSerializerForFile` to match by extension.
7. **Deserialize**: Parse contents, instantiate the object, populate UPROPERTY fields. Trigger migration if needed.
8. **Post load**: Call `PostDynamicTextAssetLoaded()` for custom initialization.
9. **Cache**: Add to `LoadedObjects` mapping and then broadcast `OnDynamicTextAssetCached`.

## Usage Example

```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    USGDynamicTextAssetSubsystem* Subsystem =
        GetGameInstance()->GetSubsystem<USGDynamicTextAssetSubsystem>();

    // Listen for new providers added to cache
    Subsystem->OnDynamicTextAssetCached.AddDynamic(this, &AMyGameMode::OnDynamicTextAssetCached);

    // Bulk load all weapons at startup
    int32 LoadedCount = Subsystem->LoadAllDynamicTextAssetsOfClass(UWeaponData::StaticClass());
    UE_LOG(LogTemp, Log, TEXT("Loaded %d weapon dynamic text assets"), LoadedCount);
}
```

## Cache Events

```cpp
UPROPERTY(BlueprintAssignable, Category = "Dynamic Text Asset|Events")
FOnDynamicTextAssetCached OnDynamicTextAssetCached;
```

This multicast delegate fires whenever a provider is added to the cache via `AddToCache()`. Subscribe to react to cache additions in both C++ and Blueprints.

## Async Load Status

Two methods provide visibility into pending async operations:

```cpp
// True if any async loads are in flight
bool IsAsyncLoadInProgress() const { return PendingAsyncLoads.GetValue() > 0; }

// Number of pending async operations
int32 GetPendingAsyncLoadCount() const { return PendingAsyncLoads.GetValue(); }
```

The `PendingAsyncLoads` counter is an `FThreadSafeCounter` (atomic), safe to read from any thread. It is incremented before dispatching to the thread pool and decremented when the game-thread callback completes.

See [Async Loading and Threading](../Advanced/AsyncLoadingAndThreading.md) for the full async pipeline details.

## Splitscreen Support

The subsystem is a `UGameInstanceSubsystem`, so **one instance exists per `UGameInstance`**. In splitscreen, all local players share the same `UGameInstance` and therefore share the same subsystem, the same `LoadedObjects` cache, and the same loaded data. No special handling is required.

This is the correct behavior for a static game data system. Weapon stats, item definitions, quest data, etc. are shared data, not per-player data. If Player 1 loads a dynamic text asset, Player 2 gets it from cache instantly.

| Aspect | Behavior |
|--------|----------|
| Subsystem instances | One per `UGameInstance` (shared across all local players) |
| Cache | Single `LoadedObjects` map shared by all local players |
| Asset data | Shared and read-only, identical for all players |
| Thread safety | All cache operations happen on the game thread |
| `FSGDynamicTextAssetRef` resolution | Resolves via `UGameInstance`, identical for all local players |

If your game requires **per-player data variants** (e.g., player-specific loadout configurations), manage that at the game logic layer rather than in the plugin. The plugin provides the shared base data that all players read from.

[Back to Table of Contents](../TableOfContents.md)
