// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "Core/SGDTAValidationUtils.h"
#include "Tests/SGDTAInstancedTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedValidation_OwnerDTA_NoViolations,
	"SGDynamicTextAssets.Runtime.Validation.InstancedObject.OwnerDTA.NoViolations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedValidation_OwnerDTA_NoViolations::RunTest(const FString& Parameters)
{
	// USGDTATestInstancedOwnerDTA has Instanced single + array properties pointing to
	// EditInlineNew classes with no nested hard refs. Should pass validation.
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGDTATestInstancedOwnerDTA::StaticClass(), violations);

	TestFalse(TEXT("Instanced owner DTA with EditInlineNew sub-objects should have no violations"), bHasViolations);
	TestTrue(TEXT("Violations array should be empty"), violations.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedValidation_HardRefOwnerDTA_DetectsNestedHardRef,
	"SGDynamicTextAssets.Runtime.Validation.InstancedObject.HardRefOwnerDTA.DetectsNestedHardRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedValidation_HardRefOwnerDTA_DetectsNestedHardRef::RunTest(const FString& Parameters)
{
	// USGDTATestInstancedHardRefOwnerDTA has an instanced object whose class
	// (USGDTATestInstancedWithHardRef) contains a TObjectPtr<UObject> hard reference.
	// Recursive validation should catch this.
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGDTATestInstancedHardRefOwnerDTA::StaticClass(), violations);

	TestTrue(TEXT("Instanced object with nested hard ref should have violations"), bHasViolations);
	TestTrue(TEXT("Should have at least one violation"), !violations.IsEmpty());

	// Verify the violation mentions the instanced property and nested hard ref
	bool bFoundNestedViolation = false;
	for (const FString& violation : violations)
	{
		if (violation.Contains(TEXT("BadInstanced")) && violation.Contains(TEXT("Instanced")))
		{
			bFoundNestedViolation = true;
			break;
		}
	}
	TestTrue(TEXT("Violation should mention BadInstanced and indicate it is an instanced object"), bFoundNestedViolation);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedValidation_InstancedArray_NoViolations,
	"SGDynamicTextAssets.Runtime.Validation.InstancedObject.InstancedArray.NoViolations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedValidation_InstancedArray_NoViolations::RunTest(const FString& Parameters)
{
	// Verify that the instanced TArray<TObjectPtr<USGDTATestInstancedBase>> property
	// on USGDTATestInstancedOwnerDTA is specifically detected as an instanced property
	// and does not trigger violations.
	const UClass* testClass = USGDTATestInstancedOwnerDTA::StaticClass();

	// Find the InstancedArray property and verify its inner element has the instanced flag.
	// CPF_InstancedReference is set on the array's inner FObjectProperty, not the outer FArrayProperty.
	const FArrayProperty* arrayProp = CastField<FArrayProperty>(
		testClass->FindPropertyByName(TEXT("InstancedArray")));
	TestNotNull(TEXT("InstancedArray property should exist"), arrayProp);

	if (arrayProp && arrayProp->Inner)
	{
		TestTrue(TEXT("InstancedArray inner element should be an instanced property"),
			FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(arrayProp->Inner));
	}

	// Full class validation should still pass with no violations
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		testClass, violations);

	TestFalse(TEXT("Owner DTA with instanced array should have no violations"), bHasViolations);
	TestTrue(TEXT("Violations array should be empty"), violations.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedValidation_IsInstancedObjectProperty_CorrectResults,
	"SGDynamicTextAssets.Runtime.Validation.InstancedObject.IsInstancedObjectProperty.CorrectResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedValidation_IsInstancedObjectProperty_CorrectResults::RunTest(const FString& Parameters)
{
	const UClass* ownerClass = USGDTATestInstancedOwnerDTA::StaticClass();

	// Instanced single property should return true
	const FProperty* instancedProp = ownerClass->FindPropertyByName(TEXT("SingleInstanced"));
	TestNotNull(TEXT("SingleInstanced property should exist"), instancedProp);
	if (instancedProp)
	{
		TestTrue(TEXT("SingleInstanced should be identified as instanced"),
			FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(instancedProp));
	}

	// Instanced array inner element should return true.
	// CPF_InstancedReference is set on the inner FObjectProperty, not the outer FArrayProperty.
	const FArrayProperty* instancedArrayProp = CastField<FArrayProperty>(
		ownerClass->FindPropertyByName(TEXT("InstancedArray")));
	TestNotNull(TEXT("InstancedArray property should exist"), instancedArrayProp);
	if (instancedArrayProp && instancedArrayProp->Inner)
	{
		TestTrue(TEXT("InstancedArray inner element should be identified as instanced"),
			FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(instancedArrayProp->Inner));
	}

	// Instanced set element should return true.
	// CPF_InstancedReference is set on the inner FObjectProperty, not the outer FSetProperty.
	const FSetProperty* instancedSetProp = CastField<FSetProperty>(
		ownerClass->FindPropertyByName(TEXT("InstancedSet")));
	TestNotNull(TEXT("InstancedSet property should exist"), instancedSetProp);
	if (instancedSetProp && instancedSetProp->ElementProp)
	{
		TestTrue(TEXT("InstancedSet element should be identified as instanced"),
			FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(instancedSetProp->ElementProp));
	}

	// Instanced map value property should return true.
	// CPF_InstancedReference is set on the value FObjectProperty, not the outer FMapProperty.
	const FMapProperty* instancedMapProp = CastField<FMapProperty>(
		ownerClass->FindPropertyByName(TEXT("InstancedMap")));
	TestNotNull(TEXT("InstancedMap property should exist"), instancedMapProp);
	if (instancedMapProp && instancedMapProp->ValueProp)
	{
		TestTrue(TEXT("InstancedMap value should be identified as instanced"),
			FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(instancedMapProp->ValueProp));
	}

	// Plain string property should return false
	const FProperty* plainStringProp = ownerClass->FindPropertyByName(TEXT("PlainString"));
	TestNotNull(TEXT("PlainString property should exist"), plainStringProp);
	if (plainStringProp)
	{
		TestFalse(TEXT("PlainString should not be identified as instanced"),
			FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(plainStringProp));
	}

	// Null property should return false
	TestFalse(TEXT("Null property should return false"),
		FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedValidation_InstancedStructDTA_NoViolations,
	"SGDynamicTextAssets.Runtime.Validation.InstancedObject.InstancedStructDTA.NoViolations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedValidation_InstancedStructDTA_NoViolations::RunTest(const FString& Parameters)
{
	// FInstancedStruct is a struct type, not a hard UObject reference.
	// Validation should not flag it.
	TArray<FString> violations;
	bool bHasViolations = FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(
		USGDTATestInstancedStructDTA::StaticClass(), violations);

	TestFalse(TEXT("DTA with FInstancedStruct properties should have no violations"), bHasViolations);
	TestTrue(TEXT("Violations array should be empty"), violations.IsEmpty());

	return true;
}
