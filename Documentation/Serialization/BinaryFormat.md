# Binary Format

[Back to Table of Contents](../TableOfContents.md)

## Overview

The binary format (`.dta.bin`) is a compressed representation of dynamic text assets used in packaged/cooked builds. It consists of a fixed 80 byte header followed by a compressed serialized payload.

## Cooked Output Path

During the cook process, source files (`.dta.json`, `.dta.xml`, `.dta.yaml`) are serialized and compressed into binary (`.dta.bin`) files. The binary files are stored in a separate directory from the source files.

| Build Stage | Path | Example |
|-------------|------|---------|
| Development source | `Content/SGDynamicTextAssets/{ClassPath}/` | `Content/SGDynamicTextAssets/WeaponData/excalibur.dta.json` |
| Cooked binary | `Content/SGDynamicTextAssetsCooked/` | `Content/SGDynamicTextAssetsCooked/{GUID}.dta.bin` |
| Packaged build | `{PackagedRoot}/Content/SGDynamicTextAssetsCooked/` | `MyGame/Content/SGDynamicTextAssetsCooked/{GUID}.dta.bin` |

The cooked root path is defined by `FSGDynamicTextAssetFileManager::GetCookedDynamicTextAssetsRootPath()`, which returns `{ProjectContentDir}/SGDynamicTextAssetsCooked/`. Binary files are named by their GUID (e.g., `A1B2C3D4-E5F6-7890-ABCD-EF1234567890.dta.bin`).

### Cooking Settings

The following settings on `USGDynamicTextAssetSettingsAsset` control cooking behavior:

| Setting | Default | Description |
|---------|---------|-------------|
| `bCleanCookedDirectoryBeforeCook` | `true` | Deletes all files in the cooked directory before cooking, preventing stale `.dta.bin` files from deleted assets |
| `bDeleteCookedAssetsAfterPackaging` | `true` | Removes cooked `.dta.bin` files from the Content directory after packaging completes, preventing generated files from cluttering the Content Browser |
| `DefaultCompressionMethod` | `Zlib` | Compression algorithm used for the binary payload |

See [Settings](../Configuration/Settings.md) for full configuration details.

## File Structure

```
[Header - 80 bytes]
  Offset  Size  Field
  0x00    5     MagicNumber: "SGDTA" (0x53 0x47 0x44 0x54 0x41)
  0x05    3     Padding (zeros)
  0x08    4     PluginSchemaVersion (uint32, currently 1)
  0x0C    4     CompressionType (uint32, maps to ESGDynamicTextAssetCompressionMethod)
  0x10    4     UncompressedSize (uint32, payload byte count before compression)
  0x14    4     CompressedSize (uint32, payload byte count after compression)
  0x18    4     SerializerTypeId (uint32, identifies which serializer produced the payload)
  0x1C    16    Guid (FGuid, 4x uint32)
  0x2C    16    AssetTypeGuid (FGuid, class identity for type resolution without decompression)
  0x3C    20    ContentHash (SHA-1 of compressed payload)
[Compressed Data]
  0x50    ...   Compressed payload bytes (CompressedSize bytes)
```

Total header size: 80 bytes. Uses `#pragma pack(push, 1)` for cross platform binary layout.

## FSGBinaryDynamicTextAssetHeader

```cpp
struct FSGBinaryDynamicTextAssetHeader
{
    uint8  MagicNumber[5];          // "SGDTA"
    uint8  MagicNumberPadding[3];   // Alignment padding
    uint32 PluginSchemaVersion;     // Currently 1
    uint32 CompressionType;         // ESGDynamicTextAssetCompressionMethod
    uint32 UncompressedSize;        // Original payload size
    uint32 CompressedSize;          // Compressed payload size
    uint32 SerializerTypeId;        // Which serializer produced the payload
    FGuid  Guid;                    // Dynamic text asset GUID
    FGuid  AssetTypeGuid;           // Class identity GUID for type resolution
    uint8  ContentHash[20];         // SHA-1 of compressed payload
};
```

### Validation

```cpp
bool IsValid() const;                         // Checks magic number
bool HasContentHash() const;                  // True if ContentHash is non zero
bool HasAssetTypeGuid() const;                // True if AssetTypeGuid is non zero
FString GetMagicNumberString() const;         // Hex string from instance
static FString GetExpectedMagicNumberString(); // Expected hex string
```

## Compression Methods

```cpp
UENUM(BlueprintType)
enum class ESGDynamicTextAssetCompressionMethod : uint8
{
    None   = 0   UMETA(DisplayName = "None"),
    Zlib   = 1   UMETA(DisplayName = "Zlib (Default)"),
    Gzip   = 2   UMETA(DisplayName = "Gzip"),
    LZ4    = 3   UMETA(DisplayName = "LZ4 (Fast)"),
    Custom = 255 UMETA(DisplayName = "Custom")
};
```

The default compression method is configurable in `USGDynamicTextAssetSettingsAsset::DefaultCompressionMethod`.
The plugin uses Unreal's `FCompression` functions for executing compression/decompression,
which allows you to extend additional compression methods if needed with minimal effort.

### Custom Compression

The `Custom` value (255) allows projects to use any compression format registered with Unreal's `FCompression` system. When `Custom` is selected:

1. Set `DefaultCompressionMethod` to `Custom` in your settings asset.
2. Set `CustomCompressionName` to the FName of the compression format (e.g., `"Oodle"`).
3. The binary serializer resolves the format name via `USGDynamicTextAssetSettingsAsset::GetCustomCompressionName()`, which is virtual and can be overridden in a settings asset subclass for dynamic resolution.

See [Settings](../Configuration/Settings.md) for configuration details.

### Compression Format Resolution

The binary serializer converts between `ESGDynamicTextAssetCompressionMethod` enum values and Unreal's `FName` based compression API:

```cpp
// Private helpers in FSGDynamicTextAssetBinarySerializer
static FName GetCompressionFormatName(ESGDynamicTextAssetCompressionMethod CompressionMethod);
static ESGDynamicTextAssetCompressionMethod GetCompressionMethodFromFormatName(FName FormatName);
```

The mapping is:

| Enum Value | FName |
|------------|-------|
| `None` | `NAME_None` |
| `Zlib` | `"ZLIB"` |
| `Gzip` | `"GZIP"` |
| `LZ4` | `"LZ4"` |
| `Custom` | Value from `USGDynamicTextAssetSettingsAsset::GetCustomCompressionName()` |

## FSGDynamicTextAssetBinarySerializer

A static utility class for binary serialization.

### Conversion

```cpp
// Serialized string -> compressed binary
static bool JsonToBinary(const FString& JsonString, const FSGDynamicTextAssetId& Id,
    TArray<uint8>& OutBinaryData,
    ESGDynamicTextAssetCompressionMethod CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib);

// Compressed binary -> serialized string
static bool BinaryToJson(const TArray<uint8>& BinaryData, FString& OutJsonString);
```

### File I/O

```cpp
static bool ReadBinaryFile(const FString& FilePath, TArray<uint8>& OutBinaryData);
static bool WriteBinaryFile(const FString& FilePath, const TArray<uint8>& BinaryData);

// Convert source file directly to binary file
static bool ConvertJsonFileToBinary(const FString& JsonFilePath, const FString& BinaryFilePath,
    ESGDynamicTextAssetCompressionMethod CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib);
```

### Header Only Access

Read metadata without decompressing the payload (located in `FSGDynamicTextAssetBinarySerializer`):

```cpp
static bool ReadHeader(const TArray<uint8>& BinaryData, FSGBinaryDynamicTextAssetHeader& OutHeader);
static bool ExtractId(const TArray<uint8>& BinaryData, FSGDynamicTextAssetId& OutId);
static bool IsValidBinaryData(const TArray<uint8>& BinaryData);

// Returns ".dta.bin"
static FString GetFileExtension();
```

## Content Hash Integrity

On write, a SHA-1 hash is computed over the compressed payload bytes and stored in the header's `ContentHash` field.
On read, the hash is recomputed and compared. A mismatch causes the load to fail, detecting file corruption and casual tampering.

If the `ContentHash` field is all zeros, hash validation is skipped.

## Safety Limits

```cpp
// 80 MB
static constexpr uint32 MAX_UNCOMPRESSED_SIZE = FSGBinaryDynamicTextAssetHeader::HEADER_SIZE * 1024 * 1024;
```

The `UncompressedSize` header field is checked against this limit before allocating memory, preventing out of memory conditions from crafted headers.

`FSGBinaryDynamicTextAssetHeader` has static constants declared for specifying these safety bounds
that requires plugin source modification to change:
```cpp
/**
 * 5 bytes for "SGDTA".
 * (0x53 = 'S', 0x47 = 'G', 0x44 = 'D', 0x54 = 'T', 0x41 = 'A')
 *
 * Increment CURRENT_SCHEMA_VERSION if this changes.
 * Expected magic number value as a char array
 */
static constexpr uint8 MAGIC_NUMBER[] = { 0x53, 0x47, 0x44, 0x54, 0x41 };

/** Current plugin schema version */
static constexpr uint32 CURRENT_SCHEMA_VERSION = 1;

/** Size of the SHA-1 content hash in bytes (160 bits) */
static constexpr uint32 CONTENT_HASH_SIZE = 20;

/** Total header size in bytes */
static constexpr uint32 HEADER_SIZE = 80;
```

[Back to Table of Contents](../TableOfContents.md)
