// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Tests/SGDynamicTextAssetUnitTest.h"

// Helper: Builds a valid JSON string with all required metadata fields.
namespace SGJsonSerializerTestUtils
{
	FString BuildValidJson(
		const FString& TypeName = TEXT("USGDynamicTextAsset"),
		const FString& IdString = TEXT("A1B2C3D4-E5F67890-ABCDEF12-34567890"),
		const FString& VersionString = TEXT("1.0.0"))
	{
		return FString::Printf(
			TEXT("{ \"%s\": { \"%s\": \"%s\", \"%s\": \"%s\", \"%s\": \"%s\" }, \"%s\": { \"Damage\": 50 } }"),
			*ISGDynamicTextAssetSerializer::KEY_METADATA,
			*ISGDynamicTextAssetSerializer::KEY_TYPE, *TypeName,
			*ISGDynamicTextAssetSerializer::KEY_ID, *IdString,
			*ISGDynamicTextAssetSerializer::KEY_VERSION, *VersionString,
			*ISGDynamicTextAssetSerializer::KEY_DATA);
	}

	FString BuildDummyJson(
		const FString& IdString = TEXT("A1B2C3D4-E5F67890-ABCDEF12-34567890"),
		const FString& VersionString = TEXT("1.0.0"),
		float Damage = 75.0f,
		const FString& Name = TEXT("TestSword"),
		int32 Count = 42)
	{
		return FString::Printf(
			TEXT("{ \"%s\": { \"%s\": \"SGDynamicTextAssetTestDummy\", \"%s\": \"%s\", \"%s\": \"%s\" }, \"%s\": { \"TestDamage\": %f, \"TestName\": \"%s\", \"TestCount\": %d } }"),
			*ISGDynamicTextAssetSerializer::KEY_METADATA,
			*ISGDynamicTextAssetSerializer::KEY_TYPE,
			*ISGDynamicTextAssetSerializer::KEY_ID, *IdString,
			*ISGDynamicTextAssetSerializer::KEY_VERSION, *VersionString,
			*ISGDynamicTextAssetSerializer::KEY_DATA, Damage, *Name, Count);
	}
}

/**
 * Test: Valid JSON with all required fields passes validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_ValidPasses,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ValidateStructureValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_ValidPasses::RunTest(const FString& Parameters)
{
	FString errorMessage;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bResult = serializer.ValidateStructure(
		SGJsonSerializerTestUtils::BuildValidJson(), errorMessage);

	TestTrue(TEXT("Valid JSON should pass validation"), bResult);
	TestTrue(TEXT("Error message should be empty on success"), errorMessage.IsEmpty());

	return true;
}

/**
 * Test: Missing type field inside metadata fails validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_MissingTypeFails,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ValidateStructureMissingType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_MissingTypeFails::RunTest(const FString& Parameters)
{
	// Has metadata block but no "type" field inside it
	const FString json = FString::Printf(TEXT("{ \"%s\": { \"%s\": \"A1B2C3D4-E5F67890-ABCDEF12-34567890\" }, \"%s\": {} }"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	FString errorMessage;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bResult = serializer.ValidateStructure(json, errorMessage);

	TestFalse(FString::Printf(TEXT("Missing %s should fail"), *ISGDynamicTextAssetSerializer::KEY_TYPE), bResult);
	TestTrue(FString::Printf(TEXT("Error message should mention %s"), *ISGDynamicTextAssetSerializer::KEY_TYPE), errorMessage.Contains(ISGDynamicTextAssetSerializer::KEY_TYPE));

	return true;
}

/**
 * Test: Missing data field fails validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_MissingDataFails,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ValidateStructureMissingData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_MissingDataFails::RunTest(const FString& Parameters)
{
	// Has metadata but no data block at root
	const FString json = FString::Printf(TEXT("{ \"%s\": { \"%s\": \"USGDynamicTextAsset\", \"%s\": \"A1B2C3D4-E5F67890-ABCDEF12-34567890\" } }"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID);

	FString errorMessage;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bResult = serializer.ValidateStructure(json, errorMessage);

	TestFalse(FString::Printf(TEXT("Missing %s should fail"), *ISGDynamicTextAssetSerializer::KEY_DATA), bResult);
	TestTrue(FString::Printf(TEXT("Error message should mention %s"), *ISGDynamicTextAssetSerializer::KEY_DATA), errorMessage.Contains(ISGDynamicTextAssetSerializer::KEY_DATA));

	return true;
}

/**
 * Test: Completely invalid JSON (not parseable) fails validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_UnparseableFails,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ValidateStructureUnparseable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_UnparseableFails::RunTest(const FString& Parameters)
{
	const FString json = TEXT("this is not json at all {{{");

	FString errorMessage;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bResult = serializer.ValidateStructure(json, errorMessage);

	TestFalse(TEXT("Unparseable JSON should fail"), bResult);
	TestFalse(TEXT("Error message should not be empty"), errorMessage.IsEmpty());

	return true;
}

/**
 * Test: Empty string fails validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_EmptyStringFails,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ValidateStructureEmptyString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ValidateJsonStructure_EmptyStringFails::RunTest(const FString& Parameters)
{
	FString errorMessage;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bResult = serializer.ValidateStructure(TEXT(""), errorMessage);

	TestFalse(TEXT("Empty string should fail"), bResult);
	TestFalse(TEXT("Error message should not be empty"), errorMessage.IsEmpty());

	return true;
}



/**
 * Test: SerializeProvider returns false for null input
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_SerializeProvider_NullFails,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.SerializeNullFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_SerializeProvider_NullFails::RunTest(const FString& Parameters)
{
	FString outJson;
	FSGDynamicTextAssetJsonSerializer serializer;

	AddExpectedError("Inputted NULL Provider");
	bool bResult = serializer.SerializeProvider(nullptr, outJson);

	TestFalse(TEXT("Null Provider should fail"), bResult);
	TestTrue(TEXT("Output should be empty"), outJson.IsEmpty());

	return true;
}

/**
 * Test: DeserializeProvider returns false for null output object
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_DeserializeProvider_NullFails,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.DeserializeNullFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_DeserializeProvider_NullFails::RunTest(const FString& Parameters)
{
	AddExpectedError("Inputted NULL OutProvider");
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bMigrated;
	bool bResult = serializer.DeserializeProvider(
		SGJsonSerializerTestUtils::BuildValidJson(), nullptr, bMigrated);

	TestFalse(TEXT("Null OutProvider should fail"), bResult);

	return true;
}

/**
 * Test: DeserializeProvider returns false for empty string
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_DeserializeProvider_EmptyStringFails,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.DeserializeEmptyStringFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_DeserializeProvider_EmptyStringFails::RunTest(const FString& Parameters)
{
	AddExpectedError("Inputted NULL OutProvider"); // Expect to hit this because we pass nullptr
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bMigrated;
	bool bResult = serializer.DeserializeProvider(TEXT(""), nullptr, bMigrated);

	TestFalse(TEXT("Empty string should fail"), bResult);

	return true;
}

/**
 * Test: SerializeProvider produces valid JSON with correct metadata for a concrete subclass
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_SerializeProvider_ProducesValidJson,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.SerializeProducesValidJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_SerializeProvider_ProducesValidJson::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* dummy = NewObject<USGDynamicTextAssetUnitTest>();

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("AABBCCDD11223344AABBCCDD11223344"));
	dummy->SetDynamicTextAssetId(testId);
	dummy->SetUserFacingId(TEXT("test_sword"));
	dummy->SetVersion(FSGDynamicTextAssetVersion(1, 2, 3));
	dummy->TestDamage = 75.0f;
	dummy->TestName = TEXT("Excalibur");
	dummy->TestCount = 42;

	FString outJson;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bSerialized = serializer.SerializeProvider(dummy, outJson);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);
	TestFalse(TEXT("Output JSON should not be empty"), outJson.IsEmpty());

	// Validate the output is parseable JSON
	FString errorMessage;
	TestTrue(TEXT("Output should be valid JSON structure"),
		serializer.ValidateStructure(outJson, errorMessage));

	// Verify metadata extraction from serialized output
	FString extractedClassName, extractedUserFacingId, extractedVersion;
	FSGDynamicTextAssetId extractedId;
	FSGDynamicTextAssetTypeId extractedTypeId;
	serializer.ExtractMetadata(outJson, extractedId, extractedClassName, extractedUserFacingId, extractedVersion, extractedTypeId);

	// Hidden test class has no TypeId — serializer falls back to class name
	TestFalse(TEXT("TypeId should be invalid for unregistered test class"), extractedTypeId.IsValid());
	TestEqual(TEXT("Extracted class name should match"), extractedClassName, TEXT("SGDynamicTextAssetUnitTest"));
	TestEqual(TEXT("Extracted ID should match"), extractedId, testId);
	TestEqual(TEXT("Extracted version should match"), extractedVersion, TEXT("1.2.3"));

	return true;
}

/**
 * Test: Serialize then Deserialize roundtrip preserves all properties
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_Roundtrip_PreservesProperties,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.RoundtripPreservesProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_Roundtrip_PreservesProperties::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("Loading with best-effort"), EAutomationExpectedMessageFlags::Contains);
	// Arrange: create and populate source object
	USGDynamicTextAssetUnitTest* source = NewObject<USGDynamicTextAssetUnitTest>();

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("DEADBEEF12345678CAFEBABE99887766"));
	source->SetDynamicTextAssetId(testId);
	source->SetUserFacingId(TEXT("test_bow"));
	source->SetVersion(FSGDynamicTextAssetVersion(2, 1, 0));
	source->TestDamage = 123.456f;
	source->TestName = TEXT("Longbow");
	source->TestCount = 99;

	FSGDynamicTextAssetJsonSerializer serializer;

	// Act: serialize then deserialize into a new instance
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGDynamicTextAssetUnitTest* target = NewObject<USGDynamicTextAssetUnitTest>();
	bool bMigrated;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert: verify all fields match
	TestEqual(TEXT("ID should match"), target->GetDynamicTextAssetId(), testId);
	TestEqual(TEXT("UserFacingId should match"), target->GetUserFacingId(), TEXT("test_bow"));
	TestEqual(TEXT("Version should match"), target->GetVersion().ToString(), TEXT("2.1.0"));
	TestEqual(TEXT("TestDamage should match"), target->TestDamage, 123.456f);
	TestEqual(TEXT("TestName should match"), target->TestName, TEXT("Longbow"));
	TestEqual(TEXT("TestCount should match"), target->TestCount, 99);

	return true;
}

/**
 * Test: DeserializeProvider correctly populates ID and version from JSON
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_DeserializeProvider_PopulatesMetadata,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.DeserializePopulatesMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_DeserializeProvider_PopulatesMetadata::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("does not match OutProvider"), EAutomationExpectedMessageFlags::Contains);
	AddExpectedMessage(TEXT("Loading with best-effort."), EAutomationExpectedMessageFlags::Contains);

	const FString json = SGJsonSerializerTestUtils::BuildDummyJson(
		TEXT("AABBCCDD-11223344-AABBCCDD-11223344"), TEXT("3.7.1"), 50.0f, TEXT("Dagger"), 10);

	USGDynamicTextAssetUnitTest* target = NewObject<USGDynamicTextAssetUnitTest>();
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bMigrated;
	bool bDeserialized = serializer.DeserializeProvider(json, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	FSGDynamicTextAssetId expectedId;
	expectedId.ParseString(TEXT("AABBCCDD11223344AABBCCDD11223344"));
	TestEqual(TEXT("ID should be set from JSON"), target->GetDynamicTextAssetId(), expectedId);
	TestEqual(TEXT("Version should be set from JSON"), target->GetVersion().ToString(), TEXT("3.7.1"));
	TestEqual(TEXT("TestDamage should be set from JSON"), target->TestDamage, 50.0f);
	TestEqual(TEXT("TestName should be set from JSON"), target->TestName, TEXT("Dagger"));
	TestEqual(TEXT("TestCount should be set from JSON"), target->TestCount, 10);

	return true;
}

/**
 * Test: SerializeProvider includes userfacingid in metadata block of output JSON
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_SerializeProvider_IncludesUserFacingId,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.UserFacingId.SerializeIncludes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_SerializeProvider_IncludesUserFacingId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* dummy = NewObject<USGDynamicTextAssetUnitTest>();

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("AABBCCDD11223344AABBCCDD11223344"));
	dummy->SetDynamicTextAssetId(testId);
	dummy->SetUserFacingId(TEXT("flame_sword"));
	dummy->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	dummy->TestDamage = 50.0f;

	FString outJson;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bSerialized = serializer.SerializeProvider(dummy, outJson);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Verify the userfacingid key is present in the JSON output
	TestTrue(TEXT("JSON should contain userfacingid key"),
		outJson.Contains(ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID));
	TestTrue(TEXT("JSON should contain the UserFacingId value"),
		outJson.Contains(TEXT("flame_sword")));

	return true;
}

/**
 * Test: DeserializeProvider restores UserFacingId from metadata block
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_DeserializeProvider_RestoresUserFacingId,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.UserFacingId.DeserializeRestores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_DeserializeProvider_RestoresUserFacingId::RunTest(const FString& Parameters)
{
	// Build JSON with metadata wrapper including userfacingid
	const FString json = FString::Printf(
		TEXT("{ \"%s\": { \"%s\": \"SGDynamicTextAssetUnitTest\", \"%s\": \"AABBCCDD-11223344-AABBCCDD-11223344\", \"%s\": \"1.0.0\", \"%s\": \"ice_staff\" }, \"%s\": { \"TestDamage\": 30.0 } }"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	USGDynamicTextAssetUnitTest* target = NewObject<USGDynamicTextAssetUnitTest>();
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bMigrated;
	bool bDeserialized = serializer.DeserializeProvider(json, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);
	TestEqual(TEXT("UserFacingId should be restored"), target->GetUserFacingId(), TEXT("ice_staff"));

	return true;
}

/**
 * Test: Serialize then Deserialize roundtrip preserves UserFacingId
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_Roundtrip_PreservesUserFacingId,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.UserFacingId.RoundtripPreserves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_Roundtrip_PreservesUserFacingId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* source = NewObject<USGDynamicTextAssetUnitTest>();

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("DEADBEEF12345678CAFEBABE99887766"));
	source->SetDynamicTextAssetId(testId);
	source->SetUserFacingId(TEXT("thunder_hammer"));
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->TestDamage = 200.0f;

	FString jsonString;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGDynamicTextAssetUnitTest* target = NewObject<USGDynamicTextAssetUnitTest>();
	bool bMigrated;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);
	TestEqual(TEXT("UserFacingId should survive roundtrip"), target->GetUserFacingId(), TEXT("thunder_hammer"));

	return true;
}

/**
 * Test: JSON without userfacingid in metadata deserializes gracefully with empty UserFacingId
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_DeserializeProvider_MissingUserFacingId,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.UserFacingId.MissingGraceful",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_DeserializeProvider_MissingUserFacingId::RunTest(const FString& Parameters)
{
	// Build JSON with metadata wrapper but WITHOUT userfacingid key
	const FString json = FString::Printf(
		TEXT("{ \"%s\": { \"%s\": \"SGDynamicTextAssetUnitTest\", \"%s\": \"AABBCCDD-11223344-AABBCCDD-11223344\", \"%s\": \"1.0.0\" }, \"%s\": { \"TestDamage\": 10.0 } }"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	USGDynamicTextAssetUnitTest* target = NewObject<USGDynamicTextAssetUnitTest>();
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bMigrated;
	bool bDeserialized = serializer.DeserializeProvider(json, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);
	TestTrue(TEXT("UserFacingId should be empty when missing from metadata"), target->GetUserFacingId().IsEmpty());

	return true;
}

/**
 * Test: ExtractMetadata reads userfacingid from metadata block
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ExtractMetadata_ReadsUserFacingId,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.UserFacingId.ExtractMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ExtractMetadata_ReadsUserFacingId::RunTest(const FString& Parameters)
{
	const FString json = FString::Printf(
		TEXT("{ \"%s\": { \"%s\": \"SGDynamicTextAssetUnitTest\", \"%s\": \"AABBCCDD-11223344-AABBCCDD-11223344\", \"%s\": \"1.0.0\", \"%s\": \"holy_shield\" }, \"%s\": {} }"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	FSGDynamicTextAssetJsonSerializer serializer;
	FSGDynamicTextAssetId outId;
	FString outClassName;
	FString outUserFacingId;
	FString outVersion;

	FSGDynamicTextAssetTypeId extractedTypeId;
	bool bExtracted = serializer.ExtractMetadata(json, outId, outClassName, outUserFacingId, outVersion, extractedTypeId);
	TestTrue(TEXT("ExtractMetadata should succeed"), bExtracted);
	TestEqual(TEXT("UserFacingId should be extracted"), outUserFacingId, TEXT("holy_shield"));
	TestFalse(TEXT("TypeId should be invalid for legacy class name format"), extractedTypeId.IsValid());

	return true;
}

/**
 * Test: FSGDynamicTextAssetRef round-trip preserves the ID (resolved on demand, not stored).
 *
 * Confirms the JSON serializer path correctly calls ImportTextItem() via
 * FJsonObjectConverter when it encounters a string-valued JSON property for
 * a struct that has HasImportTextItem() (i.e. FSGDynamicTextAssetRef).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_Roundtrip_PreservesFSGDynamicTextAssetRef,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.RoundtripPreservesFSGDynamicTextAssetRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_Roundtrip_PreservesFSGDynamicTextAssetRef::RunTest(const FString& Parameters)
{
	// Arrange: create a source object with a known FSGDynamicTextAssetRef ID.
	USGDynamicTextAssetUnitTest* source = NewObject<USGDynamicTextAssetUnitTest>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	const FSGDynamicTextAssetId testId(FGuid(0xDEADBEEF, 0x12345678, 0xCAFEBABE, 0x99887766));
	source->TestRef.SetId(testId);

	// Act: serialize then deserialize into a fresh instance.
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	const bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGDynamicTextAssetUnitTest* target = NewObject<USGDynamicTextAssetUnitTest>();
	bool bMigrated;
	const bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);

	// Assert: ID must survive the round-trip intact.
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);
	TestTrue(TEXT("TestRef should be valid after JSON round-trip"), target->TestRef.IsValid());
	TestTrue(TEXT("TestRef ID should match after JSON round-trip"), target->TestRef.GetId() == testId);

	return true;
}

/**
 * Test: ExtractMetadata correctly parses a GUID string in the type field
 * and returns a valid FSGDynamicTextAssetTypeId.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ExtractMetadata_WithGuid_ExtractsAssetTypeId,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ExtractMetadata.WithGuid.ExtractsAssetTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ExtractMetadata_WithGuid_ExtractsAssetTypeId::RunTest(const FString& Parameters)
{
	// Use a valid GUID string as the type field value (new format)
	const FString guidString = TEXT("550E8400-E29B-41D4-A716-446655440000");
	const FString json = SGJsonSerializerTestUtils::BuildValidJson(guidString);

	FSGDynamicTextAssetJsonSerializer serializer;
	FSGDynamicTextAssetId outId;
	FString outClassName;
	FString outUserFacingId;
	FString outVersion;
	FSGDynamicTextAssetTypeId extractedTypeId;

	bool bExtracted = serializer.ExtractMetadata(json, outId, outClassName, outUserFacingId, outVersion, extractedTypeId);
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
	FSGDynamicTextAssetJsonSerializer_ExtractMetadata_LegacyClassName_NoTypeId,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ExtractMetadata.LegacyClassName.NoTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ExtractMetadata_LegacyClassName_NoTypeId::RunTest(const FString& Parameters)
{
	// Use a class name as the type field value (legacy format)
	const FString json = SGJsonSerializerTestUtils::BuildValidJson(TEXT("USGDynamicTextAsset"));

	FSGDynamicTextAssetJsonSerializer serializer;
	FSGDynamicTextAssetId outId;
	FString outClassName;
	FString outUserFacingId;
	FString outVersion;
	FSGDynamicTextAssetTypeId extractedTypeId;

	bool bExtracted = serializer.ExtractMetadata(json, outId, outClassName, outUserFacingId, outVersion, extractedTypeId);
	TestTrue(TEXT("ExtractMetadata should succeed with legacy class name"), bExtracted);
	TestFalse(TEXT("TypeId should be invalid for legacy class name format"), extractedTypeId.IsValid());
	TestEqual(TEXT("Class name should match the type field value"), outClassName, TEXT("USGDynamicTextAsset"));

	return true;
}

/**
 * Test: ValidateStructure accepts a GUID string in the type field.
 * The type field is format-agnostic — it only checks for presence, not whether
 * the value is a class name or GUID.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetJsonSerializer_ValidateStructure_WithGuidTypeField_Passes,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.ValidateStructure.GuidTypeField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetJsonSerializer_ValidateStructure_WithGuidTypeField_Passes::RunTest(const FString& Parameters)
{
	const FString json = SGJsonSerializerTestUtils::BuildValidJson(TEXT("550E8400-E29B-41D4-A716-446655440000"));

	FString errorMessage;
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bResult = serializer.ValidateStructure(json, errorMessage);

	TestTrue(TEXT("JSON with GUID in type field should pass validation"), bResult);
	TestTrue(TEXT("Error message should be empty on success"), errorMessage.IsEmpty());

	return true;
}
