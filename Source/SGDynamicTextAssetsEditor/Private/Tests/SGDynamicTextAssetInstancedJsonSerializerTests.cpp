// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "Serialization/SGDynamicTextAssetSerializerBase.h"
#include "Tests/SGDynamicTextAssetInstancedTestTypes.h"

/**
 * Test: Single non-null instanced object round-trips through JSON serialization.
 * Creates a USGTestInstancedOwnerDTA with a populated SingleInstanced sub-object,
 * serializes to JSON, deserializes to a new instance, and verifies properties match.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedJsonRoundtrip_SingleNonNull,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.Instanced.SingleNonNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedJsonRoundtrip_SingleNonNull::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedOwnerDTA* source = NewObject<USGTestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->PlainString = TEXT("hello");

	USGTestInstancedBase* subObj = NewObject<USGTestInstancedBase>(source);
	subObj->Name = TEXT("Test");
	subObj->Value = 42.0f;
	source->SingleInstanced = subObj;

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGTestInstancedOwnerDTA* target = NewObject<USGTestInstancedOwnerDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
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
 * Test: Null instanced object round-trips through JSON serialization.
 * Verifies the serialized JSON contains a null value and deserialization preserves nullptr.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedJsonRoundtrip_NullPreserved,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.Instanced.NullPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedJsonRoundtrip_NullPreserved::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedOwnerDTA* source = NewObject<USGTestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	// SingleInstanced is nullptr by default

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Verify JSON contains null for the instanced property
	TestTrue(TEXT("JSON should contain null for SingleInstanced"), jsonString.Contains(TEXT("null")));

	USGTestInstancedOwnerDTA* target = NewObject<USGTestInstancedOwnerDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert
	TestNull(TEXT("SingleInstanced should remain null after round-trip"), target->SingleInstanced.Get());

	return true;
}

/**
 * Test: Polymorphic instanced object round-trips through JSON serialization.
 * Sets SingleInstanced to a USGTestInstancedDerived and verifies the derived
 * type's ExtraData property survives the round-trip.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedJsonRoundtrip_Polymorphic,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.Instanced.Polymorphic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedJsonRoundtrip_Polymorphic::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedOwnerDTA* source = NewObject<USGTestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	USGTestInstancedDerived* derived = NewObject<USGTestInstancedDerived>(source);
	derived->Name = TEXT("Derived");
	derived->Value = 10.0f;
	derived->ExtraData = 99;
	source->SingleInstanced = derived;

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Verify JSON contains the class key with the derived type name
	TestTrue(TEXT("JSON should contain SG_INST_OBJ_CLASS"), jsonString.Contains(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY));
	TestTrue(TEXT("JSON should contain SGTestInstancedDerived"), jsonString.Contains(TEXT("SGTestInstancedDerived")));

	USGTestInstancedOwnerDTA* target = NewObject<USGTestInstancedOwnerDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Assert: verify the deserialized object is the derived type
	TestNotNull(TEXT("SingleInstanced should not be null"), target->SingleInstanced.Get());
	USGTestInstancedDerived* targetDerived = Cast<USGTestInstancedDerived>(target->SingleInstanced.Get());
	TestNotNull(TEXT("SingleInstanced should be USGTestInstancedDerived"), targetDerived);
	if (targetDerived)
	{
		TestEqual(TEXT("Name should match"), targetDerived->Name, TEXT("Derived"));
		TestEqual(TEXT("Value should match"), targetDerived->Value, 10.0f);
		TestEqual(TEXT("ExtraData should match"), targetDerived->ExtraData, 99);
	}

	return true;
}

/**
 * Test: Array of instanced objects with mixed types round-trips through JSON.
 * Populates InstancedArray with base, derived, and null entries, verifies all
 * round-trip correctly with correct types and property values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedJsonRoundtrip_ArrayMixedTypes,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.Instanced.ArrayMixedTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedJsonRoundtrip_ArrayMixedTypes::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedOwnerDTA* source = NewObject<USGTestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	// Entry 0: base type
	USGTestInstancedBase* baseObj = NewObject<USGTestInstancedBase>(source);
	baseObj->Name = TEXT("Base");
	baseObj->Value = 1.0f;

	// Entry 1: derived type
	USGTestInstancedDerived* derivedObj = NewObject<USGTestInstancedDerived>(source);
	derivedObj->Name = TEXT("Derived");
	derivedObj->Value = 2.0f;
	derivedObj->ExtraData = 77;

	// Entry 2: null
	source->InstancedArray.Add(baseObj);
	source->InstancedArray.Add(derivedObj);
	source->InstancedArray.Add(nullptr);

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	USGTestInstancedOwnerDTA* target = NewObject<USGTestInstancedOwnerDTA>();
	bool bMigrated = false;
	bool bDeserialized = serializer.DeserializeProvider(jsonString, target, bMigrated);
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
		USGTestInstancedDerived* elem1 = Cast<USGTestInstancedDerived>(target->InstancedArray[1].Get());
		TestNotNull(TEXT("Element 1 should be USGTestInstancedDerived"), elem1);
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

/**
 * Test: JSON output format contains the SG_INST_OBJ_CLASS key for instanced objects.
 * Verifies the serialized JSON string has the expected structure.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancedJsonRoundtrip_OutputFormatContainsClassKey,
	"SGDynamicTextAssets.Runtime.Serialization.JsonSerializer.Instanced.OutputFormatClassKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInstancedJsonRoundtrip_OutputFormatContainsClassKey::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Arrange
	USGTestInstancedOwnerDTA* source = NewObject<USGTestInstancedOwnerDTA>();
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	USGTestInstancedBase* subObj = NewObject<USGTestInstancedBase>(source);
	subObj->Name = TEXT("FormatTest");
	subObj->Value = 5.0f;
	source->SingleInstanced = subObj;

	// Act
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonString;
	bool bSerialized = serializer.SerializeProvider(source, jsonString);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Assert: verify the class key is present in the raw JSON
	TestTrue(TEXT("JSON should contain SG_INST_OBJ_CLASS key"),
		jsonString.Contains(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY));
	TestTrue(TEXT("JSON should contain SGTestInstancedBase class name"),
		jsonString.Contains(TEXT("SGTestInstancedBase")));

	return true;
}
