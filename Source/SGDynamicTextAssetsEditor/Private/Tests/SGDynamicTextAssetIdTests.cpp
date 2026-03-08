// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/BitReader.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_DefaultState_IsInvalid,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.DefaultState.IsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_DefaultState_IsInvalid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId defaultId;

	TestFalse(TEXT("Default-constructed ID should be invalid"), defaultId.IsValid());
	TestFalse(TEXT("Default-constructed underlying GUID should be invalid"), defaultId.GetGuid().IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_NewGeneratedId_IsValid,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.NewGeneratedId.IsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_NewGeneratedId_IsValid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id = FSGDynamicTextAssetId::NewGeneratedId();

	TestTrue(TEXT("NewGeneratedId should return a valid ID"), id.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_NewGeneratedId_ProducesUniqueIds,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.NewGeneratedId.ProducesUniqueIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_NewGeneratedId_ProducesUniqueIds::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id1 = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetId id2 = FSGDynamicTextAssetId::NewGeneratedId();

	TestTrue(TEXT("First generated ID should be valid"), id1.IsValid());
	TestTrue(TEXT("Second generated ID should be valid"), id2.IsValid());
	TestNotEqual(TEXT("Two generated IDs should be different"), id1, id2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_FromGuid_ValidGuid,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.FromGuid.ValidGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_FromGuid_ValidGuid::RunTest(const FString& Parameters)
{
	FGuid validGuid = FGuid::NewGuid();
	FSGDynamicTextAssetId id = FSGDynamicTextAssetId::FromGuid(validGuid);

	TestTrue(TEXT("FromGuid with valid GUID should produce valid ID"), id.IsValid());
	TestEqual(TEXT("GetGuid should return the original GUID"), id.GetGuid(), validGuid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_FromGuid_InvalidGuid,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.FromGuid.InvalidGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_FromGuid_InvalidGuid::RunTest(const FString& Parameters)
{
	FGuid invalidGuid;
	FSGDynamicTextAssetId id = FSGDynamicTextAssetId::FromGuid(invalidGuid);

	TestFalse(TEXT("FromGuid with invalid GUID should produce invalid ID"), id.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_FromString_ValidString,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.FromString.ValidString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_FromString_ValidString::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id = FSGDynamicTextAssetId::FromString(TEXT("A1B2C3D4-E5F67890-ABCDEF12-34567890"));

	TestTrue(TEXT("FromString with valid string should produce valid ID"), id.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_FromString_InvalidString,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.FromString.InvalidString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_FromString_InvalidString::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId emptyId = FSGDynamicTextAssetId::FromString(TEXT(""));
	TestFalse(TEXT("FromString with empty string should produce invalid ID"), emptyId.IsValid());

	FSGDynamicTextAssetId garbageId = FSGDynamicTextAssetId::FromString(TEXT("not-a-valid-id"));
	TestFalse(TEXT("FromString with garbage string should produce invalid ID"), garbageId.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_ToString_ValidIdProducesNonEmpty,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.ToString.ValidIdProducesNonEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_ToString_ValidIdProducesNonEmpty::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id = FSGDynamicTextAssetId::NewGeneratedId();

	FString str = id.ToString();

	TestFalse(TEXT("ToString of valid ID should return non-empty string"), str.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_ToString_Roundtrip,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.ToString.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_ToString_Roundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId original = FSGDynamicTextAssetId::NewGeneratedId();

	FString str = original.ToString();
	FSGDynamicTextAssetId parsed = FSGDynamicTextAssetId::FromString(str);

	TestEqual(TEXT("FromString(id.ToString()) should roundtrip to the same ID"), parsed, original);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_Invalidate_BecomesInvalid,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.Invalidate.BecomesInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_Invalidate_BecomesInvalid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id = FSGDynamicTextAssetId::NewGeneratedId();
	TestTrue(TEXT("ID should be valid before invalidation"), id.IsValid());

	id.Invalidate();

	TestFalse(TEXT("ID should be invalid after Invalidate()"), id.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_Equality_SameGuid,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.Equality.SameGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_Equality_SameGuid::RunTest(const FString& Parameters)
{
	FGuid guid = FGuid::NewGuid();
	FSGDynamicTextAssetId id1 = FSGDynamicTextAssetId::FromGuid(guid);
	FSGDynamicTextAssetId id2 = FSGDynamicTextAssetId::FromGuid(guid);

	TestTrue(TEXT("Two IDs from the same GUID should be equal"), id1 == id2);
	TestFalse(TEXT("Two IDs from the same GUID should not be unequal"), id1 != id2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_Equality_DifferentGuid,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.Equality.DifferentGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_Equality_DifferentGuid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id1 = FSGDynamicTextAssetId::NewGeneratedId();
	FSGDynamicTextAssetId id2 = FSGDynamicTextAssetId::NewGeneratedId();

	TestFalse(TEXT("Two different IDs should not be equal"), id1 == id2);
	TestTrue(TEXT("Two different IDs should be unequal"), id1 != id2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_Equality_TwoInvalidIdsAreEqual,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.Equality.TwoInvalidIdsAreEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_Equality_TwoInvalidIdsAreEqual::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId id1;
	FSGDynamicTextAssetId id2;

	TestTrue(TEXT("Two default-constructed (invalid) IDs should be equal"), id1 == id2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_GetTypeHash_ConsistentForSameId,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.GetTypeHash.ConsistentForSameId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_GetTypeHash_ConsistentForSameId::RunTest(const FString& Parameters)
{
	FGuid guid = FGuid::NewGuid();
	FSGDynamicTextAssetId id1 = FSGDynamicTextAssetId::FromGuid(guid);
	FSGDynamicTextAssetId id2 = FSGDynamicTextAssetId::FromGuid(guid);

	uint32 hash1 = GetTypeHash(id1);
	uint32 hash2 = GetTypeHash(id2);

	TestEqual(TEXT("Hash of two IDs with same GUID should be equal"), hash1, hash2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_GetTypeHash_DifferentForDifferentIds,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.GetTypeHash.DifferentForDifferentIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_GetTypeHash_DifferentForDifferentIds::RunTest(const FString& Parameters)
{
	// Generate several pairs to reduce false-positive probability
	int32 differentCount = 0;
	constexpr int32 NUM_TRIALS = 10;

	for (int32 i = 0; i < NUM_TRIALS; ++i)
	{
		FSGDynamicTextAssetId id1 = FSGDynamicTextAssetId::NewGeneratedId();
		FSGDynamicTextAssetId id2 = FSGDynamicTextAssetId::NewGeneratedId();

		if (GetTypeHash(id1) != GetTypeHash(id2))
		{
			++differentCount;
		}
	}

	// With 32-bit hashes, collision probability per pair is ~1/2^32.
	// Expect all or nearly all to differ.
	TestTrue(TEXT("Hashes of different IDs should usually differ (probabilistic)"), differentCount >= NUM_TRIALS - 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_Serialize_Roundtrip,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.Serialize.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_Serialize_Roundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId original = FSGDynamicTextAssetId::NewGeneratedId();

	// Write
	TArray<uint8> buffer;
	FMemoryWriter writer(buffer);
	original.Serialize(writer);

	TestTrue(TEXT("Serialized buffer should not be empty"), buffer.Num() > 0);

	// Read
	FSGDynamicTextAssetId deserialized;
	FMemoryReader reader(buffer);
	deserialized.Serialize(reader);

	TestEqual(TEXT("Deserialized ID should match original"), deserialized, original);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_NetSerialize_Roundtrip,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.NetSerialize.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_NetSerialize_Roundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId original = FSGDynamicTextAssetId::NewGeneratedId();

	// Write
	// FGuid is 16 bytes = 128 bits, allocate enough
	TArray<uint8> buffer;
	buffer.SetNumZeroed(256);
	FBitWriter writer(256 * 8);

	bool bWriteSuccess = false;
	original.NetSerialize(writer, nullptr, bWriteSuccess);
	TestTrue(TEXT("NetSerialize write should succeed"), bWriteSuccess);

	// Read
	FBitReader reader(writer.GetData(), writer.GetNumBits());
	FSGDynamicTextAssetId deserialized;
	bool bReadSuccess = false;
	deserialized.NetSerialize(reader, nullptr, bReadSuccess);
	TestTrue(TEXT("NetSerialize read should succeed"), bReadSuccess);

	TestEqual(TEXT("Net-deserialized ID should match original"), deserialized, original);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetId_ExportImportTextItem_Roundtrip,
	"SGDynamicTextAssets.Runtime.Core.DynamicTextAssetId.ExportImportTextItem.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetId_ExportImportTextItem_Roundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId original = FSGDynamicTextAssetId::NewGeneratedId();

	// Export
	FString exported;
	FSGDynamicTextAssetId defaultValue;
	bool bExported = original.ExportTextItem(exported, defaultValue, nullptr, 0, nullptr);

	TestTrue(TEXT("ExportTextItem should succeed"), bExported);
	TestFalse(TEXT("Exported string should not be empty"), exported.IsEmpty());

	// Import
	FSGDynamicTextAssetId imported;
	const TCHAR* buffer = *exported;
	bool bImported = imported.ImportTextItem(buffer, 0, nullptr, nullptr);

	TestTrue(TEXT("ImportTextItem should succeed"), bImported);
	TestEqual(TEXT("Imported ID should match original"), imported, original);

	return true;
}
