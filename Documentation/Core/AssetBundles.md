# Asset Bundles

[Back to Table of Contents](../TableOfContents.md)

## Overview

Asset bundles group soft references on dynamic text assets into named sets for efficient batch loading. Instead of loading every soft reference at once, you can load only the "Visual" bundle when a weapon enters view, then load the "Audio" bundle when it fires.

This system adapts Unreal Engine's `meta=(AssetBundles="BundleName")` pattern from Primary Data Assets to the DTA ecosystem.

## Tagging Properties

Tag any `TSoftObjectPtr` or `TSoftClassPtr` UPROPERTY with one or more bundle names:

```cpp
UCLASS()
class UWeaponData : public USGDynamicTextAsset
{
    GENERATED_BODY()

public:
    /** Mesh loaded when the weapon becomes visible. */
    UPROPERTY(EditAnywhere, meta = (AssetBundles = "Visual"))
    TSoftObjectPtr<UStaticMesh> MeshAsset;

    /** Material shared between visual and audio systems. */
    UPROPERTY(EditAnywhere, meta = (AssetBundles = "Visual,Audio"))
    TSoftObjectPtr<UMaterialInterface> ImpactMaterial;

    /** Sound loaded when the weapon is equipped. */
    UPROPERTY(EditAnywhere, meta = (AssetBundles = "Audio"))
    TSoftObjectPtr<USoundBase> FireSound;

    /** Class reference in its own bundle. */
    UPROPERTY(EditAnywhere, meta = (AssetBundles = "Classes"))
    TSoftClassPtr<AActor> ProjectileClass;

    /** No bundle tag - not included in any bundle. */
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UTexture2D> IconTexture;
};
```

Key rules:
- Multiple bundle names are comma-separated: `meta=(AssetBundles="Visual,Audio")`
- Properties without the `AssetBundles` meta tag are excluded from all bundles
- Both `TSoftObjectPtr` and `TSoftClassPtr` are supported
- Only soft references with valid, non-null paths are included

## Container Properties

`TArray`, `TMap`, and `TSet` properties can be tagged with `AssetBundles`. The bundle names propagate to all soft reference elements within the container:

```cpp
/** Every mesh ref in this array belongs to the "Visual" bundle. */
UPROPERTY(EditAnywhere, meta = (AssetBundles = "Visual"))
TArray<TSoftObjectPtr<UStaticMesh>> LODMeshes;

/** All values in this map belong to the "Audio" bundle. */
UPROPERTY(EditAnywhere, meta = (AssetBundles = "Audio"))
TMap<FName, TSoftObjectPtr<USoundBase>> SoundEffects;
```

The inner elements do not need their own `AssetBundles` meta tag. If an inner element does have its own tag, it takes precedence over the container's tag.

## How Extraction Works

Bundle data is extracted from a UObject by walking its property hierarchy using `TFieldIterator<FProperty>`. The extraction is performed by `FSGDynamicTextAssetBundleData::ExtractFromObject()`.

### Property Walk Order

For each property on the object's UClass:

1. **Instanced sub-objects** (`CPF_InstancedReference`): The walker enters the sub-object and recurses on its own UClass properties. Each sub-object's properties are checked independently.

2. **Soft object/class properties** (`FSoftObjectProperty`, `FSoftClassProperty`): The walker checks for `AssetBundles` metadata on the property. If found, the comma-separated bundle names are parsed and the current `FSoftObjectPath` value is collected into each named bundle. If no metadata exists but the property was reached via a container with `AssetBundles`, the inherited names are used instead.

3. **Structs** (`FStructProperty`): The walker recurses into the struct's members. Each member is checked for its own `AssetBundles` metadata.

4. **Arrays** (`FArrayProperty`): The walker parses the array's own `AssetBundles` metadata (if any) and passes those names as inherited names to each element. Elements are iterated via `FScriptArrayHelper`.

5. **Maps** (`FMapProperty`): Same as arrays, but iterates key/value pairs via `FScriptMapHelper`. The map's `AssetBundles` metadata is inherited by both key and value properties.

6. **Sets** (`FSetProperty`): Same as arrays, iterates elements via `FScriptSetHelper`.

### Editor vs Runtime

Bundle extraction uses `FProperty::HasMetaData()` and `FProperty::GetMetaData()`, which are only available in editor builds (`WITH_EDITORONLY_DATA`). In packaged builds, property metadata is stripped by UHT.

To handle this:
- **Editor builds**: `ExtractFromObject()` walks properties and reads meta tags on every call. This is the source of truth.
- **Packaged builds**: `ExtractFromObject()` is a no-op. Bundle data is populated during deserialization from the `sgdtAssetBundles` block stored in the DTA file.

This dual-source approach is transparent to callers. The `FSGDynamicTextAssetBundleData` on a provider always has the correct data regardless of build type.

## Data Flow

```
[Editor Save]
    1. Serializer calls ExtractFromObject() on the provider
    2. Bundle data is extracted from UPROPERTY meta tags
    3. sgdtAssetBundles block is written to the file as a snapshot
    4. Bundle data is stored on the provider's FSGDynamicTextAssetBundleData member

[Editor Load / Development Runtime]
    1. File is deserialized, UPROPERTY values are populated
    2. PostDynamicTextAssetLoaded() calls ExtractFromObject(this)
    3. Bundle data is freshly extracted from meta tags (file snapshot is ignored)

[Packaged Runtime]
    1. Binary file is deserialized, UPROPERTY values are populated
    2. sgdtAssetBundles block is deserialized into FSGDynamicTextAssetBundleData
    3. ExtractFromObject() is a no-op (meta tags are stripped)
    4. Bundle data comes entirely from the deserialized file snapshot
```

## Core Types

### FSGDynamicTextAssetBundleEntry

A single soft reference entry within a bundle:

| Field | Type | Description |
|-------|------|-------------|
| `AssetPath` | `FSoftObjectPath` | The soft reference path to the asset |
| `PropertyName` | `FName` | Qualified property name in `OwnerClass.PropertyName` format (e.g., `UWeaponData.MeshAsset`). The owning class/struct prefix disambiguates same-named properties from different types. |

### FSGDynamicTextAssetBundle

A named bundle containing its entries:

| Field | Type | Description |
|-------|------|-------------|
| `BundleName` | `FName` | The bundle name (from the meta tag) |
| `Entries` | `TArray<FSGDynamicTextAssetBundleEntry>` | All soft references in this bundle |

Supports comparison with `FName` via `operator==` for use with `TArray::FindByKey()`.

### FSGDynamicTextAssetBundleData

Complete bundle data for a single DTA instance:

| Method | Description |
|--------|-------------|
| `ExtractFromObject(const UObject*)` | Walks properties, reads `AssetBundles` meta, collects paths |
| `HasBundles() const` | True if any bundles were extracted |
| `FindBundle(FName) const` | Returns pointer to a bundle by name, or nullptr |
| `GetBundleNames(TArray<FName>&) const` | Populates array with all bundle names |
| `GetPathsForBundle(FName, TArray<FSoftObjectPath>&) const` | Collects all paths for a specific bundle |
| `Reset()` | Clears all bundle data |

## Provider Interface

Bundle support is defined at the `ISGDynamicTextAssetProvider` level:

```cpp
// Read-only access to bundle data
virtual const FSGDynamicTextAssetBundleData& GetSGDTAssetBundleData() const = 0;

// Mutable access for serializers and editor tools
virtual FSGDynamicTextAssetBundleData& GetMutableSGDTAssetBundleData() = 0;

// Convenience check (default: delegates to HasBundles())
virtual bool HasSGDTAssetBundles() const;
```

`USGDynamicTextAsset` provides the default implementation with a transient `FSGDynamicTextAssetBundleData` member populated via `ExtractFromObject(this)` during `PostDynamicTextAssetLoaded()`.

## Loading Bundles at Runtime

### Via FSGDynamicTextAssetRef (Recommended)

The simplest way to load a DTA with its bundles is through `FSGDynamicTextAssetRef::LoadAsync`. Pass bundle names as the third parameter, and the callback fires only after both the DTA and all requested bundle assets are ready:

```cpp
TArray<FName> bundles = { FName("Visual"), FName("Audio") };
WeaponRef.LoadAsync(this,
    [this](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        if (bSuccess)
        {
            UWeaponData* weapon = Cast<UWeaponData>(Provider.GetObject());
            // Bundle assets are loaded - safe to dereference soft pointers
            UStaticMesh* mesh = weapon->MeshAsset.Get();
        }
    },
    bundles);
```

Or use the `SG_LOAD_REF_WITH_BUNDLES` convenience macro:

```cpp
TArray<FName> bundles = { FName("Visual") };
SG_LOAD_REF_WITH_BUNDLES(WeaponRef, this, bundles,
{
    if (!self.IsValid() || !bSuccess) return;
    // Use loaded provider...
});
```

The Blueprint statics function `LoadDynamicTextAssetRefAsync` delegates to this same implementation.

See [Dynamic Text Asset References](DynamicTextAssetReferences.md#async-loading) for full usage details and macro documentation.

### Via Subsystem (Direct Control)

The subsystem provides lower-level bundle-aware async loading via `FStreamableManager` for cases where you need direct control over bundle loading independently of DTA loading:

#### Load a Single DTA's Bundle

```cpp
USGDynamicTextAssetSubsystem* Subsystem = GetGameInstance()->GetSubsystem<USGDynamicTextAssetSubsystem>();

// Load all "Visual" assets for a specific weapon (DTA must already be cached)
Subsystem->LoadSGDTAssetBundle(WeaponId, FName("Visual"),
    FStreamableDelegate::CreateLambda([this]()
    {
        // Visual assets are now loaded and ready to use
        ApplyVisuals();
    }));
```

#### Load a Bundle Across All Cached DTAs

```cpp
// Pre-load all "Audio" assets across every cached DTA
int32 Count = Subsystem->LoadSGDTAssetBundleForAll(FName("Audio"),
    FStreamableDelegate::CreateLambda([this]()
    {
        // All audio assets are loaded
        InitializeAudioSystem();
    }));

UE_LOG(LogGame, Log, TEXT("Loading Audio bundle for %d DTAs"), Count);
```

#### Query Bundle Data

```cpp
// Get bundle data for a specific DTA
const FSGDynamicTextAssetBundleData* BundleData = Subsystem->GetSGDTAssetBundleData(WeaponId);
if (BundleData)
{
    TArray<FName> Names;
    BundleData->GetBundleNames(Names);
    // Names contains: "Visual", "Audio", etc.
}

// Gather all paths for a bundle across the entire cache
TArray<FSoftObjectPath> AllVisualPaths;
Subsystem->GetAllPathsForSGDTBundle(FName("Visual"), AllVisualPaths);
```

## File Format

The `sgdtAssetBundles` block is serialized into each format. See the format-specific docs for examples:

- [JSON Format](../Serialization/JsonFormat.md#asset-bundle-metadata)
- [XML Format](../Serialization/XmlFormat.md#asset-bundle-metadata)
- [YAML Format](../Serialization/YamlFormat.md#asset-bundle-metadata)

The file snapshot is informational during editor loads (meta tags are the source of truth) but becomes the primary source in packaged builds where property metadata is stripped.

## Cook Integration

`FSGDynamicTextAssetCookUtils::GatherSoftReferencesBySGDTBundle()` scans all DTA files and groups soft reference package names by their bundle name. This enables selective cook dependency inclusion, so you can include only specific bundles in certain build configurations.

Soft references without `AssetBundles` metadata are grouped under `NAME_None`.

## Editor Integration

In the DTA editor's Details panel, properties with `AssetBundles` metadata display a bundle icon next to the property's value widget. This applies to soft reference properties (`TSoftObjectPtr`, `TSoftClassPtr`) and container properties (`TArray`, `TMap`, `TSet`) whose elements are soft references. Hovering over the icon shows a tooltip with the bundle names (e.g., "Asset Bundles: Visual, Audio").

This icon display can be toggled via `bShowAssetBundleIcons` in `USGDynamicTextAssetEditorSettings` (default: true).

[Back to Table of Contents](../TableOfContents.md)
