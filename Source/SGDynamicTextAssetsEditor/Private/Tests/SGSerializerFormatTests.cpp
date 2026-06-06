// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDTASerializerFormat.h"
#include "Serialization/SGDTAJsonSerializer.h"
#include "Serialization/SGDTAXmlSerializer.h"
#include "Serialization/SGDTAYamlSerializer.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_DefaultState_IsInvalid,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.DefaultState.IsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_DefaultState_IsInvalid::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat defaultFormat;

	TestEqual(TEXT("Default-constructed TypeId should be 0"), defaultFormat.GetTypeId(), 0u);
	TestFalse(TEXT("Default-constructed format should be invalid"), defaultFormat.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_InvalidConstant_IsInvalid,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.InvalidConstant.IsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_InvalidConstant_IsInvalid::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("INVALID TypeId should be 0"), FSGDTASerializerFormat::INVALID.GetTypeId(), 0u);
	TestFalse(TEXT("INVALID should not be valid"), FSGDTASerializerFormat::INVALID.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_JsonFormat_IsValid,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.JsonFormat.IsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_JsonFormat_IsValid::RunTest(const FString& Parameters)
{
	// Use the serializer's FORMAT constant directly
	FSGDTASerializerFormat jsonFormat = FSGDynamicTextAssetJsonSerializer::FORMAT;

	TestTrue(TEXT("JSON format should be valid"), jsonFormat.IsValid());
	TestEqual(TEXT("JSON format TypeId should match JSON FORMAT"),
		jsonFormat.GetTypeId(), FSGDynamicTextAssetJsonSerializer::FORMAT.GetTypeId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_XmlFormat_IsValid,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.XmlFormat.IsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_XmlFormat_IsValid::RunTest(const FString& Parameters)
{
	// Use the serializer's FORMAT constant directly
	FSGDTASerializerFormat xmlFormat = FSGDynamicTextAssetXmlSerializer::FORMAT;

	TestTrue(TEXT("XML format should be valid"), xmlFormat.IsValid());
	TestEqual(TEXT("XML format TypeId should match XML FORMAT"),
		xmlFormat.GetTypeId(), FSGDynamicTextAssetXmlSerializer::FORMAT.GetTypeId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_YamlFormat_IsValid,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.YamlFormat.IsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_YamlFormat_IsValid::RunTest(const FString& Parameters)
{
	// Use the serializer's FORMAT constant directly
	FSGDTASerializerFormat yamlFormat = FSGDynamicTextAssetYamlSerializer::FORMAT;

	TestTrue(TEXT("YAML format should be valid"), yamlFormat.IsValid());
	TestEqual(TEXT("YAML format TypeId should match YAML FORMAT"),
		yamlFormat.GetTypeId(), FSGDynamicTextAssetYamlSerializer::FORMAT.GetTypeId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_Equality_MatchesCorrectly,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.Equality.MatchesCorrectly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_Equality_MatchesCorrectly::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat jsonA = FSGDynamicTextAssetJsonSerializer::FORMAT;
	FSGDTASerializerFormat jsonB = FSGDynamicTextAssetJsonSerializer::FORMAT;
	FSGDTASerializerFormat xmlFormat = FSGDynamicTextAssetXmlSerializer::FORMAT;

	TestTrue(TEXT("Same format should be equal"), jsonA == jsonB);
	TestFalse(TEXT("Different formats should not be equal"), jsonA == xmlFormat);
	TestTrue(TEXT("Format should equal its uint32 TypeId"),
		jsonA == FSGDynamicTextAssetJsonSerializer::FORMAT.GetTypeId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_Inequality_MatchesCorrectly,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.Inequality.MatchesCorrectly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_Inequality_MatchesCorrectly::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat jsonFormat = FSGDynamicTextAssetJsonSerializer::FORMAT;
	FSGDTASerializerFormat xmlFormat = FSGDynamicTextAssetXmlSerializer::FORMAT;

	TestTrue(TEXT("Different formats should be not-equal"), jsonFormat != xmlFormat);
	TestFalse(TEXT("Same format should not be not-equal"),
		jsonFormat != FSGDynamicTextAssetJsonSerializer::FORMAT);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_GetTypeHash_ConsistentResults,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.GetTypeHash.ConsistentResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_GetTypeHash_ConsistentResults::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat jsonA = FSGDynamicTextAssetJsonSerializer::FORMAT;
	FSGDTASerializerFormat jsonB = FSGDynamicTextAssetJsonSerializer::FORMAT;
	FSGDTASerializerFormat xmlFormat = FSGDynamicTextAssetXmlSerializer::FORMAT;

	TestEqual(TEXT("Same TypeId should produce same hash"),
		GetTypeHash(jsonA), GetTypeHash(jsonB));
	TestNotEqual(TEXT("Different TypeIds should produce different hashes"),
		GetTypeHash(jsonA), GetTypeHash(xmlFormat));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_FromTypeId_CorrectTypeId,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.FromTypeId.CorrectTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_FromTypeId_CorrectTypeId::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat format = FSGDTASerializerFormat::FromTypeId(5);

	TestEqual(TEXT("FromTypeId should preserve the TypeId"), format.GetTypeId(), 5u);
	TestTrue(TEXT("FromTypeId with non-zero value should be valid"), format.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_TextExportImport_RoundTrip,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.TextExportImport.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_TextExportImport_RoundTrip::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat original = FSGDynamicTextAssetJsonSerializer::FORMAT;

	// Export
	FString exported;
	FSGDTASerializerFormat defaultValue;
	original.ExportTextItem(exported, defaultValue, nullptr, 0, nullptr);

	TestFalse(TEXT("Exported string should not be empty"), exported.IsEmpty());

	// Import
	FSGDTASerializerFormat imported;
	const TCHAR* buffer = *exported;
	imported.ImportTextItem(buffer, 0, nullptr, nullptr);

	TestTrue(TEXT("Round-tripped format should equal original"), imported == original);
	TestEqual(TEXT("Round-tripped TypeId should match"),
		imported.GetTypeId(), original.GetTypeId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_BinarySerialize_RoundTrip,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.BinarySerialize.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_BinarySerialize_RoundTrip::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat original = FSGDynamicTextAssetJsonSerializer::FORMAT;

	// Write
	TArray<uint8> data;
	FMemoryWriter writer(data);
	original.Serialize(writer);

	TestTrue(TEXT("Serialized data should not be empty"), data.Num() > 0);

	// Read
	FSGDTASerializerFormat loaded;
	FMemoryReader reader(data);
	loaded.Serialize(reader);

	TestTrue(TEXT("Deserialized format should equal original"), loaded == original);
	TestEqual(TEXT("Deserialized TypeId should match"),
		loaded.GetTypeId(), original.GetTypeId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_FromExtension_ResolvesJson,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.FromExtension.ResolvesJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_FromExtension_ResolvesJson::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat format = FSGDTASerializerFormat::FromExtension(TEXT(".dta.json"));

	TestTrue(TEXT("Format from .dta.json extension should be valid"), format.IsValid());
	TestEqual(TEXT("Format from .dta.json should have JSON TypeId"),
		format.GetTypeId(), FSGDynamicTextAssetJsonSerializer::FORMAT.GetTypeId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_GetFormatName_ReturnsCorrectName,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.GetFormatName.ReturnsCorrectName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_GetFormatName_ReturnsCorrectName::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat jsonFormat = FSGDynamicTextAssetJsonSerializer::FORMAT;
	FText jsonName = jsonFormat.GetFormatName();

	TestEqual(TEXT("JSON format name should be 'JSON'"), jsonName.ToString(), TEXT("JSON"));

	FSGDTASerializerFormat invalidFormat;
	FText invalidName = invalidFormat.GetFormatName();

	TestEqual(TEXT("Invalid format name should be 'Invalid'"), invalidName.ToString(), TEXT("Invalid"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_GetSerializer_ValidAndInvalid,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.GetSerializer.ValidAndInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_GetSerializer_ValidAndInvalid::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat jsonFormat = FSGDynamicTextAssetJsonSerializer::FORMAT;
	TSharedPtr<ISGDynamicTextAssetSerializer> jsonSerializer = jsonFormat.GetSerializer();

	TestTrue(TEXT("JSON format should return a non-null serializer"), jsonSerializer.IsValid());

	FSGDTASerializerFormat invalidFormat;
	TSharedPtr<ISGDynamicTextAssetSerializer> nullSerializer = invalidFormat.GetSerializer();

	TestFalse(TEXT("Invalid format should return a null serializer"), nullSerializer.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerFormat_ToString_CorrectStringRepresentation,
	"SGDynamicTextAssets.Runtime.Core.SerializerFormat.ToString.CorrectStringRepresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerFormat_ToString_CorrectStringRepresentation::RunTest(const FString& Parameters)
{
	FSGDTASerializerFormat jsonFormat = FSGDynamicTextAssetJsonSerializer::FORMAT;
	FString jsonString = jsonFormat.ToString();

	TestEqual(TEXT("JSON format ToString should return its TypeId as string"),
		jsonString, FString::FromInt(static_cast<int32>(FSGDynamicTextAssetJsonSerializer::FORMAT.GetTypeId())));

	FSGDTASerializerFormat invalidFormat;
	FString invalidString = invalidFormat.ToString();

	TestEqual(TEXT("Invalid format ToString should return '0'"), invalidString, TEXT("0"));

	return true;
}
