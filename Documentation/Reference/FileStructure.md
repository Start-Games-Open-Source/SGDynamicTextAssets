# File Structure

[Back to Table of Contents](../TableOfContents.md)

## Plugin Directory Layout

```
SGDynamicTextAssets/
  SGDynamicTextAssets.uplugin                # Plugin descriptor
  Documentation/                             # This documentation
  Resources/
    Icon128.png                              # Plugin icon
  Source/
    SGDynamicTextAssetsRuntime/              # Runtime module (ships with game)
      SGDynamicTextAssetsRuntime.Build.cs
      Public/
        SGDynamicTextAssetsRuntimeModule.h
        Core/
          ISGDynamicTextAssetProvider.h      # Abstract provider interface
          SGDynamicTextAsset.h               # Abstract base class
          SGDynamicTextAssetId.h             # GUID identity wrapper
          SGDynamicTextAssetRef.h            # GUID-based reference struct
          SGDynamicTextAssetTypeId.h         # Type identity struct (FSGAssetTypeId)
          SGDynamicTextAssetVersion.h        # Semantic versioning struct
          SGDynamicTextAssetValidationResult.h  # Validation entry and result structs
          SGDynamicTextAssetValidationUtils.h   # Validation utility functions
          SGDynamicTextAssetEnums.h          # ESGLoadResult, ESGCompressionMethod, ESGReferenceType
        Subsystem/
          SGDynamicTextAssetSubsystem.h      # GameInstanceSubsystem for loading/caching
        Management/
          SGDynamicTextAssetRegistry.h       # EngineSubsystem for class registration
          SGDynamicTextAssetFileManager.h    # Static file path and I/O utilities
          SGDynamicTextAssetFileMetadata.h   # File metadata extraction struct
          SGDynamicTextAssetCookManifest.h   # Cook manifest read/write
          SGDynamicTextAssetTypeManifest.h   # Type manifest for class hierarchy
        Serialization/
          SGDynamicTextAssetSerializer.h        # Serializer registry / factory
          SGDynamicTextAssetSerializerBase.h    # Base class for all serializers
          SGDynamicTextAssetJsonSerializer.h    # JSON serialization
          SGDynamicTextAssetBinarySerializer.h  # Binary compression/decompression
          SGDynamicTextAssetXmlSerializer.h     # XML serialization
          SGDynamicTextAssetYamlSerializer.h    # YAML serialization (fkYAML)
          SGDynamicTextAssetSerializerMetadata.h # Serializer metadata struct
          SGBinaryDynamicTextAssetHeader.h      # Binary format header struct
          SGBinaryEncodeParams.h                # Binary encoding parameters
        Server/
          ISGDynamicTextAssetServerInterface.h     # Abstract server interface
          SGDynamicTextAssetServerCache.h          # SaveGame-based server cache
          SGDynamicTextAssetServerNullInterface.h  # No-op server interface
        Statics/
          SGDynamicTextAssetStatics.h        # Blueprint function library
          SGDynamicTextAssetSlateStyles.h    # Shared Slate style definitions
        Settings/
          SGDynamicTextAssetSettings.h       # Runtime settings (asset + developer settings)
      Private/
        SGDynamicTextAssetsRuntimeModule.cpp
        Core/
          ISGDynamicTextAssetProvider.cpp
          SGDynamicTextAsset.cpp
          SGDynamicTextAssetId.cpp
          SGDynamicTextAssetRef.cpp
          SGDynamicTextAssetTypeId.cpp
          SGDynamicTextAssetVersion.cpp
          SGDynamicTextAssetValidationResult.cpp
          SGDynamicTextAssetValidationUtils.cpp
        Subsystem/
          SGDynamicTextAssetSubsystem.cpp
        Management/
          SGDynamicTextAssetRegistry.cpp
          SGDynamicTextAssetFileManager.cpp
          SGDynamicTextAssetCookManifest.cpp
          SGDynamicTextAssetTypeManifest.cpp
        Serialization/
          SGDynamicTextAssetSerializer.cpp
          SGDynamicTextAssetSerializerBase.cpp
          SGDynamicTextAssetJsonSerializer.cpp
          SGDynamicTextAssetBinarySerializer.cpp
          SGDynamicTextAssetXmlSerializer.cpp
          SGDynamicTextAssetYamlSerializer.cpp
          SGDynamicTextAssetSerializerMetadata.cpp
          SGBinaryDynamicTextAssetHeader.cpp
        Server/
          ISGDynamicTextAssetServerInterface.cpp
          SGDynamicTextAssetServerCache.cpp
          SGDynamicTextAssetServerNullInterface.cpp
        Statics/
          SGDynamicTextAssetStatics.cpp
          SGDynamicTextAssetSlateStyles.cpp
        Settings/
          SGDynamicTextAssetSettings.cpp

    SGDynamicTextAssetsEditor/               # Editor module (editor only)
      SGDynamicTextAssetsEditor.Build.cs
      Public/
        SGDynamicTextAssetsEditorModule.h
        Browser/
          SSGDynamicTextAssetBrowser.h       # Main browser window
          SSGDynamicTextAssetTypeTree.h      # Type hierarchy tree widget
          SSGDynamicTextAssetTileView.h      # Asset tile view widget
          SSGDynamicTextAssetCreateDialog.h  # Create dialog
          SSGDynamicTextAssetDuplicateDialog.h
          SSGDynamicTextAssetRenameDialog.h
          SGDynamicTextAssetBrowserCommands.h  # Browser UI commands
        Editor/
          FSGDynamicTextAssetEditorToolkit.h    # Asset editor toolkit
          SGDynamicTextAssetEditorProxy.h       # Editor proxy for asset editing
          SGDynamicTextAssetEditorCommands.h    # Editor UI commands
          SGDynamicTextAssetRefCustomization.h  # FSGDynamicTextAssetRef picker
          SGDynamicTextAssetIdentityCustomization.h  # GUID/Version display
          FSGDynamicTextAssetIdCustomization.h  # FSGDynamicTextAssetId customization
          SSGDynamicTextAssetIdentityBlock.h    # Identity block widget
          SSGDynamicTextAssetRawView.h          # Raw JSON/text view widget
          SSGDynamicTextAssetIcon.h             # Icon display widget
          SGAssetTypeIdCustomization.h          # FSGAssetTypeId customization
        Commandlets/
          SGDynamicTextAssetCookCommandlet.h
          SGDynamicTextAssetValidationCommandlet.h
        ReferenceViewer/
          SSGDynamicTextAssetReferenceViewer.h
          SGDynamicTextAssetReferenceSubsystem.h
        Settings/
          SGDynamicTextAssetEditorSettings.h
        Utilities/
          SGDynamicTextAssetEditorStatics.h  # Editor-only Blueprint library
          SGDynamicTextAssetCookUtils.h      # Shared cook utilities
          SGDynamicTextAssetEditorConstants.h # Editor constant values
          SGDynamicTextAssetSourceControl.h  # Source control wrapper
        Widgets/
          SSGDynamicTextAssetClassPicker.h   # Class picker widget
      Private/
        SGDynamicTextAssetsEditorModule.cpp
        Browser/
          SSGDynamicTextAssetBrowser.cpp
          SSGDynamicTextAssetTypeTree.cpp
          SSGDynamicTextAssetTileView.cpp
          SSGDynamicTextAssetCreateDialog.cpp
          SSGDynamicTextAssetDuplicateDialog.cpp
          SSGDynamicTextAssetRenameDialog.cpp
          SGDynamicTextAssetBrowserCommands.cpp
        Editor/
          FSGDynamicTextAssetEditorToolkit.cpp
          SGDynamicTextAssetEditorProxy.cpp
          SGDynamicTextAssetEditorCommands.cpp
          SGDynamicTextAssetRefCustomization.cpp
          SGDynamicTextAssetIdentityCustomization.cpp
          FSGDynamicTextAssetIdCustomization.cpp
          SSGDynamicTextAssetIdentityBlock.cpp
          SSGDynamicTextAssetRawView.cpp
          SSGDynamicTextAssetIcon.cpp
          SGAssetTypeIdCustomization.cpp
        Commandlets/
          SGDynamicTextAssetCookCommandlet.cpp
          SGDynamicTextAssetValidationCommandlet.cpp
        ReferenceViewer/
          SSGDynamicTextAssetReferenceViewer.cpp
          SGDynamicTextAssetReferenceSubsystem.cpp
        Settings/
          SGDynamicTextAssetEditorSettings.cpp
        Utilities/
          SGDynamicTextAssetEditorStatics.cpp
          SGDynamicTextAssetCookUtils.cpp
          SGDynamicTextAssetEditorConstants.cpp
          SGDynamicTextAssetSourceControl.cpp
        Widgets/
          SSGDynamicTextAssetClassPicker.cpp
        Tests/
          SGDynamicTextAssetUnitTest.h           # Minimal concrete subclass for testing
          SGDynamicTextAssetXmlUnitTest.h         # Base test class for XML serializer tests
          SGDynamicTextAssetYamlUnitTest.h        # Base test class for YAML serializer tests
          SGDynamicTextAssetVersionTests.cpp
          SGDynamicTextAssetValidationResultTests.cpp
          SGDynamicTextAssetRefTests.cpp
          SGDynamicTextAssetJsonSerializerTests.cpp
          SGDynamicTextAssetBinarySerializerTests.cpp
          SGDynamicTextAssetXmlSerializerTests.cpp
          SGDynamicTextAssetYamlSerializerTests.cpp
          SGDynamicTextAssetFileManagerTests.cpp
          SGDynamicTextAssetCookManifestTests.cpp
          SGDynamicTextAssetStaticsTests.cpp
          SGDynamicTextAssetTypeIdTests.cpp
          SGDynamicTextAssetTypeManifestTests.cpp
          SGDynamicTextAssetTypeRegistryTests.cpp
          SGDynamicTextAssetProviderTests.cpp
          SGDynamicTextAssetHardReferenceTests.cpp
          SGDynamicTextAssetSerializerRegistryTests.cpp
          SGDynamicTextAssetServerNullInterfaceTests.cpp

    SGDynamicTextAssetsUhtPlugin/             # UHT plugin for compile-time validation
      SGDynamicTextAssetHardRefValidator.cs
      SGDynamicTextAssetsUhtPlugin.ubtplugin.csproj

    ThirdParty/
      fkYAML/                                # Third-party YAML library
```

## Module Dependency Hierarchy

```
                    Core
                     |
                CoreUObject
                   / | \
                  /  |  \
            Engine  Json  DeveloperSettings
               |     |
               |  JsonUtilities
               |
    +----------+----------+----------+
    |                     |          |
    |                     |       XmlParser
    |                     |          |
SGDynamicTextAssetsRuntime           |
    |                     |          |
    +----+----+----+      |          |
         |              UnrealEd     |
         |                |          |
         |          +-----+-----+-----+-----+-----+-----+-----+
         |          |     |     |     |     |     |     |     |
         |        Slate  SlateCore  EditorStyle  PropertyEditor  AssetRegistry  SourceControl  ToolMenus
         |          |     |     |     |     |     |     |     |
         +----------+-----+-----+-----+-----+-----+-----+-----+
                              |
                    SGDynamicTextAssetsEditor
```

### SGDynamicTextAssetsRuntime Dependencies

| Dependency | Purpose |
|------------|---------|
| `Core` | Fundamental types, containers, delegates |
| `CoreUObject` | UObject system, reflection, GC |
| `Engine` | World, GameInstance, subsystems |
| `Json` | JSON parsing and writing |
| `JsonUtilities` | `FJsonObjectConverter` for UPROPERTY serialization |
| `DeveloperSettings` | `UDeveloperSettings` base class for project settings |
| `XmlParser` | XML format support for XML serializer |

### SGDynamicTextAssetsEditor Dependencies

| Dependency | Purpose |
|------------|---------|
| `SGDynamicTextAssetsRuntime` | All runtime types and utilities |
| `UnrealEd` | Editor framework, commandlets, packaging settings |
| `Slate` | UI framework for custom widgets |
| `SlateCore` | Core Slate types |
| `EditorStyle` | Editor icons and styles |
| `PropertyEditor` | `IDetailsView`, property customizations |
| `AssetRegistry` | Asset scanning for reference viewer |
| `SourceControl` | `ISourceControlModule` integration |
| `ToolMenus` | Menu integration for editor toolbar and context menus |

## Data File Locations

### Editor (Source Files)

```
{ProjectContent}/SGDynamicTextAssets/
  {ClassHierarchy}/
    {UserFacingId}.dta.json
```

Example: `Content/SGDynamicTextAssets/WeaponData/excalibur.dta.json`

### Cooked (Packaged Builds)

```
{ProjectContent}/SGDynamicTextAssetsCooked/
  {GUID}.dta.bin
  dta_manifest.bin
```

Example: `Content/SGDynamicTextAssetsCooked/A1B2C3D4-E5F6-7890-ABCD-EF1234567890.dta.bin`

> **Source Control:** This directory contains generated artifacts and should be excluded from source control. Add `Content/SGDynamicTextAssetsCooked/` to your project's `.gitignore`. See [Cook Pipeline](../Serialization/CookPipeline.md#source-control) for details.

[Back to Table of Contents](../TableOfContents.md)
