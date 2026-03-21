# Cook Pipeline

[Back to Table of Contents](../TableOfContents.md)

## Overview

The cook pipeline converts `.dta.json` source files into flat GUID-named `.dta.bin` binary files plus a `dta_manifest.bin` for packaged builds. This eliminates the hierarchical directory structure and enables O(1) GUID-based lookups at runtime.

### Before (Source)

```
Content/SGDynamicTextAssets/
  WeaponData/
    excalibur.dta.json
    iron_sword.dta.json
  QuestData/
    main_quest_001.dta.json
```

### After (Cooked)

```
Content/SGDynamicTextAssetsCooked/
  A1B2C3D4-E5F6-7890-ABCD-EF1234567890.dta.bin
  B2C3D4E5-F6A7-8901-BCDE-F12345678901.dta.bin
  C3D4E5F6-A7B8-9012-CDEF-123456789012.dta.bin
  dta_manifest.bin
```

## Cook Triggers

### 1. Automatic (Editor Packaging)

When you use File > Package Project or BuildCookRun, the editor module's cook delegate fires automatically:

- The editor module (`FSGDynamicTextAssetsEditorModule`) subscribes to `UE::Cook::FDelegates::CookStarted`.
- It calls `FSGDynamicTextAssetCookUtils::CookAllDynamicTextAssets()`.
- Results and errors are logged as warnings (does not fail the overall cook).

### 2. Manual (Commandlet)

For CI/CD pipelines, use the `USGDynamicTextAssetCookCommandlet`:

```bash
# Cook all dynamic text assets
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook

# Validate only (no binary output)
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -validate

# Cook to custom output directory
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -output=Saved/Cooked/DynamicTextAssets

# Cook only a specific class
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -class=UWeaponData

# Clean cooked directory without cooking (CI/CD cleanup step)
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -clean

# Cook without pre-clean (iterative build machine reuse)
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -noclean
```

**Return codes:** 0 = success, 1 = failure.

## FSGDynamicTextAssetCookUtils

Shared utility class used by both the cook delegate and commandlet:

```cpp
// Clean the cooked output directory (delete all .dta.bin files and dta_manifest.bin)
static bool CleanCookedDirectory();

// Validate a single file
static bool ValidateDynamicTextAssetFile(const FString& FilePath, TArray<FString>& OutErrors);

// Cook a single file to binary + manifest entry
static bool CookDynamicTextAssetFile(const FString& JsonFilePath, const FString& OutputDir,
    ESGDynamicTextAssetCompressionMethod Compression, FSGDynamicTextAssetCookManifest& OutManifest);

// Cook all dynamic text asset files (bSkipPreClean=true skips pre-cook directory cleaning)
static bool CookAllDynamicTextAssets(const FString& OutputDir,
    const FString& ClassFilter, TArray<FString>& OutErrors, bool bSkipPreClean = false);

// Default output path
static FString GetCookedOutputRootPath();
// Returns: Content/SGDynamicTextAssetsCooked/
```

## Cook Process

For each `.dta.json` file:

1. **Validate**: Check JSON structure, GUID, class name, and version.
2. **Read**: Load the JSON content.
3. **Compress**: Convert to binary format using the configured compression method.
4. **Write**: Save as `{GUID}.dta.bin` in the output directory.
5. **Record**: Add an entry to the cook manifest (GUID, ClassName, UserFacingId).

After all files are processed, the manifest is written as `dta_manifest.bin` with an integrity hash.

## Pak Inclusion

The cooked output directory is registered with `DirectoriesToAlwaysStageAsUFS` in `UProjectPackagingSettings` to ensure it is included in the pak file during packaging. This registration happens automatically when the plugin's editor module starts up.

### Verifying Pak Inclusion

To confirm the cooked directory is being staged correctly:

1. Open **Project Settings > Packaging > Packaging** section.
2. Look for `DirectoriesToAlwaysStageAsUFS` (under "Advanced").
3. Verify that `SGDynamicTextAssetsCooked` appears in the list.

The editor module adds this entry on startup via `GetMutableDefault<UProjectPackagingSettings>()` and calls `TryUpdateDefaultConfigFile()` to persist the change to `DefaultGame.ini`.

### Troubleshooting: Cooked Directory Not Registered

If `SGDynamicTextAssetsCooked` is missing from the staging list:

1. **Check that the plugin is enabled.** The registration only runs during `FSGDynamicTextAssetsEditorModule::StartupModule()`.
2. **Check DefaultGame.ini.** Look for the entry under `[/Script/UnrealEd.ProjectPackagingSettings]`. If it is missing, you can add it manually in `DefaultGame.ini`:
   ```ini
   [/Script/UnrealEd.ProjectPackagingSettings]
   +DirectoriesToAlwaysStageAsUFS=(Path="SGDynamicTextAssetsCooked")
   ```
3. **Restart the editor** after enabling the plugin. The staging directory registration happens at module startup, not dynamically.
4. **Verify the cooked directory exists.** The directory `Content/SGDynamicTextAssetsCooked/` must contain cooked `.dta.bin` files and `dta_manifest.bin` before packaging. Run the cook commandlet or trigger a cook via File > Package Project to populate it.

If the directory is registered but files are still missing from the packaged build, confirm that the cook delegate or commandlet ran successfully by checking the Output Log for `LogSGDynamicTextAssetsEditor` messages during the cook.

## Soft Reference Cook Inclusion

### Problem

Dynamic Text Assets are external JSON files  - they are not UAssets in the Unreal asset registry. When a DTA has a `TSoftObjectPtr` or `TSoftClassPtr` property referencing a regular Unreal asset (mesh, material, texture, blueprint, etc.), the cooker cannot follow that reference the way it does for normal UObjects. If nothing else in the project hard-references that asset, the cooker excludes it from the packaged build. At runtime, the soft reference resolves to `nullptr`.

### Solution

The plugin subscribes to `UE::Cook::FDelegates::ModifyCook`, a multicast delegate that fires during `CollectFilesToCook`. This allows the plugin to declare additional packages the cooker should include.

**Flow:**

1. Cook starts → `CollectFilesToCook` → `ModifyCook` fires
2. Plugin callback calls `FSGDynamicTextAssetCookUtils::GatherSoftReferencesFromAllFiles()`
3. For each DTA file on disk:
   - Deserialize into a transient UObject using the existing serializer pipeline
   - Recursively walk all UProperties to find soft reference values
   - Collect the package name from each valid soft reference path
4. For each unique package name, add a `FPackageCookRule` with `EPackageCookRule::AddToCook`
5. The cooker now includes those packages in the build

### What Gets Gathered

The property walker finds soft references in:

- `TSoftObjectPtr<T>` / `FSoftObjectPath` properties
- `TSoftClassPtr<T>` / `FSoftClassPath` properties
- Nested inside `USTRUCT` properties (recursive)
- Elements of `TArray` properties
- Keys and values of `TMap` properties

### What Gets Skipped

- Empty/invalid soft reference paths
- `/Script/` paths (native C++ classes are always available at runtime)
- Duplicate package names (deduplicated via `TSet<FName>`)

### Troubleshooting

**Check cook log for added packages:**

Look for the `SGDynamicTextAssets` instigator in the Output Log during a cook:

```
LogSGDynamicTextAssetsEditor: Added N soft-referenced packages to cook from DTA files
```

Where `N` is the number of unique packages added.

**Verify a specific asset was included:**

If a soft-referenced asset is still missing from a packaged build:

1. Confirm the DTA file exists on disk and contains the soft reference path
2. Check that the soft reference path is valid (e.g., `/Game/Weapons/Sword.Sword`)
3. Verify the DTA's class is registered and can be deserialized
4. Look for warnings in the cook log from `GatherSoftReferencesFromAllFiles`

## Source Control

The `Content/SGDynamicTextAssetsCooked/` directory contains generated binary files that should NOT be committed to source control. These are build artifacts regenerated each time you package the project.

Add the following to your project's `.gitignore`:

```
Content/SGDynamicTextAssetsCooked/
```

The plugin displays an editor warning notification on startup if cooked `.dta.bin` files are detected in source control. This helps catch cases where the directory was accidentally committed.

## Cooked Directory Lifecycle

The cooked output directory (`Content/SGDynamicTextAssetsCooked/`) contains generated binary artifacts. Without lifecycle management, stale `.dta.bin` files from deleted dynamic text assets can survive across cooks and be packaged into builds.

### Pre-Cook Cleaning

Before the cook pass begins, `CookAllDynamicTextAssets()` calls `CleanCookedDirectory()` to delete all existing `.dta.bin` files and `dta_manifest.bin`. This ensures only freshly cooked files from the current source set are present.

- **Setting:** `bCleanCookedDirectoryBeforeCook` (default: `true`). Disable for build machines reusing cached output.
- **Commandlet:** `-noclean` flag skips pre-cook cleaning regardless of the setting.
- **Commandlet:** `-clean` flag performs standalone cleanup without cooking.

### Post-Package Cleanup

After the packaging process completes (after pak file staging), cooked directory contents are automatically deleted. This is handled by a UAT (Unreal Automation Tool) plugin using `CustomDeploymentHandler.PostPackage()`, which fires after `Platform.Package()` completes, ensuring cooked files are safely staged into the pak before deletion.

**Implementation:** `SGDynamicTextAssetsPostPackageCleanup` (C# class in `Plugins/SGDynamicTextAssets/Build/`), discovered automatically by UAT via the plugin's `.automation.csproj` project file.

**Configuration:**
- **INI:** `CustomDeployment=SGDynamicTextAssetsCleanup` in `DefaultEngine.ini` under `[/Script/WindowsTargetPlatform.WindowsTargetSettings]`
- **Setting:** `bDeleteCookedAssetsAfterPackaging` in `DefaultGame.ini` under `[SGDynamicTextAssets]` (default: `true`). Set to `false` to disable post-package cleanup.

**Note:** The `CustomDeploymentHandler` mechanism currently supports one handler per platform. If the project requires an additional custom deployment handler, a composite handler pattern would be needed to chain both handlers.

## See Also

- [Binary Format](BinaryFormat.md): Details of the `.dta.bin` file format.
- [Cook Manifest](CookManifest.md): Details of the `dta_manifest.bin` format and runtime usage.
- [Commandlets](../Reference/Commandlets.md): Full commandlet reference.

## Post-Package Cleanup Details

The UAT handler `SGDynamicTextAssetsPostPackageCleanup` is a C# class located in `Plugins/SGDynamicTextAssets/Build/`. It is discovered automatically by UAT via the plugin's `.automation.csproj` project file.

### What Gets Deleted

After pak staging, the handler deletes:
- All `.dta.bin` files in the cooked directory
- The `dta_manifest.bin` cook manifest
- Contents of the `_TypeManifests/` cooked subdirectory

### Configuration

| Setting | Location | Default | Description |
|---------|----------|---------|-------------|
| `CustomDeployment` | `DefaultEngine.ini` under platform target settings | - | Set to `SGDynamicTextAssetsCleanup` to activate the handler |
| `bDeleteCookedAssetsAfterPackaging` | `DefaultGame.ini` under `[SGDynamicTextAssets]` | `true` | Set to `false` to disable cleanup |

### Platform Support

The `CustomDeploymentHandler` mechanism currently supports one handler per platform target configuration. If the project requires an additional custom deployment handler alongside this one, a composite handler pattern would be needed to chain both handlers.

The handler includes read-only file attribute handling for C# `File.Delete()` compatibility on Windows.

[Back to Table of Contents](../TableOfContents.md)
