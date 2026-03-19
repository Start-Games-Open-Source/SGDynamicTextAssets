# Custom Serializer Guide

[Back to Table of Contents](../TableOfContents.md)

## Architecture Overview

The serialization system uses a layered inheritance chain:

```
ISGDynamicTextAssetSerializer          (pure C++ interface)
    └── FSGDynamicTextAssetSerializerBase   (JSON-intermediate helpers)
            ├── FSGDynamicTextAssetJsonSerializer  (TypeId=1, .dta.json)
            ├── FSGDynamicTextAssetXmlSerializer   (TypeId=2, .dta.xml)
            └── FSGDynamicTextAssetYamlSerializer  (TypeId=3, .dta.yaml)

FSGDynamicTextAssetBinarySerializer    (standalone, TypeId=none, .dta.bin)
```

Custom serializers can extend at either level:
- **`FSGDynamicTextAssetSerializerBase`**: Use the JSON-intermediate approach (recommended for most formats)
- **`ISGDynamicTextAssetSerializer`**: Implement everything from scratch (for formats incompatible with JSON intermediates)

## ISGDynamicTextAssetSerializer Interface

The pure interface defines the contract all serializers must fulfill:

```cpp
class ISGDynamicTextAssetSerializer : public TSharedFromThis<ISGDynamicTextAssetSerializer>
```

This is **not** a UInterface. It uses standard C++ polymorphism with `TSharedPtr`/`TSharedRef` semantics.

### Required Methods

| Method | Purpose |
|--------|---------|
| `GetFileExtension()` | File extension (e.g., `".dta.json"`) |
| `GetFormatName()` | Human-readable format name as `FText` |
| `GetFormatDescription()` | Description for editor tooling |
| `GetSerializerTypeId()` | Unique integer ID for binary header routing |
| `SerializeProvider()` | Provider → string |
| `DeserializeProvider()` | String → provider (with migration flag) |
| `ValidateStructure()` | Structural validation without full deserialization |
| `ExtractMetadata()` | Lightweight header parsing for Id, ClassName, Version, TypeId |
| `UpdateFieldsInPlace()` | Patch metadata fields without full round-trip |
| `GetDefaultFileContent()` | Generate initial file content for new assets |

### Serializer Type IDs

IDs must be globally unique. Duplicate registration is a fatal error.

| Range | Usage |
|-------|-------|
| `0` | Invalid (`INVALID_SERIALIZER_TYPE_ID`) |
| `1–97` | Reserved for built-in serializers |
| `98–99` | Reserved for test serializers |
| `100+` | Available for third-party plugins |

### Standard Keys

The interface defines standard metadata keys used across all formats:

```cpp
static const FString KEY_METADATA;       // "metadata"
static const FString KEY_TYPE;           // "type"
static const FString KEY_VERSION;        // "version"
static const FString KEY_ID;             // "id"
static const FString KEY_USER_FACING_ID; // "userfacingid"
static const FString KEY_DATA;           // "data"
```

## FSGDynamicTextAssetSerializerBase

The abstract base provides three protected virtual helpers that convert between UPROPERTYs and `FJsonValue` intermediates:

### SerializePropertyToValue

```cpp
virtual TSharedPtr<FJsonValue> SerializePropertyToValue(FProperty* Property, const void* ValuePtr) const;
```

Default implementation: `FJsonObjectConverter::UPropertyToJsonValue()`. Override to convert properties directly to your target format without the JSON intermediate step.

### DeserializeValueToProperty

```cpp
virtual bool DeserializeValueToProperty(const TSharedPtr<FJsonValue>& Value, FProperty* Property, void* ValuePtr) const;
```

Default implementation: `FJsonObjectConverter::JsonValueToUProperty()`. Override to read properties directly from your format.

### ShouldSerializeProperty

```cpp
virtual bool ShouldSerializeProperty(const FProperty* Property) const;
```

Default implementation excludes:
- **Metadata properties** (`DynamicTextAssetId`, `Version`, `UserFacingId`): stored as wrapper-level fields, not in the data block. Uses `USGDynamicTextAsset::GetMetadataPropertyNames()` with `GET_MEMBER_NAME_CHECKED` for compile-time safety.
- **`CPF_Deprecated` properties**: silently ignored during both serialization and deserialization.

Override to add format-specific or class-specific filtering rules.

## Implementing a Custom Serializer

### Step 1: Subclass

```cpp
class FMyCustomSerializer : public FSGDynamicTextAssetSerializerBase
{
public:
    virtual FString GetFileExtension() const override { return TEXT(".dta.custom"); }
    virtual FText GetFormatName() const override { return NSLOCTEXT("MySerializer", "Name", "Custom"); }
    virtual FText GetFormatDescription() const override { return NSLOCTEXT("MySerializer", "Desc", "My custom format"); }
    virtual uint32 GetSerializerTypeId() const override { return 100; } // >= 100 for third-party

    virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const override;
    virtual bool DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const override;
    virtual bool ValidateStructure(const FString& InString, FString& OutErrorMessage) const override;
    virtual bool ExtractMetadata(const FString& InString, FSGDynamicTextAssetId& OutId,
        FString& OutClassName, FString& OutUserFacingId, FString& OutVersion,
        FSGDynamicTextAssetTypeId& OutAssetTypeId) const override;
    virtual bool UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const override;
    virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass,
        const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const override;
};
```

### Step 2: Implement Core Methods

Your `SerializeProvider()` and `DeserializeProvider()` implementations can use the base class helpers:

```cpp
bool FMyCustomSerializer::SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const
{
    const UObject* obj = Cast<UObject>(Provider);
    // Iterate properties using ShouldSerializeProperty() filter
    for (TFieldIterator<FProperty> it(obj->GetClass()); it; ++it)
    {
        if (!ShouldSerializeProperty(*it)) continue;

        const void* valuePtr = it->ContainerPtrToValuePtr<void>(obj);
        TSharedPtr<FJsonValue> jsonValue = SerializePropertyToValue(*it, valuePtr);
        // Convert jsonValue to your format...
    }
    // Build output string in your format
    return true;
}
```

### Step 3: Implement ExtractMetadata

This must be lightweight because it is called frequently for file browsing and reference scanning without fully deserializing:

```cpp
bool FMyCustomSerializer::ExtractMetadata(const FString& InString,
    FSGDynamicTextAssetId& OutId, FString& OutClassName,
    FString& OutUserFacingId, FString& OutVersion,
    FSGDynamicTextAssetTypeId& OutAssetTypeId) const
{
    // Parse only the metadata/header section of your format
    // Populate: OutId, OutClassName, OutUserFacingId, OutVersion, OutAssetTypeId
    return true;
}
```

## Registration

Register during module startup, unregister during shutdown:

```cpp
void FMyModule::StartupModule()
{
    FSGDynamicTextAssetFileManager::RegisterSerializer<FMyCustomSerializer>();
}

void FMyModule::ShutdownModule()
{
    FSGDynamicTextAssetFileManager::UnregisterSerializer<FMyCustomSerializer>();
}
```

### How Registration Works

`RegisterSerializer<T>()` internally:
1. Creates a `TSharedRef<T>` instance
2. Extracts its file extension and TypeId
3. Checks for TypeId collision (fatal error if duplicate)
4. Stores in both `REGISTERED_SERIALIZERS` (by extension) and `REGISTERED_SERIALIZERS_BY_ID` (by TypeId)

## File Metadata

`FSGDynamicTextAssetFileMetadata` is the struct returned by lightweight header parsing:

```cpp
struct FSGDynamicTextAssetFileMetadata
{
    bool bIsValid = false;
    FSGDynamicTextAssetId Id;
    FSGDynamicTextAssetTypeId AssetTypeId;
    FString UserFacingId;
    FString ClassName;
    uint32 SerializerTypeId;
};
```

Used by the browser, file manager, and reference scanner for fast file inspection without full deserialization. Populated via `FSGDynamicTextAssetFileManager::ExtractMetadataFromFile()`.

## File Format Conversion

In editor builds, files can be converted between formats:

```cpp
// Editor-only
static bool ConvertFileFormat(const FString& SourceFilePath, const FString& TargetExtension, FString& OutNewFilePath);
```

The conversion flow:
1. Read source file
2. Deserialize into an in-memory provider instance using the source serializer
3. Re-serialize using the target format's serializer
4. Write the new file
5. Delete the old file

All metadata (GUID, UserFacingId, Version, AssetTypeId) is preserved through the round-trip. **Source control operations are NOT handled.** The caller is responsible for marking files for add/delete.

## Serializer Discovery

The file manager provides several lookup methods:

```cpp
// Find by file extension (case-insensitive)
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForExtension(FStringView Extension);

// Find by integer TypeId (used for binary file routing)
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForTypeId(uint32 TypeId);

// Find by file path (extracts double extension like ".dta.json")
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForFile(const FString& FilePath);

// Get TypeId for a known extension
static uint32 GetTypeIdForExtension(const FString& Extension);

// List all registered extensions
static void GetAllRegisteredExtensions(TArray<FString>& OutExtensions);

// List all registered serializer instances
static TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> GetAllRegisteredSerializers();

// Diagnostic descriptions ("TypeId=N | Extension='ext' | Format='name'")
static void GetAllRegisteredSerializerDescriptions(TArray<FString>& OutDescriptions);
```

## File Format Versioning

Custom serializers should implement file format versioning to support future structural changes to the file format.

### Required Override

Override `GetFileFormatVersion()` to return your format's current version:

```cpp
FSGDynamicTextAssetVersion GetFileFormatVersion() const override
{
    // Start at 1.0.0, increment when your format structure changes
    return FSGDynamicTextAssetVersion(1, 0, 0);
}
```

### Required Override: UpdateFileFormatVersion

Override `UpdateFileFormatVersion()` to update the version stamp in your format's raw file content. This is called by the migration pipeline after structural migration succeeds:

```cpp
bool UpdateFileFormatVersion(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& NewVersion) const override
{
    // Use regex or string replacement to find and update the
    // fileFormatVersion field in your format's syntax.
    // Return true on success, false if the field was not found.
}
```

> You don't have to use regex, I just used it for simplicity and performance.
> You can use any approach you want because its not a runtime process.

### Optional Override: MigrateFileFormat

Override `MigrateFileFormat()` when you introduce structural changes to your format (renamed keys, reorganized blocks, etc.). The default implementation returns `true` (no structural changes needed):

```cpp
bool MigrateFileFormat(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& CurrentFormatVersion,
    const FSGDynamicTextAssetVersion& TargetFormatVersion) const override
{
    // Apply structural transformations to InOutFileContents.
    // The version stamp is updated separately by UpdateFileFormatVersion().
    return true;
}
```

### Migration Pipeline Flow

When the editor detects outdated files (major version mismatch), the migration runs:

1. `MigrateFileFormat()` applies structural changes to the file content
2. `UpdateFileFormatVersion()` stamps the new version into the file
3. The updated content is written back to disk

This separation ensures that structural migration logic and version bookkeeping are independent concerns.

[Back to Table of Contents](../TableOfContents.md)
