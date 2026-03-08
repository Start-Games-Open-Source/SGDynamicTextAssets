# Versioning and Migration

[Back to Table of Contents](../TableOfContents.md)

## FSGDynamicTextAssetVersion

Every dynamic text asset carries a semantic version as an `FSGDynamicTextAssetVersion` struct with three components:

```cpp
USTRUCT(BlueprintType)
struct FSGDynamicTextAssetVersion
{
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 Major = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 Minor = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 Patch = 0;
};
```

### When to Increment Each Component

| Component | When to Increment | Example |
|-----------|------------------|---------|
| **Major** | Breaking schema changes: property removed, type changed, structure reorganized | Renaming `Damage` to `BaseDamage` and changing from `int32` to `float` |
| **Minor** | Backwards-compatible additions: new optional property added | Adding an optional `Description` field |
| **Patch** | Data value changes only, no schema change | Rebalancing weapon damage numbers |

Major version starts at 1 (minimum clamped value). Minor and Patch start at 0.

### String Conversion

```cpp
// "1.2.3"
FString ToString() const;

// Static factory - creates a new version from a "Major.Minor.Patch" string
static FSGDynamicTextAssetVersion ParseFromString(const FString& VersionString);

// Instance method - parses "Major.Minor.Patch" into this version, returns true on success
bool ParseString(const FString& VersionString);
```

### Compatibility

Two versions are compatible if they share the same major version:

```cpp
// Returns: Major == Other.Major
bool IsCompatibleWith(const FSGDynamicTextAssetVersion& Other) const;
```

This means data at version `1.5.0` can be loaded by code expecting `1.0.0` without migration (minor/patch changes are additive). But `2.0.0` data requires code that handles major version 2.

### Comparison Operators

Full ordering is supported for sorting and comparison:

```cpp
bool operator==(const FSGDynamicTextAssetVersion& Other) const;
bool operator!=(const FSGDynamicTextAssetVersion& Other) const;
bool operator<(const FSGDynamicTextAssetVersion& Other) const;
bool operator>(const FSGDynamicTextAssetVersion& Other) const;
bool operator<=(const FSGDynamicTextAssetVersion& Other) const;
bool operator>=(const FSGDynamicTextAssetVersion& Other) const;
```

Comparison order: Major, then Minor, then Patch.

### Serialization

`FSGDynamicTextAssetVersion` supports custom binary serialization (3 integers without property tags) and network serialization via `TStructOpsTypeTraits` with `WithSerializer` and `WithNetSerializer`.

## Migration System

When a dynamic text asset file has a major version older than the class's current major version, the serializer calls the migration hook before populating properties.

### Property State During Migration

**Important:** When `MigrateFromVersion()` is called, the UPROPERTY fields on the object have **not** been populated yet from the JSON.
The migration operates on the raw `FJsonObject` data, which still reflects the old version's schema.
After migration modifies the JSON in-place, the (now transformed) JSON is deserialized into UPROPERTY fields via reflection.
This means you cannot read UPROPERTY values during migration; instead, you read from the `OldData` JSON object directly.

### Implementing Migration

1. Override `GetCurrentMajorVersion()` to return your class's current major version.
2. Override `MigrateFromVersion()` to transform old JSON data to the new format.

```cpp
UCLASS(BlueprintType)
class UWeaponData : public USGDynamicTextAsset
{
    GENERATED_BODY()
public:

    // Current version 2 properties
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float BaseDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float CritMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float DamageScaling = 1.0f;

protected:

    virtual int32 GetCurrentMajorVersion() const override { return 2; }

    virtual bool MigrateFromVersion(
        const FSGDynamicTextAssetVersion& OldVersion,
        const FSGDynamicTextAssetVersion& CurrentVersion,
        const TSharedPtr<FJsonObject>& OldData) override
    {
        if (OldVersion.Major == 1)
        {
            // In version 1, "Damage" was an int32. In version 2, it became "BaseDamage" as float.
            // OldData still has the version 1 schema. UPROPERTYs are NOT populated yet.
            double oldDamage = 0;
            if (OldData->TryGetNumberField(TEXT("Damage"), oldDamage))
            {
                OldData->RemoveField(TEXT("Damage"));
                OldData->SetNumberField(TEXT("BaseDamage"), oldDamage);
            }

            // CritMultiplier is new in version 2, set a default for migrated data.
            // The current class default is 1.5, but version 1 data was balanced
            // without crit, so use 1.0 to preserve original damage behavior.
            OldData->SetNumberField(TEXT("CritMultiplier"), 1.0);

            // DamageScaling is also new in version 2. Apply a conversion factor
            // since version 1 damage values were 10x the new scale.
            OldData->SetNumberField(TEXT("DamageScaling"), 10.0);

            // After this returns true, the modified OldData JSON will be
            // deserialized into the class's UPROPERTY fields.
            return true;
        }

        // Unknown version, migration failed
        return false;
    }
};
```

### Migration Flow

During `FSGDynamicTextAssetJsonSerializer::DeserializeObject()`:

1. The `version` field is parsed from the JSON file into an `FSGDynamicTextAssetVersion`.
2. The file's version is compared to `GetCurrentVersion()` (which defaults to `{GetCurrentMajorVersion(), 0, 0}`).
3. If the file's major version is older, `MigrateFromVersion()` is called with the old version, the current version, and the raw `data` JSON object.
4. The migration modifies the JSON object in-place (rename fields, convert types, set defaults).
5. The (possibly migrated) JSON is then deserialized into UPROPERTY fields via reflection.
6. An optional `OutMigrated` flag is set to `true` so callers know migration occurred.

If migration fails (returns `false`), the load operation fails.

### Best Practices

- Always handle all previous major versions in your migration function, not just the immediately preceding one. A file at version 1 may need to migrate through versions 2, 3, and 4 sequentially.
- Set sensible defaults for new fields during migration. Consider whether the class default or a migration-specific value better preserves the intended behavior of old data.
- Test migrations with saved files from each historical version.
- Only increment the major version when existing data must be transformed. Adding new optional fields is a minor version change and requires no migration.
- Remember that UPROPERTYs are **not** populated during migration. All reads and writes must go through the `OldData` JSON object.

[Back to Table of Contents](../TableOfContents.md)
