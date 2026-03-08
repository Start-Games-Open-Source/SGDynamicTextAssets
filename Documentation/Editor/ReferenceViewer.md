# Reference Viewer

[Back to Table of Contents](../TableOfContents.md)

## Overview

The Reference Viewer (`SSGDynamicTextAssetReferenceViewer`) displays what references a dynamic text asset (referencers) and what it depends on (dependencies). It helps track cross references and understand the dependency graph.

@TODO Add image.

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

[Back to Table of Contents](../TableOfContents.md)
