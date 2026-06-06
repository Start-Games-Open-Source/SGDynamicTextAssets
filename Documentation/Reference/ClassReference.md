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
| `USGDynamicTextAssetServerCache` | `USaveGame` | `SGDTAServerCache.h/.cpp` | [Server Cache](../Server/ServerCache.md) | Persistent local cache of server-provided data |
| `USGDynamicTextAssetStatics` | `UBlueprintFunctionLibrary` | `SGDynamicTextAssetStatics.h/.cpp` | [Blueprint Statics](../Runtime/BlueprintStatics.md) | Blueprint function library for reference and validation operations |
| `USGDynamicTextAssetSettings` | `UDeveloperSettings` | `SGDTASettings.h/.cpp` | [Settings](../Configuration/Settings.md) | Project settings entry (soft ref to settings asset) |
| `USGDynamicTextAssetSettingsAsset` | `UDataAsset` | `SGDTASettings.h/.cpp` | [Settings](../Configuration/Settings.md) | Runtime configuration data asset |

### Interfaces

| Interface | File | Documentation | Description |
|-----------|------|---------------|-------------|
| `ISGDynamicTextAssetProvider` | `ISGDynamicTextAssetProvider.h` | [Dynamic Text Assets](../Core/DynamicTextAssets.md) | Core interface for identity, versioning, validation, and migration |
| `USGDynamicTextAssetServerInterface` | `ISGDTAServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Abstract interface for server backend integration |
| `USGDynamicTextAssetServerNullInterface` | `ISGDTAServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Null object pattern implementation providing no-op server responses |

### Structs

| Struct | File | Documentation | Description |
|--------|------|---------------|-------------|
| `FSGDynamicTextAssetId` | `SGDynamicTextAssetId.h` | [Dynamic Text Assets](../Core/DynamicTextAssets.md) | USTRUCT wrapper around FGuid used as the unique identifier |
| `FSGDynamicTextAssetRef` | `SGDynamicTextAssetRef.h/.cpp` | [Dynamic Text Asset References](../Core/DynamicTextAssetReferences.md) | Lightweight ID-based reference to a dynamic text asset |
| `FSGDynamicTextAssetVersion` | `SGDynamicTextAssetVersion.h` | [Versioning and Migration](../Core/VersioningAndMigration.md) | Semantic version (Major.Minor.Patch) |
| `FSGDynamicTextAssetValidationEntry` | `SGDTAValidationResult.h` | [Validation](../Core/Validation.md) | Single validation finding with severity, message, property path |
| `FSGDynamicTextAssetValidationResult` | `SGDTAValidationResult.h` | [Validation](../Core/Validation.md) | Container for validation entries (errors, warnings, infos) |
| `FSGDynamicTextAssetFileInfo` | `SGDynamicTextAssetFileInfo.h` | [File Manager](../Runtime/FileManager.md) | Extracted file information without full deserialization |
| `FSGDynamicTextAssetCookManifestEntry` | `SGDTACookManifest.h/.cpp` | [Cook Manifest](../Serialization/CookManifest.md) | Single cook manifest entry (ID, ClassName, UserFacingId) |
| `FSGBinaryDynamicTextAssetHeader` | `SGBinaryDynamicTextAssetHeader.h` | [Binary Format](../Serialization/BinaryFormat.md) | 80-byte binary file header |
| `FSGServerDynamicTextAssetRequest` | `ISGDTAServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Server request payload (FSGDynamicTextAssetId + local version) |
| `FSGServerDynamicTextAssetResponse` | `ISGDTAServerInterface.h` | [Server Interface](../Server/ServerInterface.md) | Server response payload (FSGDynamicTextAssetId + data + server version) |
| `FSGDynamicTextAssetSerializerMetadata` | `SGDTASerializerMetadata.h` | [Serializer Interface](../Serialization/SerializerInterface.md) | Metadata for a registered serializer (extension, format name, TypeId) |
| `FSGDynamicTextAssetTypeId` | `SGDynamicTextAssetTypeId.h` | [Dynamic Text Assets](../Core/DynamicTextAssets.md) | USTRUCT wrapper around FGuid for stable asset type identity |
| `FSGDynamicTextAssetTypeManifestEntry` | `SGDTATypeManifest.h` | [Registry](../Runtime/Registry.md) | Single type manifest entry (TypeId, Class, ParentTypeId) |
| `FSGBinaryEncodeParams` | `SGBinaryEncodeParams.h` | [Binary Format](../Serialization/BinaryFormat.md) | Binary encoding parameters |

### Static Utility Classes

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `FSGDynamicTextAssetFileManager` | `SGDTAFileManager.h/.cpp` | [File Manager](../Runtime/FileManager.md) | File path utilities, scanning, I/O, serializer registry, cooked build support |
| `FSGDynamicTextAssetCookManifest` | `SGDTACookManifest.h/.cpp` | [Cook Manifest](../Serialization/CookManifest.md) | Cook manifest read/write with integrity hash |
| `FSGDynamicTextAssetTypeManifest` | `SGDTATypeManifest.h/.cpp` | [Registry](../Runtime/Registry.md) | Per-root-type manifest mapping type IDs to UClass soft references |

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
| `ESGDynamicTextAssetCompressionMethod` | `SGDTASettings.h` | None, Zlib, Gzip, LZ4, Custom | Settings-level compression selection (extends core enum with Custom) |
| `ESGValidationSeverity` | `SGDTAValidationResult.h` | Info, Warning, Error | Validation entry severity levels |
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
| `SSGDynamicTextAssetBrowser` | `SSGDTABrowser.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Main browser window with type tree and asset grid |
| `SSGDynamicTextAssetTileView` | `SSGDTATileView.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Asset grid panel with search bar and tile display |
| `SSGDynamicTextAssetTypeTree` | `SSGDTATypeTree.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Class hierarchy tree panel |
| `SSGDynamicTextAssetCreateDialog` | `SSGDTACreateDialog.h` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Dialog for creating a new dynamic text asset |
| `SSGDynamicTextAssetDuplicateDialog` | `SSGDTADuplicateDialog.h` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Dialog for duplicating a dynamic text asset |
| `SSGDynamicTextAssetRenameDialog` | `SSGDTARenameDialog.h` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Dialog for renaming a dynamic text asset |
| `SSGDynamicTextAssetReferenceViewer` | `SSGDTAReferenceViewer.h/.cpp` | [Reference Viewer](../Editor/ReferenceViewer.md) | Referencers and dependencies viewer |
| `SSGDynamicTextAssetRawView` | `SSGDTARawView.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Read-only display of on-disk file content |
| `SSGDynamicTextAssetIdentityBlock` | `SSGDTAIdentityBlock.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Identity display widget (ID, UserFacingId, Version) |
| `SSGDynamicTextAssetIcon` | `SSGDynamicTextAssetIcon.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Icon display widget |
| `SSGDynamicTextAssetClassPicker` | `SSGDTAClassPicker.h/.cpp` | [Dynamic Text Asset Browser](../Editor/DynamicTextAssetBrowser.md) | Class picker widget |

### Toolkits

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `FSGDynamicTextAssetEditorToolkit` | `SGDTAEditorToolkit.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Standalone property editor for a single dynamic text asset |

### Property Customizations

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `FSGDynamicTextAssetRefCustomization` | `SGDTARefCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | Searchable dropdown picker for `FSGDynamicTextAssetRef` |
| `FSGDTADetailCustomization` | `SGDTADetailCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | Detail customization for USGDynamicTextAsset: Identity category with copy buttons for ID, UserFacingId, Version |
| `FSGDynamicTextAssetIdCustomization` | `SGDTAIdCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | FSGDynamicTextAssetId property display customization |
| `FSGDTAAssetTypeIdCustomization` | `SGDTAAssetTypeIdCustomization.h/.cpp` | [Property Customizations](../Editor/PropertyCustomizations.md) | FSGDynamicTextAssetTypeId property display customization |

### Commandlets

| Commandlet | File | Command | Documentation | Description |
|------------|------|---------|---------------|-------------|
| `USGDynamicTextAssetCookCommandlet` | `SGDTACookCommandlet.h/.cpp` | `-run=SGDynamicTextAssetCook` | [Commandlets](Commandlets.md) | Converts text files to binary for packaged builds |
| `USGDynamicTextAssetValidationCommandlet` | `SGDTAValidationCommandlet.h/.cpp` | `-run=SGDynamicTextAssetValidation` | [Commandlets](Commandlets.md) | Validates all dynamic text asset files without cooking |

### Utilities

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `USGDynamicTextAssetEditorStatics` | `SGDTAEditorStatics.h/.cpp` | | Editor-only Blueprint library for file operations |
| `FSGDynamicTextAssetSourceControl` | `SGDTASourceControl.h/.cpp` | [Source Control](../Editor/SourceControl.md) | Source control integration wrapper |
| `FSGDynamicTextAssetCookUtils` | `SGDTACookUtils.h/.cpp` | [Cook Pipeline](../Serialization/CookPipeline.md) | Shared cook utilities for commandlet and delegate |
| `USGDynamicTextAssetReferenceSubsystem` | `SGDTAReferenceSubsystem.h/.cpp` | [Reference Viewer](../Editor/ReferenceViewer.md) | Editor subsystem for scanning cross-references |
| `FSGDynamicTextAssetEditorConstants` | `SGDTAEditorConstants.h/.cpp` | | Editor constant values and shared configuration |

### Proxy

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `USGDynamicTextAssetEditorProxy` | `SGDTAEditorProxy.h/.cpp` | [Dynamic Text Asset Editor](../Editor/DynamicTextAssetEditor.md) | Transient UObject proxy for FAssetEditorToolkit integration |

### Settings

| Class | File | Documentation | Description |
|-------|------|---------------|-------------|
| `USGDynamicTextAssetEditorSettings` | `SGDTAEditorSettings.h` | [Settings](../Configuration/Settings.md) | Editor-specific configuration (reference scanning) |

[Back to Table of Contents](../TableOfContents.md)
