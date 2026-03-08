# Automated Tests

[Back to Table of Contents](../TableOfContents.md)

## Overview

The plugin includes a comprehensive suite of unit tests built on Unreal's Automation Framework. Tests cover core types, serialization (JSON, binary, XML, YAML), file management, type registration, provider interfaces, hard reference validation, and Blueprint statics.

## Running Tests

1. Open **Window > Developer Tools > Session Frontend**.
2. Go to the **Automation** tab.
3. Filter by `"SGDynamicTextAssets"` to see all plugin tests.
4. Select tests and click **Start Tests**.

## Test Location

All tests are in `SGDynamicTextAssetsEditor/Private/Tests/`. They are compiled only in editor builds.

```
Source/SGDynamicTextAssetsEditor/Private/Tests/
  SGDynamicTextAssetUnitTest.h                    # Minimal concrete subclass for testing
  SGDynamicTextAssetXmlUnitTest.h                 # Base test class for XML serializer tests
  SGDynamicTextAssetYamlUnitTest.h                # Base test class for YAML serializer tests
  SGDynamicTextAssetVersionTests.cpp
  SGDynamicTextAssetValidationResultTests.cpp
  SGDynamicTextAssetRefTests.cpp
  SGDynamicTextAssetJsonSerializerTests.cpp
  SGDynamicTextAssetBinarySerializerTests.cpp
  SGDynamicTextAssetXmlSerializerTests.cpp
  SGDynamicTextAssetYamlSerializerTests.cpp
  SGDynamicTextAssetFileManagerTests.cpp
  SGDynamicTextAssetCookManifestTests.cpp
  SGDynamicTextAssetStaticsTests.cpp
  SGDynamicTextAssetTypeIdTests.cpp
  SGDynamicTextAssetTypeManifestTests.cpp
  SGDynamicTextAssetTypeRegistryTests.cpp
  SGDynamicTextAssetProviderTests.cpp
  SGDynamicTextAssetHardReferenceTests.cpp
  SGDynamicTextAssetSerializerRegistryTests.cpp
  SGDynamicTextAssetServerNullInterfaceTests.cpp
```

## Test Data Objects

### USGDynamicTextAssetUnitTest

`USGDynamicTextAssetUnitTest` is a minimal concrete subclass used by general-purpose tests:

```cpp
/**
 * Minimal concrete subclass of USGDynamicTextAsset for unit testing.
 * Editor-only, not exposed to Blueprint.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden)
class USGDynamicTextAssetUnitTest : public USGDynamicTextAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)
    float TestDamage = 0.0f;
    UPROPERTY(EditAnywhere)
    FString TestName;
    UPROPERTY(EditAnywhere)
    int32 TestCount = 0;
};
```

### USGDynamicTextAssetXmlUnitTest

`USGDynamicTextAssetXmlUnitTest` is a base test class used by XML serializer tests. Provides helper utilities for constructing XML test fixtures and validating XML output.

### USGDynamicTextAssetYamlUnitTest

`USGDynamicTextAssetYamlUnitTest` is a base test class used by YAML serializer tests. Provides helper utilities for constructing YAML test fixtures and validating YAML output via fkYAML integration.

## Test Coverage

### SGDynamicTextAssetVersionTests

Construction, `ParseString`, `ToString`, comparison operators, `IsCompatibleWith`, clamping behavior.

### SGDynamicTextAssetValidationResultTests

Entry routing by severity, `AddError`/`AddWarning`/`AddInfo`, `HasErrors`/`HasWarnings`/`HasInfos`, `IsEmpty`, `IsValid`, `GetTotalCount`, `Append`, `Reset`, `ToFormattedString`.

### SGDynamicTextAssetRefTests

Default state, construction with GUID, construction with GUID and UserFacingId, `SetGuid`, `Reset`, equality operators, cached UserFacingId persistence, `GetTypeHash` consistency.

### SGDynamicTextAssetJsonSerializerTests

`ValidateJsonStructure`, `ExtractClassName`, `ExtractGuid`, `ExtractVersion`, serialization/deserialization roundtrip.

### SGDynamicTextAssetBinarySerializerTests

Header reading, `JsonToBinary`/`BinaryToJson` roundtrip, compression method roundtrips (Zlib, Gzip, LZ4, None), content hash verification, `ExtractGuid`, `IsValidBinaryData`.

### SGDynamicTextAssetXmlSerializerTests

XML serialization/deserialization roundtrip, schema validation, element and attribute mapping, nested property handling, XML declaration and encoding verification.

### SGDynamicTextAssetYamlSerializerTests

YAML serialization/deserialization roundtrip, fkYAML integration, scalar and sequence mapping, flow vs block style handling, multi-document support.

### SGDynamicTextAssetFileManagerTests

`SanitizeUserFacingId`, `ExtractUserFacingIdFromPath`, `IsDynamicTextAssetFile`, `GetDynamicTextAssetsRootPath`, `BuildFilePath`, `ReadRawFileContents` for both JSON and binary files, cooked path helpers.

### SGDynamicTextAssetCookManifestTests

Content hash written on save, content hash validates on load, tamper detection, missing hash rejection, deterministic hash output, full roundtrip (save/load/query).

### SGDynamicTextAssetStaticsTests

`IsValidDynamicTextAssetRef`, `SetDynamicTextAssetRefByGuid`, `ClearDynamicTextAssetRef`, `MakeDynamicTextAssetRef`, `GuidToString`/`StringToGuid`, `GetDynamicTextAssetRefUserFacingId`, validation result wrappers.

### SGDynamicTextAssetTypeIdTests

`FSGAssetTypeId` construction, validity checks, equality and inequality operators, hashing for use in `TMap`/`TSet`, serialization to and from strings.

### SGDynamicTextAssetTypeManifestTests

Manifest load/save roundtrip, entry addition and removal, type resolution by class name, manifest merging, duplicate entry handling.

### SGDynamicTextAssetTypeRegistryTests

Class registration and unregistration, instantiable vs non-instantiable class filtering, class hierarchy traversal, subclass enumeration, registry reset behavior.

### SGDynamicTextAssetProviderTests

`ISGDynamicTextAssetProvider` interface implementation, lifecycle hooks (initialize, shutdown), asset provision and retrieval, provider priority ordering.

### SGDynamicTextAssetHardReferenceTests

UHT hard reference validation, allowed property types (soft references, GUID-based refs), prohibited property types (hard UObject pointers, direct asset references), compile-time enforcement verification.

### SGDynamicTextAssetSerializerRegistryTests

Multi-format serializer registration, extension-based routing (`.dta.json`, `.dta.bin`, `.dta.xml`, `.dta.yaml`), serializer lookup by format, duplicate registration handling, unregistration.

### SGDynamicTextAssetServerNullInterfaceTests

Null server interface no-op behavior, all interface methods return expected defaults, delegate firing on simulated operations, safe usage when no server is configured.

## Conventions

- **Test class naming**: `FClassName_FunctionName_ExpectedBehavior`
- **Test path**: `SGDynamicTextAssets.Module.Class.Function`
- **Guard**: All tests wrapped in `#if WITH_AUTOMATION_WORKER` / `#endif`
- **Test flags**: `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`

[Back to Table of Contents](../TableOfContents.md)
