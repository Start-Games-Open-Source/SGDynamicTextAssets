# Cook Manifest

[Back to Table of Contents](../TableOfContents.md)

## Purpose

The cook manifest (`dta_manifest.bin`) maps IDs to class names, asset type IDs, and user-facing IDs in packaged builds. It enables metadata queries and class-based lookups without decompressing binary files.

## File Location

```
Content/SGDynamicTextAssetsCooked/dta_manifest.bin
```

## Binary Format

The manifest uses a compact binary format with three sections: a fixed-size header, fixed-size entries, and a deduplicated string table.

### Header (32 bytes)

```
Offset  Size  Field
0       4     Magic "DTAM" (0x44 0x54 0x41 0x4D)
4       4     Version (uint32, currently 1)
8       4     EntryCount (uint32)
12      4     StringTableOffset (uint32, byte offset from file start)
16      4     Reserved (zero-filled, future use)
20      12    ContentHash (truncated SHA-1, entries + string table bytes)
```

Struct: `FSGDynamicTextAssetCookManifestBinaryHeader` (packed, `#pragma pack(push, 1)`)

### Entries (40 bytes each)

Immediately following the header, `EntryCount` entries are written sequentially:

```
Offset  Size  Field
0       16    AssetGuid (FGuid raw bytes - FSGDynamicTextAssetId)
16      16    AssetTypeGuid (FGuid raw bytes - FSGDynamicTextAssetTypeId)
32      4     ClassNameStringIndex (uint32 - index into string table)
36      4     UserFacingIdStringIndex (uint32 - index into string table, or 0xFFFFFFFF if absent)
```

### String Table

Located at the byte offset specified by `StringTableOffset` in the header:

```
uint32  Count (number of unique strings)
For each string:
  uint16  ByteLength (UTF-8 byte count, no null terminator)
  uint8[] UTF-8 bytes (ByteLength count)
```

Struct: `FSGDynamicTextAssetCookManifestStringTable`

Class names are highly repetitive (e.g., all weapons share `"UWeaponData"`). The string table stores each unique string once; entries reference by index. The sentinel value `0xFFFFFFFF` (`INVALID_STRING_INDEX`) represents absent fields (e.g., entries with no `UserFacingId`).

## Content Hash

The `ContentHash` field is a 12-byte truncated SHA-1 hash computed over the raw binary bytes of the entries and string table (everything after the 32-byte header):

1. SHA-1 is computed over the byte range from entries start to string table end.
2. The full 20-byte SHA-1 is truncated to the first 12 bytes.
3. The truncated hash is stored in the header at offset 20.

On load, the hash is recomputed from the loaded bytes and compared. **A mismatch causes load failure.** If the hash field is all zeros, hash validation is skipped (for forward compatibility).

## FSGDynamicTextAssetCookManifest

### Building the Manifest

During cooking, entries are added as each file is processed:

```cpp
FSGDynamicTextAssetCookManifest Manifest;
Manifest.AddEntry(Id, TEXT("UWeaponData"), TEXT("excalibur"), WeaponTypeId);
Manifest.AddEntry(Id2, TEXT("UWeaponData"), TEXT("iron_sword"), WeaponTypeId);

// Writes dta_manifest.bin with content hash
Manifest.SaveToFileBinary(OutputDirectory);
```

### Loading the Manifest

```cpp
FSGDynamicTextAssetCookManifest Manifest;
bool bLoaded = Manifest.LoadFromFileBinary(CookedDirectory);
// Reads binary format, verifies content hash, builds lookup indices
```

### Querying

```cpp
// O(1) ID lookup via TMap<FSGDynamicTextAssetId, int32>
const FSGDynamicTextAssetCookManifestEntry* Entry = Manifest.FindById(SomeId);

// Class-based lookup via TMap<FString, TArray<int32>>
TArray<const FSGDynamicTextAssetCookManifestEntry*> WeaponEntries;
Manifest.FindByClassName(TEXT("UWeaponData"), WeaponEntries);

// Asset Type ID lookup via TMap<FSGDynamicTextAssetTypeId, TArray<int32>>
TArray<const FSGDynamicTextAssetCookManifestEntry*> TypeEntries;
Manifest.FindByAssetTypeId(WeaponTypeId, TypeEntries);

// Iteration
const TArray<FSGDynamicTextAssetCookManifestEntry>& AllEntries = Manifest.GetEntries();
int32 Count = Manifest.Num();
```

### Entry Struct

```cpp
struct FSGDynamicTextAssetCookManifestEntry
{
    FSGDynamicTextAssetId Id;
    FString ClassName;
    FString UserFacingId;
    FSGDynamicTextAssetTypeId AssetTypeId;
};
```

## Performance

| Metric | Binary Format |
|--------|---------------|
| File size (10K entries) | ~500 KB |
| Parse time | ~5-10ms |
| Peak memory | ~1-2 MB (entries array) |
| Hash verification | Hash raw bytes directly |

Key advantages over the previous JSON format: raw GUIDs (16 bytes vs 36-char strings), string deduplication, no JSON DOM parser allocations, and direct byte-range hashing without re-serialization.

## Server Integration

The cook manifest is a **local-only, cooked-build artifact**. It does NOT participate in server communication. Server data flows through two separate channels:

1. **Type manifest overlays** (`FSGDynamicTextAssetTypeManifest::ApplyServerOverrides()`) for type-level changes
2. **Individual asset data** via `FetchAllDynamicTextAssets()` cached in `FSGDynamicTextAssetServerCache`

Server-provided assets bypass the cook manifest entirely. The subsystem resolves server data before consulting local cooked files. The binary format change has zero impact on server integration.

## Runtime Usage

The manifest is lazy-loaded at runtime via `FSGDynamicTextAssetFileManager::GetCookManifest()`. It is used by:

- `FindFileForId()`: O(1) lookup to build the binary file path from an ID.
- `FindAllFilesForClass()`: Class-based filtering without scanning the file system.
- `ExtractFileInfoFromFile()`: Quick file info access without decompressing binaries.

In editor builds, `GetCookManifest()` returns `nullptr`: the editor uses direct file system scanning instead.

[Back to Table of Contents](../TableOfContents.md)
