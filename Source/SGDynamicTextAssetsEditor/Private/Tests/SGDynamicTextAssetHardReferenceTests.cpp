// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/SGDynamicTextAssetValidationUtils.h"
#include "Core/SGDynamicTextAsset.h"
#include "Tests/SGDynamicTextAssetUnitTest.h"
#include "Tests/SGDynamicTextAssetHardReferenceTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_CleanObject_NoViolations,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.CleanObject.NoViolations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_CleanObject_NoViolations::RunTest(const FString& Parameters)
{
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGDynamicTextAssetUnitTest::StaticClass(), violations);

	TestFalse(TEXT("Clean dynamic text asset (primitives only) should have no violations"), bHasViolations);
	TestEqual(TEXT("Violations array should be empty"), violations.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_DirtyObject_DetectsTObjectPtr,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.DirtyObject.DetectsTObjectPtr",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_DirtyObject_DetectsTObjectPtr::RunTest(const FString& Parameters)
{
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGTestDirtyDynamicTextAsset::StaticClass(), violations);

	TestTrue(TEXT("Dynamic text asset with TObjectPtr should have violations"), bHasViolations);
	TestTrue(TEXT("Should have at least one violation"), violations.Num() >= 1);

	// Verify the violation message mentions the property name
	bool bFoundHardObjectRef = false;
	for (const FString& violation : violations)
	{
		if (violation.Contains(TEXT("HardObjectRef")))
		{
			bFoundHardObjectRef = true;
			break;
		}
	}
	TestTrue(TEXT("Violation should mention HardObjectRef property"), bFoundHardObjectRef);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_SubclassOf_DetectsTSubclassOf,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.SubclassOf.DetectsTSubclassOf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_SubclassOf_DetectsTSubclassOf::RunTest(const FString& Parameters)
{
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGTestSubclassOfDynamicTextAsset::StaticClass(), violations);

	TestTrue(TEXT("Dynamic text asset with TSubclassOf should have violations"), bHasViolations);
	TestTrue(TEXT("Should have at least one violation"), violations.Num() >= 1);

	// Verify the violation message mentions TSubclassOf
	bool bFoundSubclassViolation = false;
	for (const FString& violation : violations)
	{
		if (violation.Contains(TEXT("TSubclassOf")))
		{
			bFoundSubclassViolation = true;
			break;
		}
	}
	TestTrue(TEXT("Violation should mention TSubclassOf"), bFoundSubclassViolation);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_ContainerObject_DetectsArrayOfHardRefs,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.Container.DetectsArrayOfHardRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_ContainerObject_DetectsArrayOfHardRefs::RunTest(const FString& Parameters)
{
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGTestContainerDynamicTextAsset::StaticClass(), violations);

	TestTrue(TEXT("Dynamic text asset with TArray<TObjectPtr> should have violations"), bHasViolations);
	TestTrue(TEXT("Should have at least one violation"), violations.Num() >= 1);

	// Verify the violation message mentions TArray
	bool bFoundArrayViolation = false;
	for (const FString& violation : violations)
	{
		if (violation.Contains(TEXT("TArray")))
		{
			bFoundArrayViolation = true;
			break;
		}
	}
	TestTrue(TEXT("Violation should mention TArray container"), bFoundArrayViolation);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_SoftRefs_AllowsSoftReferences,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.SoftRefs.AllowsSoftReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_SoftRefs_AllowsSoftReferences::RunTest(const FString& Parameters)
{
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGTestSoftRefDynamicTextAsset::StaticClass(), violations);

	TestFalse(TEXT("Dynamic text asset with only soft refs should have no violations"), bHasViolations);
	TestEqual(TEXT("Violations array should be empty"), violations.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_ProviderBoundary_ExemptsBaseClassProperties,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.ProviderBoundary.ExemptsBaseClassProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_ProviderBoundary_ExemptsBaseClassProperties::RunTest(const FString& Parameters)
{
	// USGDynamicTextAsset itself has UObject-inherited properties that could be flagged
	// if the boundary exemption didn't work. The boundary class (USGDynamicTextAsset)
	// and its ancestors are exempt from hard reference checks.
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGDynamicTextAsset::StaticClass(), violations);

	TestFalse(TEXT("Base USGDynamicTextAsset should have no violations (provider boundary exempts its properties)"), bHasViolations);
	TestEqual(TEXT("Violations array should be empty"), violations.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_NullClass_ReturnsFalse,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.NullClass.ReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_NullClass_ReturnsFalse::RunTest(const FString& Parameters)
{
	TArray<FString> violations;

	AddExpectedError(TEXT("Inputted NULL InClass"), EAutomationExpectedErrorFlags::Contains, 1);
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(nullptr, violations);

	TestFalse(TEXT("Null class should return false"), bHasViolations);
	TestEqual(TEXT("Violations array should be empty"), violations.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_ViolationMessage_ContainsSuggestion,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.ViolationMessage.ContainsSuggestion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_ViolationMessage_ContainsSuggestion::RunTest(const FString& Parameters)
{
	TArray<FString> violations;
	FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGTestDirtyDynamicTextAsset::StaticClass(), violations);

	TestTrue(TEXT("Should have violations"), violations.Num() > 0);

	// Violation messages should contain a suggestion for the correct type
	bool bFoundSuggestion = false;
	for (const FString& violation : violations)
	{
		if (violation.Contains(TEXT("Use TSoftObjectPtr<>")))
		{
			bFoundSuggestion = true;
			break;
		}
	}
	TestTrue(TEXT("Violation message should suggest TSoftObjectPtr<> replacement"), bFoundSuggestion);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_CustomProvider_DetectsHardRef,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.CustomProvider.DetectsHardRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_CustomProvider_DetectsHardRef::RunTest(const FString& Parameters)
{
	// USGTestCustomProvider implements ISGDynamicTextAssetProvider directly
	// (does NOT inherit from USGDynamicTextAsset). The runtime utility should
	// still detect hard references on it.
	TArray<FString> violations;
	bool hasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGTestCustomProvider::StaticClass(), violations);

	TestFalse(TEXT("Custom provider with TObjectPtr should have violations if they don't have `SGSkipHardRefValidation` as a meta UPROPERTY"), hasViolations);
	TestTrue(TEXT("Should have at least one violation if `HardObjectRef` does not have `SGSkipHardRefValidation` as a meta UPROPERTY"), violations.IsEmpty());

	bool foundHardObjectRef = false;
	for (const FString& violation : violations)
	{
		if (violation.Contains(TEXT("HardObjectRef")))
		{
			foundHardObjectRef = true;
			break;
		}
	}
	TestFalse(TEXT("Violation should mention HardObjectRef property"), foundHardObjectRef);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHardRef_CustomProviderChild_DetectsHardRef,
	"SGDynamicTextAssets.Runtime.Validation.HardReference.CustomProviderChild.DetectsHardRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHardRef_CustomProviderChild_DetectsHardRef::RunTest(const FString& Parameters)
{
	// USGTestCustomProviderChild inherits from USGTestCustomProvider which implements
	// ISGDynamicTextAssetProvider. This tests that inherited interface detection works
	// for subclasses of custom providers.
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGTestCustomProviderChild::StaticClass(), violations);

	TestTrue(TEXT("Custom provider child with TObjectPtr should have violations"), bHasViolations);
	TestTrue(TEXT("Should have at least one violation"), violations.Num() >= 1);

	return true;
}
