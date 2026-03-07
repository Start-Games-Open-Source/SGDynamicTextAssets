# Dynamic Text Asset References

[Back to Table of Contents](../TableOfContents.md)

## FSGDynamicTextAssetRef

`FSGDynamicTextAssetRef` is a lightweight struct for referencing dynamic text assets by `FSGDynamicTextAssetId`.
Use it instead of raw pointers or IDs in your game classes when initially retrieving them.

```cpp
USTRUCT(BlueprintType)
struct FSGDynamicTextAssetRef
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `Id` | `FSGDynamicTextAssetId` | The unique identifier referencing the dynamic text asset. Serialized. |

### Declaring References

Use `FSGDynamicTextAssetRef` as a UPROPERTY on your game classes. The `DynamicTextAssetType` metadata controls which dynamic text asset classes appear in the editor picker.

```cpp
// Unrestricted: picker shows all dynamic text asset types
UPROPERTY(EditAnywhere)
FSGDynamicTextAssetRef AnyRef;

// Restricted to UWeaponData and its subclasses
UPROPERTY(EditAnywhere, meta = (DynamicTextAssetType = "UWeaponData"))
FSGDynamicTextAssetRef WeaponRef;

// Array of references
UPROPERTY(EditAnywhere, meta = (DynamicTextAssetType = "UQuestData"))
TArray<FSGDynamicTextAssetRef> QuestRefs;
```

### Resolving References

References are resolved lazily: `Get()` returns a `TScriptInterface<ISGDynamicTextAssetProvider>` fetched from the subsystem cache. 
The dynamic text asset must already be loaded into cache (via the subsystem) for resolution to succeed. A typed `Get<T>()` template casts to your concrete class.

```cpp
// Resolve to interface (returns TScriptInterface<ISGDynamicTextAssetProvider>)
TScriptInterface<ISGDynamicTextAssetProvider> provider = WeaponRef.Get(GetGameInstance());

// Type-safe resolve (returns nullptr if wrong type)
UWeaponData* weapon = WeaponRef.Get<UWeaponData>(GetGameInstance());

// Resolve with world context object (any UObject with a valid world)
UWeaponData* weapon = WeaponRef.Get<UWeaponData>(this);
```

### Async Loading

If the referenced dynamic text asset may not be loaded yet, use `LoadAsync` to load it on demand.
The file path is resolved automatically from the ID:

```cpp
WeaponRef.LoadAsync(this,
    [](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        if (bSuccess)
        {
            UWeaponData* weapon = Cast<UWeaponData>(Provider.GetObject());
        }
    });
```

### Validity and State

```cpp
// True if ID is valid, does not mean it is loaded!
bool IsValid() const;

// True if ID is invalid (empty reference)
bool IsNull() const;

const FSGDynamicTextAssetId& GetId() const;

/**
 * Resolves and returns the user-facing ID by looking up the dynamic text asset's file metadata.
 * Works in any context: editor, runtime, without a world or game instance.
 * Returns an empty string if the ID is invalid or no matching file is found.
 */
FString GetUserFacingId() const;

void SetId(const FSGDynamicTextAssetId& InId);

// Invalidates the ID
void Reset();
```

### Equality and Hashing

References compare by ID only.

```cpp
// Id == Other.Id
bool operator==(const FSGDynamicTextAssetRef& Other) const;
bool operator!=(const FSGDynamicTextAssetRef& Other) const;

// Supports TMap/TSet usage
friend uint32 GetTypeHash(const FSGDynamicTextAssetRef& Ref);
```

### Construction

```cpp
// Invalid/null reference
FSGDynamicTextAssetRef();

// Reference by ID
FSGDynamicTextAssetRef(const FSGDynamicTextAssetId& InId);
                   
// Move construction
explicit FSGDynamicTextAssetRef(const FSGDynamicTextAssetId&& InId);
```

### Serialization

`FSGDynamicTextAssetRef` supports binary and network serialization via `TStructOpsTypeTraits`:

```cpp
// Binary: serializes Id only (GUID)
bool Serialize(FArchive& Ar);

// Network replication
bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
```

This allows `FSGDynamicTextAssetRef` to be used in replicated UPROPERTYs and properly serialized to save files.

## Editor Picker

When `FSGDynamicTextAssetRef` is displayed in a Details panel, a custom property customization (`FSGDynamicTextAssetRefCustomization`) replaces the default struct view with a searchable dropdown picker. The picker:

- Lists all available dynamic text assets matching the type filter (from `DynamicTextAssetType` UPROPERTY metadata or an editor-local filter).
- Supports text search by UserFacingId.
- Shows buttons for Copy, Paste, Clear, and Open Editor.
- The **Paste** button auto-detects clipboard content as a GUID or UserFacingId and resolves it to a dynamic text asset.

See [Property Customizations](../Editor/PropertyCustomizations.md) for details.

## Blueprint Usage

`FSGDynamicTextAssetRef` can be used directly in Blueprints. The `USGDynamicTextAssetStatics` function library provides Blueprint-friendly wrappers:

- `IsValidDynamicTextAssetRef(Ref)`: Check validity.
- `GetDynamicTextAsset(WorldContext, Ref)`: Resolve to loaded object.
- `MakeDynamicTextAssetRef(Guid)`: Create a reference from a GUID.
- `SetDynamicTextAssetRefById(Ref, Id)`: Set the ID on a reference.
- `ClearDynamicTextAssetRef(Ref)`: Reset to invalid state.
- `GetDynamicTextAssetRefUserFacingId(Ref)`: Dynamically resolves the user-facing ID via file metadata lookup.
- `LoadDynamicTextAssetRefAsync(WorldContext, Ref, OnLoaded)`: Async load.

See [Blueprint Statics](../Runtime/BlueprintStatics.md) for the full list.

[Back to Table of Contents](../TableOfContents.md)
