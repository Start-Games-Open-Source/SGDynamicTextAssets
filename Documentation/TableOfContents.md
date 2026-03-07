# SGDynamicTextAssets Plugin Documentation

## Table of Contents

### Getting Started
- [Overview](Overview.md): What the plugin does and core design principles
- [Quick Start Guide](QuickStartGuide.md): Step-by-step setup and first dynamic text asset

### Core Concepts
- [Dynamic Text Assets](Core/DynamicTextAssets.md): USGDynamicTextAsset base class, GUIDs, UserFacingId, versioning
- [Dynamic Text Asset References](Core/DynamicTextAssetReferences.md): FSGDynamicTextAssetRef, resolving, type filters
- [Versioning and Migration](Core/VersioningAndMigration.md): Semantic versioning, MigrateFromVersion, compatibility
- [Validation](Core/Validation.md): FSGDynamicTextAssetValidationResult, custom validation hooks, severity levels
- [UHT Plugin](Core/UhtPlugin.md): Compile-time validation plugin setup, file structure, distribution

### Runtime API
- [Subsystem](Runtime/Subsystem.md): USGDynamicTextAssetSubsystem: loading, caching, async operations
- [Registry](Runtime/Registry.md): USGDynamicTextAssetRegistry: class registration, discovery
- [File Manager](Runtime/FileManager.md): FSGDynamicTextAssetFileManager: file paths, scanning, I/O, serializer registry
- [Blueprint Statics](Runtime/BlueprintStatics.md): USGDynamicTextAssetStatics: Blueprint function library

### Editor Tools
- [Dynamic Text Asset Browser](Editor/DynamicTextAssetBrowser.md): Browsing, creating, duplicating, renaming, deleting, validation
- [Dynamic Text Asset Editor](Editor/DynamicTextAssetEditor.md): Property editing, saving, reverting, Raw View
- [Reference Viewer](Editor/ReferenceViewer.md): Viewing referencers and dependencies
- [Property Customizations](Editor/PropertyCustomizations.md): FSGDynamicTextAssetRef picker, identity display
- [Source Control](Editor/SourceControl.md): Auto-checkout, mark for add/delete

### Serialization and Cooking
- [Serializer Interface](Serialization/SerializerInterface.md): ISGDynamicTextAssetSerializer contract, custom serializer guide
- [JSON Format](Serialization/JsonFormat.md): .dta.json schema, metadata fields, data section
- [XML Format](Serialization/XmlFormat.md): .dta.xml schema, FXmlFile integration
- [YAML Format](Serialization/YamlFormat.md): .dta.yaml schema, fkYAML integration
- [Binary Format](Serialization/BinaryFormat.md): .dta.bin header, compression, content hashes
- [Cook Pipeline](Serialization/CookPipeline.md): Cook commandlet, cook delegate, flat GUID output, manifest
- [Cook Manifest](Serialization/CookManifest.md): Manifest format, integrity hash, runtime lookups

### Server Integration
- [Server Interface](Server/ServerInterface.md): ISGDynamicTextAssetServerInterface, implementing a backend
- [Server Cache](Server/ServerCache.md): USGDynamicTextAssetServerCache, persistent caching

### Configuration
- [Settings](Configuration/Settings.md): USGDynamicTextAssetSettings, USGDynamicTextAssetSettingsAsset, editor settings

### Testing
- [Automated Tests](Testing/AutomatedTests.md): Unit test coverage, running tests, test dynamic text assets

### Advanced Topics
- [Type Manifest System](Advanced/TypeManifestSystem.md): FSGDynamicTextAssetTypeId, type manifests, server type overrides
- [Error Handling and Debugging](Advanced/ErrorHandlingAndDebugging.md): ESGLoadResult codes, log categories, debugging workflows
- [Async Loading and Threading](Advanced/AsyncLoadingAndThreading.md): Threading model, async pipeline, cache architecture, memory management
- [Custom Serializer Guide](Advanced/CustomSerializerGuide.md): Implementing custom formats, serializer registration, format conversion

### Reference
- [Class Reference](Reference/ClassReference.md): All classes, structs, and enums at a glance
- [File Structure](Reference/FileStructure.md): Plugin directory layout and module organization
- [Commandlets](Reference/Commandlets.md): Cook and validation commandlets, CLI usage
