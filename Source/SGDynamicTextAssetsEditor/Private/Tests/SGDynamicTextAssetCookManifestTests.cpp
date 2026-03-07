// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDynamicTextAssetTypeId.h"
#include "HAL/FileManager.h"
#include "Management/SGDynamicTextAssetCookManifest.h"
#include "Management/SGDynamicTextAssetCookManifestBinaryHeader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/**
 * Test: SaveToFileBinary writes a valid binary file with DTAM magic header
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetCookManifest_SaveToFileBinary_WritesBinaryFile,
	"SGDynamicTextAssets.Runtime.Management.CookManifest.SaveToFileBinary_WritesBinaryFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetCookManifest_SaveToFileBinary_WritesBinaryFile::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetCookManifest manifest;

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("A1B2C3D4E5F67890A1B2C3D4E5F67890"));
	FSGDynamicTextAssetTypeId testTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111BBBB2222CCCC3333DDDD4444"));
	manifest.AddEntry(testId, TEXT("UTestDynamicTextAsset"), TEXT("test_item"), testTypeId);

	FString testDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests/BinaryWriteTest"));
	IFileManager::Get().MakeDirectory(*testDir, true);

	bool bSaved = manifest.SaveToFileBinary(testDir);
	TestTrue(TEXT("SaveToFileBinary should succeed"), bSaved);

	// Verify the file exists
	FString filePath = FPaths::Combine(testDir,
		FSGDynamicTextAssetCookManifestBinaryHeader::BINARY_MANIFEST_FILENAME);
	TestTrue(TEXT("Binary manifest file should exist"), IFileManager::Get().FileExists(*filePath));

	// Load file and verify first 4 bytes are 'DTAM' magic
	TArray<uint8> fileData;
	FFileHelper::LoadFileToArray(fileData, *filePath);
	TestTrue(TEXT("File should be at least 32 bytes (header size)"),
		fileData.Num() >= static_cast<int32>(FSGDynamicTextAssetCookManifestBinaryHeader::HEADER_SIZE));

	if (fileData.Num() >= 4)
	{
		TestEqual(TEXT("Magic byte 0 should be 0x44 ('D')"), fileData[0], static_cast<uint8>(0x44));
		TestEqual(TEXT("Magic byte 1 should be 0x54 ('T')"), fileData[1], static_cast<uint8>(0x54));
		TestEqual(TEXT("Magic byte 2 should be 0x41 ('A')"), fileData[2], static_cast<uint8>(0x41));
		TestEqual(TEXT("Magic byte 3 should be 0x4D ('M')"), fileData[3], static_cast<uint8>(0x4D));
	}

	// Cleanup
	IFileManager::Get().DeleteDirectory(*testDir, false, true);

	return true;
}

/**
 * Test: Roundtrip save and load preserves all entry data and lookup indices
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetCookManifest_Roundtrip_PreservesData,
	"SGDynamicTextAssets.Runtime.Management.CookManifest.Roundtrip_PreservesData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetCookManifest_Roundtrip_PreservesData::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetCookManifest saveManifest;

	FSGDynamicTextAssetId id1, id2, id3;
	id1.ParseString(TEXT("11111111111111111111111111111111"));
	id2.ParseString(TEXT("22222222222222222222222222222222"));
	id3.ParseString(TEXT("33333333333333333333333333333333"));
	FSGDynamicTextAssetTypeId weaponTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111BBBB2222CCCC3333DDDD4444"));
	FSGDynamicTextAssetTypeId armorTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("EEEE5555FFFF6666AAAA7777BBBB8888"));

	saveManifest.AddEntry(id1, TEXT("UWeaponData"), TEXT("sword"), weaponTypeId);
	saveManifest.AddEntry(id2, TEXT("UWeaponData"), TEXT("axe"), weaponTypeId);
	saveManifest.AddEntry(id3, TEXT("UArmorData"), TEXT("shield"), armorTypeId);

	FString testDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests/BinaryRoundtrip"));
	IFileManager::Get().MakeDirectory(*testDir, true);

	saveManifest.SaveToFileBinary(testDir);

	FSGDynamicTextAssetCookManifest loadManifest;
	bool bLoaded = loadManifest.LoadFromFileBinary(testDir);

	TestTrue(TEXT("LoadFromFileBinary should succeed"), bLoaded);
	TestTrue(TEXT("Manifest should be marked as loaded"), loadManifest.IsLoaded());
	TestEqual(TEXT("Entry count should match"), loadManifest.Num(), 3);

	// Verify each entry by ID lookup
	const FSGDynamicTextAssetCookManifestEntry* entry1 = loadManifest.FindById(id1);
	const FSGDynamicTextAssetCookManifestEntry* entry2 = loadManifest.FindById(id2);
	const FSGDynamicTextAssetCookManifestEntry* entry3 = loadManifest.FindById(id3);

	TestNotNull(TEXT("Entry 1 should exist"), entry1);
	TestNotNull(TEXT("Entry 2 should exist"), entry2);
	TestNotNull(TEXT("Entry 3 should exist"), entry3);

	if (entry1)
	{
		TestEqual(TEXT("Entry 1 className"), entry1->ClassName, TEXT("UWeaponData"));
		TestEqual(TEXT("Entry 1 userFacingId"), entry1->UserFacingId, TEXT("sword"));
		TestEqual(TEXT("Entry 1 assetTypeId"), entry1->AssetTypeId, weaponTypeId);
	}

	if (entry2)
	{
		TestEqual(TEXT("Entry 2 className"), entry2->ClassName, TEXT("UWeaponData"));
		TestEqual(TEXT("Entry 2 userFacingId"), entry2->UserFacingId, TEXT("axe"));
		TestEqual(TEXT("Entry 2 assetTypeId"), entry2->AssetTypeId, weaponTypeId);
	}

	if (entry3)
	{
		TestEqual(TEXT("Entry 3 className"), entry3->ClassName, TEXT("UArmorData"));
		TestEqual(TEXT("Entry 3 userFacingId"), entry3->UserFacingId, TEXT("shield"));
		TestEqual(TEXT("Entry 3 assetTypeId"), entry3->AssetTypeId, armorTypeId);
	}

	// Verify class-based lookup
	TArray<const FSGDynamicTextAssetCookManifestEntry*> weaponEntries;
	loadManifest.FindByClassName(TEXT("UWeaponData"), weaponEntries);
	TestEqual(TEXT("Should find 2 weapon entries by className"), weaponEntries.Num(), 2);

	// Verify AssetTypeId-based lookup
	TArray<const FSGDynamicTextAssetCookManifestEntry*> weaponTypeEntries;
	loadManifest.FindByAssetTypeId(weaponTypeId, weaponTypeEntries);
	TestEqual(TEXT("Should find 2 entries by weapon type ID"), weaponTypeEntries.Num(), 2);

	TArray<const FSGDynamicTextAssetCookManifestEntry*> armorTypeEntries;
	loadManifest.FindByAssetTypeId(armorTypeId, armorTypeEntries);
	TestEqual(TEXT("Should find 1 entry by armor type ID"), armorTypeEntries.Num(), 1);

	// Cleanup
	IFileManager::Get().DeleteDirectory(*testDir, false, true);

	return true;
}

/**
 * Test: LoadFromFileBinary rejects a file with tampered entry bytes (hash mismatch)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetCookManifest_LoadFromFileBinary_RejectsTamperedFile,
	"SGDynamicTextAssets.Runtime.Management.CookManifest.LoadFromFileBinary_RejectsTamperedFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetCookManifest_LoadFromFileBinary_RejectsTamperedFile::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetCookManifest saveManifest;

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("DEADBEEFCAFEBABE1234567890ABCDEF"));
	FSGDynamicTextAssetTypeId testTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("DDDD1111EEEE2222FFFF3333AAAA4444"));
	saveManifest.AddEntry(testId, TEXT("UOriginalData"), TEXT("original_item"), testTypeId);

	FString testDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests/BinaryTamperTest"));
	IFileManager::Get().MakeDirectory(*testDir, true);

	saveManifest.SaveToFileBinary(testDir);

	// Tamper with the binary file — flip a byte in the entries section (after the 32-byte header)
	FString filePath = FPaths::Combine(testDir,
		FSGDynamicTextAssetCookManifestBinaryHeader::BINARY_MANIFEST_FILENAME);
	TArray<uint8> fileData;
	FFileHelper::LoadFileToArray(fileData, *filePath);

	if (fileData.Num() > static_cast<int32>(FSGDynamicTextAssetCookManifestBinaryHeader::HEADER_SIZE) + 4)
	{
		// Flip a byte in the entries section
		int32 tamperIndex = FSGDynamicTextAssetCookManifestBinaryHeader::HEADER_SIZE + 4;
		fileData[tamperIndex] ^= 0xFF;
		FFileHelper::SaveArrayToFile(fileData, *filePath);
	}

	// Try to load — should fail due to hash mismatch
	FSGDynamicTextAssetCookManifest loadManifest;
	AddExpectedError(TEXT("Content hash mismatch"), EAutomationExpectedErrorFlags::Contains, 1);
	bool bLoaded = loadManifest.LoadFromFileBinary(testDir);

	TestFalse(TEXT("LoadFromFileBinary should fail for tampered file"), bLoaded);
	TestFalse(TEXT("Manifest should not be marked as loaded"), loadManifest.IsLoaded());

	// Cleanup
	IFileManager::Get().DeleteDirectory(*testDir, false, true);

	return true;
}

/**
 * Test: String table deduplicates repeated class names correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetCookManifest_StringDeduplication,
	"SGDynamicTextAssets.Runtime.Management.CookManifest.StringDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetCookManifest_StringDeduplication::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetCookManifest saveManifest;
	FSGDynamicTextAssetTypeId weaponTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111BBBB2222CCCC3333DDDD4444"));

	// Add 5 entries all with the same className
	TArray<FSGDynamicTextAssetId> ids;
	for (int32 i = 0; i < 5; ++i)
	{
		FSGDynamicTextAssetId id = FSGDynamicTextAssetId::NewGeneratedId();
		ids.Add(id);
		saveManifest.AddEntry(id, TEXT("UWeaponData"),
			FString::Printf(TEXT("weapon_%d"), i), weaponTypeId);
	}

	FString testDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests/BinaryDedup"));
	IFileManager::Get().MakeDirectory(*testDir, true);

	saveManifest.SaveToFileBinary(testDir);

	// Load back and verify all 5 entries have the correct className
	FSGDynamicTextAssetCookManifest loadManifest;
	bool bLoaded = loadManifest.LoadFromFileBinary(testDir);

	TestTrue(TEXT("LoadFromFileBinary should succeed"), bLoaded);
	TestEqual(TEXT("Should have 5 entries"), loadManifest.Num(), 5);

	for (int32 i = 0; i < ids.Num(); ++i)
	{
		const FSGDynamicTextAssetCookManifestEntry* entry = loadManifest.FindById(ids[i]);
		TestNotNull(*FString::Printf(TEXT("Entry %d should exist"), i), entry);
		if (entry)
		{
			TestEqual(*FString::Printf(TEXT("Entry %d className should be UWeaponData"), i),
				entry->ClassName, TEXT("UWeaponData"));
			TestEqual(*FString::Printf(TEXT("Entry %d userFacingId"), i),
				entry->UserFacingId, FString::Printf(TEXT("weapon_%d"), i));
		}
	}

	// Verify class-based lookup returns all 5
	TArray<const FSGDynamicTextAssetCookManifestEntry*> weaponEntries;
	loadManifest.FindByClassName(TEXT("UWeaponData"), weaponEntries);
	TestEqual(TEXT("Should find 5 entries by className"), weaponEntries.Num(), 5);

	// Cleanup
	IFileManager::Get().DeleteDirectory(*testDir, false, true);

	return true;
}

/**
 * Test: Large manifest stress test with 5000 entries
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetCookManifest_LargeManifest_StressTest,
	"SGDynamicTextAssets.Runtime.Management.CookManifest.LargeManifest_StressTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetCookManifest_LargeManifest_StressTest::RunTest(const FString& Parameters)
{
	static constexpr int32 ENTRY_COUNT = 5000;
	static constexpr int32 CLASS_COUNT = 10;

	// Generate class names and type IDs
	TArray<FString> classNames;
	TArray<FSGDynamicTextAssetTypeId> typeIds;
	for (int32 i = 0; i < CLASS_COUNT; ++i)
	{
		classNames.Add(FString::Printf(TEXT("UTestClass_%d"), i));
		typeIds.Add(FSGDynamicTextAssetTypeId::NewGeneratedId());
	}

	// Build manifest with 5000 entries
	FSGDynamicTextAssetCookManifest saveManifest;
	TArray<FSGDynamicTextAssetId> allIds;
	allIds.Reserve(ENTRY_COUNT);

	for (int32 i = 0; i < ENTRY_COUNT; ++i)
	{
		FSGDynamicTextAssetId id = FSGDynamicTextAssetId::NewGeneratedId();
		allIds.Add(id);

		int32 classIndex = i % CLASS_COUNT;
		saveManifest.AddEntry(id, classNames[classIndex],
			FString::Printf(TEXT("item_%d"), i), typeIds[classIndex]);
	}

	FString testDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests/BinaryStress"));
	IFileManager::Get().MakeDirectory(*testDir, true);

	bool bSaved = saveManifest.SaveToFileBinary(testDir);
	TestTrue(TEXT("SaveToFileBinary should succeed with 5000 entries"), bSaved);

	// Load back
	FSGDynamicTextAssetCookManifest loadManifest;
	bool bLoaded = loadManifest.LoadFromFileBinary(testDir);

	TestTrue(TEXT("LoadFromFileBinary should succeed"), bLoaded);
	TestEqual(TEXT("Entry count should match"), loadManifest.Num(), ENTRY_COUNT);

	// Spot-check entries at various positions
	int32 spotCheckIndices[] = { 0, 1, 100, 999, 2500, 4999 };
	for (int32 idx : spotCheckIndices)
	{
		const FSGDynamicTextAssetCookManifestEntry* entry = loadManifest.FindById(allIds[idx]);
		TestNotNull(*FString::Printf(TEXT("Entry at index %d should exist"), idx), entry);
		if (entry)
		{
			int32 classIndex = idx % CLASS_COUNT;
			TestEqual(*FString::Printf(TEXT("Entry %d className"), idx),
				entry->ClassName, classNames[classIndex]);
			TestEqual(*FString::Printf(TEXT("Entry %d userFacingId"), idx),
				entry->UserFacingId, FString::Printf(TEXT("item_%d"), idx));
		}
	}

	// Verify class-based lookup counts (5000 / 10 = 500 per class)
	TArray<const FSGDynamicTextAssetCookManifestEntry*> classEntries;
	loadManifest.FindByClassName(classNames[0], classEntries);
	TestEqual(TEXT("Each class should have 500 entries"), classEntries.Num(), ENTRY_COUNT / CLASS_COUNT);

	// Cleanup
	IFileManager::Get().DeleteDirectory(*testDir, false, true);

	return true;
}

/**
 * Test: Content hash is deterministic — same entries produce byte-identical files
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetCookManifest_ContentHash_Deterministic,
	"SGDynamicTextAssets.Runtime.Management.CookManifest.ContentHash_Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetCookManifest_ContentHash_Deterministic::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("12345678ABCDEF0012345678ABCDEF00"));
	FSGDynamicTextAssetTypeId testTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("DDDDDDDDEEEEEEEEFFFFFFFF00000000"));

	// Save manifest 1
	FSGDynamicTextAssetCookManifest manifest1;
	manifest1.AddEntry(testId, TEXT("UTestData"), TEXT("item"), testTypeId);

	FString testDir1 = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests/BinaryDeterministic1"));
	IFileManager::Get().MakeDirectory(*testDir1, true);
	manifest1.SaveToFileBinary(testDir1);

	// Save manifest 2 with identical entries
	FSGDynamicTextAssetCookManifest manifest2;
	manifest2.AddEntry(testId, TEXT("UTestData"), TEXT("item"), testTypeId);

	FString testDir2 = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests/BinaryDeterministic2"));
	IFileManager::Get().MakeDirectory(*testDir2, true);
	manifest2.SaveToFileBinary(testDir2);

	// Load both binary files and compare byte-for-byte
	FString filePath1 = FPaths::Combine(testDir1,
		FSGDynamicTextAssetCookManifestBinaryHeader::BINARY_MANIFEST_FILENAME);
	FString filePath2 = FPaths::Combine(testDir2,
		FSGDynamicTextAssetCookManifestBinaryHeader::BINARY_MANIFEST_FILENAME);

	TArray<uint8> bytes1, bytes2;
	FFileHelper::LoadFileToArray(bytes1, *filePath1);
	FFileHelper::LoadFileToArray(bytes2, *filePath2);

	TestEqual(TEXT("File sizes should be identical"), bytes1.Num(), bytes2.Num());

	if (bytes1.Num() == bytes2.Num() && bytes1.Num() > 0)
	{
		bool bIdentical = FMemory::Memcmp(bytes1.GetData(), bytes2.GetData(), bytes1.Num()) == 0;
		TestTrue(TEXT("Binary files should be byte-identical"), bIdentical);
	}

	// Cleanup
	IFileManager::Get().DeleteDirectory(*testDir1, false, true);
	IFileManager::Get().DeleteDirectory(*testDir2, false, true);

	return true;
}
