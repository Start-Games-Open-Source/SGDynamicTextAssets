// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "StructUtils/InstancedStruct.h"
#include "Tests/SGDynamicTextAssetInstancedTestTypes.h"

/**
 * Test: Single FInstancedStruct with base type round-trips through JSON serialization.
 * Creates a USGTestInstancedStructDTA with a populated SingleStruct,
 * serializes to JSON, deserializes to a new instance, and verifies properties match.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedStructJsonRoundtrip_SingleBase,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.InstancedStruct.SingleBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedStructJsonRoundtrip_SingleBase::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedStructDTA* source = NewObject<USGTestInstancedStructDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	FTestInstancedStructBase baseStruct;
	baseStruct.BaseValue = 42;
	baseStruct.BaseName = TEXT("Hello");
	source->SingleStruct.InitializeAs<FTestInstancedStructBase>(baseStruct);

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGTestInstancedStructDTA* target = NewObject<USGTestInstancedStructDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert
	TestTrue(TEXT("SingleStruct should be valid after round-trip"), target->SingleStruct.IsValid());
	if (target->SingleStruct.IsValid())
	{
		const FTestInstancedStructBase* result = target->SingleStruct.GetPtr<FTestInstancedStructBase>();
		TestNotNull(TEXT("SingleStruct should contain FTestInstancedStructBase"), result);
		if (result)
		{
			TestEqual(TEXT("BaseValue should match"), result->BaseValue, 42);
			TestEqual(TEXT("BaseName should match"), result->BaseName, TEXT("Hello"));
		}
	}

	return true;
}

/**
 * Test: FInstancedStruct array with base and derived entries round-trips through JSON.
 * Populates StructArray with base and derived entries, verifies all
 * round-trip correctly with correct types and property values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedStructJsonRoundtrip_ArrayMixed,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.InstancedStruct.ArrayMixed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedStructJsonRoundtrip_ArrayMixed::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedStructDTA* source = NewObject<USGTestInstancedStructDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	FTestInstancedStructBase baseEntry;
	baseEntry.BaseValue = 10;
	baseEntry.BaseName = TEXT("Base");

	FTestInstancedStructDerived derivedEntry;
	derivedEntry.BaseValue = 20;
	derivedEntry.BaseName = TEXT("Derived");
	derivedEntry.DerivedFloat = 3.14f;

	source->StructArray.Add(FInstancedStruct::Make<FTestInstancedStructBase>(baseEntry));
	source->StructArray.Add(FInstancedStruct::Make<FTestInstancedStructDerived>(derivedEntry));

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGTestInstancedStructDTA* target = NewObject<USGTestInstancedStructDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert
	TestEqual(TEXT("StructArray should have 2 elements"), target->StructArray.Num(), 2);
	if (target->StructArray.Num() == 2)
	{
		// Element 0: base
		TestTrue(TEXT("Element 0 should be valid"), target->StructArray[0].IsValid());
		if (target->StructArray[0].IsValid())
		{
			const FTestInstancedStructBase* elem0 = target->StructArray[0].GetPtr<FTestInstancedStructBase>();
			TestNotNull(TEXT("Element 0 should be FTestInstancedStructBase"), elem0);
			if (elem0)
			{
				TestEqual(TEXT("Element 0 BaseValue should match"), elem0->BaseValue, 10);
				TestEqual(TEXT("Element 0 BaseName should match"), elem0->BaseName, TEXT("Base"));
			}
		}

		// Element 1: derived
		TestTrue(TEXT("Element 1 should be valid"), target->StructArray[1].IsValid());
		if (target->StructArray[1].IsValid())
		{
			const FTestInstancedStructDerived* elem1 = target->StructArray[1].GetPtr<FTestInstancedStructDerived>();
			TestNotNull(TEXT("Element 1 should be FTestInstancedStructDerived"), elem1);
			if (elem1)
			{
				TestEqual(TEXT("Element 1 BaseValue should match"), elem1->BaseValue, 20);
				TestEqual(TEXT("Element 1 BaseName should match"), elem1->BaseName, TEXT("Derived"));
				TestEqual(TEXT("Element 1 DerivedFloat should match"), elem1->DerivedFloat, 3.14f);
			}
		}
	}

	return true;
}

/**
 * Test: Empty/unset FInstancedStruct round-trips through JSON serialization.
 * Leaves SingleStruct uninitialized and verifies round-trip handles it gracefully.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedStructJsonRoundtrip_EmptyGraceful,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.InstancedStruct.EmptyGraceful",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedStructJsonRoundtrip_EmptyGraceful::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedStructDTA* source = NewObject<USGTestInstancedStructDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	// SingleStruct is left uninitialized (default-constructed, invalid)

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed with empty FInstancedStruct"), bSerialized);

	USGTestInstancedStructDTA* target = NewObject<USGTestInstancedStructDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed with empty FInstancedStruct"), bDeserialized);

	// Assert
	TestFalse(TEXT("SingleStruct should remain invalid after round-trip"), target->SingleStruct.IsValid());
	TestEqual(TEXT("StructArray should be empty"), target->StructArray.Num(), 0);

	return true;
}
