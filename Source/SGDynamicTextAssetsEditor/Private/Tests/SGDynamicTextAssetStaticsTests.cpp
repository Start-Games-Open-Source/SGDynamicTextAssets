// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Core/SGDynamicTextAssetValidationResult.h"
#include "Statics/SGDynamicTextAssetStatics.h"

/**
 * Test: IsValidDynamicTextAssetRef returns true for a ref with a valid ID.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_IsValidDynamicTextAssetRef_ValidRef,
	"SGDynamicTextAssets.Runtime.Statics.IsValidDynamicTextAssetRef.ValidRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_IsValidDynamicTextAssetRef_ValidRef::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef ref(FSGDynamicTextAssetId::NewGeneratedId());

	TestTrue(TEXT("IsValidDynamicTextAssetRef should return true for a ref with a valid ID"), USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(ref));

	return true;
}

/**
 * Test: IsValidDynamicTextAssetRef returns false for a default (null) ref.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_IsValidDynamicTextAssetRef_NullRef,
	"SGDynamicTextAssets.Runtime.Statics.IsValidDynamicTextAssetRef.NullRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_IsValidDynamicTextAssetRef_NullRef::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef ref;

	TestFalse(TEXT("IsValidDynamicTextAssetRef should return false for a default ref"), USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(ref));

	return true;
}

/**
 * Test: SetDynamicTextAssetRefById sets the ID, retrievable via GetDynamicTextAssetRefId.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_SetDynamicTextAssetRefById_SetsId,
	"SGDynamicTextAssets.Runtime.Statics.SetDynamicTextAssetRefById.SetsId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_SetDynamicTextAssetRefById_SetsId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef ref;
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();

	USGDynamicTextAssetStatics::SetDynamicTextAssetRefById(ref, testId);

	FSGDynamicTextAssetId retrievedId = USGDynamicTextAssetStatics::GetDynamicTextAssetRefId(ref);
	TestEqual(TEXT("GetDynamicTextAssetRefId should return the set ID"), retrievedId, testId);
	TestTrue(TEXT("Ref should now be valid"), USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(ref));

	return true;
}

/**
 * Test: SetDynamicTextAssetRefById clears the cached ID when a new ID is set.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_SetDynamicTextAssetRefById_ClearsCachedId,
	"SGDynamicTextAssets.Runtime.Statics.SetDynamicTextAssetRefById.ClearsCachedId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_SetDynamicTextAssetRefById_ClearsCachedId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id1 = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetId id2 = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetRef ref(id1);

	// Verify cached ID is set first
	TestEqual(TEXT("Initial cached ID should be set"), ref.GetId(), id1);

	// Should override/clear the previous ID
	ref.SetId(id2);

	TestFalse(TEXT("Ref should not equal previous ID"), ref == id1);
	TestEqual(TEXT("ID should be the new one"), ref.GetId(), id2);

	return true;
}

/**
 * Test: ClearDynamicTextAssetRef resets the ref to invalid state.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_ClearDynamicTextAssetRef_ResetsRef,
	"SGDynamicTextAssets.Runtime.Statics.ClearDynamicTextAssetRef.ResetsRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_ClearDynamicTextAssetRef_ResetsRef::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef ref(FSGDynamicTextAssetId::NewGeneratedId());

	// Verify it's valid first
	TestTrue(TEXT("Ref should be valid before clear"), USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(ref));

	USGDynamicTextAssetStatics::ClearDynamicTextAssetRef(ref);

	TestFalse(TEXT("IsValidDynamicTextAssetRef should return false after clear"), USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(ref));
	TestFalse(TEXT("ID should be invalid after clear"), USGDynamicTextAssetStatics::GetDynamicTextAssetRefId(ref).IsValid());
	TestTrue(TEXT("Cached ID should be empty after clear"), USGDynamicTextAssetStatics::GetDynamicTextAssetRefUserFacingId(ref).IsEmpty());

	return true;
}

/**
 * Test: MakeDynamicTextAssetRef constructs a ref with the given ID.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_MakeDynamicTextAssetRef_ConstructsRefWithId,
	"SGDynamicTextAssets.Runtime.Statics.MakeDynamicTextAssetRef.ConstructsRefWithId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_MakeDynamicTextAssetRef_ConstructsRefWithId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();

	FSGDynamicTextAssetRef ref = USGDynamicTextAssetStatics::MakeDynamicTextAssetRef(testId);

	TestTrue(TEXT("Made ref should be valid"), USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(ref));
	TestEqual(TEXT("Made ref ID should match input"), USGDynamicTextAssetStatics::GetDynamicTextAssetRefId(ref), testId);

	return true;
}

/**
 * Test: MakeDynamicTextAssetRef with invalid ID creates an invalid ref.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_MakeDynamicTextAssetRef_InvalidId,
	"SGDynamicTextAssets.Runtime.Statics.MakeDynamicTextAssetRef.InvalidId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_MakeDynamicTextAssetRef_InvalidId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId invalidId;

	FSGDynamicTextAssetRef ref = USGDynamicTextAssetStatics::MakeDynamicTextAssetRef(invalidId);

	TestFalse(TEXT("Made ref with invalid ID should not be valid"), USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(ref));

	return true;
}

/**
 * Test: IdToString / StringToId roundtrip produces the original ID.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_IdStringRoundtrip_PreservesId,
	"SGDynamicTextAssets.Runtime.Statics.IdStringRoundtrip.PreservesId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_IdStringRoundtrip_PreservesId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId originalId = FSGDynamicTextAssetId::NewGeneratedId();

	FString idString = USGDynamicTextAssetStatics::IdToString(originalId);
	TestFalse(TEXT("IdToString should return a non-empty string"), idString.IsEmpty());

	FSGDynamicTextAssetId parsedId = FSGDynamicTextAssetId::NewGeneratedId();
	bool bParsed = USGDynamicTextAssetStatics::StringToId(idString, parsedId);

	TestTrue(TEXT("StringToId should parse the string successfully"), bParsed);
	TestEqual(TEXT("Parsed ID should match the original"), parsedId, originalId);

	return true;
}

/**
 * Test: StringToId parses a valid ID string correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_StringToId_ValidString,
	"SGDynamicTextAssets.Runtime.Statics.StringToId.ValidString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_StringToId_ValidString::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId outId;
	bool bParsed = USGDynamicTextAssetStatics::StringToId(TEXT("A1B2C3D4-E5F67890-ABCDEF12-34567890"), outId);

	TestTrue(TEXT("Should parse valid ID string"), bParsed);
	TestTrue(TEXT("Parsed ID should be valid"), outId.IsValid());

	return true;
}

/**
 * Test: StringToId fails for an invalid string.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_StringToId_InvalidString,
	"SGDynamicTextAssets.Runtime.Statics.StringToId.InvalidString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_StringToId_InvalidString::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId outId;

	bool bParsedEmpty = USGDynamicTextAssetStatics::StringToId(TEXT(""), outId);
	TestFalse(TEXT("Empty string should fail to parse"), bParsedEmpty);

	bool bParsedGarbage = USGDynamicTextAssetStatics::StringToId(TEXT("not-a-valid-id"), outId);
	TestFalse(TEXT("Garbage string should fail to parse"), bParsedGarbage);

	return true;
}

/**
 * Test: GetDynamicTextAssetRefUserFacingId returns empty for a default ref.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_GetDynamicTextAssetRefUserFacingId_EmptyForDefault,
	"SGDynamicTextAssets.Runtime.Statics.GetDynamicTextAssetRefUserFacingId.EmptyForDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_GetDynamicTextAssetRefUserFacingId_EmptyForDefault::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef ref;

	FString cachedId = USGDynamicTextAssetStatics::GetDynamicTextAssetRefUserFacingId(ref);

	TestTrue(TEXT("Default ref should have empty cached user-facing ID"), cachedId.IsEmpty());

	return true;
}

/**
 * Test: Validation wrappers return correct values for an empty result.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_ValidationWrappers_EmptyResult,
	"SGDynamicTextAssets.Runtime.Statics.ValidationWrappers.EmptyResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_ValidationWrappers_EmptyResult::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;

	TestFalse(TEXT("Empty result should not have errors"), USGDynamicTextAssetStatics::ValidationResultHasErrors(result));
	TestFalse(TEXT("Empty result should not have warnings"), USGDynamicTextAssetStatics::ValidationResultHasWarnings(result));
	TestFalse(TEXT("Empty result should not have infos"), USGDynamicTextAssetStatics::ValidationResultHasInfos(result));
	TestTrue(TEXT("Empty result should be empty"), USGDynamicTextAssetStatics::IsValidationResultEmpty(result));
	TestTrue(TEXT("Empty result should be valid"), USGDynamicTextAssetStatics::IsValidationResultValid(result));
	TestEqual(TEXT("Empty result total count should be 0"), USGDynamicTextAssetStatics::GetValidationResultTotalCount(result), 0);

	return true;
}

/**
 * Test: Validation wrappers return correct values for a result with errors.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_ValidationWrappers_WithErrors,
	"SGDynamicTextAssets.Runtime.Statics.ValidationWrappers.WithErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_ValidationWrappers_WithErrors::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Test error")), TEXT("TestProperty"));

	TestTrue(TEXT("Should have errors"), USGDynamicTextAssetStatics::ValidationResultHasErrors(result));
	TestFalse(TEXT("Should not have warnings"), USGDynamicTextAssetStatics::ValidationResultHasWarnings(result));
	TestFalse(TEXT("Should not have infos"), USGDynamicTextAssetStatics::ValidationResultHasInfos(result));
	TestFalse(TEXT("Should not be empty"), USGDynamicTextAssetStatics::IsValidationResultEmpty(result));
	TestFalse(TEXT("Should not be valid (has errors)"), USGDynamicTextAssetStatics::IsValidationResultValid(result));
	TestEqual(TEXT("Total count should be 1"), USGDynamicTextAssetStatics::GetValidationResultTotalCount(result), 1);

	return true;
}

/**
 * Test: Validation wrappers return correct values for a result with only warnings.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_ValidationWrappers_WithWarningsOnly,
	"SGDynamicTextAssets.Runtime.Statics.ValidationWrappers.WithWarningsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_ValidationWrappers_WithWarningsOnly::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddWarning(FText::FromString(TEXT("Test warning")));

	TestFalse(TEXT("Should not have errors"), USGDynamicTextAssetStatics::ValidationResultHasErrors(result));
	TestTrue(TEXT("Should have warnings"), USGDynamicTextAssetStatics::ValidationResultHasWarnings(result));
	TestTrue(TEXT("Should be valid (warnings don't invalidate)"), USGDynamicTextAssetStatics::IsValidationResultValid(result));
	TestFalse(TEXT("Should not be empty"), USGDynamicTextAssetStatics::IsValidationResultEmpty(result));

	return true;
}

/**
 * Test: Validation wrappers with mixed entries.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_ValidationWrappers_MixedEntries,
	"SGDynamicTextAssets.Runtime.Statics.ValidationWrappers.MixedEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_ValidationWrappers_MixedEntries::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Error 1")), TEXT("PropA"));
	result.AddError(FText::FromString(TEXT("Error 2")), TEXT("PropB"));
	result.AddWarning(FText::FromString(TEXT("Warning 1")));
	result.AddInfo(FText::FromString(TEXT("Info 1")));

	TestTrue(TEXT("Should have errors"), USGDynamicTextAssetStatics::ValidationResultHasErrors(result));
	TestTrue(TEXT("Should have warnings"), USGDynamicTextAssetStatics::ValidationResultHasWarnings(result));
	TestTrue(TEXT("Should have infos"), USGDynamicTextAssetStatics::ValidationResultHasInfos(result));
	TestFalse(TEXT("Should not be valid (has errors)"), USGDynamicTextAssetStatics::IsValidationResultValid(result));
	TestEqual(TEXT("Total count should be 4"), USGDynamicTextAssetStatics::GetValidationResultTotalCount(result), 4);

	return true;
}

/**
 * Test: GetValidationResultErrors returns the error entries.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_GetValidationResultErrors_ReturnsErrors,
	"SGDynamicTextAssets.Runtime.Statics.GetValidationResultErrors.ReturnsErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_GetValidationResultErrors_ReturnsErrors::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Error A")), TEXT("Health"));
	result.AddError(FText::FromString(TEXT("Error B")), TEXT("Damage"));
	result.AddWarning(FText::FromString(TEXT("Warning")));

	TArray<FSGDynamicTextAssetValidationEntry> errors;
	USGDynamicTextAssetStatics::GetValidationResultErrors(result, errors);

	TestEqual(TEXT("Should return 2 error entries"), errors.Num(), 2);
	if (errors.Num() >= 2)
	{
		TestEqual(TEXT("First error property path"), errors[0].PropertyPath, TEXT("Health"));
		TestEqual(TEXT("Second error property path"), errors[1].PropertyPath, TEXT("Damage"));
	}

	return true;
}

/**
 * Test: GetValidationResultWarnings returns the warning entries.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_GetValidationResultWarnings_ReturnsWarnings,
	"SGDynamicTextAssets.Runtime.Statics.GetValidationResultWarnings.ReturnsWarnings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_GetValidationResultWarnings_ReturnsWarnings::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddWarning(FText::FromString(TEXT("Warning 1")), TEXT("Range"));
	result.AddError(FText::FromString(TEXT("Error")));

	TArray<FSGDynamicTextAssetValidationEntry> warnings;
	USGDynamicTextAssetStatics::GetValidationResultWarnings(result, warnings);

	TestEqual(TEXT("Should return 1 warning entry"), warnings.Num(), 1);
	if (warnings.Num() >= 1)
	{
		TestEqual(TEXT("Warning property path"), warnings[0].PropertyPath, TEXT("Range"));
	}

	return true;
}

/**
 * Test: GetValidationResultInfos returns the informational entries.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_GetValidationResultInfos_ReturnsInfos,
	"SGDynamicTextAssets.Runtime.Statics.GetValidationResultInfos.ReturnsInfos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_GetValidationResultInfos_ReturnsInfos::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddInfo(FText::FromString(TEXT("Info note")), TEXT("Description"));
	result.AddInfo(FText::FromString(TEXT("Another note")));

	TArray<FSGDynamicTextAssetValidationEntry> infos;
	USGDynamicTextAssetStatics::GetValidationResultInfos(result, infos);

	TestEqual(TEXT("Should return 2 info entries"), infos.Num(), 2);

	return true;
}

/**
 * Test: ValidationResultToString returns empty for an empty result.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_ValidationResultToString_EmptyResult,
	"SGDynamicTextAssets.Runtime.Statics.ValidationResultToString.EmptyResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_ValidationResultToString_EmptyResult::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;

	FString formatted = USGDynamicTextAssetStatics::ValidationResultToString(result);

	TestTrue(TEXT("Formatted string should be empty for empty result"), formatted.IsEmpty());

	return true;
}

/**
 * Test: ValidationResultToString returns non-empty for a result with entries.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUSGDynamicTextAssetStatics_ValidationResultToString_WithEntries,
	"SGDynamicTextAssets.Runtime.Statics.ValidationResultToString.WithEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUSGDynamicTextAssetStatics_ValidationResultToString_WithEntries::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Test error message")), TEXT("TestProp"));
	result.AddWarning(FText::FromString(TEXT("Test warning")));

	FString formatted = USGDynamicTextAssetStatics::ValidationResultToString(result);

	TestFalse(TEXT("Formatted string should not be empty"), formatted.IsEmpty());
	TestTrue(TEXT("Formatted string should contain error text"), formatted.Contains(TEXT("Test error message")));
	TestTrue(TEXT("Formatted string should contain warning text"), formatted.Contains(TEXT("Test warning")));

	return true;
}


