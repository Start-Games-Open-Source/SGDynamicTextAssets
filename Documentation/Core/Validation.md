# Validation

[Back to Table of Contents](../TableOfContents.md)

## Overview

The validation system checks dynamic text assets for correctness and reports issues at three severity levels. Validation runs in the editor when saving, can be triggered via commandlet for CI/CD, and results are accessible from Blueprints.

## Severity Levels

```cpp
UENUM(BlueprintType)
enum class ESGValidationSeverity : uint8
{
   // Informational note, does not prevent saving
   Info,
   // Non-critical issue, does not prevent saving
   Warning,
   // Critical issue, prevents saving
   Error
};
```

Only **Error** entries cause validation to fail. Warnings and Info entries are reported but do not block operations.

## FSGDynamicTextAssetValidationEntry

A single validation finding:

```cpp
USTRUCT(BlueprintType)
struct FSGDynamicTextAssetValidationEntry
{
   // Human-readable description
   FText Message;
   
   // Property that caused the issue (e.g., "Damage", "Refs[0]")
   FString PropertyPath;
   
   // Optional fix suggestion
   FText SuggestedFix;
   
   ESGValidationSeverity Severity = ESGValidationSeverity::Error;
};
```

Helper queries:
- `IsError()`: True if severity is Error.
- `IsWarning()`: True if severity is Warning.
- `IsInfo()`: True if severity is Info.

## FSGDynamicTextAssetValidationResult

Container that holds arrays of entries by severity:

```cpp
USTRUCT(BlueprintType)
struct FSGDynamicTextAssetValidationResult
{
    TArray<FSGDynamicTextAssetValidationEntry> Errors;
    TArray<FSGDynamicTextAssetValidationEntry> Warnings;
    TArray<FSGDynamicTextAssetValidationEntry> Infos;
};
```

### Adding Entries

```cpp
FSGDynamicTextAssetValidationResult result;

// Convenience methods
result.AddError(LOCTEXT("DamageZero", "BaseDamage must be greater than zero"), TEXT("BaseDamage"));
result.AddWarning(LOCTEXT("NoDesc", "Description is empty"), TEXT("Description"));
result.AddInfo(LOCTEXT("Balanced", "Damage is within expected range"));

// With suggested fix
result.AddError(
    LOCTEXT("InvalidRef", "WeaponRef points to a deleted dynamic text asset"),
    TEXT("WeaponRef"),
    LOCTEXT("InvalidRefFix", "Clear the reference or select a valid weapon"));

// Generic entry
result.AddEntry(FSGDynamicTextAssetValidationEntry(
    ESGValidationSeverity::Warning,
    LOCTEXT("HighDamage", "Damage exceeds recommended maximum"),
    TEXT("BaseDamage")));
```

### Querying State

```cpp
// Any error entries?
bool HasErrors() const;

// Any warning entries?
bool HasWarnings() const;

// Any informational entries?
bool HasInfos() const;

// No entries of any severity?
bool IsEmpty() const;

// No errors (warnings/infos are OK)
bool IsValid() const;

// Total entries across all severities
int32 GetTotalCount() const;
```

### Merging and Display

```cpp
FSGDynamicTextAssetValidationResult result;

// Merge entries from another result
result.Append(OtherResult);

// Clear all entries
result.Reset();

// Multi-line display string
FString text = result.ToFormattedString();
```

## Custom Validation

Override `ValidateDynamicTextAsset()` on your dynamic text asset subclass to add domain-specific checks:

```cpp
bool UWeaponData::ValidateDynamicTextAsset_Implementation(FSGDynamicTextAssetValidationResult& OutResult) const
{
    if (BaseDamage <= 0.0f)
    {
        OutResult.AddError(
            INVTEXT("BaseDamage must be greater than zero"),
            TEXT("BaseDamage"),
            INVTEXT("Set BaseDamage to a positive value"));
    }

    if (AttackSpeed <= 0.0f)
    {
        OutResult.AddError(
            INVTEXT("AttackSpeed must be greater than zero"),
            TEXT("AttackSpeed"));
    }

    if (DisplayName.IsEmpty())
    {
        OutResult.AddWarning(
            INVTEXT("DisplayName is empty"),
            TEXT("DisplayName"));
    }

    return !OutResult.HasErrors();
}
```

### Validation Flow

`Native_ValidateDynamicTextAsset()` is the entry point called by the system. It:

1. Runs built-in checks (GUID validity, UserFacingId non-empty, Version validity).
2. Calls the `ValidateDynamicTextAsset()` BlueprintNativeEvent for custom rules.
3. Returns `true` if no error-severity entries were produced.

## When Validation Runs

- **Editor save**: Validation runs before writing to disk. Errors prevent the save.
- **Cook commandlet**: The `-validate` flag runs validation on all (or filtered) dynamic text assets without producing binary output.
- **Manual**: Call `Native_ValidateDynamicTextAsset()` on any dynamic text asset instance at any time.

## Hard Reference Prevention

In addition to runtime validation, the plugin enforces a **compile-time** rule: `USGDynamicTextAsset` subclasses cannot contain hard reference UPROPERTYs. This catches violations before the code even compiles, preventing hard references from ever reaching serialization or packaging.

For full details on the UHT plugin file structure, build discovery, and distribution setup, see [UHT Plugin](UhtPlugin.md).

### How It Works

The validator is a **UHT exporter plugin** written in C# (not C++). It runs during the **Unreal Header Tool** phase of the build process, before C++ compilation begins. UHT parses all headers first, then invokes registered exporters. This validator is one of them.

**Source file:** `Source/SGDynamicTextAssetsUhtPlugin/SGDynamicTextAssetHardRefValidator.cs`

The entry point is the `[UhtExporter]`-decorated method `ValidateHardReferences()`, which instantiates the `Validator` class:

```csharp
[UhtExporter(
    Name = "SGDynamicTextAssetHardRefValidator",
    Description = "Validates USGDynamicTextAsset subclasses have no hard reference UPROPERTYs",
    ModuleName = "SGDynamicTextAssetsRuntime",
    Options = UhtExporterOptions.Default)]
private static void ValidateHardReferences(IUhtExportFactory factory)
{
    new Validator(factory).Run();
}
```

### Validation Flow

The `Validator` class performs three key steps:

1. **`Run()`**: Finds `USGDynamicTextAsset` in the UHT type hierarchy via `FindClassByName()`. Then walks all parsed types across every module to find subclasses using `IsChildOf()`. Calls `ValidateProperties()` on each subclass found.

2. **`ValidateProperties(UhtClass)`**: Iterates `classType.Children` looking for `UhtProperty` instances. Checks for the `SGSkipHardRefValidation` meta tag at both the class level (`classType.MetaData`) and individual property level (`property.MetaData`). Properties declared on the `ISGDynamicTextAssetProvider` boundary class (e.g., `DynamicTextAssetId`, `UserFacingId`, `Version`) are automatically skipped. All other properties are passed to `CheckPropertyForHardRef()`.

3. **`CheckPropertyForHardRef(UhtProperty, string)`**: Determines if a property is a prohibited hard reference. The check order matters due to UHT type inheritance (`UhtClassProperty` inherits `UhtObjectProperty`, so derived types must be checked first):
   - **Allow:** `UhtSoftClassProperty`, `UhtSoftObjectProperty` (soft references)
   - **Allow:** `UhtWeakObjectPtrProperty`, `UhtLazyObjectPtrProperty` (non-owning references)
   - **Allow:** `UhtStructProperty` (value types)
   - **Reject:** `UhtClassProperty` → emits error suggesting `TSoftClassPtr<>`
   - **Reject:** `UhtObjectProperty` → emits error suggesting `TSoftObjectPtr<>`
   - **Recurse:** `UhtArrayProperty`, `UhtMapProperty`, `UhtSetProperty` → checks inner element types

Violations are reported via `property.LogError()`, which produces a **build error** in the UHT output visible in the compiler's error list.

### Prohibited Property Types

The following UPROPERTY types cause a **build error** on any `USGDynamicTextAsset` subclass:

| Type | Error Message |
|------|---------------|
| `TObjectPtr<T>` / `UObject*` | Use `TSoftObjectPtr<T>` instead |
| `TSubclassOf<T>` | Use `TSoftClassPtr<T>` instead |
| `TArray<TObjectPtr<T>>` | Use `TArray<TSoftObjectPtr<T>>` instead |
| `TMap<..., TObjectPtr<T>>` | Use `TMap<..., TSoftObjectPtr<T>>` instead |
| `TSet<TObjectPtr<T>>` | Use `TSet<TSoftObjectPtr<T>>` instead |

### Allowed Property Types

These types are safe and will not trigger errors:

- `TSoftObjectPtr<T>`, `TSoftClassPtr<T>` (soft asset references)
- `TWeakObjectPtr<T>`, `TLazyObjectPtr<T>` (non-owning runtime references)
- `FSGDynamicTextAssetRef` (dynamic text asset cross-reference by ID)
- All primitive types, structs, strings, containers of the above

### Opting Out

In rare cases where a hard reference is intentional (e.g., test types), use the `SGSkipHardRefValidation` meta tag to suppress the error:

```cpp
// Skip validation for an entire class
UCLASS(meta = (SGSkipHardRefValidation))
class UMySpecialDataObject : public USGDynamicTextAsset { ... };

// Skip validation for a single property
UPROPERTY(meta = (SGSkipHardRefValidation))
TObjectPtr<UObject> IntentionalHardRef = nullptr;
```

The opt-out is checked via `UhtType.MetaData.ContainsKey("SGSkipHardRefValidation")` in `ValidateProperties()`.

### Verifying the Validator Is Running

The validator emits an info-level message to the build output after every run. Search the build log for `SGDynamicTextAssetHardRefValidator` to confirm it executed:

| Message | Meaning |
|---------|---------|
| `SGDynamicTextAssetHardRefValidator: Scanned N USGDynamicTextAsset subclass(es)` | Validator ran successfully and checked N subclasses. |
| `SGDynamicTextAssetHardRefValidator: USGDynamicTextAsset not found in parsed types - skipping validation` | Validator ran but `USGDynamicTextAsset` was not found in the UHT type hierarchy. This typically means `SGDynamicTextAssetsRuntime` is not included as a dependency. |

If neither message appears in the build output, the UHT plugin itself was not discovered by UBT. See [UHT Plugin](UhtPlugin.md) for setup and troubleshooting.

### Why This Matters

Dynamic text assets are JSON-serialized and loaded by GUID. Hard references to UObjects would:
- Create hidden dependencies on `.uasset` files, breaking the JSON-as-source-of-truth principle
- Cause assets to be loaded into memory when the dynamic text asset is loaded, defeating lazy loading
- Create packaging issues since dynamic text assets ship as `.dta.bin`, not as `.uasset`

## Blueprint Access

The `USGDynamicTextAssetStatics` function library exposes validation queries to Blueprints:

| Function | Description |
|----------|-------------|
| `ValidationResultHasErrors(Result)` | True if any errors exist |
| `ValidationResultHasWarnings(Result)` | True if any warnings exist |
| `ValidationResultHasInfos(Result)` | True if any info entries exist |
| `IsValidationResultEmpty(Result)` | True if no entries at all |
| `IsValidationResultValid(Result)` | True if no errors (pass) |
| `GetValidationResultTotalCount(Result)` | Total entry count |
| `GetValidationResultErrors(Result, OutErrors)` | Get error entries |
| `GetValidationResultWarnings(Result, OutWarnings)` | Get warning entries |
| `GetValidationResultInfos(Result, OutInfos)` | Get info entries |
| `ValidationResultToString(Result)` | Formatted display string |

## Hard Reference Detection

`FSGDynamicTextAssetValidationUtils` provides static analysis to enforce the soft-reference-only design principle at the property level.

### DetectHardReferenceProperties

```cpp
static bool DetectHardReferenceProperties(const UClass* InClass, TArray<FString>& OutViolations);
```

Scans all `UPROPERTY` fields on a class for hard reference types:

| Detected Type | Example |
|---------------|---------|
| `TObjectPtr<T>` | `TObjectPtr<UStaticMesh>` |
| `TSubclassOf<T>` | `TSubclassOf<AActor>` |
| Raw `UObject*` | `UTexture2D* Texture` |
| Containers of the above | `TArray<TObjectPtr<UMaterial>>` |

### Provider Boundary Exemption

Properties declared on the highest class in the hierarchy that implements `ISGDynamicTextAssetProvider` are exempt from checks. `FindProviderBoundaryClass()` walks the class hierarchy upward to find this boundary. This exempts provider contract properties (DynamicTextAssetId, UserFacingId, Version) from triggering violations.

### Recursive Container Inspection

`IsHardReferenceProperty()` recursively inspects container inner types. A `TArray<TObjectPtr<UStaticMesh>>` is detected as a violation even though the outer type is `TArray`, and `TMap<FName, TObjectPtr<UMaterial>>` triggers on the value type.

### Instanced Object Properties

`UPROPERTY(Instanced)` object properties are an allowed exception to the hard reference rule. Instanced objects are owned sub-objects serialized inline within the DTA file, not external asset references.

**Conditions for Acceptance:**

An instanced property passes validation only when all three conditions are met:

| Condition | Check | Enforcement Layer |
|-----------|-------|-------------------|
| Has `CPF_InstancedReference` flag | `Property->HasAnyPropertyFlags(CPF_InstancedReference)` | Runtime + UHT |
| Class has `EditInlineNew` | `PropertyClass->HasAnyClassFlags(CLASS_EditInlineNew)` | Runtime + UHT |
| No nested hard references | Recursive `DetectHardReferenceProperties` on the instanced class | Runtime + UHT |

If an instanced property's class does NOT have `EditInlineNew`, the UHT validator emits an error explaining the class cannot be instantiated inline and is treated as a hard reference. If the instanced class contains nested hard references (e.g., a `TObjectPtr<>` inside it), the property is rejected to prevent circumventing the soft-reference rule.

**Helper Methods:**

```cpp
// Check if a property is an instanced object (CPF_InstancedReference flag)
static bool IsInstancedObjectProperty(const FProperty* InProperty);

// Resolve a UClass from a full class path string
// Supports both C++ ("/Script/Module.ClassName") and Blueprint
// ("/Game/Path/BP_Name.BP_Name_C") class paths
static UClass* ResolveInstancedObjectClass(const FString& ClassIdentifier);

// Recursively collect all UClass references used by instanced sub-objects
// Scans single, TArray, TSet, and TMap instanced properties
static void CollectInstancedObjectClasses(
    const UObject* Object,
    TSet<UClass*>& OutClasses);
```

These are static methods on `FSGDynamicTextAssetSerializerBase`.

**Subsystem Class Tracking:**

The `USGDynamicTextAssetSubsystem` tracks all `UClass` references used by instanced sub-objects across cached DTAs. This prevents garbage collection from unloading classes while any cached DTA depends on them.

- `GetTrackedInstancedClassCount()` returns the total number of unique tracked classes
- `GetAllTrackedInstancedClasses()` returns the deduplicated set of all tracked classes
- `GetTrackedInstancedClassesForId(Id)` returns the tracked classes for a specific DTA

When a DTA is removed from cache via `RemoveFromCache()`, the subsystem decrements reference counts for each instanced class. If any class drops to zero references, the subsystem broadcasts `OnInstancedClassesReleased` with an `FSGInstancedClassReleaseContext` containing:
- The removed DTA Id
- The list of released classes (now GC-eligible)
- The remaining tracked class count

External systems can bind to this delegate to trigger garbage collection, log diagnostics, or pre-load replacement classes as needed. The subsystem itself never force-unloads classes.

**Soft Path Validation in Instanced Objects:**

The `ValidateSoftPathsInProperty` walker recurses into instanced sub-objects, validating any `TSoftObjectPtr` or `TSoftClassPtr` properties found within them. This ensures soft references inside instanced sub-objects are checked during cook validation.

Sub-object soft paths (e.g., references to level actors like `/Game/Lvl_Basic.Lvl_Basic:PersistentLevel.MyActor`) are validated by checking that the parent asset exists in the Asset Registry, since the Asset Registry only tracks top-level assets, not individual sub-objects within packages.

See [Error Handling and Debugging](../Advanced/ErrorHandlingAndDebugging.md) for more on hard reference validation in practice.

## Asset Bundle Soft Reference Validation

Soft reference properties tagged with `meta=(AssetBundles="...")` are valid and expected. The hard reference validator does not flag them, since `TSoftObjectPtr` and `TSoftClassPtr` are always allowed. Asset bundle metadata is extracted from these properties during serialization and loading to support batch async loading.

For details on the asset bundle system, see [Section 13 of the Technical Design](../../SGDynamicTextAssets_Technical_Design.md) and the serialization format docs:
- [JSON Format - Asset Bundle Metadata](../Serialization/JsonFormat.md#asset-bundle-metadata)
- [XML Format - Asset Bundle Metadata](../Serialization/XmlFormat.md#asset-bundle-metadata)
- [YAML Format - Asset Bundle Metadata](../Serialization/YamlFormat.md#asset-bundle-metadata)

## Blueprint Soft Reference Validation

On-save validation detects `TSoftObjectPtr` properties that reference `UBlueprint` assets and reports a **warning**. `UBlueprint` objects are stripped during cooking (only `UBlueprintGeneratedClass` survives in packaged builds), so a `TSoftObjectPtr` pointing to a Blueprint asset resolves to null at runtime.

The validation uses `FAssetData::IsInstanceOf<UBlueprint>(EResolveClass::Yes)` to detect Blueprint references, which handles class redirectors and catches all Blueprint subclasses (`UAnimBlueprint`, `UWidgetBlueprint`, etc.).

**Suggested fixes shown to the user:**
- Use `TSoftClassPtr` to reference the Blueprint's generated class instead
- Reference a non-Blueprint asset (DataAsset, Material, Texture, etc.)

This check runs as part of `Native_ValidateDynamicTextAsset()` in the `ValidateSoftPathsInProperty` walker, which recurses into structs, arrays, maps, and instanced sub-objects.

[Back to Table of Contents](../TableOfContents.md)
