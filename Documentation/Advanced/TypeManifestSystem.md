# Type Manifest System

[Back to Table of Contents](../TableOfContents.md)

## Overview

The type manifest system provides stable identity for dynamic text asset **classes** (types), independent of C++ class names. This is distinct from `FSGDynamicTextAssetId`, which identifies individual asset **instances**.

| Concept | Struct | Purpose |
|---------|--------|---------|
| **Asset ID** | `FSGDynamicTextAssetId` | Unique identity for a single dynamic text asset instance |
| **Type ID** | `FSGDynamicTextAssetTypeId` | Unique identity for a dynamic text asset class (UClass) |

Type IDs allow the file system structure and binary format to reference classes by GUID rather than by C++ name, so renaming a class does not break existing files or cooked manifests.

## FSGDynamicTextAssetTypeId

A `USTRUCT(BlueprintType)` wrapper around `FGuid` that distinguishes type identity from arbitrary GUIDs at the type system level.

```cpp
USTRUCT(BlueprintType)
struct FSGDynamicTextAssetTypeId
```

### Construction and Factory Methods

```cpp
// Default (invalid/zero GUID)
FSGDynamicTextAssetTypeId();

// From existing GUID
FSGDynamicTextAssetTypeId(const FGuid& InGuid);

// Static factories
static FSGDynamicTextAssetTypeId NewGeneratedId();
static FSGDynamicTextAssetTypeId FromGuid(const FGuid& InGuid);
static FSGDynamicTextAssetTypeId FromString(const FString& InString);
```

### Core API

```cpp
bool IsValid() const;
const FGuid& GetGuid() const;
void SetGuid(const FGuid& InGuid);
void Invalidate();
void GenerateNewId();
FString ToString() const;
bool ParseString(const FString& GuidString);
```

### Serialization Support

`FSGDynamicTextAssetTypeId` supports all standard Unreal serialization paths via `TStructOpsTypeTraits`:

| Trait | Method | Purpose |
|-------|--------|---------|
| `WithSerializer` | `Serialize(FArchive&)` | Binary archive serialization (raw GUID, no property tags) |
| `WithNetSerializer` | `NetSerialize(FArchive&, UPackageMap*, bool&)` | Network replication (16 bytes) |
| `WithExportTextItem` | `ExportTextItem(...)` | Clipboard and config export |
| `WithImportTextItem` | `ImportTextItem(...)` | Clipboard and config import |
| `WithIdenticalViaEquality` | `operator==` | Identity comparison |

### Constants

```cpp
static const FSGDynamicTextAssetTypeId INVALID_TYPE_ID;
```

## Type Manifests

### FSGDynamicTextAssetTypeManifestEntry

A single entry mapping a type ID to its class and parent type:

```cpp
struct FSGDynamicTextAssetTypeManifestEntry
{
    FSGDynamicTextAssetTypeId TypeId;
    TSoftClassPtr<UObject> Class;
    FString ClassName;
    FSGDynamicTextAssetTypeId ParentTypeId;
};
```

`ParentTypeId` is an invalid (zero) GUID for root types. `ClassName` is cached during `AddType()` and `LoadFromFile()` for fast lookup without resolving the soft pointer.

### FSGDynamicTextAssetTypeManifest

Each registered root class gets its own manifest, stored alongside the root's content folder. The manifest tracks the full class hierarchy beneath that root.

```cpp
class FSGDynamicTextAssetTypeManifest
```

#### JSON Format

```json
{
  "$schema": "dta_type_manifest",
  "$version": 1,
  "rootTypeId": "A1B2C3D4-E5F6-7890-ABCD-EF1234567890",
  "types": [
    {
      "typeId": "A1B2C3D4-E5F6-7890-ABCD-EF1234567890",
      "className": "UWeaponData",
      "classPath": "/Script/MyGame.UWeaponData",
      "parentTypeId": "00000000-0000-0000-0000-000000000000"
    },
    {
      "typeId": "B2C3D4E5-F6A7-8901-BCDE-F12345678901",
      "className": "USwordData",
      "classPath": "/Script/MyGame.USwordData",
      "parentTypeId": "A1B2C3D4-E5F6-7890-ABCD-EF1234567890"
    }
  ]
}
```

#### Lookup API

All lookups are O(1) via internal index maps (`TypeIdIndex`, `ClassNameIndex`):

```cpp
// Find by type ID (local entries only)
const FSGDynamicTextAssetTypeManifestEntry* FindByTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;

// Find by class name (local entries only)
const FSGDynamicTextAssetTypeManifestEntry* FindByClassName(const FString& ClassName) const;

// Get soft class pointer by type ID (checks server overlay first, then local)
TSoftClassPtr<UObject> GetSoftClassPtr(const FSGDynamicTextAssetTypeId& TypeId) const;

// Get soft class pointer by class name (local only)
TSoftClassPtr<UObject> GetSoftClassPtrByClassName(const FString& ClassName) const;
```

#### Mutation API

```cpp
void AddType(const FSGDynamicTextAssetTypeId& TypeId, const TSoftClassPtr<UObject>& InClass,
    const FSGDynamicTextAssetTypeId& ParentTypeId);
bool RemoveType(const FSGDynamicTextAssetTypeId& TypeId);
void Clear();
```

#### Persistence

```cpp
bool LoadFromFile(const FString& FilePath);
bool SaveToFile(const FString& FilePath) const;
bool IsDirty() const;
```

## Manifest Lifecycle

### SyncManifests

`USGDynamicTextAssetRegistry::SyncManifests()` synchronizes manifests with the current class hierarchy:

1. For each registered root class, loads its manifest from disk (or creates a new one)
2. Walks all descendants via `GetDerivedClasses()` reflection
3. Assigns new `FSGDynamicTextAssetTypeId` values to classes not yet in the manifest
4. Saves the manifest if any changes were made
5. Rebuilds the TypeId/Class lookup maps

Manifests are stored in the `_TypeManifests/` directory within the dynamic text assets content folder.

```cpp
// Registry API
void SyncManifests();
static FString GetRootClassManifestFilePath(const UClass* RootClass);
```

### LoadCookedManifests

In packaged builds, source manifest files are unavailable. `LoadCookedManifests()` reads pre-cooked manifests from the `Content/SGDynamicTextAssetsCooked/_TypeManifests/` directory, resolves soft class references, and populates the TypeId/Class lookup maps.

```cpp
void LoadCookedManifests();
```

## Type Resolution

The registry provides O(1) lookups between UClass and TypeId via maps built during manifest sync:

```cpp
// UClass -> TypeId
FSGDynamicTextAssetTypeId GetTypeIdForClass(const UClass* DynamicTextAssetClass) const;

// TypeId -> soft class pointer
TSoftClassPtr<UObject> GetSoftClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;

// TypeId -> resolved UClass* (nullptr if not loaded)
UClass* ResolveClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;

// Get the manifest for a root class
const FSGDynamicTextAssetTypeManifest* GetManifestForRootClass(const UClass* RootClass) const;
```

## Folder Path Construction

The file manager uses TypeIds to build stable folder paths:

```cpp
FString GetFolderPathForClass(UClass* DynamicTextAssetClass) const;
```

Each segment is formatted as `{StrippedClassName}_{TypeId}`:

```
Content/SGDynamicTextAssets/
  WeaponData_A1B2C3D4-E5F6-7890-ABCD-EF1234567890/
    excalibur.dta.json
    SwordData_B2C3D4E5-F6A7-8901-BCDE-F12345678901/
      enchanted_blade.dta.json
```

When the registry is unavailable or TypeIds have not yet been assigned (early startup), the path falls back to class name only (e.g., `WeaponData/`).

## Server Type Overrides

The server can dynamically modify the type system at runtime without changing local manifest files.

### Flow

```
Server Interface             Subsystem                   Registry              Manifest
      |                          |                           |                    |
      |  FetchTypeManifestOverrides()                        |                    |
      |<-------------------------|                           |                    |
      |                          |                           |                    |
      |  OnComplete(JSON)        |                           |                    |
      |------------------------->|                           |                    |
      |                          |  ApplyServerTypeOverrides()|                   |
      |                          |-------------------------->|                    |
      |                          |                           |  ApplyServerOverrides()
      |                          |                           |------------------->|
      |                          |                           |  RebuildTypeIdMaps()|
      |                          |                           |<-------------------|
```

### Server JSON Format

```json
{
  "manifests": {
    "<rootTypeId>": {
      "types": [
        { "typeId": "...", "className": "UNewWeaponType", "parentTypeId": "..." }
      ]
    }
  }
}
```

### Overlay Behavior

Server overlay entries are stored separately from local entries (`ServerOverlayEntries` map) and never modify the on-disk manifest file.

| Action | How |
|--------|-----|
| **Add new type** | Server provides an entry with a TypeId not present locally |
| **Override existing type** | Server provides an entry with a TypeId that exists locally (replaces class mapping) |
| **Disable a type** | Server provides an entry with an empty `className` |

### Resolution Priority

`GetEffectiveEntry()` checks the server overlay first, then falls back to local entries. An overlay entry with empty `ClassName` returns `nullptr` (type is disabled).

```cpp
// Manifest API
void ApplyServerOverrides(const TSharedPtr<FJsonObject>& ServerData);
const FSGDynamicTextAssetTypeManifestEntry* GetEffectiveEntry(const FSGDynamicTextAssetTypeId& TypeId) const;
void GetAllEffectiveTypes(TArray<FSGDynamicTextAssetTypeManifestEntry>& OutEntries) const;
void ClearServerOverrides();
bool HasServerOverrides() const;

// Registry API
void ApplyServerTypeOverrides(const TSharedPtr<FJsonObject>& ServerData);
void ClearServerTypeOverrides();
bool HasServerTypeOverrides() const;

// Subsystem API (convenience)
void FetchAndApplyServerTypeOverrides(FOnServerTypeOverridesComplete OnComplete = {});
void ApplyServerTypeOverrides(const TSharedPtr<FJsonObject>& ServerData);
void ClearServerTypeOverrides();
```

### Change Notification

```cpp
// Broadcast when type manifests are synced or updated
FOnTypeManifestUpdated OnTypeManifestUpdated;
```

[Back to Table of Contents](../TableOfContents.md)
