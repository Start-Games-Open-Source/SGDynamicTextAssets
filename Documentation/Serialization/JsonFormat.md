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
  "sgFileInformation": {
    "type": "UWeaponData",
    "version": "1.0.0",
    "id": "A1B2C3D4-E5F6-7890-ABCD-EF1234567890",
    "userfacingid": "excalibur",
    "fileFormatVersion": "1.0.0"
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

All identity fields are nested under a `"sgFileInformation"` object at the root level, alongside the `"data"` object.

| Field | Key | Type | Description |
|-------|-----|------|-------------|
| Type | `type` | String | The `FSGDynamicTextAssetTypeId` GUID when available (preferred), or the UClass name including prefix as a fallback (e.g., `"UWeaponData"`) |
| Version | `version` | String | Semantic version in `"Major.Minor.Patch"` format |
| ID | `id` | String | The unique identifier in standard GUID format (maps to `FSGDynamicTextAssetId`) |
| User Facing ID | `userfacingid` | String | Human-readable identifier for display and lookups |
| File Format Version | `fileFormatVersion` | String | Serializer format structural version in `"Major.Minor.Patch"` format. Defaults to `"1.0.0"` if absent. Written automatically on save. |

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
| `ExtractFileInfo` | Extracts all four file information fields without full deserialization, useful for scanning and indexing |
| `UpdateFieldsInPlace` | Updates metadata fields within an already-serialized JSON string in-place (used by Rename and Duplicate operations) |
| `GetDefaultFileContent` | Generates the initial JSON file content for a new dynamic text asset with metadata and empty data block |
| `GetSerializerTypeId` | Returns `1` |

### Key Constants

Metadata keys are defined as static constants on `ISGDynamicTextAssetSerializer` and shared across all serializer formats:

| Constant | Value | Description |
|----------|-------|-------------|
| `KEY_FILE_INFORMATION` | `"sgFileInformation"` | Wrapper key for the file information block |
| `KEY_METADATA_LEGACY` | `"metadata"` | Legacy wrapper key (backward compat) |
| `KEY_TYPE` | `"type"` | Class type name key |
| `KEY_VERSION` | `"version"` | Semantic version key |
| `KEY_ID` | `"id"` | GUID key |
| `KEY_USER_FACING_ID` | `"userfacingid"` | Human-readable identifier key |
| `KEY_FILE_FORMAT_VERSION` | `"fileFormatVersion"` | File format structural version |
| `KEY_DATA` | `"data"` | Property data block key |
| `KEY_SGDT_ASSET_BUNDLES` | `"sgdtAssetBundles"` | Asset bundle metadata block key |

### Registration

The JSON serializer is registered automatically in `SGDynamicTextAssetsRuntimeModule::StartupModule()` via:

```cpp
FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetJsonSerializer>();
```

See [SerializerInterface.md](SerializerInterface.md) for the full registration pattern.

## Asset Bundle Metadata

When a dynamic text asset has soft reference properties tagged with `meta=(AssetBundles="...")`, the serializer writes an `sgdtAssetBundles` block at the root level alongside `metadata` and `data`. This block is a snapshot of the bundle data extracted from the object's UPROPERTY meta tags.

### Format

The `sgdtAssetBundles` object uses bundle names as keys. Each key maps to an array of entries, where each entry has a `property` name and a `path` (the soft object path value).

```json
{
  "sgFileInformation": { ... },
  "data": { ... },
  "sgdtAssetBundles": {
    "Visual": [
      { "property": "UWeaponData.MeshAsset", "path": "/Game/Weapons/Meshes/Sword.Sword" },
      { "property": "UWeaponData.ImpactMaterial", "path": "/Game/Weapons/Materials/ImpactMat.ImpactMat" }
    ],
    "Audio": [
      { "property": "UWeaponData.ImpactMaterial", "path": "/Game/Weapons/Materials/ImpactMat.ImpactMat" },
      { "property": "UWeaponData.FireSound", "path": "/Game/Audio/Weapons/FireSFX.FireSFX" }
    ]
  }
}
```

The `property` field uses qualified `OwnerClass.PropertyName` format to disambiguate same-named properties from different types (e.g., a base class and an instanced sub-object).

### Behavior

- The block is only written if the object has at least one bundled soft reference with a valid (non-null) path.
- Properties tagged with multiple bundles (e.g., `meta=(AssetBundles="Visual,Audio")`) appear in each named bundle.
- Properties without the `AssetBundles` meta tag are not included.
- Container properties (`TArray`, `TMap`, `TSet`) tagged with `AssetBundles` propagate their bundle names to inner soft reference elements.
- During deserialization in **editor builds**, the `sgdtAssetBundles` block is informational only - bundle data is re-extracted from UPROPERTY meta tags after properties are populated. In **packaged builds**, this block is the primary source since property metadata is stripped.

### Extraction Without Full Deserialization

The `ExtractSGDTAssetBundles()` method on the serializer can parse the `sgdtAssetBundles` block from a JSON string without deserializing the full object. This is useful for cook pipelines and editor tooling that need bundle information from files without instantiating C++ objects.

### Key Constant

| Constant | Value | Description |
|----------|-------|-------------|
| `KEY_SGDT_ASSET_BUNDLES` | `"sgdtAssetBundles"` | Root-level key for the asset bundle metadata block |

## Instanced Object Serialization

Properties declared with `UPROPERTY(Instanced)` are serialized inline within the `data` block. Each instanced sub-object is represented as a JSON object containing a reserved `SG_INST_OBJ_CLASS` key that stores the full class path, followed by the sub-object's own UPROPERTY fields.

### Reserved Key

| Key | Value | Description |
|-----|-------|-------------|
| `SG_INST_OBJ_CLASS` | Full class path string | Identifies the runtime UClass for deserialization |

The class path is produced by `UClass::GetPathName()`:
- **C++ classes:** `/Script/ModuleName.ClassName` (e.g., `/Script/MyGame.UFireDamageConfig`)
- **Blueprint classes:** `/Game/Path/BP_Name.BP_Name_C` (e.g., `/Game/Configs/BP_IceDamage.BP_IceDamage_C`)

Deserialization resolves the class via `LoadObject<UClass>`, which handles both formats and triggers asset loading for Blueprint classes not yet in memory.

This key is defined as a static constant on `FSGDynamicTextAssetSerializerBase`:

```cpp
static const FString INSTANCED_OBJECT_CLASS_KEY; // = "SG_INST_OBJ_CLASS"
```

### Single Instanced Object

A non-null instanced object is a JSON object with `SG_INST_OBJ_CLASS` plus its properties:

```json
{
  "data": {
    "DamageConfig": {
      "SG_INST_OBJ_CLASS": "/Script/MyGame.UFireDamageConfig",
      "BaseDamage": 50.0,
      "BurnDuration": 3.0
    }
  }
}
```

A null (unset) instanced object is `null`:

```json
{
  "data": {
    "DamageConfig": null
  }
}
```

### Polymorphic Instanced Objects

Since `SG_INST_OBJ_CLASS` stores the actual runtime type, a property typed as a base class can hold any subclass:

```json
{
  "data": {
    "DamageConfig": {
      "SG_INST_OBJ_CLASS": "/Script/MyGame.UPoisonDamageConfig",
      "BaseDamage": 25.0,
      "PoisonStacks": 5,
      "TickInterval": 1.5
    }
  }
}
```

Here `DamageConfig` is declared as `UPROPERTY(Instanced) UDamageConfig*` but holds a `UPoisonDamageConfig` subclass instance.

### Array of Instanced Objects

Arrays support mixed types and null entries:

```json
{
  "data": {
    "StatusEffects": [
      {
        "SG_INST_OBJ_CLASS": "/Script/MyGame.UBurnEffect",
        "Duration": 5.0,
        "DamagePerTick": 10.0
      },
      {
        "SG_INST_OBJ_CLASS": "/Game/Effects/BP_FreezeEffect.BP_FreezeEffect_C",
        "Duration": 3.0,
        "SlowPercent": 0.5
      },
      null
    ]
  }
}
```

### Map with Instanced Object Values

Maps with instanced object values follow the standard UE JSON map format, where each value entry can be an instanced object:

```json
{
  "data": {
    "ElementalConfigs": {
      "Fire": {
        "SG_INST_OBJ_CLASS": "/Script/MyGame.UFireConfig",
        "Intensity": 1.0
      },
      "Ice": {
        "SG_INST_OBJ_CLASS": "/Script/MyGame.UIceConfig",
        "FreezeChance": 0.3
      }
    }
  }
}
```

### Nested Instanced Objects

Instanced objects can contain their own instanced sub-objects. Nesting is handled recursively:

```json
{
  "data": {
    "WeaponBehavior": {
      "SG_INST_OBJ_CLASS": "/Script/MyGame.UMeleeWeaponBehavior",
      "ComboCount": 3,
      "HitEffect": {
        "SG_INST_OBJ_CLASS": "/Script/MyGame.USlashEffect",
        "ParticleScale": 1.5
      }
    }
  }
}
```

### Excluded Properties

The following properties are excluded from instanced object serialization (same rules as the parent DTA):
- Base metadata properties (`DynamicTextAssetId`, `UserFacingId`, `Version`)
- Properties flagged as `CPF_Deprecated`

The `ShouldSerializeProperty` filter on `FSGDynamicTextAssetSerializerBase` controls this behavior and can be overridden by custom serializers.

[Back to Table of Contents](../TableOfContents.md)
