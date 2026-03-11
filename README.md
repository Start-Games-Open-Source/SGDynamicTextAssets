# SG Dynamic Text Assets Plugin

Dynamic text asset configuration system for static game data in Unreal Engine. Provides external file storage similar to Primary Data Assets with runtime loading, editor tooling, server integration, and a binary cook pipeline for packaged builds.

Example Project: https://github.com/Start-Games-Open-Source/Example_SGDTA

To provide feedback or see whats in development, visit the [Issues Page](https://github.com/Start-Games-Open-Source/SGDynamicTextAssets/issues) of the repo!

Full documentation is in the [Documentation](Documentation/TableOfContents.md) folder and is included with the plugin.

# License

SGDynamicTextAssets is released under the **[Elastic License 2.0 (ELv2)](./LICENSE)** by **Start Games Open Source**.

This license is designed to keep the plugin free and open for developers while protecting it from being resold or repackaged as a commercial product.

---

## What You Can and Can't Do

| Use Case | Allowed? |
|---|---|
| Use the plugin in your game (free or commercial) | ✅ Yes |
| Modify the plugin for your own project | ✅ Yes |
| Fork the plugin into a private repository | ✅ Yes |
| Keep your game's source code private | ✅ Yes |
| Distribute the plugin as part of an open source project | ✅ Yes |
| Sell the plugin itself on a marketplace (e.g. Fab) | ❌ No |
| Repackage and sell it as your own product or tool | ❌ No |
| Offer it as a paid hosted or managed service | ❌ No |
| Remove or alter the Start Games credit/copyright notices | ❌ No |

---

## Simple Summary

> **Free to use. Free to modify. Free to ship in your game. Just don't sell it.**

The plugin will always be free. You can build on top of it, fork it privately, and ship games with it with no royalties, no fees. The only restriction is that you cannot take this plugin (or a modified version of it) and sell it as a standalone product or service.

Credit to **Start Games Open Source** must remain intact in any copy or distribution of the plugin.

*This summary is for convenience only and does not replace the full [LICENSE](./LICENSE) text.*

---

## Key Features

- **Text files as source of truth**: Human-readable JSON (`.dta.json`), XML (`.dta.xml`), or YAML (`.dta.yaml`) files
- **Stable GUID identity**: Every asset has an `FSGDynamicTextAssetId` that never changes, surviving renames, moves, and refactors
- **Runtime immutability**: Assets are read-only at runtime, simplifying caching and data flow
- **Interface-driven design**: `ISGDynamicTextAssetProvider` defines the core contract; `USGDynamicTextAsset` is the default base class
- **Soft references only**: `FSGDynamicTextAssetRef` stores a GUID, resolved lazily at runtime. Hard references are prohibited at compile time by a UHT validator
- **Semantic versioning**: Major.Minor.Patch versioning with automatic migration on breaking changes
- **Editor tooling**: Browser, inline editor, reference viewer, property customizations, source control integration
- **Server integration**: Optional backend override layer for live data (hot-fixes, A/B testing)
- **Binary cook pipeline**: Compresses assets to `.dta.bin` with a binary manifest (`dta_manifest.bin`) for O(1) lookups in packaged builds
- **Automatic cook inclusion**: Soft-referenced assets in DTAs are automatically included in packaged builds via `ModifyCook` delegate
- **Blueprint support**: `USGDynamicTextAssetStatics` function library exposes common operations to Blueprints
- **Validation framework**: File-level, data-level, and reference validation with custom hooks

## Quick Start

### 1. Enable the Plugin

Add to your `.uproject`:

```json
{
  "Plugins": [
    {
      "Name": "SGDynamicTextAssets",
      "Enabled": true
    }
  ]
}
```

Add the runtime module dependency in your game module's `.Build.cs`:

```csharp
PublicDependencyModuleNames.Add("SGDynamicTextAssetsRuntime");
```

### 2. Create a Dynamic Text Asset Subclass

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAsset.h"
#include "WeaponData.generated.h"

UCLASS(BlueprintType)
class MYGAME_API UWeaponData : public USGDynamicTextAsset
{
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float BaseDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float AttackSpeed = 1.0f;
};
```

### 3. Register Root Classes

If your class is a new root type (direct child of `USGDynamicTextAsset`), register it during module startup. Child classes of already-registered types are discovered automatically via reflection.

```cpp
#include "Management/SGDynamicTextAssetRegistry.h"
#include "WeaponData.h"

void FMyGameModule::StartupModule()
{
    USGDynamicTextAssetRegistry::GetChecked().RegisterDynamicTextAssetClass(UWeaponData::StaticClass());
}
```

### 4. Create in the Browser

Open the Dynamic Text Asset Browser from the Window menu. Select your class in the type tree, click **New**, enter a UserFacingId (e.g., `"excalibur"`), and a `.dta.json` file is created at `Content/SGDynamicTextAssets/WeaponData/excalibur.dta.json`. Double-click to edit properties.

### 5. Load at Runtime

```cpp
USGDynamicTextAssetSubsystem* Subsystem =
    GetGameInstance()->GetSubsystem<USGDynamicTextAssetSubsystem>();

FSGDynamicTextAssetId WeaponId = FSGDynamicTextAssetId::FromString(TEXT("550e8400-e29b-41d4-a716-446655440000"));
FString FilePath;

// Synchronous load
Subsystem->GetOrLoadDynamicTextAsset(WeaponId, FilePath, UWeaponData::StaticClass());
UWeaponData* Weapon = Subsystem->GetDynamicTextAsset<UWeaponData>(WeaponId);

// Asynchronous load
Subsystem->LoadDynamicTextAssetAsync(WeaponId,
    UWeaponData::StaticClass(),
    FOnDynamicTextAssetLoaded::CreateLambda(
        [](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        if (bSuccess)
        {
            UWeaponData* Weapon = Cast<UWeaponData>(Provider.GetObject());
        }
    }));
```

### 6. Reference from Other Assets

```cpp
UPROPERTY(EditAnywhere, meta = (DynamicTextAssetType = "UWeaponData"))
FSGDynamicTextAssetRef WeaponRef;

// Resolve at runtime
if (UWeaponData* Weapon = WeaponRef.Get<UWeaponData>(GetGameInstance()))
{
    float damage = Weapon->BaseDamage;
}
```

See the full [Quick Start Guide](Documentation/QuickStartGuide.md) for details.

## Architecture Overview

The plugin is split into two modules:

- **SGDynamicTextAssetsRuntime** (`Runtime`, `PreDefault`) — Core types, serializers, subsystem, file management, server interface. Ships in packaged builds.
- **SGDynamicTextAssetsEditor** (`Editor`, `Default`) — Browser, editor, reference viewer, property customizations, cook pipeline, source control. Editor-only.

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
|  Cook Pipeline & Utilities    |       |  USGDynamicTextAssetSubsystem |
|  Cook/Validation Commandlets  |       |  USGDynamicTextAssetRegistry  |
|  Editor Settings              |       |  FSGDynamicTextAssetFileManager|
+-------------------------------+       |  JSON Serializer (TypeId=1)   |
                                        |  XML Serializer  (TypeId=2)   |
                                        |  YAML Serializer (TypeId=3)   |
                                        |  Binary Serializer            |
                                        |  Cook Manifest                |
                                        |  Type Manifest                |
                                        |  Settings                     |
                                        |  Statics (BP)                 |
                                        |  Server Interface             |
                                        |  Server Cache                 |
                                        +-------------------------------+
```

### Design Principles

- **Text files as source of truth**: JSON/XML/YAML files are authoritative during development
- **GUID identity**: `FSGDynamicTextAssetId` is permanent; `UserFacingId` is human-readable and renameable
- **Runtime immutability**: Read-only at runtime, editable only in the editor
- **Soft references only**: No hard asset references; compile-time enforcement via UHT validator
- **Semantic versioning**: Major.Minor.Patch with built-in migration for breaking changes

## Plugin Structure

```
SGDynamicTextAssets/
  SGDynamicTextAssets.uplugin
  Documentation/                              # Comprehensive docs (35 pages)
  Source/
    SGDynamicTextAssetsRuntime/               # Runtime module
      Public/
        Core/                                 # Provider interface, base class, identity, refs, versioning
        Subsystem/                            # GameInstanceSubsystem for loading/caching
        Management/                           # Registry, file manager, cook manifest, type manifest
        Serialization/                        # JSON, XML, YAML, binary serializers
        Server/                               # Server interface, cache, null interface
        Statics/                              # Blueprint function library
        Settings/                             # Runtime + editor settings
      Private/
    SGDynamicTextAssetsEditor/                # Editor module
      Public/
        Browser/                              # Browser, type tree, tile view, dialogs
        Commandlets/                          # Cook and validation commandlets
        Customization/                        # Property customizations
        Editor/                               # Editor toolkit, raw view
        ReferenceViewer/                      # Reference viewer
        Settings/                             # Editor-specific settings
        Utilities/                            # Cook utils, source control, editor helpers
        Widgets/                              # Slate widget components (class picker)
      Private/
        Tests/                                # Automation tests
  Build/
    SGDynamicTextAssetsPostPackageCleanup.cs  # UAT post-package cleanup
  SGDynamicTextAssetsUhtPlugin/               # Compile-time hard reference validator
```

See [File Structure](Documentation/Reference/FileStructure.md) for the complete directory layout.

## Supported Formats

| Format | Extension | Description | Details |
|--------|-----------|-------------|---------|
| JSON | `.dta.json` | Default format, human-readable | [JSON Format](Documentation/Serialization/JsonFormat.md) |
| XML | `.dta.xml` | Via Unreal's `FXmlFile` | [XML Format](Documentation/Serialization/XmlFormat.md) |
| YAML | `.dta.yaml` | Via fkYAML third-party library | [YAML Format](Documentation/Serialization/YamlFormat.md) |
| Binary | `.dta.bin` | Compressed format for cooked builds | [Binary Format](Documentation/Serialization/BinaryFormat.md) |

Custom serialization formats can be added by implementing `ISGDynamicTextAssetSerializer` and registering during module startup. See [Serializer Interface](Documentation/Serialization/SerializerInterface.md).

## Cook Pipeline

The cook pipeline converts text source files to compressed `.dta.bin` binaries with a binary manifest (`dta_manifest.bin`) for packaged builds. Cooking is triggered automatically during editor packaging or manually via commandlet:

```bash
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook
```

The cooked directory (`Content/SGDynamicTextAssetsCooked/`) is a generated artifact and should be added to `.gitignore`.

Soft-referenced assets (`TSoftObjectPtr`, `TSoftClassPtr`) in DTAs are automatically included in cooked builds via the `UE::Cook::FDelegates::ModifyCook` delegate. This ensures assets referenced only by DTAs are not silently excluded from packaged builds.

See [Cook Pipeline](Documentation/Serialization/CookPipeline.md) and [Cook Manifest](Documentation/Serialization/CookManifest.md) for details. See [Commandlets](Documentation/Reference/Commandlets.md) for full CLI reference.

## Terminology

| Term | Description |
|------|-------------|
| **Dynamic Text Asset** | Any `UObject` implementing `ISGDynamicTextAssetProvider`. `USGDynamicTextAsset` is the default base class. |
| **FSGDynamicTextAssetId** | Wrapper around `FGuid` that uniquely identifies a dynamic text asset. Assigned at creation, never changes. |
| **UserFacingId** | Human-readable string identifier (e.g., `"excalibur"`) used as the filename and display name. Can be renamed. |
| **FSGDynamicTextAssetRef** | Lightweight struct that references a dynamic text asset by `FSGDynamicTextAssetId`, resolved lazily at runtime. |
| **`.dta.json` / `.dta.xml` / `.dta.yaml`** | Text source file formats for development. |
| **`.dta.bin`** | Compressed binary format for cooked/packaged builds. |
| **Cook Manifest** | Binary manifest (`dta_manifest.bin`) mapping GUIDs to class names and user-facing IDs in cooked builds. |

## Documentation

Full documentation is in the [Documentation](Documentation/TableOfContents.md) folder:

**Getting Started**
- [Overview](Documentation/Overview.md): What the plugin does and core design principles
- [Quick Start Guide](Documentation/QuickStartGuide.md): Step-by-step setup and first dynamic text asset

**Core Concepts**
- [Dynamic Text Assets](Documentation/Core/DynamicTextAssets.md): Base class, GUIDs, UserFacingId, versioning
- [Dynamic Text Asset References](Documentation/Core/DynamicTextAssetReferences.md): FSGDynamicTextAssetRef, resolving, type filters
- [Versioning and Migration](Documentation/Core/VersioningAndMigration.md): Semantic versioning, migration hooks
- [Validation](Documentation/Core/Validation.md): Validation framework, custom hooks, severity levels
- [UHT Plugin](Documentation/Core/UhtPlugin.md): Compile-time hard reference validation

**Runtime API**
- [Subsystem](Documentation/Runtime/Subsystem.md): Loading, caching, async operations
- [Registry](Documentation/Runtime/Registry.md): Class registration and discovery
- [File Manager](Documentation/Runtime/FileManager.md): File paths, scanning, I/O, serializer registry
- [Blueprint Statics](Documentation/Runtime/BlueprintStatics.md): Blueprint function library

**Editor Tools**
- [Browser](Documentation/Editor/DynamicTextAssetBrowser.md): Browsing, creating, duplicating, renaming, deleting
- [Editor](Documentation/Editor/DynamicTextAssetEditor.md): Property editing, saving, reverting, Raw View
- [Reference Viewer](Documentation/Editor/ReferenceViewer.md): Viewing referencers and dependencies
- [Property Customizations](Documentation/Editor/PropertyCustomizations.md): FSGDynamicTextAssetRef picker, identity display
- [Source Control](Documentation/Editor/SourceControl.md): Auto-checkout, mark for add/delete

**Serialization and Cooking**
- [Serializer Interface](Documentation/Serialization/SerializerInterface.md): ISGDynamicTextAssetSerializer contract
- [JSON Format](Documentation/Serialization/JsonFormat.md): `.dta.json` schema
- [XML Format](Documentation/Serialization/XmlFormat.md): `.dta.xml` schema
- [YAML Format](Documentation/Serialization/YamlFormat.md): `.dta.yaml` schema
- [Binary Format](Documentation/Serialization/BinaryFormat.md): `.dta.bin` header and compression
- [Cook Pipeline](Documentation/Serialization/CookPipeline.md): Cook commandlet, cook delegate, manifest
- [Cook Manifest](Documentation/Serialization/CookManifest.md): Binary manifest format

**Server Integration**
- [Server Interface](Documentation/Server/ServerInterface.md): Implementing a backend
- [Server Cache](Documentation/Server/ServerCache.md): Persistent caching

**Configuration**
- [Settings](Documentation/Configuration/Settings.md): Runtime and editor settings

**Testing**
- [Automated Tests](Documentation/Testing/AutomatedTests.md): Test coverage and running tests

**Advanced Topics**
- [Type Manifest System](Documentation/Advanced/TypeManifestSystem.md): FSGDynamicTextAssetTypeId, type manifests, server type overrides
- [Error Handling and Debugging](Documentation/Advanced/ErrorHandlingAndDebugging.md): ESGLoadResult codes, log categories, debugging workflows
- [Async Loading and Threading](Documentation/Advanced/AsyncLoadingAndThreading.md): Threading model, async pipeline, cache architecture
- [Custom Serializer Guide](Documentation/Advanced/CustomSerializerGuide.md): Implementing custom formats, serializer registration

**Reference**
- [Class Reference](Documentation/Reference/ClassReference.md): All classes, structs, and enums
- [File Structure](Documentation/Reference/FileStructure.md): Plugin directory layout
- [Commandlets](Documentation/Reference/Commandlets.md): CLI usage

---

Copyright Start Games, Inc. All Rights Reserved.
