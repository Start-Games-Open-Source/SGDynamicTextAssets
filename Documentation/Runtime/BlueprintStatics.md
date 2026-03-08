# Blueprint Statics

[Back to Table of Contents](../TableOfContents.md)

## USGDynamicTextAssetStatics

A `UBlueprintFunctionLibrary` that exposes dynamic text asset operations to Blueprints. All functions are static and appear in the Blueprint action menu under "SG Dynamic Text Assets".

```cpp
UCLASS(ClassGroup = "Start Games")
class USGDynamicTextAssetStatics : public UBlueprintFunctionLibrary
```

## Reference Functions

These functions operate on `FSGDynamicTextAssetRef` structs and do not require a world context.

| Function | Type | Description |
|----------|------|-------------|
| `IsValidDynamicTextAssetRef(Ref)` | Pure | Returns true if the ID is valid |
| `SetDynamicTextAssetRefById(Ref, Id)` | Callable | Sets the reference to the given FSGDynamicTextAssetId |
| `SetDynamicTextAssetRefByUserFacingId(Ref, UserFacingId)` | Callable | Looks up a UserFacingId via file scan and sets the ID. Returns true if found. |
| `ClearDynamicTextAssetRef(Ref)` | Callable | Resets the reference to invalid state |
| `GetDynamicTextAssetRefId(Ref)` | Pure | Returns the FSGDynamicTextAssetId from the reference |
| `GetDynamicTextAssetRefUserFacingId(Ref)` | Pure | Dynamically resolves the user-facing ID via file metadata lookup |
| `MakeDynamicTextAssetRef(Id)` | Pure | Creates a reference from an FSGDynamicTextAssetId |
| `EqualEqual_DynamicTextAssetRef(A, B)` | Pure | Returns true if two refs are equal (==) |
| `NotEqual_DynamicTextAssetRef(A, B)` | Pure | Returns true if two refs are not equal (!=) |
| `Equals_DynamicTextAssetRefDynamicTextAssetId(Ref, Id)` | Pure | Returns true if ref equals an ID (==) |
| `NotEquals_DynamicTextAssetRefDynamicTextAssetId(Ref, Id)` | Pure | Returns true if ref does not equal an ID (!=) |

## Reference Functions (World Context)

These functions require a world context object to access the subsystem.

| Function | Type | Description |
|----------|------|-------------|
| `IsDynamicTextAssetRefLoaded(WorldContext, Ref)` | Pure | Returns true if the referenced object is in cache |
| `GetDynamicTextAsset(WorldContext, Ref)` | Pure | Resolves the reference to `TScriptInterface<ISGDynamicTextAssetProvider>`. Returns invalid interface if not cached. |
| `LoadDynamicTextAssetRefAsync(WorldContext, Ref, OnLoaded, FilePath)` | Callable | Loads the referenced object asynchronously. `FilePath` is optional: if empty, the system searches by ID. |
| `UnloadDynamicTextAssetRef(WorldContext, Ref)` | Callable | Removes the referenced object from cache |

## Collection Functions

| Function | Type | Description |
|----------|------|-------------|
| `GetAllDynamicTextAssetIdsByClass(Class, bIncludeSubclasses, OutIds)` | Callable | Scans the file system for all FSGDynamicTextAssetIds of the given class |
| `GetAllDynamicTextAssetUserFacingIdsByClass(Class, bIncludeSubclasses, OutIds)` | Callable | Scans the file system for all UserFacingIds of the given class |
| `GetAllLoadedDynamicTextAssetsOfClass(WorldContext, Class, bIncludeSubclasses, OutObjects)` | Callable | Returns all cached `TScriptInterface<ISGDynamicTextAssetProvider>` of the given class |

## FSGDynamicTextAssetId Utility Functions

| Function | Type | Description |
|----------|------|-------------|
| `IdToString(Id)` | Pure | Converts an FSGDynamicTextAssetId to its string representation |
| `StringToId(String, OutId)` | Pure | Parses a string to an FSGDynamicTextAssetId. Returns true if parsing succeeded. |
| `EqualEqual_DynamicTextAssetId(A, B)` | Pure | Returns true if two IDs are equal (==) |
| `NotEqual_DynamicTextAssetId(A, B)` | Pure | Returns true if two IDs are not equal (!=) |
| `GetDynamicTextAssetTypeFromId(Id)` | Pure | Resolves the UClass for a given ID by scanning file metadata. Can be heavy with overuse. |

## Asset Type ID Functions

These functions operate on `FSGDynamicTextAssetTypeId` structs.

| Function | Type | Description |
|----------|------|-------------|
| `IsValid_DTA_TypeId(AssetTypeId)` | Pure | Returns true if the type ID has a valid (non-zero) GUID |
| `EqualEqual_DTA_TypeId(A, B)` | Pure | Returns true if two type IDs are equal (==) |
| `NotEqual_DTA_TypeId(A, B)` | Pure | Returns true if two type IDs are not equal (!=) |
| `ToString_DTA_TypeId(AssetTypeId)` | Pure | Converts to hyphenated GUID string |
| `FromString_DTA_TypeId(String, OutAssetTypeId)` | Pure | Parses a string to an asset type ID |
| `FromGuid_DTA_TypeId(Guid)` | Pure | Creates an asset type ID from an FGuid |
| `GetGuid_DTA_TypeId(AssetTypeId)` | Pure | Returns a copy of the type ID's GUID |
| `GenerateDynamicAssetTypeId()` | Pure | Creates a new randomly generated asset type ID (DevelopmentOnly) |

## Provider Functions

These functions operate on `TScriptInterface<ISGDynamicTextAssetProvider>`.

| Function | Type | Description |
|----------|------|-------------|
| `GetDynamicTextAssetId_Provider(Provider)` | Pure | Returns the unique dynamic text asset ID |
| `GetUserFacingId_Provider(Provider)` | Pure | Returns the human-readable identifier |
| `GetCurrentMajorVersion_Provider(Provider)` | Pure | Returns the current major version declared by the class |
| `GetVersion_Provider(Provider)` | Pure | Returns the version of the instance (from file) |
| `GetCurrentVersion_Provider(Provider)` | Pure | Returns the full current version declared by the class |

## Version Functions

| Function | Type | Description |
|----------|------|-------------|
| `IsVersionValid(Version)` | Pure | Returns true if version fields represent a usable version (Major >= 1) |
| `IsVersionCompatibleWith(A, B)` | Pure | Returns true if major versions match (compatible for loading) |
| `EqualEqual_DynamicTextAssetVersion(A, B)` | Pure | Returns true if two versions are equal (==) |
| `NotEqual_DynamicTextAssetVersion(A, B)` | Pure | Returns true if two versions are not equal (!=) |

## Validation Functions

These functions query `FSGDynamicTextAssetValidationResult` structs. See [Validation](../Core/Validation.md) for the full validation system.

| Function | Type | Description                         |
|----------|------|-------------------------------------|
| `ValidationResultHasErrors(Result)` | Pure | True if any error entries exist     |
| `ValidationResultHasWarnings(Result)` | Pure | True if any warning entries exist   |
| `ValidationResultHasInfos(Result)` | Pure | True if any info entries exist      |
| `IsValidationResultEmpty(Result)` | Pure | True if no entries at all           |
| `IsValidationResultValid(Result)` | Pure | True if no errors (pass)            |
| `GetValidationResultTotalCount(Result)` | Pure | Total number of entries             |
| `GetValidationResultErrors(Result, OutErrors)` | Pure | Get all error entries               |
| `GetValidationResultWarnings(Result, OutWarnings)` | Pure | Get all warning entries             |
| `GetValidationResultInfos(Result, OutInfos)` | Pure | Get all info entries                |
| `ValidationResultToString(Result)` | Pure | Formatted multi line display string |

## Diagnostics Functions

| Function | Type | Description |
|----------|------|-------------|
| `LogRegisteredSerializers()` | Callable | Logs all registered serializer types and IDs to the output log. Disabled in shipping builds by default. |
| `GetRegisteredSerializerDescriptions(OutDescriptions)` | Callable | Returns a diagnostic string for each serializer formatted as `"TypeId=N \| Extension='ext' \| Format='name'"` |

## C++ Only Utility Functions

These functions are not exposed to Blueprints (serializer instances are not UObjects).

| Function | Description |
|----------|-------------|
| `FindSerializerForTypeId(TypeId)` | Returns the serializer registered with the given integer TypeId, or nullptr |
| `FindSerializerForDynamicTextAssetId(Id)` | Locates the file for the ID and returns the matching serializer for its format |
| `GetTypeIdForExtension(Extension)` | Returns the integer TypeId for a given extension (e.g., `"dta.json"` -> `1`), or 0 if not found |

## Collection Query Performance Notes

When querying collections of dynamic text assets, be aware of the underlying data source:

| Function | Data Source | Performance |
|----------|-------------|-------------|
| `GetAllDynamicTextAssetIdsByClass()` | File system scan | Scales with file count; slower for large datasets |
| `GetAllDynamicTextAssetUserFacingIdsByClass()` | File system scan | Scales with file count; slower for large datasets |
| `GetAllLoadedDynamicTextAssetsOfClass()` | Subsystem cache | Fast; queries in-memory map only |

The file system scanning functions (`GetAllDynamicTextAssetIdsByClass`, `GetAllDynamicTextAssetUserFacingIdsByClass`) iterate over files on disk using `FindAllFilesForClass()` and extract metadata from each file. For large projects with hundreds of dynamic text assets, prefer loading assets first and then querying the cache via `GetAllLoadedDynamicTextAssetsOfClass()`.

[Back to Table of Contents](../TableOfContents.md)
