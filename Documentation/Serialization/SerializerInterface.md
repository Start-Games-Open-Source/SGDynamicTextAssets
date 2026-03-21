# Serializer Interface

[Back to Table of Contents](../TableOfContents.md)

## Overview

The SGDynamicTextAssets plugin uses a polymorphic serialization architecture that supports multiple file formats through a single interface. This design allows the plugin to read and write dynamic text assets in JSON, XML, YAML, or any custom format, while keeping format-specific complexity isolated in each serializer.

### Format Documentation

- [JSON Format](JsonFormat.md): `.dta.json` files (TypeId=1)
- [XML Format](XmlFormat.md): `.dta.xml` files (TypeId=2)
- [YAML Format](YamlFormat.md): `.dta.yaml` files (TypeId=3)
- [Binary Format](BinaryFormat.md): `.dta.bin` cooked files

## Architecture

```
ISGDynamicTextAssetSerializer              (Pure C++ interface, not a UInterface)
    │
    ├── FSGTestSerializer                      (TypeId=99, .dta.test, tests only)
    ├── FSGTestAltSerializer                   (TypeId=98, .dta.test.alt, tests only)
    │
    └── FSGDynamicTextAssetSerializerBase       (Abstract base with JSON-intermediate helpers)
            │
            ├── FSGDynamicTextAssetJsonSerializer   (TypeId=1, .dta.json)
            ├── FSGDynamicTextAssetXmlSerializer    (TypeId=2, .dta.xml)
            └── FSGDynamicTextAssetYamlSerializer   (TypeId=3, .dta.yaml)
```

## ISGDynamicTextAssetSerializer

The core interface that all serializers implement. Defined in `SGDynamicTextAssetSerializer.h`. Uses standard C++ polymorphism with `TSharedPtr`/`TSharedRef` semantics (not a UInterface).

### Pure Virtual Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `GetFileExtension` | `FString GetFileExtension() const` | Returns the file extension (e.g., `".dta.json"`) |
| `GetFormatName` | `FText GetFormatName() const` | Returns a human-readable format name (e.g., `"JSON"`) |
| `GetFormatDescription` | `FText GetFormatDescription() const` | Returns a longer description for editor UI |
| `GetSerializerTypeId` | `uint32 GetSerializerTypeId() const` | Returns the unique integer ID for binary file routing |
| `SerializeProvider` | `bool SerializeProvider(const ISGDynamicTextAssetProvider*, FString&) const` | Serializes a provider to string |
| `DeserializeProvider` | `bool DeserializeProvider(const FString&, ISGDynamicTextAssetProvider*, bool&) const` | Deserializes a string into a provider |
| `ValidateStructure` | `bool ValidateStructure(const FString&, FString&) const` | Validates format structure without creating objects |
| `ExtractFileInfo` | `bool ExtractFileInfo(const FString&, FSGDynamicTextAssetFileInfo&) const` | Extracts file information without full deserialization |
| `UpdateFieldsInPlace` | `bool UpdateFieldsInPlace(FString&, const TMap<FString, FString>&) const` | Patches metadata fields in a serialized string |
| `GetFileFormatVersion` | `FSGDynamicTextAssetVersion GetFileFormatVersion() const` | Returns the serializer's current file format version |
| `GetDefaultFileContent` | `FString GetDefaultFileContent(const UClass*, const FSGDynamicTextAssetId&, const FString&) const` | Generates initial file content for new assets |

### Virtual Methods with Default Implementations

| Method | Description |
|--------|-------------|
| `GetIconBrush` | Returns the editor icon brush (editor-only, defaults to generic object icon) |
| `GetFormatName_String` | Convenience `FString` version of `GetFormatName()` |
| `MigrateFileFormat` | Migrates file content between format versions. Default returns true (no structural changes). Override when format structure changes between versions. |
| `UpdateFileFormatVersion` | Updates the `fileFormatVersion` field in raw file contents. Each serializer must override this for its specific format syntax. Called by the migration pipeline after structural migration succeeds. |

### Serializer Type IDs

Each serializer has a unique integer ID stored in binary (`.dta.bin`) file headers. This allows the binary deserializer to route the decompressed payload to the correct format serializer without storing the extension string.

| ID | Format | Serializer Class |
|----|--------|-----------------|
| 0 | INVALID | `INVALID_SERIALIZER_TYPE_ID` |
| 1 | JSON | `FSGDynamicTextAssetJsonSerializer` |
| 2 | XML | `FSGDynamicTextAssetXmlSerializer` |
| 3 | YAML | `FSGDynamicTextAssetYamlSerializer` |
| 98 | Test Alt | `FSGTestAltSerializer` (tests only) |
| 99 | Test | `FSGTestSerializer` (tests only) |
| 100+ | Reserved | Third-party plugin serializers |

Duplicate ID registration is a fatal error caught at startup.

### Shared Key Constants

Metadata keys are defined as static `FString` constants on `ISGDynamicTextAssetSerializer` and are shared across all formats:

| Constant | Value | Usage |
|----------|-------|-------|
| `KEY_FILE_INFORMATION` | `"sgFileInformation"` | Wrapper block for identity fields (renamed from `KEY_METADATA`/`"metadata"` in format v2.0.0) |
| `KEY_METADATA_LEGACY` | `"metadata"` | Legacy wrapper key for backward compatibility with pre-2.0.0 files |
| `KEY_TYPE` | `"type"` | Class type name |
| `KEY_VERSION` | `"version"` | Semantic version string |
| `KEY_ID` | `"id"` | GUID identifier |
| `KEY_USER_FACING_ID` | `"userfacingid"` | Human-readable identifier |
| `KEY_FILE_FORMAT_VERSION` | `"fileFormatVersion"` | File format structural version (tracks serializer format changes, not asset data changes) |
| `KEY_DATA` | `"data"` | Property data block |

Each serializer format determines how these keys are structurally represented:
- **JSON:** `"sgFileInformation": { ... }` and `"data": { ... }` as JSON objects
- **XML:** `<sgFileInformation>...</sgFileInformation>` and `<data>...</data>` as XML elements
- **YAML:** `sgFileInformation:` and `data:` as YAML mappings

## FSGDynamicTextAssetSerializerBase

Abstract base class defined in `SGDynamicTextAssetSerializerBase.h`. Provides the "JSON intermediate" approach where property values are converted to/from `FJsonValue` objects before being transformed into the target format.

### Protected Virtual Helpers

| Method | Default Behavior | Override To |
|--------|-----------------|-------------|
| `SerializePropertyToValue` | Calls `FJsonObjectConverter::UPropertyToJsonValue` | Provide custom property-to-value conversion |
| `DeserializeValueToProperty` | Calls `FJsonObjectConverter::JsonValueToUProperty` | Provide custom value-to-property conversion |
| `ShouldSerializeProperty` | Excludes metadata properties and `CPF_Deprecated` properties | Add format-specific or class-specific filtering |

### JSON Intermediate Approach

The JSON intermediate approach is the recommended pattern for new serializers. Rather than converting directly between UE properties and the target format, serializers:

1. **Serialization:** Convert each UPROPERTY to a `FJsonValue` via `SerializePropertyToValue()`, then transform the `FJsonValue` tree into the target format (XML elements, YAML nodes, etc.)
2. **Deserialization:** Parse the target format into a `FJsonValue` tree, then convert each `FJsonValue` back to a UPROPERTY via `DeserializeValueToProperty()`

This approach reuses Unreal's `FJsonObjectConverter` for the complex property reflection logic, while each serializer only needs to implement the format-specific bridge.

### Property Filtering

`ShouldSerializeProperty()` excludes:
- **Metadata properties:** `DynamicTextAssetId`, `Version`, `UserFacingId` (stored in the metadata block, not the data block)
- **Deprecated properties:** Properties flagged with `CPF_Deprecated` are excluded from both serialization and deserialization. If an existing file contains a value for a now-deprecated property, it is silently ignored when loading.

## Registration Pattern

Serializers are registered with `FSGDynamicTextAssetFileManager` using template methods:

```cpp
// In SGDynamicTextAssetsRuntimeModule::StartupModule()
FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetJsonSerializer>();
FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetXmlSerializer>();
FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetYamlSerializer>();

// In SGDynamicTextAssetsRuntimeModule::ShutdownModule() (reverse order for code symmetry)
FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGDynamicTextAssetYamlSerializer>();
FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGDynamicTextAssetXmlSerializer>();
FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGDynamicTextAssetJsonSerializer>();
```

> **Note:** The reverse unregistration order is a code cleanliness convention (LIFO symmetry with startup), not a functional requirement. Each `UnregisterSerializer` call independently removes the serializer from both the extension map and TypeId map. There are no cross-serializer dependencies, so the order does not matter.

`RegisterSerializer<T>()` instantiates a `TSharedRef<T>` and stores it in two maps: one keyed by file extension (case-insensitive), and one keyed by `GetSerializerTypeId()`. Registration enforces:
- `T` must derive from `ISGDynamicTextAssetSerializer` (compile-time `static_assert`)
- The TypeId must not be `INVALID_SERIALIZER_TYPE_ID` (0)
- No duplicate TypeIds (fatal error at startup)
- Duplicate extensions are allowed but log a warning (the old serializer is replaced)

### Looking Up Serializers

```cpp
// By file extension
TSharedPtr<ISGDynamicTextAssetSerializer> Serializer =
    FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.json"));

// By TypeId (for binary file routing)
TSharedPtr<ISGDynamicTextAssetSerializer> Serializer =
    FSGDynamicTextAssetFileManager::FindSerializerForTypeId(1);

// All registered extensions
TArray<FString> Extensions = FSGDynamicTextAssetFileManager::GetAllRegisteredExtensions();
```

## Building a Custom Serializer

To add a new serialization format:

1. **Create a class** inheriting from `FSGDynamicTextAssetSerializerBase`
2. **Choose a TypeId** >= 100 (range 1-99 is reserved for built in serializers)
3. **Implement all pure virtuals** from `ISGDynamicTextAssetSerializer`
4. **Register** in your module's `StartupModule()` and unregister in `ShutdownModule()`

The JSON intermediate helpers handle all property reflection, so your serializer only needs to bridge between `FJsonValue` trees and your target format.

> You can refer to the already provided serializers as different examples of how to do this:
> - JSON: `FSGDynamicTextAssetJsonSerializer` uses a normal serialization process.
> - XML: `FSGDynamicTextAssetXmlSerializer` uses JSON as a router for the serialization process.
> - YAML: `FSGDynamicTextAssetYamlSerializer` uses JSON as a router to a third party plugin([fkYAML](https://github.com/Start-Games/fkYAML)) for serialization logic to then process.

### Overriding a Built-In Serializer

If you need to replace a built-in serializer (e.g., provide a custom JSON serializer with different formatting or validation), you can do so by unregistering the default and registering your own with the same TypeId.

The registration system handles conflicts as follows:
- **Duplicate TypeIds** are a **fatal error**. You cannot register a new serializer while the old one with the same TypeId is still registered.
- **Duplicate extensions** are **allowed**. The old serializer is silently replaced (a warning is logged).

To safely override a built-in serializer:

1. **Ensure your module loads after `SGDynamicTextAssetsRuntime`** by adding it to your module's dependencies in your `.Build.cs` file
2. In your module's `StartupModule()`, **unregister the built-in serializer first** (this clears both the extension and TypeId map entries), then **register your custom serializer** using the same TypeId

```cpp
// In your custom module's StartupModule()
void FMyCustomSerializerModule::StartupModule()
{
    // Step 1: Remove the built-in JSON serializer (clears both maps)
    FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGDynamicTextAssetJsonSerializer>();

    // Step 2: Register your custom serializer (must use the same TypeId = 1)
    FSGDynamicTextAssetFileManager::RegisterSerializer<FMyCustomJsonSerializer>();
}

void FMyCustomSerializerModule::ShutdownModule()
{
    // Unregister your custom serializer
    FSGDynamicTextAssetFileManager::UnregisterSerializer<FMyCustomJsonSerializer>();

    // Re-register the built-in so the plugin can still function if your module
    // shuts down before the plugin does (optional, depends on your use case)
    FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetJsonSerializer>();
}
```

> **Important:** Your custom serializer must use the **same TypeId** as the one it replaces (e.g., `TYPE_ID = 1` for JSON) so that existing binary (`.dta.bin`) files that reference that TypeId continue to deserialize correctly.

### Future: Direct Property Converters

The `FSGDynamicTextAssetSerializerBase` architecture supports an optional future extensibility point: overriding `SerializePropertyToValue` and `DeserializeValueToProperty` to convert properties directly to the target format (e.g., directly to XML nodes or YAML nodes) without going through the `FJsonValue` intermediate. This is not required for correctness. The JSON intermediate approach works well for all current formats, but this extensibility point may be useful for performance-sensitive scenarios or formats where the `FJsonValue` round-trip introduces unwanted overhead.

[Back to Table of Contents](../TableOfContents.md)
