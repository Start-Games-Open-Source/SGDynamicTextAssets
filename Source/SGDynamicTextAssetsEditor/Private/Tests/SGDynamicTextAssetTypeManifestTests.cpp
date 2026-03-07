// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDynamicTextAssetTypeId.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Management/SGDynamicTextAssetTypeManifest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace SGTypeManifestTestUtils
{
	/** Creates a TSoftClassPtr from a class path string. */
	TSoftClassPtr<UObject> MakeSoftClassPtr(const TCHAR* ClassPath)
	{
		return TSoftClassPtr<UObject>(FSoftObjectPath(ClassPath));
	}

	/** Returns a temp directory for manifest test I/O. */
	FString GetTestDir(const TCHAR* SubDir)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests"), SubDir);
	}

	/** Returns a full manifest file path inside the given temp directory. */
	FString GetTestFilePath(const TCHAR* SubDir)
	{
		FString dir = GetTestDir(SubDir);
		IFileManager::Get().MakeDirectory(*dir, true);
		return FPaths::Combine(dir, TEXT("test_type_manifest.json"));
	}

	/** Cleans up the temp directory used by a test. */
	void Cleanup(const TCHAR* SubDir)
	{
		FString dir = GetTestDir(SubDir);
		IFileManager::Get().DeleteDirectory(*dir, false, true);
	}
}

// ---------------------------------------------------------------------------
// 1. Default State
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_DefaultState_IsEmpty,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.DefaultState.IsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_DefaultState_IsEmpty::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	TestEqual(TEXT("Default manifest should have 0 entries"), manifest.Num(), 0);
	TestFalse(TEXT("Default manifest should not be dirty"), manifest.IsDirty());
	TestFalse(TEXT("Default RootTypeId should be invalid"), manifest.GetRootTypeId().IsValid());
	TestTrue(TEXT("GetAllTypes should return empty array"), manifest.GetAllTypes().IsEmpty());

	return true;
}

// ---------------------------------------------------------------------------
// 2. AddType — Increases Count
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_AddType_IncreasesCount,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.AddType.IncreasesCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_AddType_IncreasesCount::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	TSoftClassPtr<UObject> softClass = SGTypeManifestTestUtils::MakeSoftClassPtr(
		TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset"));

	manifest.AddType(typeId, softClass, FSGDynamicTextAssetTypeId());

	TestEqual(TEXT("Manifest should have 1 entry after AddType"), manifest.Num(), 1);
	TestTrue(TEXT("Manifest should be dirty after AddType"), manifest.IsDirty());

	return true;
}

// ---------------------------------------------------------------------------
// 3. AddType — Replaces Existing
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_AddType_ReplacesExisting,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.AddType.ReplacesExisting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_AddType_ReplacesExisting::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	TSoftClassPtr<UObject> originalClass = SGTypeManifestTestUtils::MakeSoftClassPtr(
		TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset"));
	TSoftClassPtr<UObject> replacementClass = SGTypeManifestTestUtils::MakeSoftClassPtr(
		TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAssetUnitTest"));

	manifest.AddType(typeId, originalClass, FSGDynamicTextAssetTypeId());
	manifest.AddType(typeId, replacementClass, FSGDynamicTextAssetTypeId());

	TestEqual(TEXT("Manifest should still have 1 entry after replacing"), manifest.Num(), 1);

	const FSGDynamicTextAssetTypeManifestEntry* entry = manifest.FindByTypeId(typeId);
	TestNotNull(TEXT("Entry should exist"), entry);
	if (entry)
	{
		TestEqual(TEXT("ClassName should be updated to replacement class"),
			entry->ClassName, TEXT("SGDynamicTextAssetUnitTest"));
	}

	return true;
}

// ---------------------------------------------------------------------------
// 4. RemoveType — Decreases Count
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_RemoveType_DecreasesCount,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.RemoveType.DecreasesCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_RemoveType_DecreasesCount::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId typeId1 = FSGDynamicTextAssetTypeId::NewGeneratedId();
	FSGDynamicTextAssetTypeId typeId2 = FSGDynamicTextAssetTypeId::NewGeneratedId();
	TSoftClassPtr<UObject> softClass = SGTypeManifestTestUtils::MakeSoftClassPtr(
		TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset"));

	manifest.AddType(typeId1, softClass, FSGDynamicTextAssetTypeId());
	manifest.AddType(typeId2, softClass, FSGDynamicTextAssetTypeId());
	TestEqual(TEXT("Should have 2 entries"), manifest.Num(), 2);

	bool bRemoved = manifest.RemoveType(typeId1);
	TestTrue(TEXT("RemoveType should return true for existing entry"), bRemoved);
	TestEqual(TEXT("Should have 1 entry after removal"), manifest.Num(), 1);

	return true;
}

// ---------------------------------------------------------------------------
// 5. RemoveType — Not Found Returns False
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_RemoveType_NotFoundReturnsFalse,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.RemoveType.NotFoundReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_RemoveType_NotFoundReturnsFalse::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId unknownTypeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	bool bRemoved = manifest.RemoveType(unknownTypeId);

	TestFalse(TEXT("RemoveType should return false for unknown TypeId"), bRemoved);

	return true;
}

// ---------------------------------------------------------------------------
// 6. FindByTypeId — Found
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_FindByTypeId_Found,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.FindByTypeId.Found",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_FindByTypeId_Found::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	TSoftClassPtr<UObject> softClass = SGTypeManifestTestUtils::MakeSoftClassPtr(
		TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset"));

	manifest.AddType(typeId, softClass, FSGDynamicTextAssetTypeId());

	const FSGDynamicTextAssetTypeManifestEntry* entry = manifest.FindByTypeId(typeId);
	TestNotNull(TEXT("FindByTypeId should find the added entry"), entry);
	if (entry)
	{
		TestEqual(TEXT("Entry ClassName should match"), entry->ClassName, TEXT("SGDynamicTextAsset"));
		TestTrue(TEXT("Entry TypeId should match"), entry->TypeId == typeId);
	}

	return true;
}

// ---------------------------------------------------------------------------
// 7. FindByTypeId — Not Found
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_FindByTypeId_NotFound,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.FindByTypeId.NotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_FindByTypeId_NotFound::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId unknownTypeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	const FSGDynamicTextAssetTypeManifestEntry* entry = manifest.FindByTypeId(unknownTypeId);

	TestNull(TEXT("FindByTypeId should return nullptr for unknown TypeId"), entry);

	return true;
}

// ---------------------------------------------------------------------------
// 8. FindByClassName — Found
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_FindByClassName_Found,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.FindByClassName.Found",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_FindByClassName_Found::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	TSoftClassPtr<UObject> softClass = SGTypeManifestTestUtils::MakeSoftClassPtr(
		TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset"));

	manifest.AddType(typeId, softClass, FSGDynamicTextAssetTypeId());

	const FSGDynamicTextAssetTypeManifestEntry* entry = manifest.FindByClassName(TEXT("SGDynamicTextAsset"));
	TestNotNull(TEXT("FindByClassName should find the added entry"), entry);
	if (entry)
	{
		TestTrue(TEXT("Entry TypeId should match"), entry->TypeId == typeId);
	}

	return true;
}

// ---------------------------------------------------------------------------
// 9. FindByClassName — Not Found
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_FindByClassName_NotFound,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.FindByClassName.NotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_FindByClassName_NotFound::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	const FSGDynamicTextAssetTypeManifestEntry* entry = manifest.FindByClassName(TEXT("UNonExistentClass"));
	TestNull(TEXT("FindByClassName should return nullptr for unknown class"), entry);

	return true;
}

// ---------------------------------------------------------------------------
// 10. Clear — Resets State
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_Clear_ResetsState,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.Clear.ResetsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_Clear_ResetsState::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId rootTypeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	TSoftClassPtr<UObject> softClass = SGTypeManifestTestUtils::MakeSoftClassPtr(
		TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset"));

	manifest.SetRootTypeId(rootTypeId);
	manifest.AddType(typeId, softClass, FSGDynamicTextAssetTypeId());
	TestTrue(TEXT("Manifest should have entries before Clear"), manifest.Num() > 0);

	manifest.Clear();

	TestEqual(TEXT("Manifest should have 0 entries after Clear"), manifest.Num(), 0);
	TestFalse(TEXT("RootTypeId should be invalid after Clear"), manifest.GetRootTypeId().IsValid());
	TestFalse(TEXT("Manifest should not be dirty after Clear"), manifest.IsDirty());

	return true;
}

// ---------------------------------------------------------------------------
// 11. SaveToFile / LoadFromFile — Roundtrip
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_SaveLoadFile_Roundtrip,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.SaveLoadFile.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_SaveLoadFile_Roundtrip::RunTest(const FString& Parameters)
{
	static const TCHAR* SUB_DIR = TEXT("TypeManifestRoundtrip");
	FString filePath = SGTypeManifestTestUtils::GetTestFilePath(SUB_DIR);

	// Build manifest with 3 entries
	FSGDynamicTextAssetTypeManifest saveManifest;

	FSGDynamicTextAssetTypeId rootTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111-BBBB-2222-CCCC-3333DDDD4444"));
	FSGDynamicTextAssetTypeId typeId1 = FSGDynamicTextAssetTypeId::FromString(TEXT("11111111-2222-3333-4444-555566667777"));
	FSGDynamicTextAssetTypeId typeId2 = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAAAAAA-BBBB-CCCC-DDDD-EEEEFFFF0000"));
	FSGDynamicTextAssetTypeId typeId3 = FSGDynamicTextAssetTypeId::FromString(TEXT("12345678-ABCD-EF01-2345-6789ABCDEF01"));

	saveManifest.SetRootTypeId(rootTypeId);
	saveManifest.AddType(typeId1,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset")),
		FSGDynamicTextAssetTypeId());
	saveManifest.AddType(typeId2,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAssetUnitTest")),
		typeId1);
	saveManifest.AddType(typeId3,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/TestModule.TestWeaponData")),
		typeId1);

	bool bSaved = saveManifest.SaveToFile(filePath);
	TestTrue(TEXT("SaveToFile should succeed"), bSaved);

	// Load into a new manifest
	FSGDynamicTextAssetTypeManifest loadManifest;
	bool bLoaded = loadManifest.LoadFromFile(filePath);
	TestTrue(TEXT("LoadFromFile should succeed"), bLoaded);
	TestEqual(TEXT("Loaded manifest should have 3 entries"), loadManifest.Num(), 3);
	TestTrue(TEXT("Loaded RootTypeId should match"), loadManifest.GetRootTypeId() == rootTypeId);

	// Verify each entry
	const FSGDynamicTextAssetTypeManifestEntry* entry1 = loadManifest.FindByTypeId(typeId1);
	TestNotNull(TEXT("Entry 1 should exist"), entry1);
	if (entry1)
	{
		TestEqual(TEXT("Entry 1 ClassName"), entry1->ClassName, TEXT("SGDynamicTextAsset"));
	}

	const FSGDynamicTextAssetTypeManifestEntry* entry2 = loadManifest.FindByTypeId(typeId2);
	TestNotNull(TEXT("Entry 2 should exist"), entry2);
	if (entry2)
	{
		TestEqual(TEXT("Entry 2 ClassName"), entry2->ClassName, TEXT("SGDynamicTextAssetUnitTest"));
		TestTrue(TEXT("Entry 2 ParentTypeId should match"), entry2->ParentTypeId == typeId1);
	}

	const FSGDynamicTextAssetTypeManifestEntry* entry3 = loadManifest.FindByTypeId(typeId3);
	TestNotNull(TEXT("Entry 3 should exist"), entry3);
	if (entry3)
	{
		TestEqual(TEXT("Entry 3 ClassName"), entry3->ClassName, TEXT("TestWeaponData"));
	}

	// Cleanup
	SGTypeManifestTestUtils::Cleanup(SUB_DIR);

	return true;
}

// ---------------------------------------------------------------------------
// 12. SaveToFile — Schema and Version
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_SaveLoadFile_SchemaAndVersion,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.SaveLoadFile.SchemaAndVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_SaveLoadFile_SchemaAndVersion::RunTest(const FString& Parameters)
{
	static const TCHAR* SUB_DIR = TEXT("TypeManifestSchema");
	FString filePath = SGTypeManifestTestUtils::GetTestFilePath(SUB_DIR);

	FSGDynamicTextAssetTypeManifest manifest;
	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	manifest.SetRootTypeId(typeId);
	manifest.AddType(typeId,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset")),
		FSGDynamicTextAssetTypeId());

	manifest.SaveToFile(filePath);

	// Read raw JSON and parse
	FString jsonContents;
	FFileHelper::LoadFileToString(jsonContents, *filePath);

	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonContents);
	FJsonSerializer::Deserialize(reader, jsonObject);

	TestNotNull(TEXT("JSON should parse successfully"), jsonObject.Get());
	if (jsonObject.IsValid())
	{
		FString schema;
		TestTrue(TEXT("Should have schema field"),
			jsonObject->TryGetStringField(FSGDynamicTextAssetTypeManifest::KEY_SCHEMA, schema));
		TestEqual(TEXT("Schema should be dta_type_manifest"),
			schema, FSGDynamicTextAssetTypeManifest::VALUE_SCHEMA);

		double version = 0.0;
		TestTrue(TEXT("Should have version field"),
			jsonObject->TryGetNumberField(FSGDynamicTextAssetTypeManifest::KEY_VERSION, version));
		TestEqual(TEXT("Version should be 1"),
			static_cast<int32>(version), FSGDynamicTextAssetTypeManifest::MANIFEST_VERSION);
	}

	// Cleanup
	SGTypeManifestTestUtils::Cleanup(SUB_DIR);

	return true;
}

// ---------------------------------------------------------------------------
// 13. Server Overlay — Apply and Get Effective
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_ServerOverlay_ApplyAndGetEffective,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.ServerOverlay.ApplyAndGetEffective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_ServerOverlay_ApplyAndGetEffective::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	// Add a local entry
	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111-BBBB-2222-CCCC-3333DDDD4444"));
	manifest.AddType(typeId,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset")),
		FSGDynamicTextAssetTypeId());

	// Build server overlay JSON that overrides this entry
	TSharedPtr<FJsonObject> serverData = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> typesArray;

	TSharedPtr<FJsonObject> overrideEntry = MakeShared<FJsonObject>();
	overrideEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_TYPE_ID, typeId.ToString());
	overrideEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_CLASS_NAME, TEXT("OverriddenClass"));
	overrideEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_PARENT_TYPE_ID,
		TEXT("00000000-0000-0000-0000-000000000000"));
	typesArray.Add(MakeShared<FJsonValueObject>(overrideEntry));
	serverData->SetArrayField(FSGDynamicTextAssetTypeManifest::KEY_TYPES, typesArray);

	manifest.ApplyServerOverrides(serverData);

	TestTrue(TEXT("Should have server overrides"), manifest.HasServerOverrides());

	// GetEffectiveEntry should return the server version
	const FSGDynamicTextAssetTypeManifestEntry* effective = manifest.GetEffectiveEntry(typeId);
	TestNotNull(TEXT("Effective entry should not be nullptr"), effective);
	if (effective)
	{
		TestEqual(TEXT("Effective ClassName should be the server override"),
			effective->ClassName, TEXT("OverriddenClass"));
	}

	// FindByTypeId should return the LOCAL entry (not server overlay)
	const FSGDynamicTextAssetTypeManifestEntry* localEntry = manifest.FindByTypeId(typeId);
	TestNotNull(TEXT("Local entry should still exist"), localEntry);
	if (localEntry)
	{
		TestEqual(TEXT("Local ClassName should be unchanged"),
			localEntry->ClassName, TEXT("SGDynamicTextAsset"));
	}

	return true;
}

// ---------------------------------------------------------------------------
// 14. Server Overlay — Disable Entry
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_ServerOverlay_DisableEntry,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.ServerOverlay.DisableEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_ServerOverlay_DisableEntry::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111-BBBB-2222-CCCC-3333DDDD4444"));
	manifest.AddType(typeId,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset")),
		FSGDynamicTextAssetTypeId());

	// Server overlay with empty className disables the type
	TSharedPtr<FJsonObject> serverData = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> typesArray;

	TSharedPtr<FJsonObject> disableEntry = MakeShared<FJsonObject>();
	disableEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_TYPE_ID, typeId.ToString());
	disableEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_CLASS_NAME, TEXT(""));
	typesArray.Add(MakeShared<FJsonValueObject>(disableEntry));
	serverData->SetArrayField(FSGDynamicTextAssetTypeManifest::KEY_TYPES, typesArray);

	manifest.ApplyServerOverrides(serverData);

	// GetEffectiveEntry should return nullptr for disabled type
	const FSGDynamicTextAssetTypeManifestEntry* effective = manifest.GetEffectiveEntry(typeId);
	TestNull(TEXT("Effective entry should be nullptr for disabled type"), effective);

	return true;
}

// ---------------------------------------------------------------------------
// 15. Server Overlay — Add New Entry
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_ServerOverlay_AddNewEntry,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.ServerOverlay.AddNewEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_ServerOverlay_AddNewEntry::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	// Add one local entry
	FSGDynamicTextAssetTypeId localTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111-BBBB-2222-CCCC-3333DDDD4444"));
	manifest.AddType(localTypeId,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset")),
		FSGDynamicTextAssetTypeId());

	// Server overlay adds a brand-new entry not in local manifest
	FSGDynamicTextAssetTypeId serverOnlyTypeId = FSGDynamicTextAssetTypeId::FromString(TEXT("FFFF9999-EEEE-8888-DDDD-7777CCCC6666"));

	TSharedPtr<FJsonObject> serverData = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> typesArray;

	TSharedPtr<FJsonObject> newEntry = MakeShared<FJsonObject>();
	newEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_TYPE_ID, serverOnlyTypeId.ToString());
	newEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_CLASS_NAME, TEXT("ServerOnlyClass"));
	newEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_PARENT_TYPE_ID,
		TEXT("00000000-0000-0000-0000-000000000000"));
	typesArray.Add(MakeShared<FJsonValueObject>(newEntry));
	serverData->SetArrayField(FSGDynamicTextAssetTypeManifest::KEY_TYPES, typesArray);

	manifest.ApplyServerOverrides(serverData);

	// GetAllEffectiveTypes should include both local + server entries
	TArray<FSGDynamicTextAssetTypeManifestEntry> effectiveEntries;
	manifest.GetAllEffectiveTypes(effectiveEntries);
	TestEqual(TEXT("Should have 2 effective entries (1 local + 1 server)"), effectiveEntries.Num(), 2);

	// GetEffectiveEntry should find the server-only entry
	const FSGDynamicTextAssetTypeManifestEntry* effective = manifest.GetEffectiveEntry(serverOnlyTypeId);
	TestNotNull(TEXT("Server-only entry should be accessible via GetEffectiveEntry"), effective);
	if (effective)
	{
		TestEqual(TEXT("Server-only entry ClassName should match"), effective->ClassName, TEXT("ServerOnlyClass"));
	}

	// Local Num() should still be 1
	TestEqual(TEXT("Local entry count should not change"), manifest.Num(), 1);

	return true;
}

// ---------------------------------------------------------------------------
// 16. Server Overlay — Clear Restores Local
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_ServerOverlay_ClearRestoresLocal,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.ServerOverlay.ClearRestoresLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_ServerOverlay_ClearRestoresLocal::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;

	FSGDynamicTextAssetTypeId typeId = FSGDynamicTextAssetTypeId::FromString(TEXT("AAAA1111-BBBB-2222-CCCC-3333DDDD4444"));
	manifest.AddType(typeId,
		SGTypeManifestTestUtils::MakeSoftClassPtr(TEXT("/Script/SGDynamicTextAssetsRuntime.SGDynamicTextAsset")),
		FSGDynamicTextAssetTypeId());

	// Apply server overlay that overrides the entry
	TSharedPtr<FJsonObject> serverData = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> typesArray;

	TSharedPtr<FJsonObject> overrideEntry = MakeShared<FJsonObject>();
	overrideEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_TYPE_ID, typeId.ToString());
	overrideEntry->SetStringField(FSGDynamicTextAssetTypeManifest::KEY_CLASS_NAME, TEXT("OverriddenClass"));
	typesArray.Add(MakeShared<FJsonValueObject>(overrideEntry));
	serverData->SetArrayField(FSGDynamicTextAssetTypeManifest::KEY_TYPES, typesArray);

	manifest.ApplyServerOverrides(serverData);

	// Verify overlay is active
	TestTrue(TEXT("Should have server overrides before clear"), manifest.HasServerOverrides());
	const FSGDynamicTextAssetTypeManifestEntry* overridden = manifest.GetEffectiveEntry(typeId);
	TestNotNull(TEXT("Overridden entry should exist"), overridden);
	if (overridden)
	{
		TestEqual(TEXT("Should see server override"), overridden->ClassName, TEXT("OverriddenClass"));
	}

	// Clear server overrides
	manifest.ClearServerOverrides();

	TestFalse(TEXT("Should not have server overrides after clear"), manifest.HasServerOverrides());

	// GetEffectiveEntry should now return the local entry
	const FSGDynamicTextAssetTypeManifestEntry* restored = manifest.GetEffectiveEntry(typeId);
	TestNotNull(TEXT("Local entry should be restored after clear"), restored);
	if (restored)
	{
		TestEqual(TEXT("Should see local ClassName after clear"),
			restored->ClassName, TEXT("SGDynamicTextAsset"));
	}

	return true;
}

// ---------------------------------------------------------------------------
// 17. SetRootTypeId — Marks Dirty
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTypeManifest_SetRootTypeId_MarksDirty,
	"SGDynamicTextAssets.Runtime.Management.TypeManifest.SetRootTypeId.MarksDirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTypeManifest_SetRootTypeId_MarksDirty::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetTypeManifest manifest;
	TestFalse(TEXT("Manifest should not be dirty initially"), manifest.IsDirty());

	FSGDynamicTextAssetTypeId rootTypeId = FSGDynamicTextAssetTypeId::NewGeneratedId();
	manifest.SetRootTypeId(rootTypeId);

	TestTrue(TEXT("Manifest should be dirty after SetRootTypeId"), manifest.IsDirty());
	TestTrue(TEXT("GetRootTypeId should match the set value"), manifest.GetRootTypeId() == rootTypeId);

	return true;
}
