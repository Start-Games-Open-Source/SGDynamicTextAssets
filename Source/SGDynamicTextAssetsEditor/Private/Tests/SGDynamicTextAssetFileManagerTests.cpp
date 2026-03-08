// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Serialization/SGDynamicTextAssetBinarySerializer.h"
#include "Core/SGDynamicTextAsset.h"
#include "Serialization/SGBinaryEncodeParams.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_SanitizeUserFacingId_SpacesToUnderscores,
	"SGDynamicTextAssets.Runtime.Management.FileManager.SanitizeUserFacingId.SpacesToUnderscores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_SanitizeUserFacingId_SpacesToUnderscores::RunTest(const FString& Parameters)
{
	FString input = TEXT("My Sword Item");
	FString result = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(input);

	TestEqual(TEXT("Spaces should be replaced with underscores"), result, TEXT("My_Sword_Item"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_SanitizeUserFacingId_InvalidChars,
	"SGDynamicTextAssets.Runtime.Management.FileManager.SanitizeUserFacingId.InvalidChars",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_SanitizeUserFacingId_InvalidChars::RunTest(const FString& Parameters)
{
	// Test all invalid filename characters: / : * ? " < > | \

	FString input = TEXT("Test/:*?\"<>|\\End");
	FString result = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(input);

	TestEqual(TEXT("All invalid chars should become underscores"), result, TEXT("Test_________End"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_SanitizeUserFacingId_CleanString,
	"SGDynamicTextAssets.Runtime.Management.FileManager.SanitizeUserFacingId.CleanString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_SanitizeUserFacingId_CleanString::RunTest(const FString& Parameters)
{
	FString input = TEXT("ValidIdentifier_123");
	FString result = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(input);

	TestEqual(TEXT("Clean string should pass through unchanged"), result, input);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ExtractUserFacingIdFromPath_JsonExtension,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ExtractUserFacingIdFromPath.JsonExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ExtractUserFacingIdFromPath_JsonExtension::RunTest(const FString& Parameters)
{
	FString path = TEXT("excalibur.dta.json");
	FString result = FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(path);

	TestEqual(TEXT("Should strip .dta.json extension"), result, TEXT("excalibur"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ExtractUserFacingIdFromPath_BinaryExtension,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ExtractUserFacingIdFromPath.BinaryExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ExtractUserFacingIdFromPath_BinaryExtension::RunTest(const FString& Parameters)
{
	FString path = TEXT("longbow.dta.bin");
	FString result = FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(path);

	TestEqual(TEXT("Should strip .dta.bin extension"), result, TEXT("longbow"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ExtractUserFacingIdFromPath_WithDirectories,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ExtractUserFacingIdFromPath.WithDirectories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ExtractUserFacingIdFromPath_WithDirectories::RunTest(const FString& Parameters)
{
	FString path = TEXT("C:/Project/Content/SGDynamicTextAssets/WeaponData/sword_of_power.dta.json");
	FString result = FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(path);

	TestEqual(TEXT("Should extract filename from path with directories"), result, TEXT("sword_of_power"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_IsDynamicTextAssetFile_ValidExtensions,
	"SGDynamicTextAssets.Runtime.Management.FileManager.IsDynamicTextAssetFile.ValidExtensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_IsDynamicTextAssetFile_ValidExtensions::RunTest(const FString& Parameters)
{
	TestTrue(TEXT(".dta.json should be valid"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("test.dta.json")));
	TestTrue(TEXT(".dta.bin should be valid"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("test.dta.bin")));
	TestTrue(TEXT("Full path with .dta.json should be valid"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("C:/Content/test.dta.json")));
	TestTrue(TEXT("Full path with .dta.bin should be valid"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("C:/Content/test.dta.bin")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_IsDynamicTextAssetFile_InvalidExtensions,
	"SGDynamicTextAssets.Runtime.Management.FileManager.IsDynamicTextAssetFile.InvalidExtensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_IsDynamicTextAssetFile_InvalidExtensions::RunTest(const FString& Parameters)
{
	TestFalse(TEXT(".json should be invalid (not .dta.json)"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("test.json")));
	TestFalse(TEXT(".bin should be invalid (not .dta.bin)"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("test.bin")));
	TestFalse(TEXT(".txt should be invalid"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("test.txt")));
	TestFalse(TEXT("Empty string should be invalid"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("")));
	TestFalse(TEXT(".ddo alone should be invalid"), FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(TEXT("test.dta")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_GetDynamicTextAssetsRootPath_NonEmpty,
	"SGDynamicTextAssets.Runtime.Management.FileManager.GetDynamicTextAssetsRootPath.NonEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_GetDynamicTextAssetsRootPath_NonEmpty::RunTest(const FString& Parameters)
{
	FString rootPath = FSGDynamicTextAssetFileManager::GetDynamicTextAssetsRootPath();

	TestFalse(TEXT("Root path should not be empty"), rootPath.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_GetDynamicTextAssetsRootPath_ContainsExpectedDir,
	"SGDynamicTextAssets.Runtime.Management.FileManager.GetDynamicTextAssetsRootPath.ContainsExpectedDir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_GetDynamicTextAssetsRootPath_ContainsExpectedDir::RunTest(const FString& Parameters)
{
	FString rootPath = FSGDynamicTextAssetFileManager::GetDynamicTextAssetsRootPath();

	TestTrue(TEXT("Root path should contain SGDynamicTextAssets"), rootPath.Contains(TEXT("SGDynamicTextAssets")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_BuildFilePath_JsonExtension,
	"SGDynamicTextAssets.Runtime.Management.FileManager.BuildFilePath.JsonExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_BuildFilePath_JsonExtension::RunTest(const FString& Parameters)
{
	// Passing nullptr for class should still work (uses root path)
    FString result = FSGDynamicTextAssetFileManager::BuildFilePath(nullptr, TEXT("test_item"), FSGDynamicTextAssetFileManager::DEFAULT_TEXT_EXTENSION);

	TestTrue(TEXT("Path with bBinary=false should end with .dta.json"), result.EndsWith(TEXT(".dta.json")));
	TestTrue(TEXT("Path should contain the user facing ID"), result.Contains(TEXT("test_item")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_BuildFilePath_BinaryExtension,
	"SGDynamicTextAssets.Runtime.Management.FileManager.BuildFilePath.BinaryExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_BuildFilePath_BinaryExtension::RunTest(const FString& Parameters)
{
	FString result = FSGDynamicTextAssetFileManager::BuildFilePath(nullptr, TEXT("test_item"), FSGDynamicTextAssetFileManager::BINARY_EXTENSION);

	TestTrue(TEXT("Path with bBinary=true should end with .dta.bin"), result.EndsWith(TEXT(".dta.bin")));
	TestTrue(TEXT("Path should contain the user facing ID"), result.Contains(TEXT("test_item")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_BuildFilePath_WithClass,
	"SGDynamicTextAssets.Runtime.Management.FileManager.BuildFilePath.WithClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_BuildFilePath_WithClass::RunTest(const FString& Parameters)
{
	// Use USGDynamicTextAsset as the class (even though it's abstract, BuildFilePath should handle it)
	UClass* testClass = USGDynamicTextAsset::StaticClass();
	FString result = FSGDynamicTextAssetFileManager::BuildFilePath(testClass, TEXT("my_data_object"), FSGDynamicTextAssetFileManager::DEFAULT_TEXT_EXTENSION);

	TestTrue(TEXT("Path should end with .dta.json"), result.EndsWith(TEXT(".dta.json")));
	TestTrue(TEXT("Path should contain the user facing ID"), result.Contains(TEXT("my_data_object")));
	// Note: USGDynamicTextAsset is the base class, so path may just be the root - that's OK

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ReadRawFileContents_EmptyPath,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ReadRawFileContents.EmptyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ReadRawFileContents_EmptyPath::RunTest(const FString& Parameters)
{
	FString outContents;

	AddExpectedError("Inputted EMPTY FilePath");
	bool result = FSGDynamicTextAssetFileManager::ReadRawFileContents(TEXT(""), outContents);

	TestFalse(TEXT("Empty file path should return false"), result);
	TestTrue(TEXT("Output should be empty"), outContents.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ReadRawFileContents_NonExistentFile,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ReadRawFileContents.NonExistentFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ReadRawFileContents_NonExistentFile::RunTest(const FString& Parameters)
{
	FString outContents;
	FString nonExistentPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests"), TEXT("does_not_exist_12345.dta.json"));

	AddExpectedError("Failed to read file at FilePath");
	bool result = FSGDynamicTextAssetFileManager::ReadRawFileContents(nonExistentPath, outContents);

	TestFalse(TEXT("Non-existent file should return false"), result);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ReadRawFileContents_JsonFile,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ReadRawFileContents.JsonFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ReadRawFileContents_JsonFile::RunTest(const FString& Parameters)
{
	// Create a temp JSON file
	FString testDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests"));
	FString testFilePath = FPaths::Combine(testDir, TEXT("test_read_json.dta.json"));
	FString expectedContents = FString::Printf(TEXT("{\"%s\":{\"%s\":\"TestClass\",\"%s\":\"00000000-0000-0000-0000-000000000001\",\"%s\":\"1.0.0\"},\"%s\":{}}"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	// Ensure directory exists
	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	platformFile.CreateDirectoryTree(*testDir);

	// Write the test file
	bool writeSuccess = FFileHelper::SaveStringToFile(expectedContents, *testFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	TestTrue(TEXT("Should be able to write temp JSON file"), writeSuccess);

	if (writeSuccess)
	{
		// Read it back via ReadRawFileContents
		FString outContents;
		bool readSuccess = FSGDynamicTextAssetFileManager::ReadRawFileContents(testFilePath, outContents);

		TestTrue(TEXT("ReadRawFileContents should succeed for JSON file"), readSuccess);
		TestEqual(TEXT("Contents should match what was written"), outContents, expectedContents);

		// Clean up
		platformFile.DeleteFile(*testFilePath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ReadRawFileContents_BinaryRoundtrip,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ReadRawFileContents.BinaryRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ReadRawFileContents_BinaryRoundtrip::RunTest(const FString& Parameters)
{
	// This test validates T13.1 binary-aware reading:
	// 1. Create a known JSON string
	// 2. Use FSGDynamicTextAssetBinarySerializer::StringToBinary() to convert to binary
	// 3. Use FSGDynamicTextAssetBinarySerializer::WriteBinaryFile() to write to temp file
	// 4. Call ReadRawFileContents() on that temp file
	// 5. Verify the output JSON matches the original

	FString testDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests"));
	FString testFilePath = FPaths::Combine(testDir, TEXT("test_binary_roundtrip.dta.bin"));
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();
	FString originalJson = FString::Printf(
		TEXT("{\"%s\":{\"%s\":\"TestBinaryClass\",\"%s\":\"%s\",\"%s\":\"1.0.0\"},\"%s\":{\"value\":42}}"),
		*ISGDynamicTextAssetSerializer::KEY_METADATA,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*testId.ToString(),
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_DATA
	);

	// Ensure directory exists
	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	platformFile.CreateDirectoryTree(*testDir);

	// Step 1: Convert JSON to binary
	FSGBinaryEncodeParams encodeParams;
	encodeParams.Id = testId;
	encodeParams.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;

	TArray<uint8> binaryData;
	bool convertSuccess = FSGDynamicTextAssetBinarySerializer::StringToBinary(originalJson, encodeParams, binaryData);
	TestTrue(TEXT("StringToBinary should succeed"), convertSuccess);

	if (convertSuccess)
	{
		// Step 2: Write binary to temp file
		bool writeSuccess = FSGDynamicTextAssetBinarySerializer::WriteBinaryFile(testFilePath, binaryData);
		TestTrue(TEXT("WriteBinaryFile should succeed"), writeSuccess);

		if (writeSuccess)
		{
			// Step 3: Read it back via ReadRawFileContents (which should detect .dta.bin and decompress)
			FString outContents;
			bool readSuccess = FSGDynamicTextAssetFileManager::ReadRawFileContents(testFilePath, outContents);

			TestTrue(TEXT("ReadRawFileContents should succeed for binary file"), readSuccess);
			TestEqual(TEXT("Decompressed JSON should match original"), outContents, originalJson);

			// Clean up
			platformFile.DeleteFile(*testFilePath);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_GetCookedDynamicTextAssetsRootPath_ContainsCooked,
	"SGDynamicTextAssets.Runtime.Management.FileManager.GetCookedDynamicTextAssetsRootPath.ContainsCooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_GetCookedDynamicTextAssetsRootPath_ContainsCooked::RunTest(const FString& Parameters)
{
	FString cookedPath = FSGDynamicTextAssetFileManager::GetCookedDynamicTextAssetsRootPath();

	TestFalse(TEXT("Cooked root path should not be empty"), cookedPath.IsEmpty());
	TestTrue(TEXT("Cooked root path should contain 'Cooked'"), cookedPath.Contains(TEXT("Cooked")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_BuildBinaryFilePath_ValidFormat,
	"SGDynamicTextAssets.Runtime.Management.FileManager.BuildBinaryFilePath.ValidFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_BuildBinaryFilePath_ValidFormat::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::FromGuid(FGuid(0x12345678, 0x9ABCDEF0, 0x12345678, 0x9ABCDEF0));
	FString binaryPath = FSGDynamicTextAssetFileManager::BuildBinaryFilePath(testId);

	TestTrue(TEXT("Binary path should end with .dta.bin"), binaryPath.EndsWith(TEXT(".dta.bin")));
	TestTrue(TEXT("Binary path should contain the ID"), binaryPath.Contains(testId.ToString()));
	TestTrue(TEXT("Binary path should be in cooked directory"), binaryPath.Contains(TEXT("Cooked")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetFileManager_ShouldUseCookedDirectory_EditorReturnsFalse,
	"SGDynamicTextAssets.Runtime.Management.FileManager.ShouldUseCookedDirectory.EditorReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetFileManager_ShouldUseCookedDirectory_EditorReturnsFalse::RunTest(const FString& Parameters)
{
	// In editor context, should always return false
	bool result = FSGDynamicTextAssetFileManager::ShouldUseCookedDirectory();

	TestFalse(TEXT("ShouldUseCookedDirectory should return false in editor"), result);

	return true;
}
