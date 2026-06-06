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
        SGDTARuntimeModule.h
        Core/
          ISGDynamicTextAssetProvider.h      # Abstract provider interface
          SGDynamicTextAsset.h               # Abstract base class
          SGDynamicTextAssetId.h             # GUID identity wrapper
          SGDynamicTextAssetRef.h            # GUID-based reference struct
          SGDynamicTextAssetTypeId.h         # Type identity struct (FSGDynamicTextAssetTypeId)
          SGDynamicTextAssetVersion.h        # Semantic versioning struct
          SGDTAValidationResult.h  # Validation entry and result structs
          SGDTAValidationUtils.h   # Validation utility functions
          SGDynamicTextAssetEnums.h          # ESGLoadResult, ESGCompressionMethod, ESGReferenceType
        Subsystem/
          SGDynamicTextAssetSubsystem.h      # GameInstanceSubsystem for loading/caching
        Management/
          SGDynamicTextAssetRegistry.h       # EngineSubsystem for class registration
          SGDTAFileManager.h    # Static file path and I/O utilities
          SGDynamicTextAssetFileInfo.h       # File information extraction struct
          SGDTACookManifest.h   # Cook manifest read/write
          SGDTATypeManifest.h   # Type manifest for class hierarchy
        Serialization/
          SGDTASerializer.h        # Serializer registry / factory
          SGDTASerializerBase.h    # Base class for all serializers
          SGDTAJsonSerializer.h    # JSON serialization
          SGDTABinarySerializer.h  # Binary compression/decompression
          SGDTAXmlSerializer.h     # XML serialization
          SGDTAYamlSerializer.h    # YAML serialization (fkYAML)
          SGDTASerializerMetadata.h # Serializer metadata struct
          SGBinaryDynamicTextAssetHeader.h      # Binary format header struct
          SGBinaryEncodeParams.h                # Binary encoding parameters
        Server/
          ISGDTAServerInterface.h     # Abstract server interface
          SGDTAServerCache.h          # SaveGame-based server cache
          SGDTAServerNullInterface.h  # No-op server interface
        Statics/
          SGDynamicTextAssetStatics.h        # Blueprint function library
          SGDTASlateStyles.h    # Shared Slate style definitions
        Settings/
          SGDTASettings.h       # Runtime settings (asset + developer settings)
      Private/
        SGDTARuntimeModule.cpp
        Core/
          ISGDynamicTextAssetProvider.cpp
          SGDynamicTextAsset.cpp
          SGDynamicTextAssetId.cpp
          SGDynamicTextAssetRef.cpp
          SGDynamicTextAssetTypeId.cpp
          SGDynamicTextAssetVersion.cpp
          SGDTAValidationResult.cpp
          SGDTAValidationUtils.cpp
        Subsystem/
          SGDynamicTextAssetSubsystem.cpp
        Management/
          SGDynamicTextAssetRegistry.cpp
          SGDTAFileManager.cpp
          SGDTACookManifest.cpp
          SGDTATypeManifest.cpp
        Serialization/
          SGDTASerializer.cpp
          SGDTASerializerBase.cpp
          SGDTAJsonSerializer.cpp
          SGDTABinarySerializer.cpp
          SGDTAXmlSerializer.cpp
          SGDTAYamlSerializer.cpp
          SGDTASerializerMetadata.cpp
          SGBinaryDynamicTextAssetHeader.cpp
        Server/
          ISGDTAServerInterface.cpp
          SGDTAServerCache.cpp
          SGDTAServerNullInterface.cpp
        Statics/
          SGDynamicTextAssetStatics.cpp
          SGDTASlateStyles.cpp
        Settings/
          SGDTASettings.cpp

    SGDynamicTextAssetsEditor/               # Editor module (editor only)
      SGDynamicTextAssetsEditor.Build.cs
      Public/
        SGDTAEditorModule.h
        Browser/
          SSGDTABrowser.h       # Main browser window
          SSGDTATypeTree.h      # Type hierarchy tree widget
          SSGDTATileView.h      # Asset tile view widget
          SSGDTACreateDialog.h  # Create dialog
          SSGDTADuplicateDialog.h
          SSGDTARenameDialog.h
          SGDTABrowserCommands.h  # Browser UI commands
        Editor/
          SGDTAEditorToolkit.h    # Asset editor toolkit
          SGDTAEditorProxy.h       # Editor proxy for asset editing
          SGDTAEditorCommands.h    # Editor UI commands
          SGDTARefCustomization.h  # FSGDynamicTextAssetRef picker
          SGDTADetailCustomization.h  # Detail customization (Identity category, copy buttons)
          SGDTAIdCustomization.h  # FSGDynamicTextAssetId customization
          SSGDTAIdentityBlock.h    # Identity block widget
          SSGDTARawView.h          # Raw JSON/text view widget
          SSGDynamicTextAssetIcon.h             # Icon display widget
          SGDTAAssetTypeIdCustomization.h          # FSGDynamicTextAssetTypeId customization
        Commandlets/
          SGDTACookCommandlet.h
          SGDTAValidationCommandlet.h
        ReferenceViewer/
          SSGDTAReferenceViewer.h
          SGDTAReferenceSubsystem.h
        Settings/
          SGDTAEditorSettings.h
        Utilities/
          SGDTAEditorStatics.h  # Editor-only Blueprint library
          SGDTACookUtils.h      # Shared cook utilities
          SGDTAEditorConstants.h # Editor constant values
          SGDTASourceControl.h  # Source control wrapper
        Widgets/
          SSGDTAClassPicker.h   # Class picker widget
      Private/
        SGDTAEditorModule.cpp
        Browser/
          SSGDTABrowser.cpp
          SSGDTATypeTree.cpp
          SSGDTATileView.cpp
          SSGDTACreateDialog.cpp
          SSGDTADuplicateDialog.cpp
          SSGDTARenameDialog.cpp
          SGDTABrowserCommands.cpp
        Editor/
          SGDTAEditorToolkit.cpp
          SGDTAEditorProxy.cpp
          SGDTAEditorCommands.cpp
          SGDTARefCustomization.cpp
          SGDTADetailCustomization.cpp
          SGDTAIdCustomization.cpp
          SSGDTAIdentityBlock.cpp
          SSGDTARawView.cpp
          SSGDynamicTextAssetIcon.cpp
          SGDTAAssetTypeIdCustomization.cpp
        Commandlets/
          SGDTACookCommandlet.cpp
          SGDTAValidationCommandlet.cpp
        ReferenceViewer/
          SSGDTAReferenceViewer.cpp
          SGDTAReferenceSubsystem.cpp
        Settings/
          SGDTAEditorSettings.cpp
        Utilities/
          SGDTAEditorStatics.cpp
          SGDTACookUtils.cpp
          SGDTAEditorConstants.cpp
          SGDTASourceControl.cpp
        Widgets/
          SSGDTAClassPicker.cpp
        Tests/
          SGDynamicTextAssetUnitTest.h           # Minimal concrete subclass for testing
          SGDTAXmlUnitTest.h         # Base test class for XML serializer tests
          SGDTAYamlUnitTest.h        # Base test class for YAML serializer tests
          SGDTAVersionTests.cpp
          SGDTAValidationResultTests.cpp
          SGDTARefTests.cpp
          SGDTAJsonSerializerTests.cpp
          SGDTABinarySerializerTests.cpp
          SGDTAXmlSerializerTests.cpp
          SGDTAYamlSerializerTests.cpp
          SGDTAFileManagerTests.cpp
          SGDTACookManifestTests.cpp
          SGDTAStaticsTests.cpp
          SGDTATypeIdTests.cpp
          SGDTATypeManifestTests.cpp
          SGDTATypeRegistryTests.cpp
          SGDTAProviderTests.cpp
          SGDTAHardReferenceTests.cpp
          SGDTASerializerRegistryTests.cpp
          SGDTAServerNullInterfaceTests.cpp

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
