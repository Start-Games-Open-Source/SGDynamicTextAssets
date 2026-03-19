# YAML Format

[Back to Table of Contents](../TableOfContents.md)

## File Extension and Location

YAML dynamic text asset files use the `.dta.yaml` extension and are stored under `Content/SGDynamicTextAssets/` in directories based on the class hierarchy:

```
Content/SGDynamicTextAssets/WeaponData/excalibur.dta.yaml
Content/SGDynamicTextAssets/WeaponData/SwordData/enchanted_blade.dta.yaml
Content/SGDynamicTextAssets/QuestData/main_quest_001.dta.yaml
```

## YAML Schema

Every `.dta.yaml` file follows this structure:

```yaml
metadata:
  type: UWeaponData
  version: 1.0.0
  id: A1B2C3D4-E5F6-7890-ABCD-EF1234567890
  userfacingid: excalibur
  fileFormatVersion: 1.0.0
data:
  DisplayName: Excalibur
  BaseDamage: 50.0
  AttackSpeed: 1.2
  MeshAsset: /Game/Meshes/Weapons/Excalibur.Excalibur
```

### Metadata Block

The `metadata` mapping contains four keys with the same names used across all serializer formats:

| Key | Description |
|-----|-------------|
| `type` | The `FSGDynamicTextAssetTypeId` GUID when available (preferred), or the UClass name including prefix as a fallback (e.g., `UWeaponData`) |
| `version` | Semantic version in `Major.Minor.Patch` format |
| `id` | The unique identifier in standard GUID format |
| `userfacingid` | Human-readable identifier for display and lookups |
| `fileFormatVersion` | Serializer format structural version in `Major.Minor.Patch` format. Defaults to `1.0.0` if absent. Written automatically on save. |

### Data Section

The `data` mapping contains keys for each serialized UPROPERTY field. Property values are represented in standard YAML notation:

**Scalar values:**
```yaml
data:
  BaseDamage: 50.0
  WeaponName: Excalibur
  IsLegendary: true
```

**Arrays** use YAML sequence notation:
```yaml
data:
  Tags:
    - fast
    - heavy
```

**Nested structs** are represented as nested mappings:
```yaml
data:
  ArmorStats:
    baseArmor: 50
    maxDurability: 100
```

> **Note:** Nested struct field names use camelCase in YAML (e.g., `baseArmor` instead of `BaseArmor`) because `FJsonObjectConverter::UStructToJsonObject` applies `StandardizeCase()` which lowercases the first character. The YAML serializer handles this case-insensitively during deserialization.

**Maps** use nested mappings:
```yaml
data:
  DamageModifiers:
    Fire: 1.5
    Ice: 0.8
```

**Integer optimization:** Numeric values without fractional parts are stored as YAML integers for cleaner output. Values with fractional parts are stored as floats.

## FSGDynamicTextAssetYamlSerializer

The YAML serializer implementation. Inherits from `FSGDynamicTextAssetSerializerBase` and implements the full `ISGDynamicTextAssetSerializer` interface.

- **TypeId:** `3`
- **Extension:** `.dta.yaml`
- **Format Name:** YAML

### Implementation Details

- **Reading and writing:** Uses the [fkYAML](https://github.com/Start-Games/fkYAML) header-only C++ library for parsing and emitting YAML documents
- **Property conversion:** Uses the `FSGDynamicTextAssetSerializerBase` JSON-intermediate helpers (`SerializePropertyToValue` / `DeserializeValueToProperty`), keeping format-specific complexity contained to the YAML-to-FJsonValue bridge
- **String conversion:** FString to/from std::string conversion uses `StringCast<UTF8CHAR>` and `UTF8_TO_TCHAR` for proper UTF-8 handling
- **Warning suppression:** fkYAML headers are wrapped in `THIRD_PARTY_INCLUDES_START` / `THIRD_PARTY_INCLUDES_END` macros to suppress Unreal Engine compiler warnings

### Dependencies

The YAML serializer uses the fkYAML library, included as a git submodule at `Source/ThirdParty/fkYAML/`. The include path is configured in `SGDynamicTextAssetsRuntime.Build.cs`. No linking is required as fkYAML is header-only.

### Interface Methods

All methods are inherited from `ISGDynamicTextAssetSerializer`. See [SerializerInterface.md](SerializerInterface.md) for the full interface contract.

| Method | Description |
|--------|-------------|
| `SerializeProvider` | Converts a provider's properties to YAML via fkYAML's emitter |
| `DeserializeProvider` | Parses YAML via `fkyaml::node::deserialize()`, reconstructs `FJsonObject` tree from YAML nodes, then populates UPROPERTY fields via reflection |
| `ValidateStructure` | Validates that a YAML string parses correctly and has the required structure (`metadata` and `data` mappings at root level) |
| `ExtractMetadata` | Extracts all four metadata fields from the `metadata` mapping without full deserialization |
| `UpdateFieldsInPlace` | Updates metadata fields within an already-serialized YAML string in-place |
| `GetDefaultFileContent` | Generates the initial YAML file content for a new dynamic text asset |
| `GetSerializerTypeId` | Returns `3` |

### Registration

The YAML serializer is registered automatically in `SGDynamicTextAssetsRuntimeModule::StartupModule()` via:

```cpp
FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetYamlSerializer>();
```

See [SerializerInterface.md](SerializerInterface.md) for the full registration pattern.

## Asset Bundle Metadata

When a dynamic text asset has soft reference properties tagged with `meta=(AssetBundles="...")`, the serializer writes an `sgdtAssetBundles` mapping at the root level alongside `metadata` and `data`. This mapping is a snapshot of the bundle data extracted from the object's UPROPERTY meta tags.

### Format

Bundle names are keys in the `sgdtAssetBundles` mapping. Each key maps to a sequence of entries, where each entry has a `property` and a `path`.

```yaml
metadata:
  type: UWeaponData
  version: 1.0.0
  id: A1B2C3D4-E5F6-7890-ABCD-EF1234567890
  userfacingid: excalibur
data:
  MeshAsset: /Game/Weapons/Meshes/Sword.Sword
  ImpactMaterial: /Game/Weapons/Materials/ImpactMat.ImpactMat
  FireSound: /Game/Audio/Weapons/FireSFX.FireSFX
sgdtAssetBundles:
  Visual:
    - property: UWeaponData.MeshAsset
      path: /Game/Weapons/Meshes/Sword.Sword
    - property: UWeaponData.ImpactMaterial
      path: /Game/Weapons/Materials/ImpactMat.ImpactMat
  Audio:
    - property: UWeaponData.ImpactMaterial
      path: /Game/Weapons/Materials/ImpactMat.ImpactMat
    - property: UWeaponData.FireSound
      path: /Game/Audio/Weapons/FireSFX.FireSFX
```

### Behavior

- The `sgdtAssetBundles` mapping is only written if the object has at least one bundled soft reference with a valid (non-null) path.
- Properties tagged with multiple bundles (e.g., `meta=(AssetBundles="Visual,Audio")`) appear in each named bundle.
- Properties without the `AssetBundles` meta tag are not included.
- Container properties (`TArray`, `TMap`, `TSet`) tagged with `AssetBundles` propagate their bundle names to inner soft reference elements.
- During deserialization in **editor builds**, the `sgdtAssetBundles` mapping is informational only - bundle data is re-extracted from UPROPERTY meta tags after properties are populated. In **packaged builds**, this mapping is the primary source since property metadata is stripped.

### Extraction Without Full Deserialization

The `ExtractSGDTAssetBundles()` method on the serializer can parse the `sgdtAssetBundles` mapping from a YAML string without deserializing the full object. This is useful for cook pipelines and editor tooling.

## Third-Party Attribution

The YAML serializer uses the fkYAML library under the MIT license.

- **Source:** [https://github.com/Start-Games/fkYAML](https://github.com/Start-Games/fkYAML) (fork of [https://github.com/fktn-k/fkYAML](https://github.com/fktn-k/fkYAML))
- **License:** MIT
- **Copyright:** Copyright (c) 2023-2025 Kensuke Fukutani

Full license text is available in `ThirdPartyNotices.md` at the plugin root and in the `Source/ThirdParty/fkYAML/LICENSE.txt` file.

## Instanced Object Serialization

Properties declared with `UPROPERTY(Instanced)` are serialized inline within the `data` mapping. Each instanced sub-object is represented as a YAML mapping containing an `SG_INST_OBJ_CLASS` key that stores the full class path, followed by keys for the sub-object's own UPROPERTY fields.

### Reserved Key

| Key | Value | Description |
|-----|-------|-------------|
| `SG_INST_OBJ_CLASS` | Full class path string | Identifies the runtime UClass for deserialization |

The class path is produced by `UClass::GetPathName()`:
- **C++ classes:** `/Script/ModuleName.ClassName` (e.g., `/Script/MyGame.UFireDamageConfig`)
- **Blueprint classes:** `/Game/Path/BP_Name.BP_Name_C` (e.g., `/Game/Configs/BP_IceDamage.BP_IceDamage_C`)

The YAML serializer uses the JSON-intermediate architecture (`SerializeInstancedObjectToValue` / `DeserializeValueToInstancedObject` on `FSGDynamicTextAssetSerializerBase`), with `JsonValueToYamlNode` and `YamlNodeToJsonValue` bridging between formats.

### Single Instanced Object

A non-null instanced object is a mapping with `SG_INST_OBJ_CLASS` plus its properties:

```yaml
data:
  DamageConfig:
    SG_INST_OBJ_CLASS: /Script/MyGame.UFireDamageConfig
    BaseDamage: 50.0
    BurnDuration: 3.0
```

A null (unset) instanced object is `null`:

```yaml
data:
  DamageConfig: null
```

### Polymorphic Instanced Objects

Since `SG_INST_OBJ_CLASS` stores the actual runtime type, a property typed as a base class can hold any subclass:

```yaml
data:
  DamageConfig:
    SG_INST_OBJ_CLASS: /Script/MyGame.UPoisonDamageConfig
    BaseDamage: 25.0
    PoisonStacks: 5
    TickInterval: 1.5
```

### Array of Instanced Objects

Arrays use YAML sequence notation. Supports mixed types and null entries:

```yaml
data:
  StatusEffects:
    - SG_INST_OBJ_CLASS: /Script/MyGame.UBurnEffect
      Duration: 5.0
      DamagePerTick: 10.0
    - SG_INST_OBJ_CLASS: /Game/Effects/BP_FreezeEffect.BP_FreezeEffect_C
      Duration: 3.0
      SlowPercent: 0.5
    - null
```

### Map with Instanced Object Values

Maps use nested mappings, where each value is an instanced object:

```yaml
data:
  ElementalConfigs:
    Fire:
      SG_INST_OBJ_CLASS: /Script/MyGame.UFireConfig
      Intensity: 1.0
    Ice:
      SG_INST_OBJ_CLASS: /Script/MyGame.UIceConfig
      FreezeChance: 0.3
```

### Nested Instanced Objects

Instanced objects can contain their own instanced sub-objects. Nesting is handled recursively:

```yaml
data:
  WeaponBehavior:
    SG_INST_OBJ_CLASS: /Script/MyGame.UMeleeWeaponBehavior
    ComboCount: 3
    HitEffect:
      SG_INST_OBJ_CLASS: /Script/MyGame.USlashEffect
      ParticleScale: 1.5
```

[Back to Table of Contents](../TableOfContents.md)
