# File Manager

[Back to Table of Contents](../TableOfContents.md)

## FSGDynamicTextAssetFileManager

A static utility class for managing dynamic text asset file paths, scanning, reading, writing, cooked build support, and serializer registration. 
All methods are static; there is no instance.

```cpp
class FSGDynamicTextAssetFileManager
{
    // All members are static and have no instance
};
```

> **Runtime vs. Editor:** Nearly all FileManager functions are available in both runtime and editor builds. 
> Only `ShouldUseCookedDirectory()` and `GetCookManifest()` use `#if WITH_EDITOR` guards to change behavior between contexts. 
> All other functions are runtime available and adapt their behavior based on `ShouldUseCookedDirectory()`.

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_TEXT_EXTENSION` | `.dta.json` | Default file extension for text source files |
| `BINARY_EXTENSION` | `.dta.bin` | File extension for cooked binary files |
| `DEFAULT_RELATIVE_ROOT_PATH` | `SGDynamicTextAssets/` | Root directory relative to Content |

## Path Utilities (Runtime + Editor)

### Root Paths

```cpp
// Editor/Source Files ---

// Absolute: {ProjectContent}/SGDynamicTextAssets/
static FString GetDynamicTextAssetsRootPath();

// Relative: SGDynamicTextAssets/
static FString GetDynamicTextAssetsRelativeRootPath();

// Cooked Binary Files ---

// Absolute: {ProjectContent}/SGDynamicTextAssetsCooked/
static FString GetCookedDynamicTextAssetsRootPath();
```

### Class Based Paths

Files are organized by class hierarchy under the root:

```cpp
static FString GetFolderPathForClass(const UClass* DynamicTextAssetClass);
// e.g., "{Root}/WeaponData_{TypeId}/" or "{Root}/WeaponData_{TypeId}/SwordData_{TypeId}/"

static FString GetRelativeFolderPathForClass(const UClass* DynamicTextAssetClass);
// e.g., "SGDynamicTextAssets/WeaponData_{TypeId}/"
```

### Building File Paths

```cpp
// Source file path (extension defaults to DEFAULT_TEXT_EXTENSION)
static FString BuildFilePath(const UClass* DynamicTextAssetClass, const FString& UserFacingId,
    const FString& Extension = DEFAULT_TEXT_EXTENSION);
// e.g., "{Root}/WeaponData_{TypeId}/excalibur.dta.json"

static FString BuildRelativeFilePath(const UClass* DynamicTextAssetClass, const FString& UserFacingId,
    const FString& Extension = DEFAULT_TEXT_EXTENSION);

// Cooked: flat GUID-named path
static FString BuildBinaryFilePath(const FSGDynamicTextAssetId& Id);
// e.g., "{CookedRoot}/A1B2C3D4-E5F6-7890-ABCD-EF1234567890.dta.bin"
```

The `Extension` parameter accepts any registered extension (e.g., `.dta.json`, `.dta.xml`, `.dta.yaml`).

## File Scanning (Runtime + Editor)

These functions work in both editor and packaged builds. In cooked builds, they use the manifest and flat binary directory. In editor builds, they scan the hierarchical source directory.

### Find Files

```cpp
// All dynamic text asset files in the entire tree
static void FindAllDynamicTextAssetFiles(TArray<FString>& OutFilePaths);

// Files for a specific class (optionally including subclasses)
static void FindAllFilesForClass(const UClass* DynamicTextAssetClass,
    TArray<FString>& OutFilePaths, bool bIncludeSubclasses = true);

// Find file by ID
static bool FindFileForId(const FSGDynamicTextAssetId& Id, FString& OutFilePath,
    const UClass* SearchClass = nullptr);
```

`FindFileForId` is O(1) in cooked builds (uses the cook manifest) and O(N) in editor (scans files).

### File Information Extraction

```cpp
// Quick file info without full deserialization
static FSGDynamicTextAssetFileInfo ExtractFileInfoFromFile(const FString& FilePath);

// Extract UserFacingId from path
static FString ExtractUserFacingIdFromPath(const FString& FilePath);
// "excalibur" from ".../excalibur.dta.json"

// Check if path is a dynamic text asset file
static bool IsDynamicTextAssetFile(const FString& FilePath);

// Sanitize a UserFacingId for use as a filename
static FString SanitizeUserFacingId(const FString& UserFacingId);
```

## File I/O (Runtime + Editor)

### Reading

```cpp
static bool ReadRawFileContents(const FString& FilePath, FString& OutContents,
    uint32* OutSerializerTypeId = nullptr);
```

This method transparently handles both text and binary files:

- **Binary files (`.dta.bin`):** Reads the binary data, decompresses it, returns the text payload, and writes the `SerializerTypeId` from the binary header to `OutSerializerTypeId`. Pass the TypeId to `FindSerializerForTypeId()` to obtain the correct deserializer.
- **Text files (`.dta.json`, `.dta.xml`, `.dta.yaml`):** Reads the text directly and sets `OutSerializerTypeId` to `0`.

### Writing

```cpp
static bool WriteRawFileContents(const FString& FilePath, const FString& Contents);
```

Creates parent directories if they don't exist.

### Create, Delete, Rename, Duplicate

```cpp
static bool CreateDynamicTextAssetFile(const UClass* DynamicTextAssetClass,
    const FString& UserFacingId, const FString& Extension,
    FSGDynamicTextAssetId& OutId, FString& OutFilePath);

static bool DeleteDynamicTextAssetFile(const FString& FilePath);

static bool RenameDynamicTextAsset(const FString& SourceFilePath,
    const FString& NewUserFacingId, FString& OutNewFilePath);

static bool DuplicateDynamicTextAsset(const FString& SourceFilePath,
    const FString& NewUserFacingId, FSGDynamicTextAssetId& OutNewId, FString& OutNewFilePath);

static bool FileExists(const FString& FilePath);
static bool EnsureFolderExistsForClass(const UClass* DynamicTextAssetClass);
```

`CreateDynamicTextAssetFile` accepts an `Extension` parameter to determine the file format. It looks up the registered serializer for that extension and passes it to the `ON_GENERATE_DEFAULT_CONTENT` delegate when generating the initial file content.

`RenameDynamicTextAsset` moves the file and updates the `userfacingid` metadata field in-place via the serializer's `UpdateFieldsInPlace` method. The GUID remains unchanged.

`DuplicateDynamicTextAsset` copies the file with a new `FSGDynamicTextAssetId` and `UserFacingId`.

## Format Conversion (Editor Only)

```cpp
#if WITH_EDITOR
static bool ConvertFileFormat(const FString& SourceFilePath, const FString& TargetExtension, FString& OutNewFilePath);
#endif
```

Converts a dynamic text asset file from one serialization format to another (e.g., JSON to XML, YAML to JSON). The conversion process:

1. Reads the source file and deserializes into an in-memory provider instance
2. Re-serializes using the target format's serializer
3. Writes the new file (same directory, same UserFacingId, different extension)
4. Deletes the old source file

All metadata (GUID, UserFacingId, Version, AssetTypeId) is preserved through the round-trip.

**Important:** Source control operations (mark-for-add, mark-for-delete) are NOT handled by this method. The caller is responsible for managing source control state. The browser's `ConvertSelectedItems()` handles source control when invoking this from the UI.

**Parameters:**
- `SourceFilePath`: Absolute path to the source file
- `TargetExtension`: Target format extension (e.g., `.dta.xml`, `.dta.yaml`)
- `OutNewFilePath`: Output for the new file's absolute path

**Returns:** `true` if the conversion succeeded

## Default Content Delegate

```cpp
DECLARE_DELEGATE_RetVal_FourParams(FString, FSGDataGenerateDefaultContentDelegate,
    // DynamicTextAssetClass
    const UClass*,
    // Id
    const FSGDynamicTextAssetId&,
    // UserFacingId
    const FString&,
    // Serializer
    TSharedRef<ISGDynamicTextAssetSerializer>);

static FSGDataGenerateDefaultContentDelegate ON_GENERATE_DEFAULT_CONTENT;
```

Fired by `CreateDynamicTextAssetFile` after a new ID is generated. 
The fourth parameter is the registered `ISGDynamicTextAssetSerializer` for the file's extension. 
The default binding (set in `SGDynamicTextAssetsRuntimeModule::StartupModule`) calls `Serializer->GetDefaultFileContent(DynamicTextAssetClass, Id, UserFacingId)` to produce format-appropriate default content.

Override this delegate to customize the initial file content for new dynamic text assets. 
The serializer is provided so your override can produce format-correct output regardless of which extension was chosen.

## Serializer Registry

The file manager includes a pluggable serializer registry that enables custom serialization formats. 
Serializers implement the `ISGDynamicTextAssetSerializer` interface and are registered by both file extension and integer TypeId.

### Registering Serializers

```cpp
// Register by type (T must implement ISGDynamicTextAssetSerializer)
template<typename T>
static void RegisterSerializer();

// Unregister by type
template<typename T>
static void UnregisterSerializer();
```

Built-in serializers are registered automatically during `SGDynamicTextAssetsRuntimeModule::StartupModule()`:

| Serializer | TypeId | Extension |
|------------|--------|-----------|
| `FSGDynamicTextAssetJsonSerializer` | 1 | `.dta.json` |
| `FSGDynamicTextAssetXmlSerializer` | 2 | `.dta.xml` |
| `FSGDynamicTextAssetYamlSerializer` | 3 | `.dta.yaml` |

See [Serializer Interface](../Serialization/SerializerInterface.md) for the full interface contract and custom serializer guide.

### Format Dispatch by Extension

The file manager selects the correct serializer based on file extension:

```cpp
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForExtension(FStringView Extension);
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForFile(const FString& FilePath);
static FString GetSupportedExtensionForFile(const FString& FilePath);
```

`FindSerializerForFile` extracts the double extension from the file path (e.g., `dta.json` from `excalibur.dta.json`) and delegates to `FindSerializerForExtension`. 
Returns `nullptr` if no serializer is registered for the extension.

`GetSupportedExtensionForFile` returns the canonical extension string (e.g., `.dta.json`) if the file matches a registered format, or an empty string otherwise.

### Format Dispatch by TypeId

Binary files (`.dta.bin`) embed a `SerializerTypeId` in their header that identifies which serializer produced the payload. 
Use these functions to route the payload to the correct deserializer:

```cpp
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForTypeId(uint32 TypeId);
static uint32 GetTypeIdForExtension(const FString& Extension);
```

`FindSerializerForTypeId` returns the serializer registered with the given integer TypeId, or `nullptr` if not found. 
This is used internally by the loading flow when reading cooked binary files.

`GetTypeIdForExtension` converts a known extension string (e.g., `"dta.json"`) to the corresponding TypeId (e.g., `1`). 
Returns `0` if no serializer is registered for that extension.

### Querying Registered Serializers

```cpp
static void GetAllRegisteredExtensions(TArray<FString>& OutExtensions);
static void GetAllRegisteredSerializerDescriptions(TArray<FString>& OutDescriptions);
static TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> GetAllRegisteredSerializers();
```

`GetAllRegisteredExtensions` populates an array with all registered extension strings.

`GetAllRegisteredSerializerDescriptions` builds a diagnostic string for each registered serializer, 
formatted as `"TypeId=N | Extension='ext' | Format='name'"`. 
Used by `USGDynamicTextAssetStatics::LogRegisteredSerializers()`.

`GetAllRegisteredSerializers` returns all currently registered serializer instances.

## Cooked Build Support (Behavior Changes by Context)

In packaged builds, dynamic text assets are stored as flat GUID-named binary files with a cook manifest:

```cpp
static bool ShouldUseCookedDirectory();
// Returns true in packaged builds (#if WITH_EDITOR returns false, else returns true)

static const FSGDynamicTextAssetCookManifest* GetCookManifest();
// In editor: returns nullptr (no manifest)
// In packaged builds: lazy-loads the manifest from the cooked directory
```

When `ShouldUseCookedDirectory()` returns true, file lookups use the cooked directory and manifest for O(1) ID-based access instead of scanning the hierarchical directory tree. 
See [Binary Format](../Serialization/BinaryFormat.md) for the cooked output path and binary file structure.

Binary file header details and encoding parameters are defined in separate headers: `SGBinaryDynamicTextAssetHeader.h` defines the 80-byte binary file header struct, and `SGBinaryEncodeParams.h` defines the encoding parameters used during binary compression. See [Binary Format](../Serialization/BinaryFormat.md) for full details.

## File Information Extraction

`ExtractFileInfoFromFile()` provides lightweight file inspection without full deserialization:

```cpp
static FSGDynamicTextAssetFileInfo ExtractFileInfoFromFile(const FString& FilePath);
```

Returns an `FSGDynamicTextAssetFileInfo` struct:

```cpp
struct FSGDynamicTextAssetFileInfo
{
    bool bIsValid = false;
    FSGDynamicTextAssetId Id;
    FSGDynamicTextAssetTypeId AssetTypeId;
    FString UserFacingId;
    FString ClassName;
    uint32 SerializerTypeId;
};
```

This is used by the browser for populating tile displays, the reference scanner for building dependency graphs, and the editor for resolving class types, all without the overhead of full property deserialization.

## Format Conversion

In editor builds, files can be converted between registered serialization formats:

```cpp
#if WITH_EDITOR
static bool ConvertFileFormat(const FString& SourceFilePath, const FString& TargetExtension, FString& OutNewFilePath);
#endif
```

The method reads the source file, deserializes into an in-memory provider, re-serializes using the target format, writes the new file, and deletes the old one. All metadata is preserved through the round-trip. The caller is responsible for source control operations (marking old file for delete, new file for add).

See [Custom Serializer Guide](../Advanced/CustomSerializerGuide.md) for implementation details.

## Serializer Discovery API

The file manager exposes several methods for querying registered serializers:

```cpp
// Find by extension (case-insensitive)
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForExtension(FStringView Extension);

// Find by TypeId (used for binary file routing)
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForTypeId(uint32 TypeId);

// Find by file path (extracts double extension)
static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForFile(const FString& FilePath);

// Convert extension to TypeId
static uint32 GetTypeIdForExtension(const FString& Extension);

// List all registered serializers
static TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> GetAllRegisteredSerializers();
static void GetAllRegisteredExtensions(TArray<FString>& OutExtensions);
static void GetAllRegisteredSerializerDescriptions(TArray<FString>& OutDescriptions);
```

[Back to Table of Contents](../TableOfContents.md)
