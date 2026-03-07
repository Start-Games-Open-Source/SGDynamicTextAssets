# XML Format

[Back to Table of Contents](../TableOfContents.md)

## File Extension and Location

XML dynamic text asset files use the `.dta.xml` extension and are stored under `Content/SGDynamicTextAssets/` in directories based on the class hierarchy:

```
Content/SGDynamicTextAssets/WeaponData/excalibur.dta.xml
Content/SGDynamicTextAssets/WeaponData/SwordData/enchanted_blade.dta.xml
Content/SGDynamicTextAssets/QuestData/main_quest_001.dta.xml
```

## XML Schema

Every `.dta.xml` file follows this structure:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<DynamicTextAsset>
    <metadata>
        <type>UWeaponData</type>
        <version>1.0.0</version>
        <id>A1B2C3D4-E5F6-7890-ABCD-EF1234567890</id>
        <userfacingid>excalibur</userfacingid>
    </metadata>
    <data>
        <DisplayName>Excalibur</DisplayName>
        <BaseDamage>50.0</BaseDamage>
        <AttackSpeed>1.2</AttackSpeed>
        <MeshAsset>/Game/Meshes/Weapons/Excalibur.Excalibur</MeshAsset>
    </data>
</DynamicTextAsset>
```

### Root Element

The root element is always `<DynamicTextAsset>`. It contains exactly two child elements: `<metadata>` and `<data>`.

### Metadata Block

The `<metadata>` element contains four child elements with the same keys used across all serializer formats:

| Element | Description |
|---------|-------------|
| `<type>` | The UClass name including prefix (e.g., `UWeaponData`) |
| `<version>` | Semantic version in `Major.Minor.Patch` format |
| `<id>` | The unique identifier in standard GUID format |
| `<userfacingid>` | Human-readable identifier for display and lookups |

### Data Section

The `<data>` element contains child elements for each serialized UPROPERTY field. Property values are represented as follows:

**Scalar values** are stored as text content:
```xml
<BaseDamage>50.0</BaseDamage>
<WeaponName>Excalibur</WeaponName>
<IsLegendary>true</IsLegendary>
```

**Arrays** use repeated `<Item>` child elements:
```xml
<Tags>
    <Item>fast</Item>
    <Item>heavy</Item>
</Tags>
```

**Nested structs** are represented as child elements matching field names:
```xml
<ArmorStats>
    <BaseArmor>50</BaseArmor>
    <MaxDurability>100</MaxDurability>
</ArmorStats>
```

**Maps** use `<Item>` elements with `<Key>` and `<Value>` children:
```xml
<DamageModifiers>
    <Item>
        <Key>Fire</Key>
        <Value>1.5</Value>
    </Item>
    <Item>
        <Key>Ice</Key>
        <Value>0.8</Value>
    </Item>
</DamageModifiers>
```

### XML Escaping

Special characters in string values are automatically escaped during serialization and unescaped during deserialization:

| Character | Escaped Form |
|-----------|-------------|
| `&` | `&amp;` |
| `<` | `&lt;` |
| `>` | `&gt;` |
| `"` | `&quot;` |
| `'` | `&apos;` |

## FSGDynamicTextAssetXmlSerializer

The XML serializer implementation. Inherits from `FSGDynamicTextAssetSerializerBase` and implements the full `ISGDynamicTextAssetSerializer` interface.

- **TypeId:** `2`
- **Extension:** `.dta.xml`
- **Format Name:** XML

### Implementation Details

- **Reading:** Uses Unreal's built-in `XmlParser` module (`FXmlFile`) for DOM-based parsing
- **Writing:** Constructs XML strings manually (the `XmlParser` module is read-only)
- **Property conversion:** Uses the `FSGDynamicTextAssetSerializerBase` JSON-intermediate helpers (`SerializePropertyToValue` / `DeserializeValueToProperty`), keeping format-specific complexity contained to the XML-to-FJsonValue bridge

### Dependencies

The XML serializer requires the `XmlParser` module, which is added to `PublicDependencyModuleNames` in `SGDynamicTextAssetsRuntime.Build.cs`.

### Interface Methods

All methods are inherited from `ISGDynamicTextAssetSerializer`. See [SerializerInterface.md](SerializerInterface.md) for the full interface contract.

| Method | Description |
|--------|-------------|
| `SerializeProvider` | Converts a provider's properties to formatted XML with 4-space indentation (UTF-8) |
| `DeserializeProvider` | Parses XML via `FXmlFile`, reconstructs `FJsonObject` tree from XML elements, then populates UPROPERTY fields via reflection |
| `ValidateStructure` | Validates that an XML string parses correctly and has the required structure (`<DynamicTextAsset>` root with `<metadata>` and `<data>` children) |
| `ExtractMetadata` | Extracts all four metadata fields from the `<metadata>` element without full deserialization |
| `UpdateFieldsInPlace` | Updates metadata fields within an already-serialized XML string in-place |
| `GetDefaultFileContent` | Generates the initial XML file content for a new dynamic text asset |
| `GetSerializerTypeId` | Returns `2` |

### Registration

The XML serializer is registered automatically in `SGDynamicTextAssetsRuntimeModule::StartupModule()` via:

```cpp
FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetXmlSerializer>();
```

See [SerializerInterface.md](SerializerInterface.md) for the full registration pattern.

[Back to Table of Contents](../TableOfContents.md)
