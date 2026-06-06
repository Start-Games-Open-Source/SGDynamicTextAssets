// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Management/SGDTAProjectManifest.h"
#include "Management/SGDTAFileManager.h"
#include "Core/SGDTASerializerFormat.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace SGProjectManifestTestUtils
{
	/** Returns a temp directory for project manifest test I/O. */
	FString GetTestDir(const TCHAR* SubDir)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SGDynamicTextAssetTests"), SubDir);
	}

	/** Returns a full manifest file path inside the given temp directory. */
	FString GetTestFilePath(const TCHAR* SubDir)
	{
		FString dir = GetTestDir(SubDir);
		IFileManager::Get().MakeDirectory(*dir, true);
		return FPaths::Combine(dir, TEXT("ProjectManifest.json"));
	}

	/** Cleans up the temp directory used by a test. */
	void Cleanup(const TCHAR* SubDir)
	{
		FString dir = GetTestDir(SubDir);
		IFileManager::Get().DeleteDirectory(*dir, false, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_DefaultState_HasVersionZero,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.DefaultState.HasVersionZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_DefaultState_HasVersionZero::RunTest(const FString& Parameters)
{
	FSGDTAProjectManifest manifest;

	TestEqual(TEXT("Default organizationVersion should be 0"), manifest.GetOrganizationVersion(), 0);
	TestFalse(TEXT("Default manifest should not be loaded"), manifest.IsLoaded());
	TestFalse(TEXT("Default manifest should not be current version"), manifest.IsCurrentVersion());
	TestTrue(TEXT("Default type manifest paths should be empty"), manifest.GetTypeManifestPaths().IsEmpty());
	TestTrue(TEXT("Default extender manifest paths should be empty"), manifest.GetExtenderManifestPaths().IsEmpty());
	TestTrue(TEXT("Default format versions should be empty"), manifest.GetAllFormatVersions().IsEmpty());
	TestTrue(TEXT("Format versions should need scan"), manifest.FormatVersionsNeedScan());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_SaveLoadRoundTrip_PreservesAllFields,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.SaveLoadRoundTrip.PreservesAllFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_SaveLoadRoundTrip_PreservesAllFields::RunTest(const FString& Parameters)
{
	const TCHAR* subDir = TEXT("ProjectManifest_RoundTrip");
	FString filePath = SGProjectManifestTestUtils::GetTestFilePath(subDir);

	// Arrange
	FSGDTAProjectManifest source;
	source.SetOrganizationVersion(FSGDTAProjectManifest::CURRENT_ORGANIZATION_VERSION);

	TArray<FString> typePaths = { TEXT("TypeManifests/Foo_TypeManifest.json"), TEXT("TypeManifests/Bar_TypeManifest.json") };
	source.SetTypeManifestPaths(typePaths);

	TArray<FString> extenderPaths = { TEXT("SerializerExtenders/DTA_AssetBundles_extenders.dta.json") };
	source.SetExtenderManifestPaths(extenderPaths);

	FSGDTASerializerFormat testFormat(1);
	FSGDynamicTextAssetVersion testVersion(2, 0, 0);
	source.RecordFileVersion(testFormat, testVersion);

	// Act
	bool bSaved = source.SaveToFile(filePath);
	TestTrue(TEXT("SaveToFile should succeed"), bSaved);

	FSGDTAProjectManifest loaded;
	bool bLoaded = loaded.LoadFromFile(filePath);
	TestTrue(TEXT("LoadFromFile should succeed"), bLoaded);

	// Assert
	TestEqual(TEXT("Organization version should match"),
		loaded.GetOrganizationVersion(), FSGDTAProjectManifest::CURRENT_ORGANIZATION_VERSION);
	TestTrue(TEXT("Loaded manifest should report IsLoaded"), loaded.IsLoaded());
	TestTrue(TEXT("Loaded manifest should be current version"), loaded.IsCurrentVersion());

	TestEqual(TEXT("Type manifest paths count should match"),
		loaded.GetTypeManifestPaths().Num(), 2);
	TestEqual(TEXT("Type manifest path 0 should match"),
		loaded.GetTypeManifestPaths()[0], TEXT("TypeManifests/Foo_TypeManifest.json"));
	TestEqual(TEXT("Type manifest path 1 should match"),
		loaded.GetTypeManifestPaths()[1], TEXT("TypeManifests/Bar_TypeManifest.json"));

	TestEqual(TEXT("Extender manifest paths count should match"),
		loaded.GetExtenderManifestPaths().Num(), 1);
	TestEqual(TEXT("Extender manifest path 0 should match"),
		loaded.GetExtenderManifestPaths()[0], TEXT("SerializerExtenders/DTA_AssetBundles_extenders.dta.json"));

	const FSGDTASerializerFormatVersionInfo* info = loaded.GetInfoForSerializer(testFormat);
	TestNotNull(TEXT("Format info should exist for test format"), info);
	if (info)
	{
		TestEqual(TEXT("TotalFileCount should be 1"), info->TotalFileCount, 1);
		TestEqual(TEXT("HighestFound should match"), info->HighestFound, testVersion);
		TestEqual(TEXT("LowestFound should match"), info->LowestFound, testVersion);
	}

	SGProjectManifestTestUtils::Cleanup(subDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_LoadFromFile_MissingFile_ReturnsFalse,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.LoadFromFile.MissingFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_LoadFromFile_MissingFile_ReturnsFalse::RunTest(const FString& Parameters)
{
	FSGDTAProjectManifest manifest;
	bool bResult = manifest.LoadFromFile(TEXT("C:/NonExistent/Path/ProjectManifest.json"));

	TestFalse(TEXT("LoadFromFile should return false for missing file"), bResult);
	TestFalse(TEXT("Manifest should not be loaded"), manifest.IsLoaded());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_LoadFromFile_InvalidJson_ReturnsFalse,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.LoadFromFile.InvalidJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_LoadFromFile_InvalidJson_ReturnsFalse::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Failed to parse JSON"), EAutomationExpectedErrorFlags::Contains, 1);

	const TCHAR* subDir = TEXT("ProjectManifest_InvalidJson");
	FString filePath = SGProjectManifestTestUtils::GetTestFilePath(subDir);

	FFileHelper::SaveStringToFile(TEXT("this is not json {{{"), *filePath);

	FSGDTAProjectManifest manifest;
	bool bResult = manifest.LoadFromFile(filePath);

	TestFalse(TEXT("LoadFromFile should return false for invalid JSON"), bResult);
	TestFalse(TEXT("Manifest should not be loaded"), manifest.IsLoaded());

	SGProjectManifestTestUtils::Cleanup(subDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_LoadFromFile_WrongSchema_ReturnsFalse,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.LoadFromFile.WrongSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_LoadFromFile_WrongSchema_ReturnsFalse::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Invalid or missing schema field"), EAutomationExpectedErrorFlags::Contains, 1);

	const TCHAR* subDir = TEXT("ProjectManifest_WrongSchema");
	FString filePath = SGProjectManifestTestUtils::GetTestFilePath(subDir);

	FFileHelper::SaveStringToFile(TEXT("{\"schema\": \"wrong_schema\", \"organizationVersion\": 1}"), *filePath);

	FSGDTAProjectManifest manifest;
	bool bResult = manifest.LoadFromFile(filePath);

	TestFalse(TEXT("LoadFromFile should return false for wrong schema"), bResult);

	SGProjectManifestTestUtils::Cleanup(subDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_VersionDetection_OutdatedNotCurrent,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.VersionDetection.Outdated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_VersionDetection_OutdatedNotCurrent::RunTest(const FString& Parameters)
{
	const TCHAR* subDir = TEXT("ProjectManifest_Outdated");
	FString filePath = SGProjectManifestTestUtils::GetTestFilePath(subDir);

	FSGDTAProjectManifest source;
	source.SetOrganizationVersion(0);
	source.SaveToFile(filePath);

	FSGDTAProjectManifest loaded;
	loaded.LoadFromFile(filePath);

	TestFalse(TEXT("Version 0 should not be current"), loaded.IsCurrentVersion());
	TestEqual(TEXT("Version should be 0"), loaded.GetOrganizationVersion(), 0);

	SGProjectManifestTestUtils::Cleanup(subDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_VersionDetection_CurrentIsCurrent,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.VersionDetection.Current",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_VersionDetection_CurrentIsCurrent::RunTest(const FString& Parameters)
{
	const TCHAR* subDir = TEXT("ProjectManifest_Current");
	FString filePath = SGProjectManifestTestUtils::GetTestFilePath(subDir);

	FSGDTAProjectManifest source;
	source.SetOrganizationVersion(FSGDTAProjectManifest::CURRENT_ORGANIZATION_VERSION);
	source.SaveToFile(filePath);

	FSGDTAProjectManifest loaded;
	loaded.LoadFromFile(filePath);

	TestTrue(TEXT("Current version should be current"), loaded.IsCurrentVersion());

	SGProjectManifestTestUtils::Cleanup(subDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_SaveToFile_CreatesDirectories,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.SaveToFile.CreatesDirectories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_SaveToFile_CreatesDirectories::RunTest(const FString& Parameters)
{
	const TCHAR* subDir = TEXT("ProjectManifest_CreateDirs");
	FString dir = SGProjectManifestTestUtils::GetTestDir(subDir);
	FString filePath = FPaths::Combine(dir, TEXT("nested"), TEXT("deep"), TEXT("ProjectManifest.json"));

	// Ensure the nested directory does not exist yet
	IFileManager::Get().DeleteDirectory(*dir, false, true);

	FSGDTAProjectManifest manifest;
	manifest.SetOrganizationVersion(1);
	bool bSaved = manifest.SaveToFile(filePath);

	TestTrue(TEXT("SaveToFile should succeed"), bSaved);
	TestTrue(TEXT("File should exist after save"), IFileManager::Get().FileExists(*filePath));

	SGProjectManifestTestUtils::Cleanup(subDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_ResolveRelativePath_ProducesAbsolutePath,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.ResolveRelativePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_ResolveRelativePath_ProducesAbsolutePath::RunTest(const FString& Parameters)
{
	FString resolved = FSGDTAProjectManifest::ResolveRelativePath(TEXT("TypeManifests/Foo.json"));
	FString internalRoot = FSGDynamicTextAssetFileManager::GetInternalFilesRootPath();

	TestTrue(TEXT("Resolved path should start with internal files root"),
		resolved.StartsWith(internalRoot));
	TestTrue(TEXT("Resolved path should end with TypeManifests/Foo.json"),
		resolved.EndsWith(TEXT("TypeManifests/Foo.json")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectManifest_RecordFileVersion_TracksData,
	"SGDynamicTextAssets.Editor.Management.ProjectManifest.RecordFileVersion.TracksData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectManifest_RecordFileVersion_TracksData::RunTest(const FString& Parameters)
{
	FSGDTAProjectManifest manifest;
	FSGDTASerializerFormat testFormat(1);

	FSGDynamicTextAssetVersion versionLow(1, 0, 0);
	FSGDynamicTextAssetVersion versionHigh(2, 0, 0);

	manifest.RecordFileVersion(testFormat, versionLow);
	manifest.RecordFileVersion(testFormat, versionHigh);

	const FSGDTASerializerFormatVersionInfo* info = manifest.GetInfoForSerializer(testFormat);
	TestNotNull(TEXT("Format info should exist"), info);

	if (info)
	{
		TestEqual(TEXT("TotalFileCount should be 2"), info->TotalFileCount, 2);
		TestEqual(TEXT("HighestFound should be 2.0.0"), info->HighestFound, versionHigh);
		TestEqual(TEXT("LowestFound should be 1.0.0"), info->LowestFound, versionLow);

		const int32* lowCount = info->CountByVersion.Find(versionLow.ToString());
		TestNotNull(TEXT("CountByVersion should have 1.0.0 entry"), lowCount);
		if (lowCount)
		{
			TestEqual(TEXT("Count for 1.0.0 should be 1"), *lowCount, 1);
		}

		const int32* highCount = info->CountByVersion.Find(versionHigh.ToString());
		TestNotNull(TEXT("CountByVersion should have 2.0.0 entry"), highCount);
		if (highCount)
		{
			TestEqual(TEXT("Count for 2.0.0 should be 1"), *highCount, 1);
		}
	}

	// Invalid format should be ignored
	FSGDTASerializerFormat invalidFormat;
	manifest.RecordFileVersion(invalidFormat, versionLow);
	TestEqual(TEXT("Format versions map should still have 1 entry"),
		manifest.GetAllFormatVersions().Num(), 1);

	return true;
}
