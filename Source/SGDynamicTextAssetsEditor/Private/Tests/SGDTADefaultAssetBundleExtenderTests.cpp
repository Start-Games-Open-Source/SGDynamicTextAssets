// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDTASerializerFormat.h"
#include "Core/SGDynamicTextAssetBundleData.h"
#include "Serialization/AssetBundleExtenders/SGDTADefaultAssetBundleExtender.h"
#include "Serialization/SGDTASerializer.h"
#include "Statics/SGDTAConstants.h"

namespace SGDTADefaultExtenderTestHelpers
{
	USGDTADefaultAssetBundleExtender* CreateExtender()
	{
		return NewObject<USGDTADefaultAssetBundleExtender>();
	}

	FSGDTASerializerFormat JsonFormat()
	{
		return FSGDTASerializerFormat::FromTypeId(SGDynamicTextAssetConstants::JSON_SERIALIZER_TYPE_ID);
	}

	FSGDTASerializerFormat XmlFormat()
	{
		return FSGDTASerializerFormat::FromTypeId(SGDynamicTextAssetConstants::XML_SERIALIZER_TYPE_ID);
	}

	FSGDTASerializerFormat YamlFormat()
	{
		return FSGDTASerializerFormat::FromTypeId(SGDynamicTextAssetConstants::YAML_SERIALIZER_TYPE_ID);
	}

	FSGDynamicTextAssetBundleData MakeSingleBundleData()
	{
		FSGDynamicTextAssetBundleData data;

		FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
		bundle.BundleName = FName(TEXT("Visual"));
		bundle.Entries.Emplace(
			FSoftObjectPath(TEXT("/Game/Textures/T_Icon.T_Icon")),
			FName(TEXT("UMyAsset.IconTexture")));

		return data;
	}

	FSGDynamicTextAssetBundleData MakeTestBundleData()
	{
		FSGDynamicTextAssetBundleData data;

		// Visual bundle with 2 entries
		{
			FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
			bundle.BundleName = FName(TEXT("Visual"));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Textures/T_Icon.T_Icon")),
				FName(TEXT("UMyAsset.IconTexture")));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Materials/M_Default.M_Default")),
				FName(TEXT("UMyAsset.DefaultMaterial")));
		}

		// Audio bundle with 2 entries
		{
			FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
			bundle.BundleName = FName(TEXT("Audio"));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Sounds/S_Hit.S_Hit")),
				FName(TEXT("UMyAsset.HitSound")));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Sounds/S_Ambient.S_Ambient")),
				FName(TEXT("UMyAsset.AmbientSound")));
		}

		return data;
	}

	FString GetMinimalJsonContent()
	{
		return TEXT("{\n  \"sgdtFormatVersion\": \"1.0.0\"\n}");
	}

	FString GetMinimalXmlContent()
	{
		return TEXT("<DynamicTextAsset>\n    <sgdtFormatVersion>1.0.0</sgdtFormatVersion>\n</DynamicTextAsset>");
	}

	FString GetMinimalYamlContent()
	{
		return TEXT("sgdtFormatVersion: \"1.0.0\"\n");
	}

	void VerifyBundleDataMatches(FAutomationTestBase* Test,
		const FSGDynamicTextAssetBundleData& Expected,
		const FSGDynamicTextAssetBundleData& Actual,
		const TCHAR* Context)
	{
		Test->TestEqual(
			FString::Printf(TEXT("[%s] Bundle count should match"), Context),
			Actual.Bundles.Num(), Expected.Bundles.Num());

		for (const FSGDynamicTextAssetBundle& expectedBundle : Expected.Bundles)
		{
			const FSGDynamicTextAssetBundle* actualBundle = Actual.FindBundle(expectedBundle.BundleName);
			Test->TestNotNull(
				FString::Printf(TEXT("[%s] Bundle '%s' should exist"),
					Context, *expectedBundle.BundleName.ToString()),
				actualBundle);

			if (!actualBundle)
			{
				continue;
			}

			Test->TestEqual(
				FString::Printf(TEXT("[%s] Bundle '%s' entry count should match"),
					Context, *expectedBundle.BundleName.ToString()),
				actualBundle->Entries.Num(), expectedBundle.Entries.Num());

			for (int32 i = 0; i < expectedBundle.Entries.Num() && i < actualBundle->Entries.Num(); ++i)
			{
				Test->TestEqual(
					FString::Printf(TEXT("[%s] Bundle '%s' entry %d path"),
						Context, *expectedBundle.BundleName.ToString(), i),
					actualBundle->Entries[i].AssetPath.ToString(),
					expectedBundle.Entries[i].AssetPath.ToString());

				Test->TestEqual(
					FString::Printf(TEXT("[%s] Bundle '%s' entry %d property"),
						Context, *expectedBundle.BundleName.ToString(), i),
					actualBundle->Entries[i].PropertyName,
					expectedBundle.Entries[i].PropertyName);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_JsonRoundTrip_SingleBundle,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.JsonRoundTrip.SingleBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_JsonRoundTrip_SingleBundle::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData original = MakeSingleBundleData();

	// Serialize
	FString content = GetMinimalJsonContent();
	extender->NotifyPostSerialize(original, content, format);
	TestTrue(TEXT("Content should contain sgdtAssetBundles key"),
		content.Contains(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES));

	// Deserialize
	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("JSON SingleBundle"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_JsonRoundTrip_MultipleBundles,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.JsonRoundTrip.MultipleBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_JsonRoundTrip_MultipleBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData original = MakeTestBundleData();

	FString content = GetMinimalJsonContent();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("JSON MultipleBundles"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_JsonRoundTrip_PreservesExistingData,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.JsonRoundTrip.PreservesExistingData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_JsonRoundTrip_PreservesExistingData::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData bundleData = MakeSingleBundleData();

	// JSON with extra fields that must survive round-trip
	FString content = TEXT("{\n  \"sgdtFormatVersion\": \"1.0.0\",\n  \"customField\": \"customValue\"\n}");
	extender->NotifyPostSerialize(bundleData, content, format);

	TestTrue(TEXT("Existing field should be preserved"),
		content.Contains(TEXT("customField")));
	TestTrue(TEXT("Existing value should be preserved"),
		content.Contains(TEXT("customValue")));
	TestTrue(TEXT("Bundle block should be added"),
		content.Contains(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_XmlRoundTrip_SingleBundle,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.XmlRoundTrip.SingleBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_XmlRoundTrip_SingleBundle::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = XmlFormat();
	const FSGDynamicTextAssetBundleData original = MakeSingleBundleData();

	FString content = GetMinimalXmlContent();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("XML SingleBundle"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_XmlRoundTrip_MultipleBundles,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.XmlRoundTrip.MultipleBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_XmlRoundTrip_MultipleBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = XmlFormat();
	const FSGDynamicTextAssetBundleData original = MakeTestBundleData();

	FString content = GetMinimalXmlContent();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("XML MultipleBundles"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_XmlRoundTrip_SpecialCharacterEscaping,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.XmlRoundTrip.SpecialCharacterEscaping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_XmlRoundTrip_SpecialCharacterEscaping::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = XmlFormat();

	// Bundle data with XML-special characters in asset path
	FSGDynamicTextAssetBundleData original;
	FSGDynamicTextAssetBundle& bundle = original.Bundles.AddDefaulted_GetRef();
	bundle.BundleName = FName(TEXT("Visual"));
	bundle.Entries.Emplace(
		FSoftObjectPath(TEXT("/Game/Assets/T_Icon&Special.T_Icon&Special")),
		FName(TEXT("UMyAsset.SpecialTexture")));

	FString content = GetMinimalXmlContent();
	extender->NotifyPostSerialize(original, content, format);

	// Verify escaped characters are in the XML
	TestTrue(TEXT("Should contain escaped ampersand"), content.Contains(TEXT("&amp;")));

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	if (!TestTrue(TEXT("Should have at least one bundle"), deserialized.Bundles.Num() > 0))
	{
		return false;
	}

	TestEqual(TEXT("Entry count should match"), deserialized.Bundles[0].Entries.Num(), 1);
	TestEqual(TEXT("Asset path should be unescaped on round-trip"),
		deserialized.Bundles[0].Entries[0].AssetPath.ToString(),
		original.Bundles[0].Entries[0].AssetPath.ToString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_YamlRoundTrip_SingleBundle,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.YamlRoundTrip.SingleBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_YamlRoundTrip_SingleBundle::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = YamlFormat();
	const FSGDynamicTextAssetBundleData original = MakeSingleBundleData();

	FString content = GetMinimalYamlContent();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("YAML SingleBundle"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_YamlRoundTrip_MultipleBundles,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.YamlRoundTrip.MultipleBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_YamlRoundTrip_MultipleBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = YamlFormat();
	const FSGDynamicTextAssetBundleData original = MakeTestBundleData();

	FString content = GetMinimalYamlContent();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("YAML MultipleBundles"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_Deserialize_JsonNoBundlesReturnsFalse,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.Deserialize.JsonNoBundlesReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_Deserialize_JsonNoBundlesReturnsFalse::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	FSGDynamicTextAssetBundleData outData;

	// Valid JSON but no sgdtAssetBundles key
	FString jsonContent = GetMinimalJsonContent();
	const bool result = extender->NotifyPreDeserialize(
		jsonContent, outData, JsonFormat());
	TestFalse(TEXT("Should return false when no bundles key exists"), result);
	TestFalse(TEXT("OutData should have no bundles"), outData.HasBundles());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_Deserialize_XmlNoBundlesReturnsFalse,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.Deserialize.XmlNoBundlesReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_Deserialize_XmlNoBundlesReturnsFalse::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	FSGDynamicTextAssetBundleData outData;

	FString xmlContent = GetMinimalXmlContent();
	const bool result = extender->NotifyPreDeserialize(
		xmlContent, outData, XmlFormat());
	TestFalse(TEXT("Should return false when no bundles node exists"), result);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_Deserialize_YamlNoBundlesReturnsFalse,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.Deserialize.YamlNoBundlesReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_Deserialize_YamlNoBundlesReturnsFalse::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	FSGDynamicTextAssetBundleData outData;

	FString yamlContent = GetMinimalYamlContent();
	const bool result = extender->NotifyPreDeserialize(
		yamlContent, outData, YamlFormat());
	TestFalse(TEXT("Should return false when no bundles key exists"), result);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_Deserialize_InvalidJsonReturnsFalse,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.Deserialize.InvalidJsonReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_Deserialize_InvalidJsonReturnsFalse::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	FSGDynamicTextAssetBundleData outData;

	AddExpectedError(TEXT("Failed to parse JSON content"), EAutomationExpectedErrorFlags::Contains, 1);

	FString malformedJson = TEXT("{this is not valid json!!!");
	const bool result = extender->NotifyPreDeserialize(malformedJson, outData, JsonFormat());
	TestFalse(TEXT("Should return false for invalid JSON"), result);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_Serialize_EmptyBundlesNoOp,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.Serialize.EmptyBundlesNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_Serialize_EmptyBundlesNoOp::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDynamicTextAssetBundleData emptyData;

	const FString originalContent = GetMinimalJsonContent();
	FString content = originalContent;
	extender->NotifyPostSerialize(emptyData, content, JsonFormat());

	TestEqual(TEXT("Content should be unchanged when no bundles exist"),
		content, originalContent);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_Serialize_UnrecognizedFormatLogsWarning,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.Serialize.UnrecognizedFormatLogsWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_Serialize_UnrecognizedFormatLogsWarning::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	const FSGDynamicTextAssetBundleData bundleData = MakeSingleBundleData();
	const FSGDTASerializerFormat unknownFormat = FSGDTASerializerFormat::FromTypeId(99);

	AddExpectedError(TEXT("Unrecognized format"), EAutomationExpectedErrorFlags::Contains, 1);

	const FString originalContent = GetMinimalJsonContent();
	FString content = originalContent;
	extender->NotifyPostSerialize(bundleData, content, unknownFormat);

	TestEqual(TEXT("Content should be unchanged for unrecognized format"),
		content, originalContent);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefaultAssetBundleExtender_Dispatch_UnrecognizedFormatDeserializeReturnsFalse,
	"SGDynamicTextAssets.Runtime.Serialization.DefaultAssetBundleExtender.Dispatch.UnrecognizedFormatDeserializeReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefaultAssetBundleExtender_Dispatch_UnrecognizedFormatDeserializeReturnsFalse::RunTest(const FString& Parameters)
{
	using namespace SGDTADefaultExtenderTestHelpers;

	USGDTADefaultAssetBundleExtender* extender = CreateExtender();
	FSGDynamicTextAssetBundleData outData;
	const FSGDTASerializerFormat unknownFormat = FSGDTASerializerFormat::FromTypeId(99);

	AddExpectedError(TEXT("Unrecognized format"), EAutomationExpectedErrorFlags::Contains, 1);

	FString unknownContent = GetMinimalJsonContent();
	const bool result = extender->NotifyPreDeserialize(
		unknownContent, outData, unknownFormat);
	TestFalse(TEXT("Should return false for unrecognized format"), result);

	return true;
}
