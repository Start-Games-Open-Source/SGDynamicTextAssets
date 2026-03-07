# Error Handling and Debugging

[Back to Table of Contents](../TableOfContents.md)

## ESGLoadResult Codes

All load operations return or communicate results through `ESGLoadResult`:

```cpp
UENUM(BlueprintType)
enum class ESGLoadResult : uint8
```

| Code | Typical Cause | Recovery |
|------|--------------|----------|
| `Success` | File found, deserialized, and cached | Normal path |
| `AlreadyLoaded` | Asset was already in the subsystem cache | Returns cached object |
| `FileNotFound` | No `.dta.json` or `.dta.bin` file exists for the ID | Callback receives empty provider and `false` |
| `DeserializationFailed` | JSON parse error, schema mismatch, or missing serializer | Error logged, callback receives `false` |
| `ValidationFailed` | `Native_ValidateDynamicTextAsset()` returned errors | Warning logged, but object is **still cached** |
| `ServerDeletedObject` | Server response contains empty `Data` for this ID | Local cache entry should be cleared |
| `ServerFetchFailed` | Network error or server returned failure | Falls back to local file if available |
| `Timeout` | Operation exceeded time limit | Retry or fallback |

### Validation Warning Behavior

`ValidationFailed` is a special case: the object is still added to cache and returned successfully, but a warning is logged. Validation errors do not prevent usage; they alert developers to data quality issues.

```
[Warning] Loaded dynamic text asset has validation errors: Id(550E8400-...) - [Error] BaseDamage must be > 0 (PropertyPath: BaseDamage)
```

## Error Recovery Patterns

### File Not Found

When `ReadRawFileContents()` fails or no file matches the requested ID:
- Sync load returns an empty `TScriptInterface`
- Async load invokes the callback with `(emptyProvider, false)`
- Error logged to `LogSGDynamicTextAssetsRuntime`

### Deserialization Failure

When `DeserializeProvider()` returns `false`:
- The partially-constructed `UObject` is abandoned (GC will collect it)
- Callback receives `(emptyProvider, false)`

### Server Fallback

When the server is unavailable or returns failure:
- The subsystem falls back to local file data
- `USGDynamicTextAssetServerNullInterface` (null object pattern) is the default, so all server operations succeed as no-ops

### Migration Failure

When `MigrateFromVersion()` returns `false`:
- Deserialization fails entirely (treated as `DeserializationFailed`)
- The file is not modified

### Migration Success (Editor Only)

When migration succeeds in the editor:
- The migrated data is re-serialized and saved back to the file
- If the re-save fails, a warning is logged but the loaded object is still valid

## Log Categories

| Category | Module | Declaration |
|----------|--------|-------------|
| `LogSGDynamicTextAssetsRuntime` | Runtime | `SGDynamicTextAssetsRuntimeModule.h` |
| `LogSGDynamicTextAssetsEditor` | Editor | `SGDynamicTextAssetsEditorModule.h` |

### Enabling Verbose Logging

In the Unreal console or `DefaultEngine.ini`:

```
// Console command
LogSGDynamicTextAssetsRuntime Verbose

// DefaultEngine.ini
[Core.Log]
LogSGDynamicTextAssetsRuntime=Verbose
LogSGDynamicTextAssetsEditor=Verbose
```

Verbose mode logs cache hits, async pipeline steps, and serializer routing decisions.

## Common Log Messages

### Errors

| Message Pattern | Meaning |
|----------------|---------|
| `Inputted INVALID Id` | A function was called with an uninitialized or zero `FSGDynamicTextAssetId` |
| `Inputted EMPTY FilePath` | A load function received an empty string path |
| `No serializer found for FilePath(...)` | No registered serializer matches the file extension |
| `Failed to deserialize dynamic text asset from FilePath(...)` | JSON/XML/YAML parsing or property population failed |
| `Failed to create instance of class(...)` | `NewObject` failed (class may be abstract or invalid) |
| `Provider UObject(...) does not implement ISGDynamicTextAssetProvider` | Attempted to cache an object that doesn't implement the interface |
| `Provider(...) has INVALID id` | Attempted to cache an object with an uninitialized GUID |
| `DynamicTextAssetClass was nullptr and could not be resolved from file` | Class couldn't be determined from file metadata or TypeId |

### Warnings

| Message Pattern | Meaning |
|----------------|---------|
| `Provider already in cache: id(...)` | Duplicate cache insertion attempted |
| `Loaded dynamic text asset has validation errors: ...` | Validation found issues (object still cached) |
| `Migration succeeded but failed to re-save FilePath(...)` | File write failed after successful migration |
| `Async load callback: subsystem no longer valid` | Subsystem was destroyed before async callback fired |
| `Could not extract class name from FilePath(...)` | Metadata extraction failed during batch load |

### Info (Log)

| Message Pattern | Meaning |
|----------------|---------|
| `Loaded dynamic text asset: Id(...) Class(...) FilePath(...)` | Successful load |
| `Added provider to cache: id(...) Class(...)` | Object added to cache |
| `Cleared N dynamic text assets from cache` | `ClearCache()` was called |
| `Loaded N dynamic text assets of class(...)` | `LoadAllDynamicTextAssetsOfClass` completed |

## Debugging Workflows

### Checking Cache State

```cpp
USGDynamicTextAssetSubsystem* Subsystem = UGameInstance::GetSubsystem<USGDynamicTextAssetSubsystem>(GetGameInstance());

// Is a specific asset cached?
bool bCached = Subsystem->IsDynamicTextAssetCached(SomeId);

// How many assets are currently in memory?
int32 Count = Subsystem->GetCachedObjectCount();
```

### Monitoring Async Loads

```cpp
// Are there any loads in flight?
bool bLoading = Subsystem->IsAsyncLoadInProgress();

// How many?
int32 Pending = Subsystem->GetPendingAsyncLoadCount();
```

### Batch Validation

Use the validation commandlet to check all files without loading the full editor:

```
UE4Editor.exe MyProject.uproject -run=SGDynamicTextAssetValidation
```

This scans all `.dta.json` files, deserializes each one, runs validation, and reports errors.

### Verifying Serializer Registration

```cpp
// In console or C++
USGDynamicTextAssetStatics::LogRegisteredSerializers();
```

Outputs one line per registered serializer:
```
TypeId=1 | Extension='.dta.json' | Format='JSON'
TypeId=2 | Extension='.dta.xml'  | Format='XML'
TypeId=3 | Extension='.dta.yaml' | Format='YAML'
```

## Hard Reference Validation

`FSGDynamicTextAssetValidationUtils` provides static analysis to enforce the soft-reference-only design principle.

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

Properties declared on the highest class that implements `ISGDynamicTextAssetProvider` (typically `USGDynamicTextAsset`) are exempt. These are part of the provider contract, not user data. `FindProviderBoundaryClass()` walks the class hierarchy upward to identify this boundary.

### Recursive Container Inspection

`IsHardReferenceProperty()` recursively inspects container inner types. A `TArray<TObjectPtr<UStaticMesh>>` is detected as a violation even though the outer type is `TArray`.

### Usage

Hard reference detection runs automatically during validation. It can also be invoked manually:

```cpp
TArray<FString> violations;
if (FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(UWeaponData::StaticClass(), violations))
{
    for (const FString& v : violations)
    {
        UE_LOG(LogTemp, Warning, TEXT("Hard reference: %s"), *v);
    }
}
```

[Back to Table of Contents](../TableOfContents.md)
