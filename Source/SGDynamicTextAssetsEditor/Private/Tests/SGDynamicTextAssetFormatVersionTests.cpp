// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Commandlets/SGDynamicTextAssetFormatVersionCommandlet.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "Serialization/SGDynamicTextAssetXmlSerializer.h"
#include "Serialization/SGDynamicTextAssetYamlSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// Helper utilities for building format-version test content.
namespace SGFormatVersionTestUtils
{
	/** Builds valid JSON with a specific fileFormatVersion. */
	FString BuildJsonWithVersion(const FString& Version)
	{
		return FString::Printf(
			TEXT("{ \"%s\": { \"%s\": \"USGDynamicTextAsset\", \"%s\": \"A1B2C3D4-E5F67890-ABCDEF12-34567890\", \"%s\": \"1.0.0\", \"%s\": \"%s\" }, \"%s\": {} }"),
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
			*ISGDynamicTextAssetSerializer::KEY_TYPE,
			*ISGDynamicTextAssetSerializer::KEY_ID,
			*ISGDynamicTextAssetSerializer::KEY_VERSION,
			*ISGDynamicTextAssetSerializer::KEY_FILE_FORMAT_VERSION, *Version,
			*ISGDynamicTextAssetSerializer::KEY_DATA);
	}

	/** Builds valid JSON without a fileFormatVersion field. */
	FString BuildJsonWithoutVersion()
	{
		return FString::Printf(
			TEXT("{ \"%s\": { \"%s\": \"USGDynamicTextAsset\", \"%s\": \"A1B2C3D4-E5F67890-ABCDEF12-34567890\", \"%s\": \"1.0.0\" }, \"%s\": {} }"),
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
			*ISGDynamicTextAssetSerializer::KEY_TYPE,
			*ISGDynamicTextAssetSerializer::KEY_ID,
			*ISGDynamicTextAssetSerializer::KEY_VERSION,
			*ISGDynamicTextAssetSerializer::KEY_DATA);
	}

	/** Builds valid XML with a specific fileFormatVersion. */
	FString BuildXmlWithVersion(const FString& Version)
	{
		return FString::Printf(
			TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<DynamicTextAsset>\n    <%s>\n        <%s>USGDynamicTextAsset</%s>\n        <%s>1.0.0</%s>\n        <%s>A1B2C3D4-E5F67890-ABCDEF12-34567890</%s>\n        <%s>%s</%s>\n    </%s>\n    <data/>\n</DynamicTextAsset>"),
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
			*ISGDynamicTextAssetSerializer::KEY_TYPE, *ISGDynamicTextAssetSerializer::KEY_TYPE,
			*ISGDynamicTextAssetSerializer::KEY_VERSION, *ISGDynamicTextAssetSerializer::KEY_VERSION,
			*ISGDynamicTextAssetSerializer::KEY_ID, *ISGDynamicTextAssetSerializer::KEY_ID,
			*ISGDynamicTextAssetSerializer::KEY_FILE_FORMAT_VERSION, *Version, *ISGDynamicTextAssetSerializer::KEY_FILE_FORMAT_VERSION,
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION);
	}

	/** Builds valid XML without a fileFormatVersion element. */
	FString BuildXmlWithoutVersion()
	{
		return FString::Printf(
			TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<DynamicTextAsset>\n    <%s>\n        <%s>USGDynamicTextAsset</%s>\n        <%s>1.0.0</%s>\n        <%s>A1B2C3D4-E5F67890-ABCDEF12-34567890</%s>\n    </%s>\n    <data/>\n</DynamicTextAsset>"),
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
			*ISGDynamicTextAssetSerializer::KEY_TYPE, *ISGDynamicTextAssetSerializer::KEY_TYPE,
			*ISGDynamicTextAssetSerializer::KEY_VERSION, *ISGDynamicTextAssetSerializer::KEY_VERSION,
			*ISGDynamicTextAssetSerializer::KEY_ID, *ISGDynamicTextAssetSerializer::KEY_ID,
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION);
	}

	/** Builds valid YAML with a specific fileFormatVersion. */
	FString BuildYamlWithVersion(const FString& Version)
	{
		return FString::Printf(
			TEXT("%s:\n  %s: USGDynamicTextAsset\n  %s: 1.0.0\n  %s: A1B2C3D4-E5F67890-ABCDEF12-34567890\n  %s: %s\n%s: {}\n"),
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
			*ISGDynamicTextAssetSerializer::KEY_TYPE,
			*ISGDynamicTextAssetSerializer::KEY_VERSION,
			*ISGDynamicTextAssetSerializer::KEY_ID,
			*ISGDynamicTextAssetSerializer::KEY_FILE_FORMAT_VERSION, *Version,
			*ISGDynamicTextAssetSerializer::KEY_DATA);
	}

	/** Builds valid YAML without a fileFormatVersion field. */
	FString BuildYamlWithoutVersion()
	{
		return FString::Printf(
			TEXT("%s:\n  %s: USGDynamicTextAsset\n  %s: 1.0.0\n  %s: A1B2C3D4-E5F67890-ABCDEF12-34567890\n%s: {}\n"),
			*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
			*ISGDynamicTextAssetSerializer::KEY_TYPE,
			*ISGDynamicTextAssetSerializer::KEY_VERSION,
			*ISGDynamicTextAssetSerializer::KEY_ID,
			*ISGDynamicTextAssetSerializer::KEY_DATA);
	}

	/** Returns the temp directory used for test files, creating it if needed. */
	FString GetTestTempDir()
	{
		const FString tempDir = FPaths::ProjectSavedDir() / TEXT("SGDynamicTextAssets") / TEXT("TestTemp");
		IFileManager::Get().MakeDirectory(*tempDir, true);
		return tempDir;
	}

	/** Cleans up the test temp directory. */
	void CleanupTestTempDir()
	{
		const FString tempDir = FPaths::ProjectSavedDir() / TEXT("SGDynamicTextAssets") / TEXT("TestTemp");
		IFileManager::Get().DeleteDirectory(*tempDir, false, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Json_ReadFormatVersion,
	"SGDynamicTextAssets.Editor.FormatVersion.Json.ReadFormatVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Json_ReadFormatVersion::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	FSGDynamicTextAssetFileInfo fileInfo;

	const FString json = SGFormatVersionTestUtils::BuildJsonWithVersion(TEXT("2.3.1"));
	const bool bResult = serializer.ExtractFileInfo(json, fileInfo);

	TestTrue(TEXT("ExtractFileInfo should succeed"), bResult);
	TestTrue(TEXT("File info should be valid"), fileInfo.bIsValid);
	TestEqual(TEXT("Major should be 2"), fileInfo.FileFormatVersion.Major, static_cast<uint32>(2));
	TestEqual(TEXT("Minor should be 3"), fileInfo.FileFormatVersion.Minor, static_cast<uint32>(3));
	TestEqual(TEXT("Patch should be 1"), fileInfo.FileFormatVersion.Patch, static_cast<uint32>(1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Json_MissingVersionDefaultsTo1_0_0,
	"SGDynamicTextAssets.Editor.FormatVersion.Json.MissingVersionDefaultsTo1_0_0",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Json_MissingVersionDefaultsTo1_0_0::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	FSGDynamicTextAssetFileInfo fileInfo;

	const FString json = SGFormatVersionTestUtils::BuildJsonWithoutVersion();
	const bool bResult = serializer.ExtractFileInfo(json, fileInfo);

	TestTrue(TEXT("ExtractFileInfo should succeed"), bResult);
	TestTrue(TEXT("File info should be valid"), fileInfo.bIsValid);
	TestEqual(TEXT("Default Major should be 1"), fileInfo.FileFormatVersion.Major, static_cast<uint32>(1));
	TestEqual(TEXT("Default Minor should be 0"), fileInfo.FileFormatVersion.Minor, static_cast<uint32>(0));
	TestEqual(TEXT("Default Patch should be 0"), fileInfo.FileFormatVersion.Patch, static_cast<uint32>(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Xml_ReadFormatVersion,
	"SGDynamicTextAssets.Editor.FormatVersion.Xml.ReadFormatVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Xml_ReadFormatVersion::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetXmlSerializer serializer;
	FSGDynamicTextAssetFileInfo fileInfo;

	const FString xml = SGFormatVersionTestUtils::BuildXmlWithVersion(TEXT("3.1.0"));
	const bool bResult = serializer.ExtractFileInfo(xml, fileInfo);

	TestTrue(TEXT("ExtractFileInfo should succeed"), bResult);
	TestTrue(TEXT("File info should be valid"), fileInfo.bIsValid);
	TestEqual(TEXT("Major should be 3"), fileInfo.FileFormatVersion.Major, static_cast<uint32>(3));
	TestEqual(TEXT("Minor should be 1"), fileInfo.FileFormatVersion.Minor, static_cast<uint32>(1));
	TestEqual(TEXT("Patch should be 0"), fileInfo.FileFormatVersion.Patch, static_cast<uint32>(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Xml_MissingVersionDefaultsTo1_0_0,
	"SGDynamicTextAssets.Editor.FormatVersion.Xml.MissingVersionDefaultsTo1_0_0",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Xml_MissingVersionDefaultsTo1_0_0::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetXmlSerializer serializer;
	FSGDynamicTextAssetFileInfo fileInfo;

	const FString xml = SGFormatVersionTestUtils::BuildXmlWithoutVersion();
	const bool bResult = serializer.ExtractFileInfo(xml, fileInfo);

	TestTrue(TEXT("ExtractFileInfo should succeed"), bResult);
	TestTrue(TEXT("File info should be valid"), fileInfo.bIsValid);
	TestEqual(TEXT("Default Major should be 1"), fileInfo.FileFormatVersion.Major, static_cast<uint32>(1));
	TestEqual(TEXT("Default Minor should be 0"), fileInfo.FileFormatVersion.Minor, static_cast<uint32>(0));
	TestEqual(TEXT("Default Patch should be 0"), fileInfo.FileFormatVersion.Patch, static_cast<uint32>(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Yaml_ReadFormatVersion,
	"SGDynamicTextAssets.Editor.FormatVersion.Yaml.ReadFormatVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Yaml_ReadFormatVersion::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetYamlSerializer serializer;
	FSGDynamicTextAssetFileInfo fileInfo;

	const FString yaml = SGFormatVersionTestUtils::BuildYamlWithVersion(TEXT("1.5.2"));
	const bool bResult = serializer.ExtractFileInfo(yaml, fileInfo);

	TestTrue(TEXT("ExtractFileInfo should succeed"), bResult);
	TestTrue(TEXT("File info should be valid"), fileInfo.bIsValid);
	TestEqual(TEXT("Major should be 1"), fileInfo.FileFormatVersion.Major, static_cast<uint32>(1));
	TestEqual(TEXT("Minor should be 5"), fileInfo.FileFormatVersion.Minor, static_cast<uint32>(5));
	TestEqual(TEXT("Patch should be 2"), fileInfo.FileFormatVersion.Patch, static_cast<uint32>(2));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Yaml_MissingVersionDefaultsTo1_0_0,
	"SGDynamicTextAssets.Editor.FormatVersion.Yaml.MissingVersionDefaultsTo1_0_0",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Yaml_MissingVersionDefaultsTo1_0_0::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetYamlSerializer serializer;
	FSGDynamicTextAssetFileInfo fileInfo;

	const FString yaml = SGFormatVersionTestUtils::BuildYamlWithoutVersion();
	const bool bResult = serializer.ExtractFileInfo(yaml, fileInfo);

	TestTrue(TEXT("ExtractFileInfo should succeed"), bResult);
	TestTrue(TEXT("File info should be valid"), fileInfo.bIsValid);
	TestEqual(TEXT("Default Major should be 1"), fileInfo.FileFormatVersion.Major, static_cast<uint32>(1));
	TestEqual(TEXT("Default Minor should be 0"), fileInfo.FileFormatVersion.Minor, static_cast<uint32>(0));
	TestEqual(TEXT("Default Patch should be 0"), fileInfo.FileFormatVersion.Patch, static_cast<uint32>(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Json_UpdateFileFormatVersion_UpdatesCorrectly,
	"SGDynamicTextAssets.Editor.FormatVersion.Json.UpdateFileFormatVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Json_UpdateFileFormatVersion_UpdatesCorrectly::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	FString content = SGFormatVersionTestUtils::BuildJsonWithVersion(TEXT("1.0.0"));

	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.UpdateFileFormatVersion(content, newVersion);

	TestTrue(TEXT("UpdateFileFormatVersion should succeed"), bResult);
	TestTrue(TEXT("Content should contain new version"), content.Contains(TEXT("\"fileFormatVersion\": \"2.0.0\"")));
	TestFalse(TEXT("Content should not contain old version in format field"),
		content.Contains(TEXT("\"fileFormatVersion\": \"1.0.0\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Json_UpdateFileFormatVersion_MissingFieldReturnsFalse,
	"SGDynamicTextAssets.Editor.FormatVersion.Json.UpdateFileFormatVersionMissingField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Json_UpdateFileFormatVersion_MissingFieldReturnsFalse::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	FString content = SGFormatVersionTestUtils::BuildJsonWithoutVersion();

	AddExpectedMessage(TEXT("UpdateFileFormatVersion"), EAutomationExpectedMessageFlags::Contains, 1);

	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.UpdateFileFormatVersion(content, newVersion);

	TestFalse(TEXT("UpdateFileFormatVersion should fail when field is missing"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Xml_UpdateFileFormatVersion_UpdatesCorrectly,
	"SGDynamicTextAssets.Editor.FormatVersion.Xml.UpdateFileFormatVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Xml_UpdateFileFormatVersion_UpdatesCorrectly::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetXmlSerializer serializer;
	FString content = SGFormatVersionTestUtils::BuildXmlWithVersion(TEXT("1.0.0"));

	const FSGDynamicTextAssetVersion newVersion(3, 1, 0);
	const bool bResult = serializer.UpdateFileFormatVersion(content, newVersion);

	TestTrue(TEXT("UpdateFileFormatVersion should succeed"), bResult);
	TestTrue(TEXT("Content should contain new version"),
		content.Contains(TEXT("<fileFormatVersion>3.1.0</fileFormatVersion>")));
	TestFalse(TEXT("Content should not contain old version in format element"),
		content.Contains(TEXT("<fileFormatVersion>1.0.0</fileFormatVersion>")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Xml_UpdateFileFormatVersion_MissingFieldReturnsFalse,
	"SGDynamicTextAssets.Editor.FormatVersion.Xml.UpdateFileFormatVersionMissingField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Xml_UpdateFileFormatVersion_MissingFieldReturnsFalse::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetXmlSerializer serializer;
	FString content = SGFormatVersionTestUtils::BuildXmlWithoutVersion();

	AddExpectedMessage(TEXT("UpdateFileFormatVersion"), EAutomationExpectedMessageFlags::Contains, 1);

	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.UpdateFileFormatVersion(content, newVersion);

	TestFalse(TEXT("UpdateFileFormatVersion should fail when element is missing"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Yaml_UpdateFileFormatVersion_UpdatesCorrectly,
	"SGDynamicTextAssets.Editor.FormatVersion.Yaml.UpdateFileFormatVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Yaml_UpdateFileFormatVersion_UpdatesCorrectly::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetYamlSerializer serializer;
	FString content = SGFormatVersionTestUtils::BuildYamlWithVersion(TEXT("1.0.0"));

	const FSGDynamicTextAssetVersion newVersion(2, 5, 0);
	const bool bResult = serializer.UpdateFileFormatVersion(content, newVersion);

	TestTrue(TEXT("UpdateFileFormatVersion should succeed"), bResult);
	TestTrue(TEXT("Content should contain new version"), content.Contains(TEXT("fileFormatVersion: 2.5.0")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Yaml_UpdateFileFormatVersion_MissingFieldReturnsFalse,
	"SGDynamicTextAssets.Editor.FormatVersion.Yaml.UpdateFileFormatVersionMissingField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Yaml_UpdateFileFormatVersion_MissingFieldReturnsFalse::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetYamlSerializer serializer;
	FString content = SGFormatVersionTestUtils::BuildYamlWithoutVersion();

	AddExpectedMessage(TEXT("UpdateFileFormatVersion"), EAutomationExpectedMessageFlags::Contains, 1);

	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.UpdateFileFormatVersion(content, newVersion);

	TestFalse(TEXT("UpdateFileFormatVersion should fail when field is missing"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_MigrateFileFormat_SameVersionReturnsTrue,
	"SGDynamicTextAssets.Editor.FormatVersion.MigrateFileFormat.SameVersionReturnsTrue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_MigrateFileFormat_SameVersionReturnsTrue::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	const FString original = SGFormatVersionTestUtils::BuildJsonWithVersion(TEXT("1.0.0"));
	FString content = original;

	const FSGDynamicTextAssetVersion version(1, 0, 0);
	const bool bResult = serializer.MigrateFileFormat(content, version, version);

	TestTrue(TEXT("MigrateFileFormat should return true for same version"), bResult);
	TestEqual(TEXT("Content should be unchanged"), content, original);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_MigrateFileFormat_DifferentVersionReturnsTrue,
	"SGDynamicTextAssets.Editor.FormatVersion.MigrateFileFormat.DifferentVersionReturnsTrue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_MigrateFileFormat_DifferentVersionReturnsTrue::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	const FString original = SGFormatVersionTestUtils::BuildJsonWithVersion(TEXT("1.0.0"));
	FString content = original;

	const FSGDynamicTextAssetVersion oldVersion(1, 0, 0);
	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.MigrateFileFormat(content, oldVersion, newVersion);

	TestTrue(TEXT("MigrateFileFormat should return true (no structural changes)"), bResult);
	TestEqual(TEXT("Content should be unchanged (no structural migration)"), content, original);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Commandlet_MigrateSingleFile_UpdatesVersion,
	"SGDynamicTextAssets.Editor.FormatVersion.Commandlet.MigrateSingleFileUpdatesVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Commandlet_MigrateSingleFile_UpdatesVersion::RunTest(const FString& Parameters)
{
	// Create a temp JSON file with an old format version
	const FString tempDir = SGFormatVersionTestUtils::GetTestTempDir();
	const FString tempFile = tempDir / TEXT("MigrateTest.dta.json");
	const FString content = SGFormatVersionTestUtils::BuildJsonWithVersion(TEXT("0.5.0"));

	FSGDynamicTextAssetFileManager::WriteRawFileContents(tempFile, content);

	// Run migration
	FString error;
	const bool bResult = USGDynamicTextAssetFormatVersionCommandlet::MigrateSingleFile(tempFile, error);

	TestTrue(TEXT("MigrateSingleFile should succeed"), bResult);
	TestTrue(TEXT("Error should be empty on success"), error.IsEmpty());

	// Read back and verify version was updated
	FString updatedContent;
	FSGDynamicTextAssetFileManager::ReadRawFileContents(tempFile, updatedContent);

	FSGDynamicTextAssetJsonSerializer serializer;
	FSGDynamicTextAssetFileInfo fileInfo;
	serializer.ExtractFileInfo(updatedContent, fileInfo);

	const FSGDynamicTextAssetVersion expectedVersion = serializer.GetFileFormatVersion();
	TestEqual(TEXT("File format version should match serializer's current version"),
		fileInfo.FileFormatVersion.ToString(), expectedVersion.ToString());

	// Cleanup
	SGFormatVersionTestUtils::CleanupTestTempDir();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_Commandlet_MigrateSingleFile_UpToDateReturnsTrue,
	"SGDynamicTextAssets.Editor.FormatVersion.Commandlet.MigrateSingleFileUpToDate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_Commandlet_MigrateSingleFile_UpToDateReturnsTrue::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;

	// Create a temp JSON file already at the current version
	const FString tempDir = SGFormatVersionTestUtils::GetTestTempDir();
	const FString tempFile = tempDir / TEXT("UpToDateTest.dta.json");
	const FString content = SGFormatVersionTestUtils::BuildJsonWithVersion(serializer.GetFileFormatVersion().ToString());

	FSGDynamicTextAssetFileManager::WriteRawFileContents(tempFile, content);

	FString error;
	const bool bResult = USGDynamicTextAssetFormatVersionCommandlet::MigrateSingleFile(tempFile, error);

	TestTrue(TEXT("MigrateSingleFile should succeed for up-to-date file"), bResult);
	TestTrue(TEXT("Error should be empty"), error.IsEmpty());

	// Cleanup
	SGFormatVersionTestUtils::CleanupTestTempDir();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_MigrateFileFormat_Json_RenamesMetadataKey,
	"SGDynamicTextAssets.Editor.FormatVersion.MigrateFileFormat.Json.RenamesMetadataKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_MigrateFileFormat_Json_RenamesMetadataKey::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	FString content = TEXT("{ \"metadata\": { \"type\": \"Test\", \"id\": \"ABC\", \"version\": \"1.0.0\" }, \"data\": {} }");

	const FSGDynamicTextAssetVersion oldVersion(1, 0, 0);
	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.MigrateFileFormat(content, oldVersion, newVersion);

	TestTrue(TEXT("MigrateFileFormat should succeed"), bResult);
	TestTrue(TEXT("Content should contain new key"), content.Contains(TEXT("\"sgFileInformation\"")));
	TestFalse(TEXT("Content should not contain old key"), content.Contains(TEXT("\"metadata\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_MigrateFileFormat_Xml_RenamesMetadataTag,
	"SGDynamicTextAssets.Editor.FormatVersion.MigrateFileFormat.Xml.RenamesMetadataTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_MigrateFileFormat_Xml_RenamesMetadataTag::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetXmlSerializer serializer;
	FString content = TEXT("<?xml version=\"1.0\"?>\n<DynamicTextAsset>\n    <metadata>\n        <type>Test</type>\n    </metadata>\n    <data/>\n</DynamicTextAsset>");

	const FSGDynamicTextAssetVersion oldVersion(1, 0, 0);
	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.MigrateFileFormat(content, oldVersion, newVersion);

	TestTrue(TEXT("MigrateFileFormat should succeed"), bResult);
	TestTrue(TEXT("Content should contain new opening tag"), content.Contains(TEXT("<sgFileInformation>")));
	TestTrue(TEXT("Content should contain new closing tag"), content.Contains(TEXT("</sgFileInformation>")));
	TestFalse(TEXT("Content should not contain old opening tag"), content.Contains(TEXT("<metadata>")));
	TestFalse(TEXT("Content should not contain old closing tag"), content.Contains(TEXT("</metadata>")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_MigrateFileFormat_Yaml_RenamesMetadataKey,
	"SGDynamicTextAssets.Editor.FormatVersion.MigrateFileFormat.Yaml.RenamesMetadataKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_MigrateFileFormat_Yaml_RenamesMetadataKey::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetYamlSerializer serializer;
	FString content = TEXT("metadata:\n  type: Test\n  version: 1.0.0\n  id: ABC\ndata: {}\n");

	const FSGDynamicTextAssetVersion oldVersion(1, 0, 0);
	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.MigrateFileFormat(content, oldVersion, newVersion);

	TestTrue(TEXT("MigrateFileFormat should succeed"), bResult);
	TestTrue(TEXT("Content should contain new key"), content.Contains(TEXT("sgFileInformation:")));
	TestFalse(TEXT("Content should not contain old key at root level"), content.StartsWith(TEXT("metadata:")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormatVersion_UpdateFileFormatVersion_InsertsWhenMissing,
	"SGDynamicTextAssets.Editor.FormatVersion.UpdateFileFormatVersion.InsertsWhenMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFormatVersion_UpdateFileFormatVersion_InsertsWhenMissing::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetJsonSerializer serializer;
	// JSON content without fileFormatVersion field
	FString content = FString::Printf(
		TEXT("{ \"%s\": { \"type\": \"Test\", \"id\": \"ABC\", \"version\": \"1.0.0\" }, \"data\": {} }"),
		*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION);

	const FSGDynamicTextAssetVersion newVersion(2, 0, 0);
	const bool bResult = serializer.UpdateFileFormatVersion(content, newVersion);

	TestTrue(TEXT("UpdateFileFormatVersion should succeed by inserting"), bResult);
	TestTrue(TEXT("Content should now contain fileFormatVersion"),
		content.Contains(TEXT("\"fileFormatVersion\": \"2.0.0\"")));

	return true;
}
