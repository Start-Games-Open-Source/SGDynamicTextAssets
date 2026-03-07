// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/BitReader.h"

// ---------------------------------------------------------------------------
// 1. Default State
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_DefaultState_IsInvalid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.DefaultState.IsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_DefaultState_IsInvalid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId defaultId;

	TestFalse(TEXT("Default-constructed TypeId should be invalid"), defaultId.IsValid());
	TestFalse(TEXT("Default-constructed underlying GUID should be invalid"), defaultId.GetGuid().IsValid());

	return true;
}

// ---------------------------------------------------------------------------
// 2. NewGeneratedId — Valid
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_NewGeneratedId_IsValid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.NewGeneratedId.IsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_NewGeneratedId_IsValid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId id = FSGDynamicTextAssetTypeId::NewGeneratedId();

	TestTrue(TEXT("NewGeneratedId should return a valid TypeId"), id.IsValid());

	return true;
}

// ---------------------------------------------------------------------------
// 3. NewGeneratedId — Unique
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_NewGeneratedId_ProducesUniqueIds,
	"SGDynamicTextAssets.Runtime.Core.TypeId.NewGeneratedId.ProducesUniqueIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_NewGeneratedId_ProducesUniqueIds::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId id1 = FSGDynamicTextAssetTypeId::NewGeneratedId();
	FSGDynamicTextAssetTypeId id2 = FSGDynamicTextAssetTypeId::NewGeneratedId();

	TestTrue(TEXT("First generated TypeId should be valid"), id1.IsValid());
	TestTrue(TEXT("Second generated TypeId should be valid"), id2.IsValid());
	TestTrue(TEXT("Two generated TypeIds should be different"), id1 != id2);

	return true;
}

// ---------------------------------------------------------------------------
// 4. FromGuid — Valid GUID
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_FromGuid_ValidGuid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.FromGuid.ValidGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_FromGuid_ValidGuid::RunTest(const FString& Parameters)
{
	FGuid validGuid = FGuid::NewGuid();
	FSGDynamicTextAssetTypeId id = FSGDynamicTextAssetTypeId::FromGuid(validGuid);

	TestTrue(TEXT("FromGuid with valid GUID should produce valid TypeId"), id.IsValid());
	TestTrue(TEXT("GetGuid should return the original GUID"), id.GetGuid() == validGuid);

	return true;
}

// ---------------------------------------------------------------------------
// 5. FromGuid — Invalid GUID
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_FromGuid_InvalidGuid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.FromGuid.InvalidGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_FromGuid_InvalidGuid::RunTest(const FString& Parameters)
{
	FGuid invalidGuid;
	FSGDynamicTextAssetTypeId id = FSGDynamicTextAssetTypeId::FromGuid(invalidGuid);

	TestFalse(TEXT("FromGuid with invalid GUID should produce invalid TypeId"), id.IsValid());

	return true;
}

// ---------------------------------------------------------------------------
// 6. FromString — Valid String
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_FromString_ValidString,
	"SGDynamicTextAssets.Runtime.Core.TypeId.FromString.ValidString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_FromString_ValidString::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId id = FSGDynamicTextAssetTypeId::FromString(TEXT("550E8400-E29B-41D4-A716-446655440000"));

	TestTrue(TEXT("FromString with valid GUID string should produce valid TypeId"), id.IsValid());

	return true;
}

// ---------------------------------------------------------------------------
// 7. FromString — Invalid String
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_FromString_InvalidString,
	"SGDynamicTextAssets.Runtime.Core.TypeId.FromString.InvalidString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_FromString_InvalidString::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId emptyId = FSGDynamicTextAssetTypeId::FromString(TEXT(""));
	TestFalse(TEXT("FromString with empty string should produce invalid TypeId"), emptyId.IsValid());

	FSGDynamicTextAssetTypeId garbageId = FSGDynamicTextAssetTypeId::FromString(TEXT("not-a-valid-guid"));
	TestFalse(TEXT("FromString with garbage string should produce invalid TypeId"), garbageId.IsValid());

	return true;
}

// ---------------------------------------------------------------------------
// 8. ToString — Roundtrip
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_ToString_Roundtrip,
	"SGDynamicTextAssets.Runtime.Core.TypeId.ToString.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_ToString_Roundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId original = FSGDynamicTextAssetTypeId::NewGeneratedId();

	FString str = original.ToString();
	TestFalse(TEXT("ToString of valid TypeId should return non-empty string"), str.IsEmpty());

	FSGDynamicTextAssetTypeId parsed = FSGDynamicTextAssetTypeId::FromString(str);
	TestTrue(TEXT("FromString(id.ToString()) should roundtrip to the same TypeId"), parsed == original);

	return true;
}

// ---------------------------------------------------------------------------
// 9. Invalidate
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_Invalidate_BecomesInvalid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.Invalidate.BecomesInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_Invalidate_BecomesInvalid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId id = FSGDynamicTextAssetTypeId::NewGeneratedId();
	TestTrue(TEXT("TypeId should be valid before invalidation"), id.IsValid());

	id.Invalidate();

	TestFalse(TEXT("TypeId should be invalid after Invalidate()"), id.IsValid());

	return true;
}

// ---------------------------------------------------------------------------
// 10. Equality — Same GUID
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_Equality_SameGuid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.Equality.SameGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_Equality_SameGuid::RunTest(const FString& Parameters)
{
	FGuid guid = FGuid::NewGuid();
	FSGDynamicTextAssetTypeId id1 = FSGDynamicTextAssetTypeId::FromGuid(guid);
	FSGDynamicTextAssetTypeId id2 = FSGDynamicTextAssetTypeId::FromGuid(guid);

	TestTrue(TEXT("Two TypeIds from the same GUID should be equal"), id1 == id2);
	TestFalse(TEXT("Two TypeIds from the same GUID should not be unequal"), id1 != id2);

	return true;
}

// ---------------------------------------------------------------------------
// 11. Equality — Different GUID
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_Equality_DifferentGuid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.Equality.DifferentGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_Equality_DifferentGuid::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId id1 = FSGDynamicTextAssetTypeId::NewGeneratedId();
	FSGDynamicTextAssetTypeId id2 = FSGDynamicTextAssetTypeId::NewGeneratedId();

	TestFalse(TEXT("Two different TypeIds should not be equal"), id1 == id2);
	TestTrue(TEXT("Two different TypeIds should be unequal"), id1 != id2);

	return true;
}

// ---------------------------------------------------------------------------
// 12. Equality — Two Invalid Are Equal
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_Equality_TwoInvalidAreEqual,
	"SGDynamicTextAssets.Runtime.Core.TypeId.Equality.TwoInvalidAreEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_Equality_TwoInvalidAreEqual::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId id1;
	FSGDynamicTextAssetTypeId id2;

	TestTrue(TEXT("Two default-constructed (invalid) TypeIds should be equal"), id1 == id2);

	return true;
}

// ---------------------------------------------------------------------------
// 13. Equality — TypeId vs Raw FGuid (cross-type operators)
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_Equality_TypeIdVsRawGuid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.Equality.TypeIdVsRawGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_Equality_TypeIdVsRawGuid::RunTest(const FString& Parameters)
{
	FGuid guid = FGuid::NewGuid();
	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::FromGuid(guid);

	// operator==(const FGuid&)
	TestTrue(TEXT("TypeId should equal the FGuid it was constructed from"), typeId == guid);
	TestFalse(TEXT("TypeId should not be unequal to the FGuid it was constructed from"), typeId != guid);

	// Different GUID
	FGuid otherGuid = FGuid::NewGuid();
	TestFalse(TEXT("TypeId should not equal a different FGuid"), typeId == otherGuid);
	TestTrue(TEXT("TypeId should be unequal to a different FGuid"), typeId != otherGuid);

	return true;
}

// ---------------------------------------------------------------------------
// 14. GetTypeHash — Consistent For Same Id
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_GetTypeHash_ConsistentForSameId,
	"SGDynamicTextAssets.Runtime.Core.TypeId.GetTypeHash.ConsistentForSameId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_GetTypeHash_ConsistentForSameId::RunTest(const FString& Parameters)
{
	FGuid guid = FGuid::NewGuid();
	FSGDynamicTextAssetTypeId id1 = FSGDynamicTextAssetTypeId::FromGuid(guid);
	FSGDynamicTextAssetTypeId id2 = FSGDynamicTextAssetTypeId::FromGuid(guid);

	uint32 hash1 = GetTypeHash(id1);
	uint32 hash2 = GetTypeHash(id2);

	TestEqual(TEXT("Hash of two TypeIds with same GUID should be equal"), hash1, hash2);

	return true;
}

// ---------------------------------------------------------------------------
// 15. GetTypeHash — Different For Different Ids
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_GetTypeHash_DifferentForDifferentIds,
	"SGDynamicTextAssets.Runtime.Core.TypeId.GetTypeHash.DifferentForDifferentIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_GetTypeHash_DifferentForDifferentIds::RunTest(const FString& Parameters)
{
	// Generate several pairs to reduce false-positive probability
	int32 differentCount = 0;
	constexpr int32 NUM_TRIALS = 10;

	for (int32 i = 0; i < NUM_TRIALS; ++i)
	{
		FSGDynamicTextAssetTypeId id1 = FSGDynamicTextAssetTypeId::NewGeneratedId();
		FSGDynamicTextAssetTypeId id2 = FSGDynamicTextAssetTypeId::NewGeneratedId();

		if (GetTypeHash(id1) != GetTypeHash(id2))
		{
			++differentCount;
		}
	}

	// With 32-bit hashes, collision probability per pair is ~1/2^32.
	// Expect all or nearly all to differ.
	TestTrue(TEXT("Hashes of different TypeIds should usually differ (probabilistic)"), differentCount >= NUM_TRIALS - 1);

	return true;
}

// ---------------------------------------------------------------------------
// 16. Serialize — Binary Roundtrip
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_Serialize_BinaryRoundtrip,
	"SGDynamicTextAssets.Runtime.Core.TypeId.Serialize.BinaryRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_Serialize_BinaryRoundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId original = FSGDynamicTextAssetTypeId::NewGeneratedId();

	// Write
	TArray<uint8> buffer;
	FMemoryWriter writer(buffer);
	original.Serialize(writer);

	TestTrue(TEXT("Serialized buffer should not be empty"), buffer.Num() > 0);

	// Read
	FSGDynamicTextAssetTypeId deserialized;
	FMemoryReader reader(buffer);
	deserialized.Serialize(reader);

	TestTrue(TEXT("Deserialized TypeId should match original"), deserialized == original);

	return true;
}

// ---------------------------------------------------------------------------
// 17. NetSerialize — Roundtrip
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_NetSerialize_Roundtrip,
	"SGDynamicTextAssets.Runtime.Core.TypeId.NetSerialize.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_NetSerialize_Roundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId original = FSGDynamicTextAssetTypeId::NewGeneratedId();

	// Write — FGuid is 16 bytes = 128 bits, allocate enough
	TArray<uint8> buffer;
	buffer.SetNumZeroed(256);
	FBitWriter writer(256 * 8);

	bool bWriteSuccess = false;
	original.NetSerialize(writer, nullptr, bWriteSuccess);
	TestTrue(TEXT("NetSerialize write should succeed"), bWriteSuccess);

	// Read
	FBitReader reader(writer.GetData(), writer.GetNumBits());
	FSGDynamicTextAssetTypeId deserialized;
	bool bReadSuccess = false;
	deserialized.NetSerialize(reader, nullptr, bReadSuccess);
	TestTrue(TEXT("NetSerialize read should succeed"), bReadSuccess);

	TestTrue(TEXT("Net-deserialized TypeId should match original"), deserialized == original);

	return true;
}

// ---------------------------------------------------------------------------
// 18. ExportTextItem / ImportTextItem — Roundtrip
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_ExportImportTextItem_Roundtrip,
	"SGDynamicTextAssets.Runtime.Core.TypeId.ExportImportTextItem.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_ExportImportTextItem_Roundtrip::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeId original = FSGDynamicTextAssetTypeId::NewGeneratedId();

	// Export
	FString exported;
	FSGDynamicTextAssetTypeId defaultValue;
	bool bExported = original.ExportTextItem(exported, defaultValue, nullptr, 0, nullptr);

	TestTrue(TEXT("ExportTextItem should succeed"), bExported);
	TestFalse(TEXT("Exported string should not be empty"), exported.IsEmpty());

	// Import
	FSGDynamicTextAssetTypeId imported;
	const TCHAR* importBuffer = *exported;
	bool bImported = imported.ImportTextItem(importBuffer, 0, nullptr, nullptr);

	TestTrue(TEXT("ImportTextItem should succeed"), bImported);
	TestTrue(TEXT("Imported TypeId should match original"), imported == original);

	return true;
}

// ---------------------------------------------------------------------------
// 19. INVALID_TYPE_ID — Static Constant
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeId_INVALID_TYPE_ID_IsInvalid,
	"SGDynamicTextAssets.Runtime.Core.TypeId.INVALID_TYPE_ID.IsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeId_INVALID_TYPE_ID_IsInvalid::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("INVALID_TYPE_ID should be invalid"), FSGDynamicTextAssetTypeId::INVALID_TYPE_ID.IsValid());

	return true;
}
