# Overview

[Back to Table of Contents](TableOfContents.md)

## What is SGDynamicTextAssets?

SGDynamicTextAssets is an Unreal Engine plugin that provides a text-file-based data configuration system for static game data. It serves a similar role to Unreal's built-in Primary Data Assets, but stores data in external text files rather than `.uasset` binaries. This makes data human-readable, diffable in version control, and editable outside the Unreal Editor.

Dynamic text assets are ideal for game configuration that is defined by designers and consumed read-only at runtime: items, weapons, quests, dialogue, enemy stats, level metadata, and similar structured data.

## Why Not Primary Data Assets?

Primary Data Assets work well for many projects, but have limitations that SGDynamicTextAssets addresses:

| Concern | Primary Data Assets | SGDynamicTextAssets |
|---------|-------------------|---------------------|
| **File format** | Binary `.uasset` | Human-readable text (`.dta.json`, `.dta.xml`, `.dta.yaml`) |
| **Version control** | Binary diffs, merge conflicts | Text diffs, mergeable files |
| **External editing** | Requires Unreal Editor | Any text editor or tooling pipeline |
| **Identity** | Asset path (fragile) | Stable GUID (survives moves/renames) |
| **Server override** | Not built-in | Built-in server integration layer |
| **Packaged format** | Included in pak as uasset | Compressed `.dta.bin` with manifest |
| **Validation** | Manual | Built-in validation framework |
| **Migration** | Custom versioning | Semantic versioning with auto-migration |

## Core Design Principles

### Text Files as Source of Truth
All dynamic text assets are authored and stored as text files in the editor. The default format is JSON (`.dta.json`), with XML (`.dta.xml`) and YAML (`.dta.yaml`) also supported through the pluggable serializer architecture. Each file includes a metadata section (`type`, `version`, `id`, `userfacingid`) and a `data` section containing the serialized UPROPERTY fields. Binary `.dta.bin` files are produced only during cooking for packaged builds.

Custom serialization formats can be added by implementing the `ISGDynamicTextAssetSerializer` interface and registering the serializer during module startup. See [Serializer Interface](Serialization/SerializerInterface.md).

### GUID Identity
Every dynamic text asset has a unique `FSGDynamicTextAssetId` (a wrapper around `FGuid`) that never changes after creation. IDs are stable across renames, moves, and refactors. All references between assets use `FSGDynamicTextAssetId` rather than file paths, eliminating broken references when files are reorganized.

### Runtime Immutability
Dynamic text assets are read-only at runtime. They represent constant configuration data, not mutable game state. This simplifies caching, threading, and reasoning about data flow.

### Interface-Driven Design
The core contract is defined by the `ISGDynamicTextAssetProvider` interface. `USGDynamicTextAsset` is the default implementation, but any `UObject` that implements `ISGDynamicTextAssetProvider` can participate in the ecosystem. Registration guards prevent `AActor` and `UActorComponent` subclasses from being registered as providers.

### Soft References Only
Dynamic text assets use `FSGDynamicTextAssetRef` (a lightweight `FSGDynamicTextAssetId` wrapper) for cross-references. Hard reference UPROPERTYs (`TObjectPtr<>`, `TSubclassOf<>`) are prohibited at compile time by a UHT validator. Soft references (`TSoftObjectPtr`, `TSoftClassPtr`) can be tagged with named asset bundles for selective batch loading. Objects are loaded on-demand through the subsystem and resolved lazily.

### Separation of Concerns
The plugin is split into two modules:
- **SGDynamicTextAssetsRuntime** (`Runtime`, `PreDefault`): Core types, serialization, subsystem, file management. Ships in packaged builds.
- **SGDynamicTextAssetsEditor** (`Editor`, `Default`): Browser, editor, reference viewer, property customizations, cook pipeline, source control. Editor-only.

### Dual Versioning System

The plugin tracks two separate version numbers per file, each serving a distinct purpose:

**Data Version** (`version` field in metadata)
- Tracks asset schema changes (property renames, type changes, removed fields)
- Incremented by the asset class author in their `USGDynamicTextAsset` subclass
- Major version changes trigger `MigrateFromVersion()` on the asset class, which transforms old data JSON in-place before property deserialization
- See [Versioning and Migration](Core/VersioningAndMigration.md)

**File Format Version** (`fileFormatVersion` field in metadata)
- Tracks serializer format structural changes (key renames, block reorganization, encoding changes)
- Managed automatically by each serializer implementation
- Major version changes are detected on editor startup and prompt for batch migration via `MigrateFileFormat()` and `UpdateFileFormatVersion()` on the serializer
- Can also be validated and migrated via the `SGDynamicTextAssetFormatVersion` commandlet
- See [Serializer Interface](Serialization/SerializerInterface.md)

Both use `FSGDynamicTextAssetVersion` (Major.Minor.Patch). If the `fileFormatVersion` field is absent from a file, it defaults to `1.0.0`.

## Architecture

```
+-------------------------------+       +-------------------------------+
|   SGDynamicTextAssetsEditor   |       |   SGDynamicTextAssetsRuntime  |
|  (Editor module)              |       |  (Runtime module)             |
|                               |       |                               |
|  Browser (UI)                 |       |  ISGDynamicTextAssetProvider  |
|  Editor (UI)                  |       |  USGDynamicTextAsset (base)   |
|  Reference Viewer (UI)        |       |  FSGDynamicTextAssetId        |
|  Property Customizations      |       |  FSGDynamicTextAssetRef       |
|  Source Control Utils         |       |  FSGDynamicTextAssetVersion   |
|  Cook Pipeline                |       |  FSGDynamicTextAssetBundleData|
|  Cook Commandlet              |       |  USGDynamicTextAssetSubsystem |
|  Editor Settings              |       |  USGDynamicTextAssetRegistry  |
+-------------------------------+       |  FSGDynamicTextAssetFileManager|
                                        |  JSON Serializer (TypeId=1)   |
                                        |  XML Serializer  (TypeId=2)   |
                                        |  YAML Serializer (TypeId=3)   |
                                        |  Binary Serializer            |
                                        |  Cook Manifest                |
                                        |  Settings                     |
                                        |  Statics (BP)                 |
                                        |  Server Interface             |
                                        |  Server Cache                 |
                                        +-------------------------------+
```

## Key Terminology

| Term | Description |
|------|-------------|
| **Dynamic Text Asset** | Any `UObject` implementing `ISGDynamicTextAssetProvider`. `USGDynamicTextAsset` is the default base class. |
| **ISGDynamicTextAssetProvider** | The core C++ interface defining identity, versioning, validation, and migration |
| **FSGDynamicTextAssetId** | A wrapper struct around `FGuid` that uniquely and permanently identifies a dynamic text asset |
| **UserFacingId** | A human-readable string identifier (e.g., `"excalibur"`) used as the filename and display name |
| **GUID** | The raw `FGuid` inside an `FSGDynamicTextAssetId`. Assigned at creation, never changes. |
| **FSGDynamicTextAssetRef** | A lightweight struct that references a dynamic text asset by `FSGDynamicTextAssetId`, resolved lazily at runtime |
| **Asset Bundle** | A named group of soft references on a DTA, loaded selectively via `FStreamableManager`. Tagged with `meta=(AssetBundles="BundleName")`. |
| **Instanced Sub-object** | A polymorphic `UObject` owned by a DTA via `UPROPERTY(Instanced)`. Serialized inline with full class type information. |
| **`.dta.json`** | The default JSON source file format |
| **`.dta.xml`** | The XML source file format |
| **`.dta.yaml`** | The YAML source file format |
| **`.dta.bin`** | The compressed binary format produced during cooking for packaged builds |
| **Cook Manifest** | A manifest file that maps GUIDs to class names and user-facing IDs in cooked builds |
| **Registry** | The `USGDynamicTextAssetRegistry` singleton that tracks all registered dynamic text asset classes |
| **Subsystem** | The `USGDynamicTextAssetSubsystem` game instance subsystem that loads and caches dynamic text assets at runtime |

[Back to Table of Contents](TableOfContents.md)
