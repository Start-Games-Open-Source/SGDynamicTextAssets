// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/SGDynamicTextAssetValidationResult.h"

/**
 * Test: Default-constructed result is empty and valid
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_EmptyState_IsEmptyAndValid,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.EmptyState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_EmptyState_IsEmptyAndValid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;

	TestTrue(TEXT("New result should be empty"), result.IsEmpty());
	TestTrue(TEXT("New result should be valid"), result.IsValid());
	TestFalse(TEXT("New result should not have errors"), result.HasErrors());
	TestFalse(TEXT("New result should not have warnings"), result.HasWarnings());
	TestFalse(TEXT("New result should not have infos"), result.HasInfos());
	TestEqual(TEXT("New result total count should be 0"), result.GetTotalCount(), 0);

	return true;
}

/**
 * Test: AddError marks result as invalid and non-empty
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_AddError_MarksInvalid,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.AddError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_AddError_MarksInvalid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Test error")));

	TestTrue(TEXT("Should have errors"), result.HasErrors());
	TestFalse(TEXT("Should not be valid"), result.IsValid());
	TestFalse(TEXT("Should not be empty"), result.IsEmpty());
	TestEqual(TEXT("Total count should be 1"), result.GetTotalCount(), 1);
	TestEqual(TEXT("Errors array should have 1 entry"), result.Errors.Num(), 1);
	TestEqual(TEXT("Error message should match"), result.Errors[0].Message.ToString(), TEXT("Test error"));
	TestTrue(TEXT("Entry should report as error"), result.Errors[0].IsError());

	return true;
}

/**
 * Test: AddError with property path and suggested fix stores all fields
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_AddError_WithAllFields,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.AddErrorAllFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_AddError_WithAllFields::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(
		FText::FromString(TEXT("Damage is negative")),
		TEXT("Damage"),
		FText::FromString(TEXT("Set Damage to a value >= 0")));

	TestEqual(TEXT("Property path should match"), result.Errors[0].PropertyPath, TEXT("Damage"));
	TestEqual(TEXT("Suggested fix should match"), result.Errors[0].SuggestedFix.ToString(), TEXT("Set Damage to a value >= 0"));

	return true;
}

/**
 * Test: AddWarning does not invalidate the result
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_AddWarning_StaysValid,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.AddWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_AddWarning_StaysValid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddWarning(FText::FromString(TEXT("Test warning")));

	TestTrue(TEXT("Should have warnings"), result.HasWarnings());
	TestTrue(TEXT("Should still be valid (warnings don't invalidate)"), result.IsValid());
	TestFalse(TEXT("Should not be empty"), result.IsEmpty());
	TestFalse(TEXT("Should not have errors"), result.HasErrors());
	TestEqual(TEXT("Total count should be 1"), result.GetTotalCount(), 1);
	TestEqual(TEXT("Warnings array should have 1 entry"), result.Warnings.Num(), 1);
	TestTrue(TEXT("Entry should report as warning"), result.Warnings[0].IsWarning());

	return true;
}

/**
 * Test: AddInfo does not invalidate the result
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_AddInfo_StaysValid,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.AddInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_AddInfo_StaysValid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddInfo(FText::FromString(TEXT("Test info")));

	TestTrue(TEXT("Should have infos"), result.HasInfos());
	TestTrue(TEXT("Should still be valid"), result.IsValid());
	TestFalse(TEXT("Should not be empty"), result.IsEmpty());
	TestFalse(TEXT("Should not have errors"), result.HasErrors());
	TestFalse(TEXT("Should not have warnings"), result.HasWarnings());
	TestEqual(TEXT("Total count should be 1"), result.GetTotalCount(), 1);
	TestEqual(TEXT("Infos array should have 1 entry"), result.Infos.Num(), 1);
	TestTrue(TEXT("Entry should report as info"), result.Infos[0].IsInfo());

	return true;
}

/**
 * Test: Mixed entries track all severities correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_MixedEntries_TracksAllSeverities,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.MixedEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_MixedEntries_TracksAllSeverities::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Error message")));
	result.AddWarning(FText::FromString(TEXT("Warning message")));
	result.AddInfo(FText::FromString(TEXT("Info message")));

	TestEqual(TEXT("Total count should be 3"), result.GetTotalCount(), 3);
	TestTrue(TEXT("Should have errors"), result.HasErrors());
	TestTrue(TEXT("Should have warnings"), result.HasWarnings());
	TestTrue(TEXT("Should have infos"), result.HasInfos());
	TestFalse(TEXT("Should not be valid (has error)"), result.IsValid());
	TestFalse(TEXT("Should not be empty"), result.IsEmpty());

	TestEqual(TEXT("Errors array count"), result.Errors.Num(), 1);
	TestEqual(TEXT("Warnings array count"), result.Warnings.Num(), 1);
	TestEqual(TEXT("Infos array count"), result.Infos.Num(), 1);

	return true;
}

/**
 * Test: AddEntry routes by severity
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_AddEntry_RoutesBySeverity,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.AddEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_AddEntry_RoutesBySeverity::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;

	FSGDynamicTextAssetValidationEntry errorEntry(ESGValidationSeverity::Error, FText::FromString(TEXT("Error via AddEntry")));
	FSGDynamicTextAssetValidationEntry warningEntry(ESGValidationSeverity::Warning, FText::FromString(TEXT("Warning via AddEntry")));
	FSGDynamicTextAssetValidationEntry infoEntry(ESGValidationSeverity::Info, FText::FromString(TEXT("Info via AddEntry")));

	result.AddEntry(errorEntry);
	result.AddEntry(warningEntry);
	result.AddEntry(infoEntry);

	TestEqual(TEXT("Errors array should have 1"), result.Errors.Num(), 1);
	TestEqual(TEXT("Warnings array should have 1"), result.Warnings.Num(), 1);
	TestEqual(TEXT("Infos array should have 1"), result.Infos.Num(), 1);
	TestEqual(TEXT("Total count should be 3"), result.GetTotalCount(), 3);

	return true;
}

/**
 * Test: Append merges entries from another result
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_Append_MergesResults,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.Append",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_Append_MergesResults::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult resultA;
	resultA.AddError(FText::FromString(TEXT("Error A")));
	resultA.AddWarning(FText::FromString(TEXT("Warning A")));

	FSGDynamicTextAssetValidationResult resultB;
	resultB.AddError(FText::FromString(TEXT("Error B")));
	resultB.AddInfo(FText::FromString(TEXT("Info B")));

	resultA.Append(resultB);

	TestEqual(TEXT("Combined errors count"), resultA.Errors.Num(), 2);
	TestEqual(TEXT("Combined warnings count"), resultA.Warnings.Num(), 1);
	TestEqual(TEXT("Combined infos count"), resultA.Infos.Num(), 1);
	TestEqual(TEXT("Combined total count"), resultA.GetTotalCount(), 4);

	// Verify original entries preserved
	TestEqual(TEXT("First error from A"), resultA.Errors[0].Message.ToString(), TEXT("Error A"));
	TestEqual(TEXT("Second error from B"), resultA.Errors[1].Message.ToString(), TEXT("Error B"));

	return true;
}

/**
 * Test: Append with move semantics transfers entries
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_AppendMove_TransfersEntries,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.AppendMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_AppendMove_TransfersEntries::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult resultA;
	resultA.AddWarning(FText::FromString(TEXT("Warning A")));

	FSGDynamicTextAssetValidationResult resultB;
	resultB.AddError(FText::FromString(TEXT("Error B")));
	resultB.AddInfo(FText::FromString(TEXT("Info B")));

	resultA.Append(MoveTemp(resultB));

	TestEqual(TEXT("Target errors count"), resultA.Errors.Num(), 1);
	TestEqual(TEXT("Target warnings count"), resultA.Warnings.Num(), 1);
	TestEqual(TEXT("Target infos count"), resultA.Infos.Num(), 1);
	TestEqual(TEXT("Target total count"), resultA.GetTotalCount(), 3);

	return true;
}

/**
 * Test: Reset clears all entries and returns to empty state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_Reset_ClearsAllEntries,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_Reset_ClearsAllEntries::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Error")));
	result.AddWarning(FText::FromString(TEXT("Warning")));
	result.AddInfo(FText::FromString(TEXT("Info")));

	// Verify non-empty before reset
	TestEqual(TEXT("Pre-reset total count"), result.GetTotalCount(), 3);

	result.Reset();

	TestTrue(TEXT("After reset should be empty"), result.IsEmpty());
	TestTrue(TEXT("After reset should be valid"), result.IsValid());
	TestFalse(TEXT("After reset should not have errors"), result.HasErrors());
	TestFalse(TEXT("After reset should not have warnings"), result.HasWarnings());
	TestFalse(TEXT("After reset should not have infos"), result.HasInfos());
	TestEqual(TEXT("After reset total count should be 0"), result.GetTotalCount(), 0);

	return true;
}

/**
 * Test: ToFormattedString produces non-empty output with entries
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_ToFormattedString_ProducesOutput,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.ToFormattedString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_ToFormattedString_ProducesOutput::RunTest(const FString& Parameters)
{
	// Empty result should produce empty string
	FSGDynamicTextAssetValidationResult emptyResult;
	TestTrue(TEXT("Empty result formatted string should be empty"), emptyResult.ToFormattedString().IsEmpty());

	// Result with entries should produce non-empty string with severity prefixes
	FSGDynamicTextAssetValidationResult result;
	result.AddError(FText::FromString(TEXT("Something failed")), TEXT("Health"));
	result.AddWarning(FText::FromString(TEXT("Something suspicious")));
	result.AddInfo(FText::FromString(TEXT("FYI")));

	FString formatted = result.ToFormattedString();
	TestFalse(TEXT("Formatted string should not be empty"), formatted.IsEmpty());
	TestTrue(TEXT("Should contain ERROR prefix"), formatted.Contains(TEXT("[ERROR]")));
	TestTrue(TEXT("Should contain WARNING prefix"), formatted.Contains(TEXT("[WARNING]")));
	TestTrue(TEXT("Should contain INFO prefix"), formatted.Contains(TEXT("[INFO]")));
	TestTrue(TEXT("Should contain error message"), formatted.Contains(TEXT("Something failed")));
	TestTrue(TEXT("Should contain property path"), formatted.Contains(TEXT("Health")));

	return true;
}

/**
 * Test: ToFormattedString includes suggested fix when present
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetValidationResult_ToFormattedString_IncludesSuggestedFix,
	"SGDynamicTextAssets.Runtime.Core.ValidationResult.ToFormattedStringWithFix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetValidationResult_ToFormattedString_IncludesSuggestedFix::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetValidationResult result;
	result.AddError(
		FText::FromString(TEXT("Value out of range")),
		TEXT("Damage"),
		FText::FromString(TEXT("Set value between 0 and 100")));

	FString formatted = result.ToFormattedString();
	TestTrue(TEXT("Should contain Fix prefix"), formatted.Contains(TEXT("Fix:")));
	TestTrue(TEXT("Should contain suggested fix text"), formatted.Contains(TEXT("Set value between 0 and 100")));

	return true;
}
