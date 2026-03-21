# Class Reference

[Back to Table of Contents](../TableOfContents.md)

Quick reference of all classes, structs, and enums in the SGDynamicTextAssets plugin.

## Runtime Module (SGDynamicTextAssetsRuntime)

### Classes

| Class | Base | File | Documentation | Description |
|-------|------|------|---------------|-------------|
| `USGDynamicTextAsset` | `UObject` | `SGDynamicTextAsset.h/.cpp` | [Dynamic Text Assets](../Core/DynamicTextAssets.md) | Abstract base class for all dynamic text assets |
| `USGDynamicTextAssetSubsystem` | `UGameInstanceSubsystem` | `SGDynamicTextAssetSubsystem.h/.cpp` | [Subsystem](../Runtime/Subsystem.md) | Runtime loading and caching of dynamic text assets |
| `USGDynamicTextAssetRegistry` | `UEngineSubsystem` | `SGDynamicTextAssetRegistry.h/.cpp` | [Registry](../Runtime/Registry.md) | Singleton registry for dynamic text asset class registration |
| `USGDynamicTextAssetServerCache` | `USaveGame` | `SGDynamicTextAssetServerCache.h/.cpp` | [Server Cache](../Server/ServerCache.md) | Persistent local cache of server-provided data |
| `USGDynamicTextAssetStatics` | `UBlueprintFunctionLibrary` | `SGDynamicTextAssetStatics.h/.cpp` | [Blueprint Statics](../Runtime/BlueprintStatics.md) | Blueprint function library for reference and validation operations |
| `USGDynamicTextAssetSettings` | `UDeveloperSettings` | `SGDynamicTextAssetSettings.h/.cpp` | [Settings](../Configuration/Settings.md) | Project settings entry (soft ref to settings asset) |
| `USGDynamicTextAssetSettingsAsset` | `UDataAsset` | `SGDynamicTextAssetSettings.h/.cpp` | [Settings](../Configuration/Settings.md) | Runtime configuration data asset |

### Interfaces

| Interface | File | Documentation | Description |
|-----------|------|---------------|-------------|
| `ISGDynamicTextAssetProvider` | `ISGDynamicTextAssetProvider.h` | [Dynamic Text Assets](../Core/DynamicTextAssets.md) | Core interface for identity, versioning, validation, and migration |
| `USGDynamicTextAssetServerInterface` | `ISGDynamicTextAssetServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Abstract interface for server backend integration |
| `USGDynamicTextAssetServerNullInterface` | `ISGDynamicTextAssetServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Null object pattern implementation providing no-op server responses |

### Structs

| Struct | File | Documentation | Description |
|--------|------|---------------|-------------|
| `FSGDynamicTextAssetId` | `SGDynamicTextAssetId.h` | [Dynamic Text Assets](../Core/DynamicTextAssets.md) | USTRUCT wrapper around FGuid used as the unique identifier |
| `FSGDynamicTextAssetRef` | `SGDynamicTextAssetRef.h/.cpp` | [Dynamic Text Asset References](../Core/DynamicTextAssetReferences.md) | Lightweight ID-based reference to a dynamic text asset |
| `FSGDynamicTextAssetVersion` | `SGDynamicTextAssetVersion.h` | [Versioning and Migration](../Core/VersioningAndMigration.md) | Semantic version (Major.Minor.Patch) |
| `FSGDynamicTextAssetValidationEntry` | `SGDynamicTextAssetValidationResult.h` | [Validation](../Core/Validation.md) | Single validation finding with severity, message, property path |
| `FSGDynamicTextAssetValidationResult` | `SGDynamicTextAssetValidationResult.h` | [Validation](../Core/Validation.md) | Container for validation entries (errors, warnings, infos) |
| `FSGDynamicTextAssetFileInfo` | `SGDynamicTextAssetFileInfo.h` | [File Manager](../Runtime/FileManager.md) | Extracted file information without full deserialization |
| `FSGDynamicTextAssetCookManifestEntry` | `SGDynamicTextAssetCookManifest.h/.cpp` | [Cook Manifest](../Serialization/CookManifest.md) | Single cook manifest entry (ID, ClassName, UserFacingId) |
| `FSGBinaryDynamicTextAssetHeader` | `SGBinaryDynamicTextAssetHeader.h` | [Binary Format](../Serialization/BinaryFormat.md) | 80-byte binary file header |
| `FSGServerDynamicTextAssetRequest` | `ISGDynamicTextAssetServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Server request payload (FSGDynamicTextAssetId + local version) |
| `FSGServerDynamicTextAssetResponse` | `ISGDynamicTextAssetServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Server response payload (FSGDynamicTextAssetId + data + server version) |
| `FSGDynamicTextAssetSerializerMetadata` | `SGDynamicTextAssetSerializerMetadata.h` | [Serializer Interface](../Serialization/SerializerInterface.md) | Metadata for a registered serializer (extension, format name, TypeId) |
| `FSGDynamicTextAssetTypeId` | `SGDynamicTextAssetTypeId.h` | [Dynamic Text Assets](../Core/DynamicTextAssets.md) | USTRUCT wrapper around FGuid for stable asset type identity |
| `FSGDynamicTextAssetTypeManifestEntry` | `SGDynamicTextAssetTypeManifest.h` | [Registry](../Runtime/Registry.md) | Single type manifest entry (TypeId, Class, ParentTypeId) |
| `FSGBinaryEncodeParams` | `SGBinaryEncodeParams.h` | [Binary Format](../Serialization/BinaryFormat.md) | Binary encoding parameters |

### Static Utility Classes

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `FSGDynamicTextAssetFileManager` | `SGDynamicTextAssetFileManager.h/.cpp` | [File Manager](../Runtime/FileManager.md) | File path utilities, scanning, I/O, serializer registry, cooked build support |
| `FSGDynamicTextAssetCookManifest` | `SGDynamicTextAssetCookManifest.h/.cpp` | [Cook Manifest](../Serialization/CookManifest.md) | Cook manifest read/write with integrity hash |
| `FSGDynamicTextAssetTypeManifest` | `SGDynamicTextAssetTypeManifest.h/.cpp` | [Registry](../Runtime/Registry.md) | Per-root-type manifest mapping type IDs to UClass soft references |

### Serializer Interfaces and Implementations

| Class | Type | TypeId | Extension | Documentation | Description |
|-------|------|--------|-----------|---------------|-------------|
| `ISGDynamicTextAssetSerializer` | Interface | - | - | [Serializer Interface](../Serialization/SerializerInterface.md) | Pure C++ interface for pluggable serialization formats |
| `FSGDynamicTextAssetSerializerBase` | Abstract Base | - | - | [Serializer Interface](../Serialization/SerializerInterface.md) | JSON-intermediate base with virtual property conversion helpers; shared by JSON, XML, and YAML serializers |
| `FSGDynamicTextAssetJsonSerializer` | Serializer | 1 | `.dta.json` | [JSON Format](../Serialization/JsonFormat.md) | JSON serialization (default format) |
| `FSGDynamicTextAssetXmlSerializer` | Serializer | 2 | `.dta.xml` | [XML Format](../Serialization/XmlFormat.md) | XML serialization via UE XmlParser module |
| `FSGDynamicTextAssetYamlSerializer` | Serializer | 3 | `.dta.yaml` | [YAML Format](../Serialization/YamlFormat.md) | YAML serialization via fkYAML library |
| `FSGDynamicTextAssetBinarySerializer` | Serializer | - | `.dta.bin` | [Binary Format](../Serialization/BinaryFormat.md) | Binary compression and decompression for cooked builds |

### Enums

| Enum | File | Values | Description |
|------|------|--------|-------------|
| `ESGLoadResult` | `SGDynamicTextAssetEnums.h` | Success, AlreadyLoaded, FileNotFound, DeserializationFailed, ValidationFailed, ServerDeletedObject, ServerFetchFailed, Timeout | Load operation result codes |
| `ESGCompressionMethod` | `SGDynamicTextAssetEnums.h` | None, Zlib, Gzip, LZ4 | Binary compression algorithms (core enum) |
| `ESGDynamicTextAssetCompressionMethod` | `SGDynamicTextAssetSettings.h` | None, Zlib, Gzip, LZ4, Custom | Settings-level compression selection (extends core enum with Custom) |
| `ESGValidationSeverity` | `SGDynamicTextAssetValidationResult.h` | Info, Warning, Error | Validation entry severity levels |
| `ESGReferenceType` | `SGDynamicTextAssetEnums.h` | Blueprint, DynamicTextAsset, Level, Other | Source type of a reference |

### Delegates

| Delegate | Signature | Description |
|----------|-----------|-------------|
| `FOnDynamicTextAssetLoaded` | `(const TScriptInterface<ISGDynamicTextAssetProvider>&, bool)` | Async load completion callback |
| `FOnDynamicTextAssetCached` | `(const TScriptInterface<ISGDynamicTextAssetProvider>&)` | Broadcast when provider added to cache |
| `FOnDynamicTextAssetRegistryChanged` | `()` | Broadcast on class registration change |
| `FOnServerFetchComplete` | `(bool bSuccess, const TArray<FSGServerDynamicTextAssetResponse>&)` | Server bulk fetch completion callback |
| `FOnServerExistsComplete` | `(bool bSuccess, bool bExists, const FString& UpdatedContent)` | Server single exists check callback |
| `FSGDataGenerateDefaultContentDelegate` | `(const UClass*, const FSGDynamicTextAssetId&, const FString&, TSharedRef<ISGDynamicTextAssetSerializer>)` | Fired when creating new files to generate default content |
| `FOnTypeManifestUpdated` | `()` | Broadcast when type manifests are synced or updated |

## Editor Module (SGDynamicTextAssetsEditor)

### Widgets

| Widget | File | Documentation | Description |
|--------|------|---------------|-------------|
| `SSGDynamicTextAssetBrowser` | `SSGDynamicTextAssetBrowser.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Main browser window with type tree and asset grid |
| `SSGDynamicTextAssetTileView` | `SSGDynamicTextAssetTileView.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Asset grid panel with search bar and tile display |
| `SSGDynamicTextAssetTypeTree` | `SSGDynamicTextAssetTypeTree.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Class hierarchy tree panel |
| `SSGDynamicTextAssetCreateDialog` | `SSGDynamicTextAssetCreateDialog.h` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Dialog for creating a new dynamic text asset |
| `SSGDynamicTextAssetDuplicateDialog` | `SSGDynamicTextAssetDuplicateDialog.h` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Dialog for duplicating a dynamic text asset |
| `SSGDynamicTextAssetRenameDialog` | `SSGDynamicTextAssetRenameDialog.h` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Dialog for renaming a dynamic text asset |
| `SSGDynamicTextAssetReferenceViewer` | `SSGDynamicTextAssetReferenceViewer.h/.cpp` | [Reference Viewer](../Editor/ReferenceViewer.md) | Referencers and dependencies viewer |
| `SSGDynamicTextAssetRawView` | `SSGDynamicTextAssetRawView.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Read-only display of on-disk file content |
| `SSGDynamicTextAssetIdentityBlock` | `SSGDynamicTextAssetIdentityBlock.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Identity display widget (ID, UserFacingId, Version) |
| `SSGDynamicTextAssetIcon` | `SSGDynamicTextAssetIcon.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Icon display widget |
| `SSGDynamicTextAssetClassPicker` | `SSGDynamicTextAssetClassPicker.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Class picker widget |

### Toolkits

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `FSGDynamicTextAssetEditorToolkit` | `FSGDynamicTextAssetEditorToolkit.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Standalone property editor for a single dynamic text asset |

### Property Customizations

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `FSGDynamicTextAssetRefCustomization` | `SGDynamicTextAssetRefCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | Searchable dropdown picker for `FSGDynamicTextAssetRef` |
| `SGDynamicTextAssetIdentityCustomization` | `SGDynamicTextAssetIdentityCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | Read-only display with copy buttons for ID, UserFacingId, Version |
| `FSGDynamicTextAssetIdCustomization` | `FSGDynamicTextAssetIdCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | FSGDynamicTextAssetId property display customization |
| `FSGAssetTypeIdCustomization` | `SGAssetTypeIdCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | FSGAssetTypeId property display customization |

### Commandlets

| Commandlet | File | Command | Documentation | Description |
|------------|------|---------|---------------|-------------|
| `USGDynamicTextAssetCookCommandlet` | `SGDynamicTextAssetCookCommandlet.h/.cpp` | `-run=SGDynamicTextAssetCook` | [Commandlets](Commandlets.md) | Converts text files to binary for packaged builds |
| `USGDynamicTextAssetValidationCommandlet` | `SGDynamicTextAssetValidationCommandlet.h/.cpp` | `-run=SGDynamicTextAssetValidation` | [Commandlets](Commandlets.md) | Validates all dynamic text asset files without cooking |

### Utilities

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `USGDynamicTextAssetEditorStatics` | `SGDynamicTextAssetEditorStatics.h/.cpp` | | Editor-only Blueprint library for file operations |
| `FSGDynamicTextAssetSourceControl` | `SGDynamicTextAssetSourceControl.h/.cpp` | [Source Control](../Editor/SourceControl.md) | Source control integration wrapper |
| `FSGDynamicTextAssetCookUtils` | `SGDynamicTextAssetCookUtils.h/.cpp` | [Cook Pipeline](../Serialization/CookPipeline.md) | Shared cook utilities for commandlet and delegate |
| `USGDynamicTextAssetReferenceSubsystem` | `SGDynamicTextAssetReferenceSubsystem.h/.cpp` | [Reference Viewer](../Editor/ReferenceViewer.md) | Editor subsystem for scanning cross-references |
| `FSGDynamicTextAssetEditorConstants` | `SGDynamicTextAssetEditorConstants.h/.cpp` | | Editor constant values and shared configuration |

### Proxy

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `USGDynamicTextAssetEditorProxy` | `SGDynamicTextAssetEditorProxy.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Transient UObject proxy for FAssetEditorToolkit integration |

### Settings

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `USGDynamicTextAssetEditorSettings` | `SGDynamicTextAssetEditorSettings.h` | [Settings](../Configuration/Settings.md) | Editor-specific configuration (reference scanning) |

[Back to Table of Contents](../TableOfContents.md)
