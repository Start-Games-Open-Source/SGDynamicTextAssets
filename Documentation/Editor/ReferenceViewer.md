# Reference Viewer

[Back to Table of Contents](../TableOfContents.md)

## Overview

The Reference Viewer (`SSGDynamicTextAssetReferenceViewer`) displays what references a dynamic text asset (referencers) and what it depends on (dependencies). It helps track cross references and understand the dependency graph.

## Opening the Viewer

- **Context menu > Show in Reference Viewer** in the browser.
- **Reference Viewer** button in the editor toolbar.
- Programmatically via `SSGDynamicTextAssetReferenceViewer::OpenViewer(const FSGDynamicTextAssetId& Id, const FString& UserFacingId)`.

The viewer title bar displays the UserFacingId of the object being inspected. If a viewer is already open, calling `OpenViewer` updates it in place with `SetDataObject()` rather than creating a duplicate.

## Layout

### Referencers Panel (Left)

Always expanded. Shows assets that contain an `FSGDynamicTextAssetRef` pointing to this dynamic text asset.

**Search Bar**: A search box at the top of the panel filters the list of referencers. The hint text reads "Search referencers...".

Each entry displays:

- Asset name and type
- Reference type icon and color based on `ESGReferenceType`:
  - **Blueprint**: References from Blueprint classes
  - **DynamicTextAsset**: References from other dynamic text assets
  - **Level**: References from level actors
  - **Other**: References from other UObject types

A count badge shows the number of matching results in the format `(filtered/total)`.

### Dependencies Panel (Right)

Shows dynamic text assets that this object references via its own `FSGDynamicTextAssetRef` properties. The system inspects the object's UPROPERTY fields to find outgoing references.

**Collapsible**: The dependencies panel is collapsed by default. When collapsed, it appears as a narrow 28 pixel wide sidebar with an expander arrow and a count badge. Click the panel header to toggle between expanded and collapsed states.

**Search Bar**: When expanded, a search box at the top of the panel filters the list of dependencies. The hint text reads "Search dependencies...".

A count badge shows the number of matching results in the format `(filtered/total)`.

## Reference Scanning

The viewer uses `USGDynamicTextAssetReferenceSubsystem` (an editor subsystem) to perform reference scanning. When opened, an asynchronous scan begins:

1. A loading overlay with a progress indicator is displayed.
2. Progress updates appear as the scan proceeds.
3. When complete, both panels populate with results.

**Double click** an entry to navigate:
- **Referencers**: Navigate to the referencer asset (opens the relevant editor).
- **Dependencies**: Navigate to the referenced dynamic text asset (opens the Dynamic Text Asset Editor).

## Local Reference Cache

The reference subsystem maintains a **two tier caching system** to avoid rescanning the entire project on every viewer open:

### In Memory Cache

A `TMap<FSGDynamicTextAssetId, TArray<FSGDynamicTextAssetReferenceEntry>>` maps each dynamic text asset ID to its known referencers. This cache is populated during scans and provides instant results for previously scanned objects. It is invalidated when `InvalidateCache()` or `ClearCacheAndRescan()` is called.

### Persistent Disk Cache

Reference scan results are persisted to disk as JSON files in the cache folder (configurable via `USGDynamicTextAssetEditorSettings::ReferenceCacheFolderPath`, default: `Saved/SGDynamicTextAssets/ReferenceCache/`). The cache is organized by content type:

| Cache File          | Contents                                             |
|---------------------|------------------------------------------------------|
| `GameContent.json`  | References found in project content assets           |
| `PluginContent.json`| References found in plugin content assets            |
| `EngineContent.json`| References found in engine content (optional, disabled by default) |
| `DynamicTextAssets.json`  | Cross references between dynamic text assets                |

Each cached entry stores the asset path, last modified timestamp, found ID references with property paths, display name, and reference type.

### Incremental Scanning

The scanner uses **timestamp based incremental updates**. When a scan is triggered, it compares the file's current modification timestamp against the cached entry's timestamp. Only files that have been modified since the last scan are rescanned. This significantly reduces scan time on subsequent opens.

Scanning respects the editor settings for content scope:
- `bScanEngineContent` (default: `false`): Whether to scan Engine content. Enabling this increases scan time.
- `bScanPluginContent` (default: `true`): Whether to scan Plugin content.

The scan is time budgeted at approximately 10ms per editor frame to avoid freezing the editor, and runs across three phases: Blueprints, Levels, then DynamicTextAssets.

## Reference Types

```cpp
UENUM(BlueprintType)
enum class ESGReferenceType : uint8
{
    // Reference from a Blueprint class
    Blueprint,
    // Reference from another dynamic text asset
    DynamicTextAsset,
    // Reference from a level actor
    Level,
    // Reference from other UObject type
    Other
};
```

## Async Reference Scanning

`USGDynamicTextAssetReferenceSubsystem` is an editor subsystem that performs incremental reference scanning across the project. Scans run asynchronously using `FTSTicker` with a time budget of approximately 10ms per frame.

### Scanning Phases

The scanner processes content in three sequential phases:

1. **Blueprints**: Scans all Blueprint assets for `FSGDynamicTextAssetRef` properties
2. **Levels**: Scans level actors for dynamic text asset references
3. **Dynamic Text Assets**: Scans `.dta.json` files for cross-references between dynamic text assets

### Progress Tracking

```cpp
// Progress delegate (Current, Total, StatusText)
FOnReferenceScanProgress OnReferenceScanProgress;

// Completion delegate
FOnReferenceScanComplete OnReferenceScanComplete;
```

The editor displays a toast notification with a progress bar during scanning.

## Persistent Cache

The reference subsystem supports persisting its scan results to disk for faster subsequent startups:

- `SaveCacheToDisk()`: Writes the current reference cache to the configured folder
- `LoadCacheFromDisk()`: Restores cached data from disk on startup

Cache entries track file modification timestamps for incremental rescanning. Only files modified since the last scan are re-processed.

### Scanning Configuration

Configure scanning behavior in **Project Settings > Plugins > SG Dynamic Text Assets Editor**:

| Setting | Default | Description |
|---------|---------|-------------|
| `bScanEngineContent` | `false` | Whether to scan Engine content directories (increases scan time significantly) |
| `bScanPluginContent` | `true` | Whether to scan Plugin content directories |
| `ReferenceCacheFolderPath` | `SGDynamicTextAssets/ReferenceCache` | Relative path (from `Saved/`) for the persistent cache |

See [Settings](../Configuration/Settings.md) for more details.

## Internals

> This section documents the internal architecture for developers who need to understand, debug, extend, or maintain the reference viewer system.

### Architecture Overview

The reference viewer is split into two layers: a **Slate widget** for display and a **subsystem** for data.

```
+---------------------------------------+      +------------------------------------------+
| SSGDynamicTextAssetReferenceViewer    |      | USGDynamicTextAssetReferenceSubsystem    |
| (Slate Widget)                        |      | (UEditorSubsystem)                       |
|                                       |      |                                          |
|  Referencers Panel   Deps Panel       | ---> |  FindReferencers(Id)                     |
|  [Search] [List]     [Search] [List]  |      |  FindDependencies(Id)                    |
|                                       | <--- |  OnReferenceScanProgress                 |
|  Loading Overlay                      |      |  OnReferenceScanComplete                 |
+---------------------------------------+      +------------------------------------------+
                                                             |
                                                  +----------+-----------+
                                                  |                      |
                                           +------v-------+    +--------v-----------+
                                           | ReferencerCache|  | PersistentAssetCache|
                                           | (In-Memory)    |  | (Disk JSON files)   |
                                           | Id -> Entries  |  | Path -> CacheEntry  |
                                           +----------------+  +---------------------+
```

The widget is purely a display layer. It calls `FindReferencers()` and `FindDependencies()` on the subsystem, subscribes to scan progress/completion delegates, and renders the results in list views with search filtering. All scanning logic, caching, and property inspection live in the subsystem.

### Data Structures

Three structs form the backbone of the reference system:

**`FSGDynamicTextAssetReferenceEntry`** - An inbound reference (something that points TO an asset).

| Field | Type | Description |
|-------|------|-------------|
| `ReferencedId` | `FSGDynamicTextAssetId` | The dynamic text asset being referenced |
| `SourceAsset` | `FSoftObjectPath` | The asset containing the reference |
| `PropertyPath` | `FString` | Where in the source the reference was found (e.g. `"WeaponRef"`, `"Loadout.Weapons[0]"`) |
| `ReferenceType` | `ESGReferenceType` | Blueprint, DynamicTextAsset, Level, or Other |
| `SourceDisplayName` | `FString` | Human-readable name for the UI (e.g. `"BP_Player > WeaponComponent"`) |
| `SourceFilePath` | `FString` | Raw file path (only for DynamicTextAsset-type entries) |

**`FSGDynamicTextAssetDependencyEntry`** - An outbound reference (what an asset depends on). This is a dual-purpose struct:
- When `bIsAssetReference == false`: represents a DTA-to-DTA dependency via `DependencyId` and `UserFacingId`
- When `bIsAssetReference == true`: represents a soft asset reference via `AssetPath` and `AssetClass`

**`FSGReferenceCacheEntry`** - A persistent cache record for one scanned asset.

| Field | Type | Description |
|-------|------|-------------|
| `AssetPath` | `FString` | Full path to the scanned asset or file |
| `LastModifiedTime` | `FDateTime` | Timestamp when this entry was last scanned |
| `ContentType` | `ESGReferenceCacheContentType` | GameContent, PluginContent, EngineContent, or DynamicTextAssets |
| `FoundReferences` | `TMap<FSGDynamicTextAssetId, TArray<FString>>` | Map of referenced ID to encoded property path strings |
| `SourceDisplayName` | `FString` | Display name for UI |
| `ReferenceType` | `ESGReferenceType` | Type classification for all entries from this asset |

The `FoundReferences` strings are encoded as `"DisplayName|PropertyPath"` (split on the last `|` delimiter during deserialization).

**Relationship between caches:**

```
FSGReferenceCacheEntry (per scanned asset, e.g. BP_Player)
  |
  |-- FoundReferences: { Id_Sword -> ["BP_Player|WeaponRef"],
  |                       Id_Shield -> ["BP_Player|Loadout.ShieldRef"] }
  |
  v (inverted by RebuildReferencerCacheFromPersistent)

ReferencerCache
  |-- Id_Sword  -> [ ReferenceEntry{Source=BP_Player, Path="WeaponRef"} ]
  |-- Id_Shield -> [ ReferenceEntry{Source=BP_Player, Path="Loadout.ShieldRef"} ]
```

### Reference Discovery

The subsystem provides two directions of reference scanning. All functions listed below live in `SGDynamicTextAssetReferenceSubsystem.cpp` unless otherwise noted.

#### Finding Referencers (Inbound)

`FindReferencers()` answers: "What assets reference this dynamic text asset?"

```
FindReferencers(Id):                                          // line ~62
  1. If cache is populated and !bForceRescan:
       Return ReferencerCache[Id]
  2. Otherwise:
       RebuildReferenceCache()      // synchronous, blocking  // line ~186
       Return ReferencerCache[Id]
```

The cache rebuild scans three asset types in sequential phases. The synchronous path calls `ScanBlueprintAssets()`, `ScanLevelAssets()`, and `ScanDynamicTextAssetFiles()` directly. The async path uses per-item functions instead (see [Async Scanning Pipeline](#async-scanning-pipeline)).

**Phase 0 - Blueprints** (`ProcessBlueprintAsset()` for async, `ScanBlueprintAssets()` for sync):
```
For each Blueprint asset in AssetRegistry:
  Load Blueprint -> get GeneratedClass -> get CDO
  ExtractRefsFromObject(CDO)
  If CDO is an Actor:
    ForEachComponentOfActorClassDefault -> ExtractRefsFromObject(Component)
  Store results in PersistentAssetCache[PackagePath]
```

Component display names are formatted as `"BP_Name > ComponentName"` (with the `_GEN_VARIABLE` suffix stripped from editor-added components).

**Phase 1 - Levels** (`ProcessLevelAsset()` for async, `ScanLevelAssets()` for sync):
```
For each Level/World asset in AssetRegistry:
  Load World -> get PersistentLevel
  For each Actor in Level:
    ExtractRefsFromObject(Actor)
    For each Component on Actor:
      ExtractRefsFromObject(Component)
  Store results in PersistentAssetCache[PackagePath]
```

Display names follow `"LevelName > ActorLabel > ComponentName"` hierarchy.

**Phase 2 - Dynamic Text Asset Files** (`ProcessDynamicTextAssetFile()` for async, `ScanDynamicTextAssetFiles()` for sync):
```
For each .dta.json file on disk:
  Read JSON, extract metadata (ClassName, UserFacingId)
  Lookup UClass via FindFirstObject, verify it implements ISGDynamicTextAssetProvider
  Skip abstract classes (cannot be instantiated)
  Create transient object in GetTransientPackage()
  Deserialize JSON into transient object
  ExtractRefsFromObject(TransientObject)
  Store results in PersistentAssetCache[FilePath]
```

After all phases, the PersistentAssetCache is **inverted** into the ReferencerCache by `RebuildReferencerCacheFromPersistent()`:

```
RebuildReferencerCacheFromPersistent():                       // line ~1638
  Clear ReferencerCache
  For each (AssetPath, CacheEntry) in PersistentAssetCache:
    For each (RefId, EncodedStrings) in CacheEntry.FoundReferences:
      For each encoded string "DisplayName|PropertyPath":
        Split on last '|'
        Reconstruct FSoftObjectPath (append ".AssetName" for bare package paths)
        ReferencerCache[RefId].Add(ReferenceEntry)
```

**Key functions for referencers scanning:**

| Function | Purpose |
|----------|---------|
| `FindReferencers()` | Public API entry point, checks cache or triggers rebuild |
| `RebuildReferenceCache()` | Synchronous full scan (blocks with `FScopedSlowTask`) |
| `RebuildReferenceCacheAsync()` | Async incremental scan (see [Async Scanning Pipeline](#async-scanning-pipeline)) |
| `ProcessBlueprintAsset()` | Scans one Blueprint (loads BP, gets CDO + components) |
| `ProcessLevelAsset()` | Scans one Level (loads World, iterates actors + components) |
| `ProcessDynamicTextAssetFile()` | Scans one .dta.json file (deserializes into temp object) |
| `ScanBlueprintAssets()` | Sync batch: scans all Blueprints |
| `ScanLevelAssets()` | Sync batch: scans all Levels |
| `ScanDynamicTextAssetFiles()` | Sync batch: scans all DTA files |
| `ExtractRefsFromObject()` | Iterates all UPROPERTY fields on a UClass, delegates to `ExtractRefsFromProperty()` |
| `ExtractRefsFromProperty()` | Recursive DFS through property tree (see [Property Traversal Algorithm](#property-traversal-algorithm)) |
| `RebuildReferencerCacheFromPersistent()` | Inverts PersistentAssetCache into ReferencerCache |

#### Finding Dependencies (Outbound)

`FindDependencies()` answers: "What does this dynamic text asset reference?"

Dependencies are always computed fresh (not cached) because they only inspect a single file:

```
FindDependencies(Id):                                         // line ~80
  1. FindFileForId(Id) via FileManager
  2. Read raw file contents
  3. Extract metadata (ClassName)
  4. FindFirstObject<UClass>(ClassName), verify ISGDynamicTextAssetProvider
  5. Skip if abstract
  6. NewObject<UObject>(GetTransientPackage(), Class) -> temp object
  7. Deserialize JSON into temp object
  8. ExtractDependenciesFromObject(TempObject):               // line ~1079
     Walk all UPROPERTY fields, collect:
     - FSGDynamicTextAssetRef values (DTA dependencies)
     - TSoftObjectPtr values (asset references)
     - TSoftClassPtr values (class references)
  9. Return combined array
```

**Key functions for dependency scanning:**

| Function | Purpose |
|----------|---------|
| `FindDependencies()` | Public API entry point, loads file and deserializes |
| `ExtractDependenciesFromObject()` | Iterates all UPROPERTY fields, delegates to `ExtractDepsFromProperty()` |
| `ExtractDepsFromProperty()` | Recursive DFS for dependencies, also checks `FSoftObjectProperty` and `FSoftClassProperty` |

### Property Traversal Algorithm

Both `ExtractRefsFromProperty()` (line ~935) and `ExtractDepsFromProperty()` (line ~1097) in `SGDynamicTextAssetReferenceSubsystem.cpp` use the same recursive depth-first pattern to find references at any nesting depth.

```
ExtractRefsFromProperty(Property, ContainerPtr, PropertyPath):

  // Base case: Direct FSGDynamicTextAssetRef match
  if Property is FStructProperty AND Struct == FSGDynamicTextAssetRef::StaticStruct():
    Read ref value from ContainerPtr
    if ref.IsValid():
      Emit entry with PropertyPath
    return    // Do NOT recurse into the ref struct itself

  // Recursive case 1: Non-ref struct
  if Property is FStructProperty:
    for each InnerProperty in Struct.Fields:
      ExtractRefsFromProperty(InnerProperty, StructPtr, "Path.FieldName")
    return

  // Recursive case 2: Array
  if Property is FArrayProperty:
    for i in 0..ArraySize-1:
      if Inner is FSGDynamicTextAssetRef struct -> emit directly
      else if Inner is another struct -> recurse into its fields
    return

  // Recursive case 3: Map (values only)
  if Property is FMapProperty:
    for each (Key, Value) in Map:
      if Value is FSGDynamicTextAssetRef struct -> emit directly
      else if Value is another struct -> recurse into its fields
    return
```

The `ExtractDepsFromProperty` variant additionally checks for `FSoftObjectProperty` and `FSoftClassProperty` at each level, emitting asset dependency entries for non-null soft references.

**Example:** Given this class hierarchy:

```cpp
UCLASS()
class UCharacterLoadout : public USGDynamicTextAsset
{
    GENERATED_BODY()
public:
    UPROPERTY()
    FSGDynamicTextAssetRef PrimaryWeaponRef;

    UPROPERTY()
    TArray<FSGDynamicTextAssetRef> AbilityRefs;

    UPROPERTY()
    FEquipmentSlot HelmetSlot;
};

USTRUCT()
struct FEquipmentSlot
{
    GENERATED_BODY()

    UPROPERTY()
    FSGDynamicTextAssetRef ItemRef;

    UPROPERTY()
    TArray<FSGDynamicTextAssetRef> AttachmentRefs;
};
```

The traversal produces these property paths:

| Property Path | Source |
|---------------|--------|
| `PrimaryWeaponRef` | Direct struct property |
| `AbilityRefs[0]` | Array element 0 |
| `AbilityRefs[1]` | Array element 1 |
| `HelmetSlot.ItemRef` | Nested struct field |
| `HelmetSlot.AttachmentRefs[0]` | Array inside nested struct |
| `HelmetSlot.AttachmentRefs[1]` | Array inside nested struct |

Property names in paths use `GetDisplayNameText()` (the Unreal display name, not the raw C++ name).

### Async Scanning Pipeline

`RebuildReferenceCacheAsync()` (line ~217) performs non-blocking incremental scanning. The per-frame work is driven by `ProcessScanBatch()` (line ~356):

```
RebuildReferenceCacheAsync()
  |
  [1] Guard: if bScanningInProgress, return early
  [2] Set bScanningInProgress = true
  |
  [3] RebuildReferencerCacheFromPersistent()
      (immediate results from disk cache while scan runs)
  |
  [4] Gather pending items (no loading, just queries):
      - PendingBlueprintAssets  (AssetRegistry query, filtered by content type + timestamps)
      - PendingLevelAssets      (AssetRegistry query, filtered by content type + timestamps)
      - PendingDynamicTextAssetFiles (file enumeration, filtered by timestamps)
      - TotalItemsToScan = sum of all three
  |
  [5] If TotalItemsToScan == 0:
      Cache is up to date, broadcast OnReferenceScanComplete, return
  |
  [6] Register ProcessScanBatch() with FTSTicker (every frame)
  |
  [7] Each frame, ProcessScanBatch(DeltaTime) runs:
      |
      Start timer
      While elapsed < 10ms:
        Phase 0: Pop and process next PendingBlueprintAsset
        Phase 1: Pop and process next PendingLevelAsset
        Phase 2: Pop and process next PendingDynamicTextAssetFile
        ItemsScanned++
      |
      Broadcast OnReferenceScanProgress(ItemsScanned, Total, StatusText)
      Update toast notification with percentage
      |
      If all phases complete:
        RebuildReferencerCacheFromPersistent()
        SaveCacheToDisk()
        Set bCachePopulated = true, bScanningInProgress = false
        CloseScanNotification(success)
        Broadcast OnReferenceScanComplete
        Return false (unregister ticker)
```

Key behaviors:
- The **10ms time budget** (`TIME_BUDGET_SECONDS = 0.010`) prevents editor freezes
- **Stale results** from the PersistentAssetCache are shown immediately while scanning runs in the background
- The ticker is **unregistered** on completion (`return false`) to avoid idle CPU cost
- Progress is reported per-phase with descriptive text (e.g. "Scanning Blueprints... (42 remaining)")
- A **toast notification** with percentage is displayed during the scan

### Cache Architecture Deep Dive

#### In-Memory Cache (Hot Path)

```cpp
TMap<FSGDynamicTextAssetId, TArray<FSGDynamicTextAssetReferenceEntry>> ReferencerCache;
```

This is a **reverse index** mapping each DTA ID to its list of referencers. Lookups are O(1) by GUID. It is populated by inverting the PersistentAssetCache.

**Invalidation triggers:**
- `InvalidateCache()` (line ~180) - clears ReferencerCache and sets `bCachePopulated = false`
- `ClearCacheAndRescan()` (line ~1329) - clears both caches, deletes disk cache folder, and triggers a full async rescan
- `RebuildReferenceCache()` (line ~186, sync) / `RebuildReferenceCacheAsync()` (line ~217, async) - rebuilds from scratch

#### Persistent Disk Cache (Cold Storage)

```cpp
TMap<FString, FSGReferenceCacheEntry> PersistentAssetCache;
```

This is a **forward index** mapping each scanned asset path to its cache entry. It persists to disk as JSON files organized by content type:

```
Saved/SGDynamicTextAssets/ReferenceCache/
  GameContent.json        - References from project content
  PluginContent.json      - References from plugin content
  EngineContent.json      - References from engine content (optional)
  DynamicTextAssets.json  - Cross-references between DTAs
```

**Disk format:**

```json
{
  "version": 1,
  "savedAt": "2026-03-11T14:30:00Z",
  "entries": [
    {
      "assetPath": "/Game/Blueprints/BP_PlayerCharacter",
      "lastModified": "2026-03-10T09:15:00Z",
      "displayName": "BP_PlayerCharacter",
      "referenceType": 0,
      "references": {
        "a1b2c3d4-e5f6-7890-abcd-ef1234567890": [
          "BP_PlayerCharacter|WeaponRef",
          "BP_PlayerCharacter > WeaponComponent|MeleeWeaponRef"
        ]
      }
    }
  ]
}
```

The `references` object maps DTA IDs (as GUID strings) to arrays of encoded strings. Each string is `"DisplayName|PropertyPath"`.

The `referenceType` field maps to the `ESGReferenceType` enum as an integer: 0=Blueprint, 1=DynamicTextAsset, 2=Level, 3=Other.

#### Incremental Scanning

The scanner compares file timestamps to skip unchanged assets via `DoesAssetNeedRescan()` (line ~1619):

```
DoesAssetNeedRescan(AssetPath, CurrentTimestamp):
  if bForceFullRescan: return true
  if AssetPath not in PersistentAssetCache: return true   // Never scanned
  return CurrentTimestamp > PersistentAssetCache[AssetPath].LastModifiedTime
```

For Blueprint and Level assets, the timestamp comes from the package file on disk via `IFileManager::Get().GetTimeStamp()`. For DTA files, it comes directly from the `.dta.json` file's modification time.

#### Content Type Classification

Assets are classified by their path using `GetContentTypeForPath()` (line ~1519):

| Path Pattern | Content Type |
|-------------|-------------|
| Contains DTA root path or has DTA extension | `DynamicTextAssets` |
| Starts with `/Game/` | `GameContent` |
| Mount point has content in `<ProjectDir>/Plugins/` | `PluginContent` |
| Everything else (Engine, Engine plugins) | `EngineContent` |

#### Lifecycle

1. **Initialization** (`Initialize()`, line ~35): `LoadCacheFromDisk()` (line ~1409) reads all four JSON files into PersistentAssetCache, then calls `RebuildReferencerCacheFromPersistent()` to populate the in-memory ReferencerCache
2. **First query**: If `bCachePopulated` is false and no disk cache exists, `FindReferencers()` triggers a synchronous `RebuildReferenceCache()`
3. **Viewer open**: The widget calls `RebuildReferenceCacheAsync()` for a non-blocking incremental scan
4. **Completion**: After async scan finishes, `SaveCacheToDisk()` (line ~1350) persists results for next session

### Widget Data Flow

All widget functions below live in `SSGDynamicTextAssetReferenceViewer.cpp`. When the viewer opens or switches to a new asset:

```
OpenViewer(Id, "WP_Sword")
  |
  If GLOBAL_REFERENCE_VIEWER_TAB is valid:
    Reuse existing tab, call SetDynamicTextAsset(Id, "WP_Sword")
  Else:
    Create new SDockTab with SSGDynamicTextAssetReferenceViewer
  |
  v
RefreshReferences()
  |
  +---> Subsystem->FindReferencers(Id) -> AllReferencerItems
  |       If cache not ready: shows loading overlay, starts async scan
  |
  +---> Subsystem->FindDependencies(Id) -> AllDependencyItems
  |       Always computed fresh from the .dta.json file
  |
  v
ApplyReferencersFilter() / ApplyDependenciesFilter()
  |
  Text match against: SourceDisplayName, PropertyPath, SourceAsset path
  Dependencies are sorted: DTA refs first, then asset refs, alphabetical within each group
  |
  v
FilteredReferencerItems / FilteredDependencyItems -> SListView updates
```

**Double-click navigation:**

| Entry Type | Action |
|------------|--------|
| Referencer (Blueprint) | Opens Blueprint Editor via `GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()` |
| Referencer (Level) | Opens Level via `FEditorFileUtils::LoadMap()` |
| Referencer (DynamicTextAsset) | Opens DTA Editor via `FSGDynamicTextAssetEditorToolkit::OpenEditor()` |
| Dependency (DynamicTextAsset) | Opens DTA Editor for the dependency |
| Dependency (Asset) | Syncs Content Browser to the asset path |

### Tab Management

The reference viewer uses a global singleton tab pattern via static weak pointers:

```cpp
static TWeakPtr<SDockTab> GLOBAL_REFERENCE_VIEWER_TAB;
static TWeakPtr<SSGDynamicTextAssetReferenceViewer> GLOBAL_OPEN_REFERENCE_VIEWER;
```

Only one reference viewer tab exists at a time. Calling `OpenViewer()` with a different asset updates the existing viewer in-place via `SetDynamicTextAsset()` rather than creating a duplicate tab. The weak pointers ensure proper cleanup if the tab is closed by the user.

[Back to Table of Contents](../TableOfContents.md)
