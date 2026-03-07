// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Core/SGDynamicTextAssetId.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_DefaultState_IsNullAndInvalid,
	"SGDynamicTextAssets.Runtime.Core.Ref.DefaultState.IsNullAndInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_DefaultState_IsNullAndInvalid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef defaultRef;

	TestTrue(TEXT("Default-constructed ref should be null"), defaultRef.IsNull());
	TestFalse(TEXT("Default-constructed ref should not be valid"), defaultRef.IsValid());
	TestFalse(TEXT("Default-constructed ref ID should be invalid"), defaultRef.GetId().IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_SetId_BecomesValid,
	"SGDynamicTextAssets.Runtime.Core.Ref.SetId.BecomesValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_SetId_BecomesValid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetRef ref;

	ref.SetId(testId);

	TestFalse(TEXT("After SetId, ref should not be null"), ref.IsNull());
	TestTrue(TEXT("After SetId, ref should be valid"), ref.IsValid());
	TestEqual(TEXT("GetId should return the set ID"), ref.GetId(), testId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_ConstructorWithId_IsValid,
	"SGDynamicTextAssets.Runtime.Core.Ref.Constructor.WithId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_ConstructorWithId_IsValid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetRef ref(testId);

	TestFalse(TEXT("Ref constructed with ID should not be null"), ref.IsNull());
	TestTrue(TEXT("Ref constructed with ID should be valid"), ref.IsValid());
	TestEqual(TEXT("GetId should return the constructor ID"), ref.GetId(), testId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_Reset_ReturnsToDefaultState,
	"SGDynamicTextAssets.Runtime.Core.Ref.Reset.ReturnsToDefaultState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_Reset_ReturnsToDefaultState::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetRef ref(testId);

	// Verify it's valid first
	TestTrue(TEXT("Ref should be valid before reset"), ref.IsValid());

	// Reset
	ref.Reset();

	// Verify default state
	TestTrue(TEXT("After Reset, ref should be null"), ref.IsNull());
	TestFalse(TEXT("After Reset, ref should not be valid"), ref.IsValid());
	TestFalse(TEXT("After Reset, ID should be invalid"), ref.GetId().IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_Equality_SameId,
	"SGDynamicTextAssets.Runtime.Core.Ref.Equality.SameId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_Equality_SameId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetRef ref1(testId);
	FSGDynamicTextAssetRef ref2(testId);

	TestTrue(TEXT("Two refs with same ID should be equal"), ref1 == ref2);
	TestFalse(TEXT("Two refs with same ID should not be unequal"), ref1 != ref2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_Equality_DifferentId,
	"SGDynamicTextAssets.Runtime.Core.Ref.Equality.DifferentId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_Equality_DifferentId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef ref1(FSGDynamicTextAssetId::NewGeneratedId());
	FSGDynamicTextAssetRef ref2(FSGDynamicTextAssetId::NewGeneratedId());

	TestFalse(TEXT("Two refs with different IDs should not be equal"), ref1 == ref2);
	TestTrue(TEXT("Two refs with different IDs should be unequal"), ref1 != ref2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_Equality_DefaultRefs,
	"SGDynamicTextAssets.Runtime.Core.Ref.Equality.DefaultRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_Equality_DefaultRefs::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetRef ref1;
	FSGDynamicTextAssetRef ref2;

	TestTrue(TEXT("Two default refs should be equal"), ref1 == ref2);
	TestFalse(TEXT("Two default refs should not be unequal"), ref1 != ref2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetRef_TypeHash_ConsistentForSameId,
	"SGDynamicTextAssets.Runtime.Core.Ref.TypeHash.Consistent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetRef_TypeHash_ConsistentForSameId::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetRef ref1(testId);
	FSGDynamicTextAssetRef ref2(testId);

	uint32 hash1 = GetTypeHash(ref1);
	uint32 hash2 = GetTypeHash(ref2);

	TestEqual(TEXT("Hash of refs with same ID should be equal"), hash1, hash2);

	return true;
}
