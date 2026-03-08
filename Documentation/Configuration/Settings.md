# Settings

[Back to Table of Contents](../TableOfContents.md)

## USGDynamicTextAssetSettingsAsset

A `UDataAsset` containing runtime configuration for the plugin. Using a Data Asset instead of INI files prevents players from modifying settings in shipped builds.

### Creating the Asset

1. In the Content Browser, right click > Miscellaneous > Data Asset.
2. Select `SGDynamicTextAssetSettingsAsset` as the class.
3. Configure the properties and save.
4. Assign it in Project Settings > Game > SG Dynamic Text Assets.

### Properties

| Property | Type | Default | Description                                                                                      |
|----------|------|---------|--------------------------------------------------------------------------------------------------|
| `bEnableServerOverride` | `uint8 : 1` | `false` | Enable server side data override for live games                                                  |
| `bDeleteLocalOnServerMismatch` | `uint8 : 1` | `false` | Delete local files when server reports object as deleted                                         |
| `ServerRequestTimeoutSeconds` | `float` | `5.0` | Timeout for server requests (clamped 1-60 seconds)                                               |
| `ServerCacheFilename` | `FString` | `"SGDynamicTextAssetServerCache"` | SaveGame slot name for the server cache                                                          |
| `DefaultCompressionMethod` | `ESGDynamicTextAssetCompressionMethod` | `Zlib` | Compression method for binary cooking                                                            |
| `CustomCompressionName` | `FName` | `NAME_None` | FName of a custom compression format. Only editable when `DefaultCompressionMethod` is `Custom`. |
| `bStripUserFacingIdFromCookedManifest` | `uint8 : 1` | `false` | When enabled, UserFacingId is omitted from the cook manifest. Reduces manifest size by removing debug IDs. |
| `bCleanCookedDirectoryBeforeCook` | `uint8 : 1` | `true` | Deletes all files in the cooked directory before cooking. Prevents stale `.dta.bin` files from deleted assets. |
| `bDeleteCookedAssetsAfterPackaging` | `uint8 : 1` | `true` | Deletes cooked `.dta.bin` files after packaging completes. Prevents generated files from cluttering the Content Browser. |

### Compression Methods

| Method | Description                                                                                                                      |
|--------|----------------------------------------------------------------------------------------------------------------------------------|
| **None** | No compression. Fastest loading, largest files.                                                                                  |
| **Zlib** | Default. Good balance of compression ratio and speed.                                                                            |
| **Gzip** | Slightly better ratio, slower decompression.                                                                                     |
| **LZ4** | Very fast decompression, moderate ratio. Best for load time sensitive projects.                                                  |
| **Custom** | Uses a named compression format registered with Unreal's `FCompression` system. Set `CustomCompressionName` to the format FName. |

### Custom Compression

The `CustomCompressionName` property uses `EditCondition` metadata to only be editable when `Custom` is selected:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking",
    meta = (EditCondition = "DefaultCompressionMethod == ESGDynamicTextAssetCompressionMethod::Custom"))
FName CustomCompressionName = NAME_None;
```

The settings asset provides a virtual method for resolving the custom compression name:

```cpp
virtual FName GetCustomCompressionName() const;
```

Override this in a `USGDynamicTextAssetSettingsAsset` subclass for dynamic resolution (e.g., selecting compression based on platform or build configuration).

## USGDynamicTextAssetSettings

A `UDeveloperSettings` subclass that appears in Project Settings > Game > SG Dynamic Text Assets. It holds a soft reference to the `USGDynamicTextAssetSettingsAsset`.

### Accessing Settings

```cpp
// Get the settings asset (loads it if necessary, returns nullptr if not configured)
USGDynamicTextAssetSettingsAsset* settings = USGDynamicTextAssetSettings::GetSettings();

if (settings)
{
    float timeout = settings->GetServerRequestTimeoutSeconds();
    bool bServerEnabled = settings->IsServerOverrideEnabled();
    FString cacheFilename = settings->GetServerCacheFilename();
    bool bDeleteLocal = settings->ShouldDeleteLocalOnServerMismatch();
    ESGDynamicTextAssetCompressionMethod compression = settings->GetDefaultCompressionMethod();
    FName customCompression = settings->GetCustomCompressionName();
    bool bStripIds = settings->ShouldStripUserFacingIdFromCookedManifest();
}
```

`GetSettings()` returns `nullptr` if no settings asset has been assigned. Call sites should null check before accessing properties. Property defaults on the asset itself ensure sensible values when configured.

## USGDynamicTextAssetEditorSettings

A separate `UDeveloperSettings` for editor specific configuration. Accessible in Project Settings > Plugins > SG Dynamic Text Assets Editor.

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bScanEngineContent` | `bool` | `false` | Whether to scan Engine content when finding references |
| `bScanPluginContent` | `bool` | `true` | Whether to scan Plugin content when finding references |
| `ReferenceCacheFolderPath` | `FString` | `"SGDynamicTextAssets/ReferenceCache"` | Folder for reference cache files |

These settings affect the Reference Viewer's scanning behavior. Enabling Engine content scanning increases scan time but finds references in engine level assets.

```cpp
USGDynamicTextAssetEditorSettings* editorSettings = USGDynamicTextAssetEditorSettings::Get();
```

See [Reference Viewer](../Editor/ReferenceViewer.md) for how these settings affect scanning.

## Build and Packaging Settings

### Post-Package Cleanup

The `bDeleteCookedAssetsAfterPackaging` setting controls whether cooked binary files are deleted after pak file staging:

```ini
; DefaultGame.ini
[SGDynamicTextAssets]
bDeleteCookedAssetsAfterPackaging=true
```

When enabled (default), the UAT handler `SGDynamicTextAssetsPostPackageCleanup` deletes `.dta.bin` files, `dta_manifest.bin`, and `_TypeManifests/` contents after they have been staged into the pak. See [Cook Pipeline](../Serialization/CookPipeline.md) for details.

## Source Control Guidance

### Recommended .gitignore Entries

```gitignore
# SGDynamicTextAssets generated files
Content/SGDynamicTextAssetsCooked/
```

The `Content/SGDynamicTextAssetsCooked/` directory contains generated binary artifacts that should not be committed to source control. The `_TypeManifests/` directory within `Content/SGDynamicTextAssets/` contains auto-generated manifest files that **should** be committed, as they provide stable TypeId mappings across team members.

[Back to Table of Contents](../TableOfContents.md)
