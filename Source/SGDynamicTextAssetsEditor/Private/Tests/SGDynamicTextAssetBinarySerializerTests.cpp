// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Serialization/SGBinaryEncodeParams.h"
#include "Serialization/SGDynamicTextAssetBinarySerializer.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"

/**
 * Test: Default-constructed header has valid magic number and correct constants
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGBinaryDynamicTextAssetHeader_DefaultConstructor_HasValidMagic,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.HeaderDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGBinaryDynamicTextAssetHeader_DefaultConstructor_HasValidMagic::RunTest(const FString& Parameters)
{
	FSGBinaryDynamicTextAssetHeader header;

	TestTrue(TEXT("Default header should have valid magic number"), header.IsValid());
	TestEqual(TEXT("PluginSchemaVersion should equal CURRENT_SCHEMA_VERSION"),
		header.PluginSchemaVersion, FSGBinaryDynamicTextAssetHeader::CURRENT_SCHEMA_VERSION);
	TestEqual(TEXT("HEADER_SIZE should be 80"), FSGBinaryDynamicTextAssetHeader::HEADER_SIZE, 80u);
	TestEqual(TEXT("sizeof(FSGBinaryDynamicTextAssetHeader) should equal HEADER_SIZE"),
		static_cast<uint32>(sizeof(FSGBinaryDynamicTextAssetHeader)), FSGBinaryDynamicTextAssetHeader::HEADER_SIZE);

	return true;
}

/**
 * Test: JsonToBinary -> BinaryToJson roundtrip with Zlib compression
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_RoundtripZlib_MatchesInput,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.RoundtripZlib",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_RoundtripZlib_MatchesInput::RunTest(const FString& Parameters)
{
	const FString inputJson = FString::Printf(TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":\"A1B2C3D4-E5F67890-ABCDEF12-34567890\",\"%s\":{\"Damage\":50}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("A1B2C3D4E5F67890ABCDEF1234567890"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> binaryData;
	bool bConverted = FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);
	TestTrue(TEXT("StringToBinary with Zlib should succeed"), bConverted);
	TestTrue(TEXT("Binary data should not be empty"), binaryData.Num() > 0);

	FString outputJson;
	uint32 unusedSerializerTypeId = 0;
	bool bExtracted = FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, outputJson, unusedSerializerTypeId);
	TestTrue(TEXT("BinaryToString should succeed"), bExtracted);
	TestEqual(TEXT("Roundtrip output should match input"), outputJson, inputJson);

	return true;
}

/**
 * Test: JsonToBinary -> BinaryToJson roundtrip with no compression
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_RoundtripNone_MatchesInput,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.RoundtripNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_RoundtripNone_MatchesInput::RunTest(const FString& Parameters)
{
	const FString inputJson = FString::Printf(TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":{\"Health\":100}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("11111111222233334444555566667777"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::None;

	TArray<uint8> binaryData;
	bool bConverted = FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);
	TestTrue(TEXT("StringToBinary with None should succeed"), bConverted);

	FString outputJson;
	uint32 unusedSerializerTypeId = 0;
	bool bExtracted = FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, outputJson, unusedSerializerTypeId);
	TestTrue(TEXT("BinaryToString should succeed"), bExtracted);
	TestEqual(TEXT("Roundtrip output should match input"), outputJson, inputJson);

	return true;
}

/**
 * Test: JsonToBinary -> BinaryToJson roundtrip with LZ4 compression
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_RoundtripLZ4_MatchesInput,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.RoundtripLZ4",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_RoundtripLZ4_MatchesInput::RunTest(const FString& Parameters)
{
	const FString inputJson = FString::Printf(TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":{\"Speed\":25.5}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("AABBCCDD11223344AABBCCDD11223344"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::LZ4;

	TArray<uint8> binaryData;
	bool bConverted = FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);
	TestTrue(TEXT("StringToBinary with LZ4 should succeed"), bConverted);

	FString outputJson;
	uint32 unusedSerializerTypeId = 0;
	bool bExtracted = FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, outputJson, unusedSerializerTypeId);
	TestTrue(TEXT("BinaryToString should succeed"), bExtracted);
	TestEqual(TEXT("Roundtrip output should match input"), outputJson, inputJson);

	return true;
}

/**
 * Test: ReadHeader extracts correct metadata from binary data
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_ReadHeader_ExtractsMetadata,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ReadHeader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_ReadHeader_ExtractsMetadata::RunTest(const FString& Parameters)
{
	const FString inputJson = TEXT("{\"test\":\"data\"}");
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("DEADBEEF12345678ABCDEF9876543210"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> binaryData;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);

	FSGBinaryDynamicTextAssetHeader header;
	bool bRead = FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData, header);
	TestTrue(TEXT("ReadHeader should succeed"), bRead);
	TestTrue(TEXT("Header should be valid"), header.IsValid());
	TestEqual(TEXT("Header ID should match input"), header.Guid, testId.GetGuid());
	TestTrue(TEXT("UncompressedSize should be > 0"), header.UncompressedSize > 0);
	TestTrue(TEXT("CompressedSize should be > 0"), header.CompressedSize > 0);
	TestEqual(TEXT("CompressionType should be Zlib"),
		header.CompressionType, static_cast<uint32>(ESGDynamicTextAssetCompressionMethod::Zlib));

	return true;
}

/**
 * Test: ReadHeader with None compression stores correct type
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_ReadHeader_NoneCompression,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ReadHeaderNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_ReadHeader_NoneCompression::RunTest(const FString& Parameters)
{
	const FString inputJson = TEXT("{\"test\":\"uncompressed\"}");
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("11111111111111111111111111111111"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::None;

	TArray<uint8> binaryData;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);

	FSGBinaryDynamicTextAssetHeader header;
	bool bRead = FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData, header);
	TestTrue(TEXT("ReadHeader should succeed"), bRead);
	TestEqual(TEXT("CompressionType should be None"),
		header.CompressionType, static_cast<uint32>(ESGDynamicTextAssetCompressionMethod::None));
	TestEqual(TEXT("CompressedSize should equal UncompressedSize for None"),
		header.CompressedSize, header.UncompressedSize);

	return true;
}

/**
 * Test: ExtractId returns correct ID without full decompression
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_ExtractId_MatchesInput,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ExtractId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_ExtractId_MatchesInput::RunTest(const FString& Parameters)
{
	const FString inputJson = TEXT("{\"extract\":\"id\"}");
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("CAFEBABE12345678DEADBEEFAABBCCDD"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;

	TArray<uint8> binaryData;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);

	FSGDynamicTextAssetId extractedId;
	bool bExtracted = FSGDynamicTextAssetBinarySerializer::ExtractId(binaryData, extractedId);
	TestTrue(TEXT("ExtractId should succeed"), bExtracted);
	TestEqual(TEXT("Extracted ID should match input"), extractedId, testId);

	return true;
}

/**
 * Test: IsValidBinaryData returns true for valid data, false for invalid
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_IsValidBinaryData_DetectsValidity,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.IsValidBinaryData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_IsValidBinaryData_DetectsValidity::RunTest(const FString& Parameters)
{
	// Valid data
	const FString inputJson = TEXT("{\"valid\":true}");
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("12345678ABCDEF0012345678ABCDEF00"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;

	TArray<uint8> validBinary;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, validBinary);
	TestTrue(TEXT("Valid binary data should return true"), FSGDynamicTextAssetBinarySerializer::IsValidBinaryData(validBinary));

	// Empty array
	TArray<uint8> emptyData;
	AddExpectedError("Binary data too small for header");      // empty data
	TestFalse(TEXT("Empty data should return false"), FSGDynamicTextAssetBinarySerializer::IsValidBinaryData(emptyData));

	// Garbage data (random bytes, enough to cover header size)
	TArray<uint8> garbageData;
	garbageData.SetNumZeroed(FSGBinaryDynamicTextAssetHeader::HEADER_SIZE);
	garbageData[0] = 0xFF;
	garbageData[1] = 0xFE;
	AddExpectedError("Invalid magic number");                   // garbage data
	TestFalse(TEXT("Garbage data should return false"), FSGDynamicTextAssetBinarySerializer::IsValidBinaryData(garbageData));

	// Too small (less than header size)
	TArray<uint8> tooSmall;
	tooSmall.SetNumZeroed(10);
	AddExpectedError("Binary data too small for header");      // too small data
	TestFalse(TEXT("Data smaller than HEADER_SIZE should return false"), FSGDynamicTextAssetBinarySerializer::IsValidBinaryData(tooSmall));

	return true;
}

/**
 * Test: JsonToBinary rejects empty string
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_JsonToBinary_EmptyStringFails,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.InvalidInputEmptyString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_JsonToBinary_EmptyStringFails::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("12345678ABCDEF0012345678ABCDEF00"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;

	TArray<uint8> binaryData;
	AddExpectedError("Inputted EMPTY PayloadString");
	bool bResult = FSGDynamicTextAssetBinarySerializer::StringToBinary(TEXT(""), params, binaryData);
	TestFalse(TEXT("Empty string should fail"), bResult);
	TestEqual(TEXT("Output should be empty on failure"), binaryData.Num(), 0);

	return true;
}

/**
 * Test: JsonToBinary rejects invalid ID
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_JsonToBinary_InvalidIdFails,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.InvalidInputBadId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_JsonToBinary_InvalidIdFails::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId invalidId; // Default is invalid (all zeros)

	FSGBinaryEncodeParams params;
	params.Id = invalidId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;

	TArray<uint8> binaryData;
	AddExpectedError("Inputted INVALID Id");
	bool bResult = FSGDynamicTextAssetBinarySerializer::StringToBinary(TEXT("{\"data\":1}"), params, binaryData);
	TestFalse(TEXT("Invalid Id should fail"), bResult);

	return true;
}

/**
 * Test: BinaryToJson rejects empty array
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_BinaryToJson_EmptyArrayFails,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.InvalidInputEmptyArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_BinaryToJson_EmptyArrayFails::RunTest(const FString& Parameters)
{
	TArray<uint8> emptyData;
	FString outputJson;
	AddExpectedError("Inputted Empty BinaryData");
	uint32 unusedSerializerTypeId = 0;
	bool bResult = FSGDynamicTextAssetBinarySerializer::BinaryToString(emptyData, outputJson, unusedSerializerTypeId);
	TestFalse(TEXT("Empty array should fail"), bResult);
	TestTrue(TEXT("Output string should be empty on failure"), outputJson.IsEmpty());

	return true;
}

/**
 * Test: Compressed data is smaller than uncompressed for large payloads
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_Zlib_CompressesData,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ZlibCompresses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_Zlib_CompressesData::RunTest(const FString& Parameters)
{
	// Build a large, repetitive JSON string that should compress well
	FString largeJson = TEXT("{\"data\":\"");
	for (int32 index = 0; index < 1000; ++index)
	{
		largeJson += TEXT("ABCDEFGHIJ");
	}
	largeJson += TEXT("\"}");

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("AAAABBBBCCCCDDDDEEEEFFFFAAAABBBB"));

	FSGBinaryEncodeParams compressedParams;
	compressedParams.Id = testId;
	compressedParams.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	compressedParams.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> compressedBinary;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(largeJson, compressedParams, compressedBinary);

	FSGBinaryEncodeParams uncompressedParams;
	uncompressedParams.Id = testId;
	uncompressedParams.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	uncompressedParams.CompressionMethod = ESGDynamicTextAssetCompressionMethod::None;

	TArray<uint8> uncompressedBinary;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(largeJson, uncompressedParams, uncompressedBinary);

	TestTrue(TEXT("Zlib compressed should be smaller than uncompressed for large data"),
		compressedBinary.Num() < uncompressedBinary.Num());

	return true;
}

/**
 * Test: MAX_UNCOMPRESSED_SIZE constant is 80 MB (HEADER_SIZE * 1024 * 1024)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_MaxUncompressedSize_Is80MB,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.MaxUncompressedSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_MaxUncompressedSize_Is80MB::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("MAX_UNCOMPRESSED_SIZE should be 80 MB"),
		FSGDynamicTextAssetBinarySerializer::MAX_UNCOMPRESSED_SIZE, 80u * 1024u * 1024u);

	return true;
}

/**
 * Test: Default header has all-zero ContentHash and HasContentHash returns false
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGBinaryDynamicTextAssetHeader_DefaultConstructor_HasNoContentHash,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ContentHashDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGBinaryDynamicTextAssetHeader_DefaultConstructor_HasNoContentHash::RunTest(const FString& Parameters)
{
	FSGBinaryDynamicTextAssetHeader header;

	TestFalse(TEXT("Default header should not have a content hash"), header.HasContentHash());
	TestEqual(TEXT("CONTENT_HASH_SIZE should be 20 bytes"), FSGBinaryDynamicTextAssetHeader::CONTENT_HASH_SIZE, 20u);

	// Verify all ContentHash bytes are zero
	for (uint32 index = 0; index < FSGBinaryDynamicTextAssetHeader::CONTENT_HASH_SIZE; ++index)
	{
		TestEqual(TEXT("ContentHash byte should be zero"), header.ContentHash[index], static_cast<uint8>(0));
	}

	return true;
}

/**
 * Test: JsonToBinary writes a non-zero ContentHash in the header (v2 behavior)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_JsonToBinary_WritesContentHash,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ContentHashWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_JsonToBinary_WritesContentHash::RunTest(const FString& Parameters)
{
	const FString inputJson = FString::Printf(TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":{\"Value\":42}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("AABB1122CCDD3344EEFF5566AABB7788"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> binaryData;
	bool bConverted = FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);
	TestTrue(TEXT("StringToBinary should succeed"), bConverted);

	FSGBinaryDynamicTextAssetHeader header;
	bool bRead = FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData, header);
	TestTrue(TEXT("ReadHeader should succeed"), bRead);
	TestTrue(TEXT("Header should have a content hash after StringToBinary"), header.HasContentHash());
	TestEqual(TEXT("Schema version should be 1"), header.PluginSchemaVersion, 1u);

	return true;
}

/**
 * Test: ContentHash is consistent - same input produces the same hash
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_ContentHash_DeterministicForSameInput,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ContentHashDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_ContentHash_DeterministicForSameInput::RunTest(const FString& Parameters)
{
	const FString inputJson = FString::Printf(TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":{\"Name\":\"TestItem\"}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("11223344556677889900AABBCCDDEEFF"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> binaryData1;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData1);

	TArray<uint8> binaryData2;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData2);

	FSGBinaryDynamicTextAssetHeader header1;
	FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData1, header1);

	FSGBinaryDynamicTextAssetHeader header2;
	FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData2, header2);

	TestTrue(TEXT("Both headers should have content hashes"), header1.HasContentHash() && header2.HasContentHash());
	TestEqual(TEXT("Content hashes should match for identical input"),
		FMemory::Memcmp(header1.ContentHash, header2.ContentHash, FSGBinaryDynamicTextAssetHeader::CONTENT_HASH_SIZE), 0);

	return true;
}

/**
 * Test: ContentHash differs for different compressed payloads
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_ContentHash_DiffersForDifferentInput,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ContentHashDiffers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_ContentHash_DiffersForDifferentInput::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("FFEEDDCCBBAA99887766554433221100"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> binaryData1;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(TEXT("{\"data\":{\"A\":1}}"), params, binaryData1);

	TArray<uint8> binaryData2;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(TEXT("{\"data\":{\"B\":2}}"), params, binaryData2);

	FSGBinaryDynamicTextAssetHeader header1;
	FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData1, header1);

	FSGBinaryDynamicTextAssetHeader header2;
	FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData2, header2);

	TestTrue(TEXT("Both headers should have content hashes"), header1.HasContentHash() && header2.HasContentHash());
	TestNotEqual(TEXT("Content hashes should differ for different input"),
		FMemory::Memcmp(header1.ContentHash, header2.ContentHash, FSGBinaryDynamicTextAssetHeader::CONTENT_HASH_SIZE), 0);

	return true;
}

/**
 * Test: BinaryToJson detects tampered payload via content hash mismatch
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_BinaryToJson_DetectsTamperedPayload,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ContentHashTamperDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_BinaryToJson_DetectsTamperedPayload::RunTest(const FString& Parameters)
{
	const FString inputJson = FString::Printf(TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":{\"Integrity\":true}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("ABCDEF0123456789ABCDEF0123456789"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::None;

	TArray<uint8> binaryData;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);

	// Tamper with the payload (flip a byte after the fixed 80-byte header)
	FSGBinaryDynamicTextAssetHeader tamperedHeader;
	FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData, tamperedHeader);
	const int32 payloadOffset = FSGBinaryDynamicTextAssetHeader::HEADER_SIZE;
	if (binaryData.Num() > payloadOffset + 1)
	{
		binaryData[payloadOffset + 1] ^= 0xFF;
	}

	FString outputJson;
	uint32 unusedSerializerTypeId = 0;
	AddExpectedError("Content hash mismatch");
	bool bResult = FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, outputJson, unusedSerializerTypeId);
	TestFalse(TEXT("BinaryToString should fail for tampered data"), bResult);

	return true;
}

/**
 * Test: BinaryToJson succeeds with valid (untampered) content hash
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_BinaryToJson_PassesValidContentHash,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ContentHashValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_BinaryToJson_PassesValidContentHash::RunTest(const FString& Parameters)
{
	const FString inputJson = FString::Printf(TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":{\"HashTest\":\"pass\"}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("99887766554433221100FFEEDDCCBBAA"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> binaryData;
	FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);

	FString outputJson;
	uint32 unusedSerializerTypeId = 0;
	bool bResult = FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, outputJson, unusedSerializerTypeId);
	TestTrue(TEXT("BinaryToString should succeed with valid content hash"), bResult);
	TestEqual(TEXT("Output should match input"), outputJson, inputJson);

	return true;
}

/**
 * Test: Content hash works correctly with None compression
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_ContentHash_WorksWithNoneCompression,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.ContentHashNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_ContentHash_WorksWithNoneCompression::RunTest(const FString& Parameters)
{
	const FString inputJson = TEXT("{\"compression\":\"none\",\"data\":{\"Test\":1}}");
	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("A1A1B2B2C3C3D4D4E5E5F6F6A7A7B8B8"));

	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::None;

	TArray<uint8> binaryData;
	bool bConverted = FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);
	TestTrue(TEXT("StringToBinary with None compression should succeed"), bConverted);

	FSGBinaryDynamicTextAssetHeader header;
	FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData, header);
	TestTrue(TEXT("Header should have content hash even with None compression"), header.HasContentHash());

	// Roundtrip should work
	FString outputJson;
	uint32 unusedSerializerTypeId = 0;
	bool bExtracted = FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, outputJson, unusedSerializerTypeId);
	TestTrue(TEXT("BinaryToString should succeed"), bExtracted);
	TestEqual(TEXT("Roundtrip should match"), outputJson, inputJson);

	return true;
}

/**
 * Test: Binary roundtrip preserves $userfacingid in JSON content
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetBinarySerializer_Roundtrip_PreservesUserFacingId,
	"SGDynamicTextAssets.Runtime.Serialization.BinarySerializer.RoundtripPreservesUserFacingId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetBinarySerializer_Roundtrip_PreservesUserFacingId::RunTest(const FString& Parameters)
{
	// Build JSON that includes the $userfacingid key
	const FString inputJson = FString::Printf(
		TEXT("{\"%s\":\"USGDynamicTextAsset\",\"%s\":\"A1B2C3D4-E5F67890-ABCDEF12-34567890\",\"%s\":\"1.0.0\",\"%s\":\"excalibur\",\"%s\":{\"Damage\":50}}"),
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	FSGDynamicTextAssetId testId;
	testId.ParseString(TEXT("A1B2C3D4E5F67890ABCDEF1234567890"));

	// JSON -> Binary
	FSGBinaryEncodeParams params;
	params.Id = testId;
	params.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;
	params.CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;

	TArray<uint8> binaryData;
	bool bConverted = FSGDynamicTextAssetBinarySerializer::StringToBinary(inputJson, params, binaryData);
	TestTrue(TEXT("StringToBinary should succeed"), bConverted);

	// Binary -> JSON
	FString outputJson;
	uint32 unusedSerializerTypeId = 0;
	bool bExtracted = FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, outputJson, unusedSerializerTypeId);
	TestTrue(TEXT("BinaryToString should succeed"), bExtracted);

	// Verify the $userfacingid survived the roundtrip
	TestEqual(TEXT("Roundtrip output should match input exactly"), outputJson, inputJson);
	TestTrue(TEXT("Output should contain $userfacingid key"),
		outputJson.Contains(ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID));
	TestTrue(TEXT("Output should contain the UserFacingId value"),
		outputJson.Contains(TEXT("excalibur")));

	return true;
}


