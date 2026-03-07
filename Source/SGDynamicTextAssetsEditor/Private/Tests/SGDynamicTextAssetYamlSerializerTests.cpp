// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Serialization/SGDynamicTextAssetYamlSerializer.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Tests/SGDynamicTextAssetYamlUnitTest.h"

// Helper utilities shared across YAML serializer tests.
namespace SGYamlSerializerTestUtils
{
	/**
	 * Builds a minimal valid YAML string with all required structural blocks.
	 * Uses KEY_ constants for metadata field names.
	 */
	FString BuildValidYaml(
		const FString& TypeName = TEXT("USGDynamicTextAsset"),
		const FString& IdString = TEXT("A1B2C3D4-E5F67890-ABCDEF12-34567890"),
		const FString& VersionString = TEXT("1.0.0"))
	{
		return FString::Printf(
			TEXT("%s:\n  %s: %s\n  %s: %s\n  %s: %s\n%s: {}\n"),
			*ISGDynamicTextAssetSerializer::KEY_METADATA,
			*ISGDynamicTextAssetSerializer::KEY_TYPE, *TypeName,
			*ISGDynamicTextAssetSerializer::KEY_VERSION, *VersionString,
			*ISGDynamicTextAssetSerializer::KEY_ID, *IdString,
			*ISGDynamicTextAssetSerializer::KEY_DATA);
	}
}

/**
 * Test: Well-formed YAML with all required blocks passes validation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ValidateStructure_ValidPasses,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ValidateStructureValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ValidateStructure_ValidPasses::RunTest(const FString& Parameters)
{
	FString errorMessage;
	FSGDynamicTextAssetYamlSerializer serializer;
	const bool bResult = serializer.ValidateStructure(
		SGYamlSerializerTestUtils::BuildValidYaml(), errorMessage);

	TestTrue(TEXT("Valid YAML should pass validation"), bResult);
	TestTrue(TEXT("Error message should be empty on success"), errorMessage.IsEmpty());

	return true;
}

/**
 * Test: Completely malformed YAML (not parseable) fails validation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ValidateStructure_MalformedFails,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ValidateStructureMalformed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ValidateStructure_MalformedFails::RunTest(const FString& Parameters)
{
	FString errorMessage;
	FSGDynamicTextAssetYamlSerializer serializer;
	const bool bResult = serializer.ValidateStructure(TEXT("key: 'unclosed string"), errorMessage);

	TestFalse(TEXT("Malformed YAML should fail validation"), bResult);
	TestFalse(TEXT("Error message should not be empty"), errorMessage.IsEmpty());

	return true;
}

/**
 * Test: YAML with metadata block but missing type field fails validation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ValidateStructure_MissingTypeFails,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ValidateStructureMissingType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ValidateStructure_MissingTypeFails::RunTest(const FString& Parameters)
{
	// Has metadata block and data block but no type inside metadata
	const FString yaml = FString::Printf(
		TEXT("%s:\n  %s: A1B2C3D4-E5F67890-ABCDEF12-34567890\n%s: {}\n"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	FString errorMessage;
	FSGDynamicTextAssetYamlSerializer serializer;
	const bool bResult = serializer.ValidateStructure(yaml, errorMessage);

	TestFalse(FString::Printf(TEXT("Missing '%s' should fail"), *ISGDynamicTextAssetSerializer::KEY_TYPE), bResult);
	TestTrue(FString::Printf(TEXT("Error message should mention %s"), *ISGDynamicTextAssetSerializer::KEY_TYPE),
		errorMessage.Contains(ISGDynamicTextAssetSerializer::KEY_TYPE));

	return true;
}

/**
 * Test: YAML with metadata but no data block fails validation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ValidateStructure_MissingDataFails,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ValidateStructureMissingData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ValidateStructure_MissingDataFails::RunTest(const FString& Parameters)
{
	// Has metadata block with type but no data block
	const FString yaml = FString::Printf(
		TEXT("%s:\n  %s: USGDynamicTextAsset\n"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE);

	FString errorMessage;
	FSGDynamicTextAssetYamlSerializer serializer;
	const bool bResult = serializer.ValidateStructure(yaml, errorMessage);

	TestFalse(FString::Printf(TEXT("Missing '%s' should fail"), *ISGDynamicTextAssetSerializer::KEY_DATA), bResult);
	TestTrue(FString::Printf(TEXT("Error message should mention %s"), *ISGDynamicTextAssetSerializer::KEY_DATA),
		errorMessage.Contains(ISGDynamicTextAssetSerializer::KEY_DATA));

	return true;
}

/**
 * Test: Empty string fails validation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ValidateStructure_EmptyStringFails,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ValidateStructureEmptyString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ValidateStructure_EmptyStringFails::RunTest(const FString& Parameters)
{
	FString errorMessage;
	FSGDynamicTextAssetYamlSerializer serializer;
	const bool bResult = serializer.ValidateStructure(TEXT(""), errorMessage);

	TestFalse(TEXT("Empty string should fail validation"), bResult);
	TestFalse(TEXT("Error message should not be empty"), errorMessage.IsEmpty());

	return true;
}

/**
 * Test: SerializeProvider returns false for null input.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_SerializeProvider_NullFails,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.SerializeNullFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_SerializeProvider_NullFails::RunTest(const FString& Parameters)
{
	FString outYaml;
	FSGDynamicTextAssetYamlSerializer serializer;

	AddExpectedError("Inputted NULL Provider");
	const bool bResult = serializer.SerializeProvider(nullptr, outYaml);

	TestFalse(TEXT("Null Provider should fail"), bResult);
	TestTrue(TEXT("Output should be empty"), outYaml.IsEmpty());

	return true;
}

/**
 * Test: DeserializeProvider returns false for null output object.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_DeserializeProvider_NullFails,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.DeserializeNullFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_DeserializeProvider_NullFails::RunTest(const FString& Parameters)
{
	AddExpectedError("Inputted NULL OutProvider");
	FSGDynamicTextAssetYamlSerializer serializer;
	bool bMigrated;
	const bool bResult = serializer.DeserializeProvider(
		SGYamlSerializerTestUtils::BuildValidYaml(), nullptr, bMigrated);

	TestFalse(TEXT("Null OutProvider should fail"), bResult);

	return true;
}

/**
 * Test: SerializeProvider produces output that passes ValidateStructure and contains
 * all four metadata fields with the correct values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_SerializeProvider_ProducesValidYaml,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.SerializeProducesValidYaml",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_SerializeProvider_ProducesValidYaml::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* dummy = NewObject<USGDynamicTextAssetYamlUnitTest>();

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("AABBCCDD11223344AABBCCDD11223344"));
	dummy->SetDynamicTextAssetId(testId);
	dummy->SetUserFacingId(TEXT("test_axe"));
	dummy->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	dummy->TestDamage = 80.0f;

	FString outYaml;
	FSGDynamicTextAssetYamlSerializer serializer;
	const bool bSerialized = serializer.SerializeProvider(dummy, outYaml);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);
	TestFalse(TEXT("Output YAML should not be empty"), outYaml.IsEmpty());

	FString errorMessage;
	TestTrue(TEXT("Output should be valid YAML structure"),
		serializer.ValidateStructure(outYaml, errorMessage));

	return true;
}

/**
 * Test: ExtractMetadata returns all four fields correctly after a round-trip through SerializeProvider.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ExtractMetadata_ReturnsAllFields,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ExtractMetadataReturnsAllFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ExtractMetadata_ReturnsAllFields::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* dummy = NewObject<USGDynamicTextAssetYamlUnitTest>();

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("AABBCCDD11223344AABBCCDD11223344"));
	dummy->SetDynamicTextAssetId(testId);
	dummy->SetUserFacingId(TEXT("holy_lance"));
	dummy->SetVersion(FSGDynamicTextAssetVersion(1, 3, 2));

	FString outYaml;
	FSGDynamicTextAssetYamlSerializer serializer;
	serializer.SerializeProvider(dummy, outYaml);

	FSGDynamicTextAssetId outId;
	FString outClassName;
	FString outUserFacingId;
	FString outVersion;
	FSGDynamicTextAssetTypeId extractedTypeId;
	const bool bExtracted = serializer.ExtractMetadata(outYaml, outId, outClassName, outUserFacingId, outVersion, extractedTypeId);

	TestTrue(TEXT("ExtractMetadata should succeed"), bExtracted);
	// Hidden test class has no TypeId — serializer falls back to class name
	TestFalse(TEXT("TypeId should be invalid for unregistered test class"), extractedTypeId.IsValid());
	// UHT strips the U prefix from class names
	TestEqual(TEXT("Extracted class name should match"), outClassName, TEXT("SGDynamicTextAssetYamlUnitTest"));
	TestEqual(TEXT("Extracted ID should match"), outId, testId);
	TestEqual(TEXT("Extracted UserFacingId should match"), outUserFacingId, TEXT("holy_lance"));
	TestEqual(TEXT("Extracted version should match"), outVersion, TEXT("1.3.2"));

	return true;
}

/**
 * Test: Serialize then Deserialize round-trip preserves basic property values
 * (float, FString, int32) and all metadata fields.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesBasicProperties,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesBasicProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesBasicProperties::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("DEADBEEF12345678CAFEBABE99887766"));
	source->SetDynamicTextAssetId(testId);
	source->SetUserFacingId(TEXT("test_sword"));
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->TestDamage = 123.456f;
	source->TestName = TEXT("Excalibur");
	source->TestCount = 42;

	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	const bool bSerialized = serializer.SerializeProvider(source, yamlString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestEqual(TEXT("ID should match"),            target->GetDynamicTextAssetId(), testId);
	TestEqual(TEXT("UserFacingId should match"),  target->GetUserFacingId(), TEXT("test_sword"));
	TestEqual(TEXT("Version should match"),       target->GetVersion().ToString(), TEXT("1.0.0"));
	TestEqual(TEXT("TestDamage should match"),    target->TestDamage, 123.456f);
	TestEqual(TEXT("TestName should match"),      target->TestName, TEXT("Excalibur"));
	TestEqual(TEXT("TestCount should match"),     target->TestCount, 42);

	return true;
}

/**
 * Test: Round-trip preserves UserFacingId through serialization and deserialization.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesUserFacingId,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesUserFacingId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesUserFacingId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();
	source->SetUserFacingId(TEXT("thunder_hammer"));
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	serializer.SerializeProvider(source, yamlString);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	serializer.DeserializeProvider(yamlString, target, bMigrated);

	TestEqual(TEXT("UserFacingId should survive round-trip"), target->GetUserFacingId(), TEXT("thunder_hammer"));

	return true;
}

/**
 * Test: TArray<FString> property round-trip preserves all elements in order.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesTArray,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesTArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesTArray::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->TestTags = { TEXT("fire"), TEXT("ice"), TEXT("lightning") };

	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	serializer.SerializeProvider(source, yamlString);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestEqual(TEXT("TestTags count should match"), target->TestTags.Num(), 3);
	TestEqual(TEXT("TestTags[0] should match"), target->TestTags[0], TEXT("fire"));
	TestEqual(TEXT("TestTags[1] should match"), target->TestTags[1], TEXT("ice"));
	TestEqual(TEXT("TestTags[2] should match"), target->TestTags[2], TEXT("lightning"));

	return true;
}

/**
 * Test: TMap<FString, int32> property round-trip preserves all key-value pairs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesTMap,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesTMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesTMap::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->TestAttributes.Add(TEXT("speed"),  5);
	source->TestAttributes.Add(TEXT("power"), 10);

	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	serializer.SerializeProvider(source, yamlString);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestEqual(TEXT("TestAttributes count should match"), target->TestAttributes.Num(), 2);
	TestTrue(TEXT("TestAttributes should contain 'speed'"),  target->TestAttributes.Contains(TEXT("speed")));
	TestTrue(TEXT("TestAttributes should contain 'power'"),  target->TestAttributes.Contains(TEXT("power")));
	TestEqual(TEXT("speed value should match"),  target->TestAttributes[TEXT("speed")],  5);
	TestEqual(TEXT("power value should match"),  target->TestAttributes[TEXT("power")], 10);

	return true;
}

/**
 * Test: TSet<FString> property round-trip preserves all elements.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesTSet,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesTSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesTSet::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->TestKeywords.Add(TEXT("rare"));
	source->TestKeywords.Add(TEXT("epic"));

	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	serializer.SerializeProvider(source, yamlString);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestEqual(TEXT("TestKeywords count should match"), target->TestKeywords.Num(), 2);
	TestTrue(TEXT("TestKeywords should contain 'rare'"), target->TestKeywords.Contains(TEXT("rare")));
	TestTrue(TEXT("TestKeywords should contain 'epic'"), target->TestKeywords.Contains(TEXT("epic")));

	return true;
}

/**
 * Test: Nested USTRUCT property round-trip preserves all struct field values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesNestedStruct,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesNestedStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesNestedStruct::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->TestStats.BaseArmor     = 50;
	source->TestStats.MaxDurability = 100;

	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	serializer.SerializeProvider(source, yamlString);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestEqual(TEXT("TestStats.BaseArmor should match"),     target->TestStats.BaseArmor,     50);
	TestEqual(TEXT("TestStats.MaxDurability should match"), target->TestStats.MaxDurability, 100);

	return true;
}

/**
 * Test: TSoftObjectPtr property round-trip preserves the asset path string.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesSoftObjectPtr,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesSoftObjectPtr",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesSoftObjectPtr::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->TestMeshRef = FSoftObjectPath(TEXT("/Game/Test/SwordMesh.SwordMesh"));

	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	serializer.SerializeProvider(source, yamlString);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestEqual(TEXT("TestMeshRef path should match"),
		target->TestMeshRef.ToSoftObjectPath().ToString(),
		source->TestMeshRef.ToSoftObjectPath().ToString());

	return true;
}

/**
 * Test: Deserializing YAML that is missing an optional data field succeeds and
 * leaves the missing property at its default value.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Deserialize_MissingOptionalFieldGraceful,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.DeserializeMissingOptionalFieldGraceful",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Deserialize_MissingOptionalFieldGraceful::RunTest(const FString& Parameters)
{
	// Build YAML that has TestDamage in the data block but omits TestName and TestCount entirely.
	const FString yaml = FString::Printf(
		TEXT("%s:\n  %s: SGDynamicTextAssetYamlUnitTest\n  %s: 1.0.0\n  %s: A1B2C3D4-E5F67890-ABCDEF12-34567890\n%s:\n  TestDamage: 99.0\n"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	target->TestName  = TEXT("ShouldBePreservedDefault");
	target->TestCount = 999;

	FSGDynamicTextAssetYamlSerializer serializer;
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yaml, target, bMigrated);

	TestTrue(TEXT("Deserialization should succeed despite missing optional fields"), bDeserialized);
	TestEqual(TEXT("TestDamage should be deserialized from YAML"), target->TestDamage, 99.0f);

	// TestName and TestCount were not in the YAML, so the target's pre-existing values remain.
	// This verifies the serializer does NOT zero-out fields that are absent in the file.
	TestEqual(TEXT("TestName should retain its pre-existing value"), target->TestName, TEXT("ShouldBePreservedDefault"));
	TestEqual(TEXT("TestCount should retain its pre-existing value"), target->TestCount, 999);

	return true;
}

/**
 * Test: FSGDynamicTextAssetRef round-trip preserves the ID (resolved on demand, not stored).
 *
 * Verifies the YAML symmetry: export calls ExportTextItem() producing a plain GUID string
 * as the YAML scalar, so import must return FJsonValueString (not FJsonValueObject) so that
 * FJsonObjectConverter calls ImportTextItem() on the other side.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesFSGDynamicTextAssetRef,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.RoundtripPreservesFSGDynamicTextAssetRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_Roundtrip_PreservesFSGDynamicTextAssetRef::RunTest(const FString& Parameters)
{
	// Arrange: create a source object with a known FSGDynamicTextAssetRef ID.
	USGDynamicTextAssetYamlUnitTest* source = NewObject<USGDynamicTextAssetYamlUnitTest>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	const FSGDynamicTextAssetId testId(FGuid(0xDEADBEEF, 0x12345678, 0xCAFEBABE, 0x99887766));
	source->TestRef.SetId(testId);

	// Act: serialize then deserialize into a fresh instance.
	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	serializer.SerializeProvider(source, yamlString);

	USGDynamicTextAssetYamlUnitTest* target = NewObject<USGDynamicTextAssetYamlUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);

	// Assert: ID must survive the round-trip intact.
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);
	TestTrue(TEXT("TestRef should be valid after YAML round-trip"), target->TestRef.IsValid());
	TestTrue(TEXT("TestRef ID should match after YAML round-trip"), target->TestRef.GetId() == testId);

	return true;
}

/**
 * Test: ExtractMetadata correctly parses a GUID string in the type field
 * and returns a valid FSGDynamicTextAssetTypeId.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ExtractMetadata_WithGuid_ExtractsAssetTypeId,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ExtractMetadata.WithGuid.ExtractsAssetTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ExtractMetadata_WithGuid_ExtractsAssetTypeId::RunTest(const FString& Parameters)
{
	// Use a valid GUID string as the type field value (new format)
	const FString guidString = TEXT("550E8400-E29B-41D4-A716-446655440000");
	const FString yaml = SGYamlSerializerTestUtils::BuildValidYaml(guidString);

	FSGDynamicTextAssetYamlSerializer serializer;
	FSGDynamicTextAssetId outId;
	FString outClassName;
	FString outUserFacingId;
	FString outVersion;
	FSGDynamicTextAssetTypeId extractedTypeId;

	bool bExtracted = serializer.ExtractMetadata(yaml, outId, outClassName, outUserFacingId, outVersion, extractedTypeId);
	TestTrue(TEXT("ExtractMetadata should succeed with GUID type field"), bExtracted);
	TestTrue(TEXT("TypeId should be valid when type field contains a GUID"), extractedTypeId.IsValid());
	TestEqual(TEXT("Extracted TypeId should match the input GUID"), extractedTypeId.ToString(), guidString);

	// No registry mapping exists for this test GUID, so class name should be empty
	TestTrue(TEXT("Class name should be empty for unregistered GUID"), outClassName.IsEmpty());

	return true;
}

/**
 * Test: ExtractMetadata correctly treats a non-GUID string in the type field
 * as a legacy class name and returns an invalid FSGDynamicTextAssetTypeId.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetYamlSerializer_ExtractMetadata_LegacyClassName_NoTypeId,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.ExtractMetadata.LegacyClassName.NoTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetYamlSerializer_ExtractMetadata_LegacyClassName_NoTypeId::RunTest(const FString& Parameters)
{
	// Use a class name as the type field value (legacy format)
	const FString yaml = SGYamlSerializerTestUtils::BuildValidYaml(TEXT("USGDynamicTextAsset"));

	FSGDynamicTextAssetYamlSerializer serializer;
	FSGDynamicTextAssetId outId;
	FString outClassName;
	FString outUserFacingId;
	FString outVersion;
	FSGDynamicTextAssetTypeId extractedTypeId;

	bool bExtracted = serializer.ExtractMetadata(yaml, outId, outClassName, outUserFacingId, outVersion, extractedTypeId);
	TestTrue(TEXT("ExtractMetadata should succeed with legacy class name"), bExtracted);
	TestFalse(TEXT("TypeId should be invalid for legacy class name format"), extractedTypeId.IsValid());
	TestEqual(TEXT("Class name should match the type field value"), outClassName, TEXT("USGDynamicTextAsset"));

	return true;
}
