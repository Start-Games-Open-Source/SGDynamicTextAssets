# Registry

[Back to Table of Contents](../TableOfContents.md)

## USGDynamicTextAssetRegistry

The registry is a `UEngineSubsystem` singleton that tracks all registered dynamic text asset classes. 
It maintains a list of base classes and uses UClass reflection to discover their children.

```cpp
UCLASS(NotBlueprintType, NotBlueprintable, ClassGroup = "Start Games")
class USGDynamicTextAssetRegistry : public UEngineSubsystem
```

### Accessing the Registry

```cpp
// May return nullptr
USGDynamicTextAssetRegistry* Registry = USGDynamicTextAssetRegistry::Get();

// Asserts if null
USGDynamicTextAssetRegistry& Registry = USGDynamicTextAssetRegistry::GetChecked();
```

## Registering Classes

Register your base dynamic text asset classes during module startup. 
Child classes are discovered automatically via Unreal's reflection system.

```cpp
void FMyGameModule::StartupModule()
{
    USGDynamicTextAssetRegistry::GetChecked().RegisterDynamicTextAssetClass(UWeaponData::StaticClass());
    USGDynamicTextAssetRegistry::GetChecked().RegisterDynamicTextAssetClass(UQuestData::StaticClass());
}
```

Only register the **base** classes in each hierarchy. 
For example, if `USwordData` extends `UWeaponData`, registering `UWeaponData` is sufficient. 
`USwordData` will be found via reflection.

```cpp
bool RegisterDynamicTextAssetClass(UClass* DynamicTextAssetClass);
bool UnregisterDynamicTextAssetClass(UClass* DynamicTextAssetClass);
```

### Registration Guards

`RegisterDynamicTextAssetClass` validates the class before accepting it. Registration fails with an error log if:

- The class does **not** implement `ISGDynamicTextAssetProvider`
- The class is derived from `AActor` or `UActorComponent` (dynamic text assets are data-only, not world entities)

These guards prevent accidental registration of non-data classes.

## Querying Classes

### GetAllRegisteredClasses

Returns all registered base classes and their children (discovered via `UClass` reflection). 
Filtered to exclude classes marked `Hidden`, `Deprecated`, or `HideDropdown`.

```cpp
void GetAllRegisteredClasses(TArray<UClass*>& OutClasses) const;
```

### GetAllRegisteredBaseClasses

Returns only the explicitly registered base classes, not their children.

```cpp
void GetAllRegisteredBaseClasses(TArray<UClass*>& OutClasses) const;
```

### GetAllInstantiableClasses

Returns all non-abstract classes that can be instantiated. 
This is what the Dynamic Text Asset Browser uses to populate its "New" menu.

```cpp
void GetAllInstantiableClasses(TArray<UClass*>& OutClasses) const;
```

### IsRegisteredClass

Checks if a given class is a registered base class or a derivative of a registered class.

```cpp
bool IsRegisteredClass(UClass* DynamicTextAssetClass) const;
```

### IsInstantiableClass

Returns true if the class is registered and instantiable (non-abstract).

```cpp
bool IsInstantiableClass(UClass* DynamicTextAssetClass) const;
```

### IsValidDynamicTextAssetClass

Checks if a class is registered or is a child of a registered class.

```cpp
bool IsValidDynamicTextAssetClass(UClass* TestClass) const;
```

## Folder Paths

The registry provides class-hierarchy-based folder paths for file storage:

```cpp
FString GetFolderPathForClass(UClass* DynamicTextAssetClass) const;
// e.g., "USGDynamicTextAsset_{TypeId}/UWeaponData_{TypeId}/USwordData_{TypeId}" (relative)
```

Each segment is formatted as `{ClassName}_{TypeId}` when a valid `FSGDynamicTextAssetTypeId` is available. Falls back to class name only when TypeIds have not yet been assigned. This path determines where dynamic text asset files are stored on disk within the `Content/SGDynamicTextAssets/` directory.

## Type ID Lookups

The registry maintains bidirectional maps between `UClass` and `FSGDynamicTextAssetTypeId` for all classes across all manifests.

```cpp
// Get the type ID assigned to a class
FSGDynamicTextAssetTypeId GetTypeIdForClass(const UClass* DynamicTextAssetClass) const;

// Reverse lookup: get the soft class pointer for a type ID
TSoftClassPtr<UObject> GetSoftClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;

// Resolve a type ID to its loaded UClass* (returns nullptr if not loaded)
UClass* ResolveClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;
```

## Type Manifest System

Each registered root class has an associated `FSGDynamicTextAssetTypeManifest` that maps all classes in its hierarchy to stable `FSGDynamicTextAssetTypeId` values.

```cpp
// Get the manifest for a registered root class
const FSGDynamicTextAssetTypeManifest* GetManifestForRootClass(const UClass* RootClass) const;

// Sync manifests with the current class hierarchy (editor: loads/creates/saves manifests)
void SyncManifests();

// Load pre-cooked manifests for packaged builds
void LoadCookedManifests();

// Compute the absolute path to a root class's manifest file
static FString GetRootClassManifestFilePath(const UClass* RootClass);
```

`SyncManifests()` is called during `Initialize()` in editor builds. It walks all descendants of each registered root class via reflection, assigns type IDs to new classes, and saves the manifest if it changed. In packaged builds, `LoadCookedManifests()` loads pre-baked manifests from the `_TypeManifests/` cooked directory instead.

See [Type Manifest System](../Advanced/TypeManifestSystem.md) for full details.

## Server Type Overrides

The registry supports server-driven type overlay for runtime builds:

```cpp
// Apply server-provided type overrides (routes by root type ID, rebuilds lookup maps)
void ApplyServerTypeOverrides(const TSharedPtr<FJsonObject>& ServerData);

// Clear all server type overrides, reverting to local-only state
void ClearServerTypeOverrides();

// Check if any manifest has active server overlay entries
bool HasServerTypeOverrides() const;
```

These are no-ops in editor builds. The expected JSON format uses root type IDs as keys with arrays of type entries. See [Type Manifest System](../Advanced/TypeManifestSystem.md) for the JSON schema.

## Cache Invalidation

The registry caches the full class list (base classes + all derived children discovered via reflection) for performance. This is a **runtime** cache that exists in both editor and packaged builds, since `USGDynamicTextAssetRegistry` is a `UEngineSubsystem`.

The cache is invalidated automatically whenever:
- A class is registered via `RegisterDynamicTextAssetClass()`
- A class is unregistered via `UnregisterDynamicTextAssetClass()`

The cache is rebuilt lazily on the next query (e.g., `GetAllRegisteredClasses()`). 
During the rebuild, `GetDerivedClasses()` is called to discover child classes, and classes with `CLASS_Hidden`, `CLASS_HideDropDown`, or `CLASS_Deprecated` flags are excluded.

In the **editor**, the cache effectively updates whenever new C++ classes are compiled and hot-reloaded, 
since the class hierarchy changes trigger re-queries. In **packaged builds**, the class hierarchy is fixed at compile time, so the cache is typically built once and never invalidated.

## Change Notification

```cpp
// Broadcast when classes are registered or unregistered
FOnDynamicTextAssetRegistryChanged OnRegistryChanged;

// Broadcast when type manifests are synced or updated
FOnTypeManifestUpdated OnTypeManifestUpdated;
```

`OnRegistryChanged` fires when classes are registered or unregistered. The editor browser listens to this to refresh its type tree.

`OnTypeManifestUpdated` fires when manifests are synced, cooked manifests are loaded, or server type overrides are applied/cleared.

[Back to Table of Contents](../TableOfContents.md)
