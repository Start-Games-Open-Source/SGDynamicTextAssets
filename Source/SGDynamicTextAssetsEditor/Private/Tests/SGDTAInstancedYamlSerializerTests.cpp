// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Serialization/SGDTAYamlSerializer.h"
#include "Serialization/SGDTASerializerBase.h"
#include "Tests/SGDTAInstancedTestTypes.h"

/**
 * Test: Single non-null instanced object round-trips through YAML serialization.
 * Creates a USGDTATestInstancedOwnerDTA with a populated SingleInstanced sub-object,
 * serializes to YAML, deserializes to a new instance, and verifies properties match.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedYamlRoundtrip_SingleNonNull,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.Instanced.SingleNonNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedYamlRoundtrip_SingleNonNull::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGDTATestInstancedOwnerDTA* source = NewObject<USGDTATestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->PlainString = TEXT("hello");

	USGDTATestInstancedBase* subObj = NewObject<USGDTATestInstancedBase>(source);
	subObj->Name = TEXT("Test");
	subObj->Value = 42.0f;
	source->SingleInstanced = subObj;

	// Act
	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	bool bSerialized = serializer.SerializeProvider(source, yamlString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGDTATestInstancedOwnerDTA* target = NewObject<USGDTATestInstancedOwnerDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert
	TestNotNull(TEXT("SingleInstanced should not be null after round-trip"), target->SingleInstanced.Get());
	if (target->SingleInstanced)
	{
		TestEqual(TEXT("Name should match"), target->SingleInstanced->Name, TEXT("Test"));
		TestEqual(TEXT("Value should match"), target->SingleInstanced->Value, 42.0f);
	}
	TestEqual(TEXT("PlainString should match"), target->PlainString, TEXT("hello"));

	return true;
}

/**
 * Test: Polymorphic instanced object round-trips through YAML serialization.
 * Sets SingleInstanced to a USGDTATestInstancedDerived and verifies the derived
 * type's ExtraData property survives the round-trip.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedYamlRoundtrip_Polymorphic,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.Instanced.Polymorphic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedYamlRoundtrip_Polymorphic::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGDTATestInstancedOwnerDTA* source = NewObject<USGDTATestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	USGDTATestInstancedDerived* derived = NewObject<USGDTATestInstancedDerived>(source);
	derived->Name = TEXT("Derived");
	derived->Value = 10.0f;
	derived->ExtraData = 99;
	source->SingleInstanced = derived;

	// Act
	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	bool bSerialized = serializer.SerializeProvider(source, yamlString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Verify YAML contains the class key with the derived type name
	TestTrue(TEXT("YAML should contain SG_INST_OBJ_CLASS"),
		yamlString.Contains(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY));
	TestTrue(TEXT("YAML should contain SGDTATestInstancedDerived"),
		yamlString.Contains(TEXT("SGDTATestInstancedDerived")));

	USGDTATestInstancedOwnerDTA* target = NewObject<USGDTATestInstancedOwnerDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert: verify the deserialized object is the derived type
	TestNotNull(TEXT("SingleInstanced should not be null"), target->SingleInstanced.Get());
	USGDTATestInstancedDerived* targetDerived = Cast<USGDTATestInstancedDerived>(target->SingleInstanced.Get());
	TestNotNull(TEXT("SingleInstanced should be USGDTATestInstancedDerived"), targetDerived);
	if (targetDerived)
	{
		TestEqual(TEXT("Name should match"), targetDerived->Name, TEXT("Derived"));
		TestEqual(TEXT("Value should match"), targetDerived->Value, 10.0f);
		TestEqual(TEXT("ExtraData should match"), targetDerived->ExtraData, 99);
	}

	return true;
}

/**
 * Test: Array of instanced objects with mixed types round-trips through YAML.
 * Populates InstancedArray with base, derived, and null entries, verifies all
 * round-trip correctly with correct types and property values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedYamlRoundtrip_ArrayMixedTypes,
	"SGDynamicTextAssets.Runtime.Serialization.YamlSerializer.Instanced.ArrayMixedTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedYamlRoundtrip_ArrayMixedTypes::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGDTATestInstancedOwnerDTA* source = NewObject<USGDTATestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	// Entry 0: base type
	USGDTATestInstancedBase* baseObj = NewObject<USGDTATestInstancedBase>(source);
	baseObj->Name = TEXT("Base");
	baseObj->Value = 1.0f;

	// Entry 1: derived type
	USGDTATestInstancedDerived* derivedObj = NewObject<USGDTATestInstancedDerived>(source);
	derivedObj->Name = TEXT("Derived");
	derivedObj->Value = 2.0f;
	derivedObj->ExtraData = 77;

	// Entry 2: null
	source->InstancedArray.Add(baseObj);
	source->InstancedArray.Add(derivedObj);
	source->InstancedArray.Add(nullptr);

	// Act
	FSGDynamicTextAssetYamlSerializer serializer;
	FString yamlString;
	bool bSerialized = serializer.SerializeProvider(source, yamlString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGDTATestInstancedOwnerDTA* target = NewObject<USGDTATestInstancedOwnerDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(yamlString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert
	TestEqual(TEXT("Array should have 3 elements"), target->InstancedArray.Num(), 3);
	if (target->InstancedArray.Num() == 3)
	{
		// Element 0: base
		TestNotNull(TEXT("Element 0 should not be null"), target->InstancedArray[0].Get());
		if (target->InstancedArray[0])
		{
			TestEqual(TEXT("Element 0 Name should match"), target->InstancedArray[0]->Name, TEXT("Base"));
			TestEqual(TEXT("Element 0 Value should match"), target->InstancedArray[0]->Value, 1.0f);
		}

		// Element 1: derived
		USGDTATestInstancedDerived* elem1 = Cast<USGDTATestInstancedDerived>(target->InstancedArray[1].Get());
		TestNotNull(TEXT("Element 1 should be USGDTATestInstancedDerived"), elem1);
		if (elem1)
		{
			TestEqual(TEXT("Element 1 Name should match"), elem1->Name, TEXT("Derived"));
			TestEqual(TEXT("Element 1 Value should match"), elem1->Value, 2.0f);
			TestEqual(TEXT("Element 1 ExtraData should match"), elem1->ExtraData, 77);
		}

		// Element 2: null
		TestNull(TEXT("Element 2 should be null"), target->InstancedArray[2].Get());
	}

	return true;
}
