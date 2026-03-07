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
| `type` | The UClass name including prefix (e.g., `UWeaponData`) |
| `version` | Semantic version in `Major.Minor.Patch` format |
| `id` | The unique identifier in standard GUID format |
| `userfacingid` | Human-readable identifier for display and lookups |

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

## Third-Party Attribution

The YAML serializer uses the fkYAML library under the MIT license.

- **Source:** [https://github.com/Start-Games/fkYAML](https://github.com/Start-Games/fkYAML) (fork of [https://github.com/fktn-k/fkYAML](https://github.com/fktn-k/fkYAML))
- **License:** MIT
- **Copyright:** Copyright (c) 2023-2025 Kensuke Fukutani

Full license text is available in `ThirdPartyNotices.md` at the plugin root and in the `Source/ThirdParty/fkYAML/LICENSE.txt` file.

[Back to Table of Contents](../TableOfContents.md)
