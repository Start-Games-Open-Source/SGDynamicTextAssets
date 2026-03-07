# Dynamic Text Assets

[Back to Table of Contents](../TableOfContents.md)

## ISGDynamicTextAssetProvider Interface

The core dynamic text asset contract is defined by `ISGDynamicTextAssetProvider`, 
a C++ interface that specifies identity, versioning, validation, and migration capabilities. 
Any `UObject` that implements this interface can participate in the dynamic text asset ecosystem (loading, caching, serialization, editor tooling).

It was purposely implemented where the class implementing the interface must be done through C++ to keep numerous data assets manageable.

```cpp
UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class USGDynamicTextAssetProvider : public UInterface
```

`ISGDynamicTextAssetProvider` declares these pure virtual methods:

| Method | Description                                                                             |
|--------|-----------------------------------------------------------------------------------------|
| `GetDynamicTextAssetId()` / `SetDynamicTextAssetId()` | Unique `FSGDynamicTextAssetId` identity                                                 |
| `GetUserFacingId()` / `SetUserFacingId()` | Human-readable display name                                                             |
| `GetVersion()` / `SetVersion()` | `FSGDynamicTextAssetVersion` semantic version                                           |
| `GetCurrentMajorVersion()` | Returns the class's current schema major version                                        |
| `GetCurrentVersion()` | Returns full `FSGDynamicTextAssetVersion` (default: `{GetCurrentMajorVersion(), 0, 0}`) |
| `PostDynamicTextAssetLoaded()` | Called after deserialization; delegates to `BP_PostDynamicTextAssetLoaded()` BlueprintNativeEvent |
| `Native_ValidateDynamicTextAsset()` | Runs built-in + custom validation                                                       |
| `MigrateFromVersion()` | Transforms old text data to current schema                                              |

**Registration guards** in `USGDynamicTextAssetRegistry` enforce that registered classes:
- Implement `ISGDynamicTextAssetProvider`
- Are **not** subclasses of `AActor` or `UActorComponent`

See [Registry](../Runtime/Registry.md) for details.

## USGDynamicTextAsset Base Class

`USGDynamicTextAsset` is the default abstract implementation of `ISGDynamicTextAssetProvider`. 
It inherits from `UObject`, implements all interface methods, and adds editor delegates and lifecycle hooks. 
Most projects subclass `USGDynamicTextAsset` directly.

```cpp
UCLASS(Abstract, BlueprintType, ClassGroup = "Start Games")
class USGDynamicTextAsset : public UObject, public ISGDynamicTextAssetProvider
```

## Creating a Subclass

Define a C++ class that derives from `USGDynamicTextAsset` (or another concrete dynamic text asset class). Add UPROPERTY fields for the data you want to configure.

```cpp
UCLASS(BlueprintType)
class MYGAME_API UWeaponData : public USGDynamicTextAsset
{
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float BaseDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    TSoftObjectPtr<UStaticMesh> MeshAsset;
};
```

All `UPROPERTY` fields are automatically serialized to/from JSON by the reflection-based serializer. Standard Unreal property types are supported: primitives, FString, FName, FText, FVector, FRotator, FColor, TArray, TMap, TSet, TSoftObjectPtr, TSoftClassPtr, nested USTRUCTs, and enums.

## Identity

Every dynamic text asset has three identity fields managed by the base class:

### DynamicTextAssetId (`FSGDynamicTextAssetId`)
A wrapper around `FGuid` assigned at creation time. The ID never changes: it survives renames, moves, and file reorganization. All cross-references use `FSGDynamicTextAssetId`.

```cpp
const FSGDynamicTextAssetId& GetDynamicTextAssetId() const;

// Typically only called during deserialization
void SetDynamicTextAssetId(const FSGDynamicTextAssetId& InId);
```

`FSGDynamicTextAssetId` provides `IsValid()`, `ToString()`, `FromString()`, `NewGeneratedId()`, binary `Serialize()`, and `NetSerialize()` for network replication. See [Class Reference](../Reference/ClassReference.md) for the full API.

### UserFacingId (`FString`)
A human-readable identifier used as the filename and display name (e.g., `"excalibur"`, `"main_quest_001"`). Can be renamed through the browser. Sanitized to remove invalid filesystem characters.

```cpp
const FString& GetUserFacingId() const;
void SetUserFacingId(const FString& InUserFacingId);
```

### Version (`FSGDynamicTextAssetVersion`)
Semantic version (Major.Minor.Patch) for the data instance. See [Versioning and Migration](VersioningAndMigration.md) for details.

```cpp
const FSGDynamicTextAssetVersion& GetVersion() const;

// Returns "Major.Minor.Patch"
FString GetVersionString() const;
```

## File Storage

Dynamic text assets are stored under `Content/SGDynamicTextAssets/` in directories based on the class hierarchy. The filename is the UserFacingId with a `.dta.json` extension.

```
Content/SGDynamicTextAssets/
  WeaponData/
    excalibur.dta.json
    iron_sword.dta.json
    SwordData/                    # Subclass gets subfolder
      enchanted_blade.dta.json
  QuestData/
    main_quest_001.dta.json
```

## Lifecycle Hooks

### PostDynamicTextAssetLoaded

Called after all properties have been populated from the source file during deserialization. Override to perform custom initialization, build caches, or resolve derived data.

This uses a two-layer pattern:
- `PostDynamicTextAssetLoaded()` is the **interface-level** method (from `ISGDynamicTextAssetProvider`). The base class implementation delegates to the BlueprintNativeEvent below.
- `BP_PostDynamicTextAssetLoaded()` is the **BlueprintNativeEvent** that subclasses should override.

```cpp
// Interface method (called by the subsystem after deserialization)
virtual void PostDynamicTextAssetLoaded() override;

// BlueprintNativeEvent (override this in your subclass)
UFUNCTION(BlueprintNativeEvent, Category = "Dynamic Text Asset",
    meta = (DisplayName = "Post Dynamic Text Asset Loaded"))
void BP_PostDynamicTextAssetLoaded();

// Override in your subclass:
void UWeaponData::BP_PostDynamicTextAssetLoaded_Implementation()
{
    Super::BP_PostDynamicTextAssetLoaded_Implementation();
    // Build lookup tables, pre-compute values, etc.
    EffectiveDPS = BaseDamage * AttackSpeed;
}
```

The `DisplayName` metadata ensures it appears as "Post Dynamic Text Asset Loaded" in Blueprint graphs. This can also be overridden in Blueprint subclasses (though dynamic text asset classes are typically C++ only).

### ValidateDynamicTextAsset

Called during validation to check custom rules. Override to add domain-specific validation entries. See [Validation](Validation.md) for the full validation system.

```cpp
UFUNCTION(BlueprintNativeEvent, Category = "Dynamic Text Asset")
bool ValidateDynamicTextAsset(FSGDynamicTextAssetValidationResult& OutResult) const;
```

### MigrateFromVersion

Called when loading data whose major version is older than the class's current major version. Override to transform old data structures to the current format. See [Versioning and Migration](VersioningAndMigration.md).

```cpp
virtual bool MigrateFromVersion(
    const FSGDynamicTextAssetVersion& OldVersion,
    const FSGDynamicTextAssetVersion& CurrentVersion,
    const TSharedPtr<FJsonObject>& OldData);

// Default returns 1
virtual int32 GetCurrentMajorVersion() const;

// Default returns {GetCurrentMajorVersion(), 0, 0}
virtual FSGDynamicTextAssetVersion GetCurrentVersion() const;
```

## Editor Delegates

In editor builds, `USGDynamicTextAsset` exposes multicast delegates for change notifications:

- `OnDynamicTextAssetChanged`: Broadcast when any property changes.
- `OnDynamicTextAssetIdChanged`: Broadcast when the DynamicTextAssetId is modified.
- `OnUserFacingIdChanged`: Broadcast when the UserFacingId is modified.
- `OnVersionChanged`: Broadcast when the version is modified.

These are used by the editor UI to keep the browser and editors in sync.

## Runtime Immutability

Dynamic text assets are intended to be read-only at runtime. The subsystem loads them into an in-memory cache and serves them as constant data. Setters for identity fields (`SetDynamicTextAssetId`, `SetUserFacingId`, `SetVersion`) exist for deserialization and editor use, not for runtime mutation.

## Class Registration

Dynamic text asset classes must be registered with `USGDynamicTextAssetRegistry` to appear in the editor browser. Registration is typically done in your module's `StartupModule()`. See [Registry](../Runtime/Registry.md) for details.

### Registration Example

```cpp
void FMyGameModule::StartupModule()
{
    // Register root dynamic text asset classes. Child classes are discovered automatically.
    USGDynamicTextAssetRegistry::GetChecked().RegisterDynamicTextAssetClass(UWeaponData::StaticClass());
    USGDynamicTextAssetRegistry::GetChecked().RegisterDynamicTextAssetClass(UQuestData::StaticClass());
}
```

> **Note:** Only registered root dynamic text asset classes (direct children of `USGDynamicTextAsset`). 
> Subclasses like `USwordData : public UWeaponData` are discovered automatically via Unreal's reflection system.

### Browser Exclusion Specifiers

Classes with certain UCLASS specifiers are excluded from the browser's class list:

| Specifier | Effect |
|-----------|--------|
| `Hidden` | Class is not shown in the browser or any class pickers |
| `Deprecated` | Class is filtered out as obsolete |
| `HideDropdown` | Class does not appear in dropdown selections |
| `Abstract` | Class appears in the type hierarchy but cannot be instantiated (no "New" option) |

**Use cases for exclusion specifiers:**

```cpp
// Internal base class that designers should never instantiate directly
UCLASS(Abstract, BlueprintType)
class UProjectileData : public USGDynamicTextAsset { ... };

// Legacy class being phased out, existing instances can still be loaded
UCLASS(Deprecated)
class UDEPRECATED_OldWeaponData : public USGDynamicTextAsset { ... };

// Test/debug dynamic text asset that should not appear in editor tooling
UCLASS(Hidden)
class UDebugTestData : public USGDynamicTextAsset { ... };

// Internal helper class that other code references but designers don't pick
UCLASS(HideDropdown)
class UInternalRewardData : public USGDynamicTextAsset { ... };
```

[Back to Table of Contents](../TableOfContents.md)
