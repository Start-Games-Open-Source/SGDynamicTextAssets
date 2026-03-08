# JSON Format

[Back to Table of Contents](../TableOfContents.md)

## File Extension and Location

Dynamic text asset source files use the `.dta.json` extension and are stored under `Content/SGDynamicTextAssets/` in directories based on the class hierarchy:

```
Content/SGDynamicTextAssets/WeaponData/excalibur.dta.json
Content/SGDynamicTextAssets/WeaponData/SwordData/enchanted_blade.dta.json
Content/SGDynamicTextAssets/QuestData/main_quest_001.dta.json
```

## JSON Schema

Every `.dta.json` file follows this structure:

```json
{
  "metadata": {
    "type": "UWeaponData",
    "version": "1.0.0",
    "id": "A1B2C3D4-E5F6-7890-ABCD-EF1234567890",
    "userfacingid": "excalibur"
  },
  "data": {
    "DisplayName": "Excalibur",
    "BaseDamage": 50.0,
    "AttackSpeed": 1.2,
    "MeshAsset": "/Game/Meshes/Weapons/Excalibur.Excalibur"
  }
}
```

### Metadata Block

All identity fields are nested under a `"metadata"` object at the root level, alongside the `"data"` object.

| Field | Key | Type | Description |
|-------|-----|------|-------------|
| Type | `type` | String | The UClass name including prefix (e.g., `"UWeaponData"`) |
| Version | `version` | String | Semantic version in `"Major.Minor.Patch"` format |
| ID | `id` | String | The unique identifier in standard GUID format (maps to `FSGDynamicTextAssetId`) |
| User Facing ID | `userfacingid` | String | Human-readable identifier for display and lookups |

### Data Section

The `data` object contains all serialized UPROPERTY fields. The serializer uses Unreal's property reflection system (`FJsonObjectConverter`) to convert between UPROPERTY fields and JSON. Standard property types are supported:

- Primitives: `bool`, `int32`, `float`, `double`
- Strings: `FString`, `FName`, `FText`
- Math: `FVector`, `FRotator`, `FTransform`, `FColor`, `FLinearColor`
- Containers: `TArray`, `TMap`, `TSet`
- Soft references: `TSoftObjectPtr`, `TSoftClassPtr`
- Nested structs: Any `USTRUCT` with `UPROPERTY` fields
- Enums: Serialized as string names
- Dynamic text asset references: `FSGDynamicTextAssetRef` (serialized as GUID string)

> **Note:** `TSubclassOf` is a hard reference and is not supported. Use `TSoftClassPtr` instead for class references in dynamic text assets.

## FSGDynamicTextAssetJsonSerializer

The default serializer implementation for the `.dta.json` format. Inherits from `FSGDynamicTextAssetSerializerBase` and implements the full `ISGDynamicTextAssetSerializer` interface.

- **TypeId:** `1`
- **Extension:** `.dta.json`
- **Format Name:** JSON

### Interface Methods

All methods are inherited from `ISGDynamicTextAssetSerializer`. See [SerializerInterface.md](SerializerInterface.md) for the full interface contract.

| Method | Description |
|--------|-------------|
| `SerializeProvider` | Converts a provider's properties to formatted JSON with consistent indentation (UTF-8) |
| `DeserializeProvider` | Parses JSON, extracts metadata, checks version compatibility, triggers migration if needed, then populates UPROPERTY fields via reflection |
| `ValidateStructure` | Validates that a JSON string has the correct structure (`metadata` block with `type`/`id`/`version`/`userfacingid`, and `data` block) without creating or modifying any objects |
| `ExtractMetadata` | Extracts all four metadata fields without full deserialization, useful for scanning and indexing |
| `UpdateFieldsInPlace` | Updates metadata fields within an already-serialized JSON string in-place (used by Rename and Duplicate operations) |
| `GetDefaultFileContent` | Generates the initial JSON file content for a new dynamic text asset with metadata and empty data block |
| `GetSerializerTypeId` | Returns `1` |

### Key Constants

Metadata keys are defined as static constants on `ISGDynamicTextAssetSerializer` and shared across all serializer formats:

| Constant | Value | Description |
|----------|-------|-------------|
| `KEY_METADATA` | `"metadata"` | Wrapper key for the metadata block |
| `KEY_TYPE` | `"type"` | Class type name key |
| `KEY_VERSION` | `"version"` | Semantic version key |
| `KEY_ID` | `"id"` | GUID key |
| `KEY_USER_FACING_ID` | `"userfacingid"` | Human-readable identifier key |
| `KEY_DATA` | `"data"` | Property data block key |

### Registration

The JSON serializer is registered automatically in `SGDynamicTextAssetsRuntimeModule::StartupModule()` via:

```cpp
FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetJsonSerializer>();
```

See [SerializerInterface.md](SerializerInterface.md) for the full registration pattern.

[Back to Table of Contents](../TableOfContents.md)
