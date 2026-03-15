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
| `<type>` | The `FSGDynamicTextAssetTypeId` GUID when available (preferred), or the UClass name including prefix as a fallback (e.g., `UWeaponData`) |
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

## Asset Bundle Metadata

When a dynamic text asset has soft reference properties tagged with `meta=(AssetBundles="...")`, the serializer writes an `<sgdtAssetBundles>` element at the root level alongside `<metadata>` and `<data>`. This element is a snapshot of the bundle data extracted from the object's UPROPERTY meta tags.

### Format

Each bundle is represented as a `<bundle>` element with a `name` attribute. Entries within a bundle are `<entry>` elements with `property` and `path` attributes.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<DynamicTextAsset>
    <metadata> ... </metadata>
    <data> ... </data>
    <sgdtAssetBundles>
        <bundle name="Visual">
            <entry property="UWeaponData.MeshAsset" path="/Game/Weapons/Meshes/Sword.Sword"/>
            <entry property="UWeaponData.ImpactMaterial" path="/Game/Weapons/Materials/ImpactMat.ImpactMat"/>
        </bundle>
        <bundle name="Audio">
            <entry property="UWeaponData.ImpactMaterial" path="/Game/Weapons/Materials/ImpactMat.ImpactMat"/>
            <entry property="UWeaponData.FireSound" path="/Game/Audio/Weapons/FireSFX.FireSFX"/>
        </bundle>
    </sgdtAssetBundles>
</DynamicTextAsset>
```

### Behavior

- The `<sgdtAssetBundles>` element is only written if the object has at least one bundled soft reference with a valid (non-null) path.
- Properties tagged with multiple bundles (e.g., `meta=(AssetBundles="Visual,Audio")`) appear in each named bundle.
- Properties without the `AssetBundles` meta tag are not included.
- Container properties (`TArray`, `TMap`, `TSet`) tagged with `AssetBundles` propagate their bundle names to inner soft reference elements.
- During deserialization in **editor builds**, the `<sgdtAssetBundles>` element is informational only - bundle data is re-extracted from UPROPERTY meta tags after properties are populated. In **packaged builds**, this element is the primary source since property metadata is stripped.

### Extraction Without Full Deserialization

The `ExtractSGDTAssetBundles()` method on the serializer can parse the `<sgdtAssetBundles>` element from an XML string without deserializing the full object. This is useful for cook pipelines and editor tooling.

## Instanced Object Serialization

Properties declared with `UPROPERTY(Instanced)` are serialized inline within the `<data>` block. Each instanced sub-object is represented as an XML element containing a `<SG_INST_OBJ_CLASS>` child element that stores the full class path, followed by child elements for the sub-object's own UPROPERTY fields.

### Reserved Key

| Element | Value | Description |
|---------|-------|-------------|
| `<SG_INST_OBJ_CLASS>` | Full class path string | Identifies the runtime UClass for deserialization |

The class path is produced by `UClass::GetPathName()`:
- **C++ classes:** `/Script/ModuleName.ClassName` (e.g., `/Script/MyGame.UFireDamageConfig`)
- **Blueprint classes:** `/Game/Path/BP_Name.BP_Name_C` (e.g., `/Game/Configs/BP_IceDamage.BP_IceDamage_C`)

`SG_INST_OBJ_CLASS` is always a **child element**, not an XML attribute. The XML serializer uses the JSON-intermediate architecture (`SerializeInstancedObjectToValue` / `DeserializeValueToInstancedObject` on `FSGDynamicTextAssetSerializerBase`), with `JsonValueToXmlElement` and `XmlNodeToJsonValue` bridging between formats.

### Single Instanced Object

A non-null instanced object is an element containing `<SG_INST_OBJ_CLASS>` plus its properties:

```xml
<data>
    <DamageConfig>
        <SG_INST_OBJ_CLASS>/Script/MyGame.UFireDamageConfig</SG_INST_OBJ_CLASS>
        <BaseDamage>50.0</BaseDamage>
        <BurnDuration>3.0</BurnDuration>
    </DamageConfig>
</data>
```

A null (unset) instanced object is a self-closing tag:

```xml
<data>
    <DamageConfig/>
</data>
```

### Polymorphic Instanced Objects

Since `<SG_INST_OBJ_CLASS>` stores the actual runtime type, a property typed as a base class can hold any subclass:

```xml
<data>
    <DamageConfig>
        <SG_INST_OBJ_CLASS>/Script/MyGame.UPoisonDamageConfig</SG_INST_OBJ_CLASS>
        <BaseDamage>25.0</BaseDamage>
        <PoisonStacks>5</PoisonStacks>
        <TickInterval>1.5</TickInterval>
    </DamageConfig>
</data>
```

### Array of Instanced Objects

Arrays use repeated `<Item>` child elements. Supports mixed types and null entries (self-closing `<Item/>`):

```xml
<data>
    <StatusEffects>
        <Item>
            <SG_INST_OBJ_CLASS>/Script/MyGame.UBurnEffect</SG_INST_OBJ_CLASS>
            <Duration>5.0</Duration>
            <DamagePerTick>10.0</DamagePerTick>
        </Item>
        <Item>
            <SG_INST_OBJ_CLASS>/Game/Effects/BP_FreezeEffect.BP_FreezeEffect_C</SG_INST_OBJ_CLASS>
            <Duration>3.0</Duration>
            <SlowPercent>0.5</SlowPercent>
        </Item>
        <Item/>
    </StatusEffects>
</data>
```

### Map with Instanced Object Values

Maps use `<Item>` elements with `<Key>` and `<Value>` children, where the `<Value>` contains the instanced object:

```xml
<data>
    <ElementalConfigs>
        <Item>
            <Key>Fire</Key>
            <Value>
                <SG_INST_OBJ_CLASS>/Script/MyGame.UFireConfig</SG_INST_OBJ_CLASS>
                <Intensity>1.0</Intensity>
            </Value>
        </Item>
        <Item>
            <Key>Ice</Key>
            <Value>
                <SG_INST_OBJ_CLASS>/Script/MyGame.UIceConfig</SG_INST_OBJ_CLASS>
                <FreezeChance>0.3</FreezeChance>
            </Value>
        </Item>
    </ElementalConfigs>
</data>
```

### Nested Instanced Objects

Instanced objects can contain their own instanced sub-objects. Nesting is handled recursively:

```xml
<data>
    <WeaponBehavior>
        <SG_INST_OBJ_CLASS>/Script/MyGame.UMeleeWeaponBehavior</SG_INST_OBJ_CLASS>
        <ComboCount>3</ComboCount>
        <HitEffect>
            <SG_INST_OBJ_CLASS>/Script/MyGame.USlashEffect</SG_INST_OBJ_CLASS>
            <ParticleScale>1.5</ParticleScale>
        </HitEffect>
    </WeaponBehavior>
</data>
```

[Back to Table of Contents](../TableOfContents.md)
