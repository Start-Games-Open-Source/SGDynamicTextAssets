// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDTASerializerFormat.h"
#include "Core/SGDynamicTextAssetBundleData.h"
#include "Serialization/AssetBundleExtenders/SGDTADefaultAssetBundleExtender.h"
#include "Serialization/AssetBundleExtenders/SGDTAPerPropertyAssetBundleExtender.h"
#include "Serialization/SGDTASerializer.h"
#include "Serialization/SGDTASerializerBase.h"
#include "Statics/SGDTAConstants.h"

namespace SGDTAPerPropertyExtenderTestHelpers
{
	USGDTAPerPropertyAssetBundleExtender* CreateExtender()
	{
		return NewObject<USGDTAPerPropertyAssetBundleExtender>();
	}

	USGDTADefaultAssetBundleExtender* CreateDefaultExtender()
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

	FSGDynamicTextAssetBundleData MakeFlatBundleData()
	{
		FSGDynamicTextAssetBundleData data;

		FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
		bundle.BundleName = FName(TEXT("Visual"));
		bundle.Entries.Emplace(
			FSoftObjectPath(TEXT("/Game/Textures/T_Icon.T_Icon")),
			FName(TEXT("VisualRef")));

		return data;
	}

	FSGDynamicTextAssetBundleData MakeNestedBundleData()
	{
		FSGDynamicTextAssetBundleData data;

		// Root-level property (unqualified)
		{
			FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
			bundle.BundleName = FName(TEXT("Visual"));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Textures/T_Icon.T_Icon")),
				FName(TEXT("VisualRef")));
		}

		// Instanced object property (class-qualified, UClass FName omits the U prefix)
		{
			FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
			bundle.BundleName = FName(TEXT("InstancedBundle"));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Meshes/SM_Test.SM_Test")),
				FName(TEXT("SGDTABundleTestInstancedObject.InstancedSoftRef")));
		}

		// Struct property (class-qualified)
		{
			FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
			bundle.BundleName = FName(TEXT("StructBundle"));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Materials/M_Test.M_Test")),
				FName(TEXT("SGDTABundleTestStruct.StructSoftRef")));
		}

		return data;
	}

	FSGDynamicTextAssetBundleData MakeMultiEntryBundleData()
	{
		FSGDynamicTextAssetBundleData data;

		// Visual bundle with 3 entries
		{
			FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
			bundle.BundleName = FName(TEXT("Visual"));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Textures/T_Icon.T_Icon")),
				FName(TEXT("IconTexture")));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Materials/M_Default.M_Default")),
				FName(TEXT("DefaultMaterial")));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Meshes/SM_Sword.SM_Sword")),
				FName(TEXT("MeshRef")));
		}

		// Audio bundle with 3 entries
		{
			FSGDynamicTextAssetBundle& bundle = data.Bundles.AddDefaulted_GetRef();
			bundle.BundleName = FName(TEXT("Audio"));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Sounds/S_Hit.S_Hit")),
				FName(TEXT("HitSound")));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Sounds/S_Ambient.S_Ambient")),
				FName(TEXT("AmbientSound")));
			bundle.Entries.Emplace(
				FSoftObjectPath(TEXT("/Game/Sounds/S_Pickup.S_Pickup")),
				FName(TEXT("PickupSound")));
		}

		return data;
	}

	FString GetJsonContentWithData()
	{
		return TEXT(
			"{\n"
			"  \"sgdtFormatVersion\": \"2.0.0\",\n"
			"  \"data\": {\n"
			"    \"VisualRef\": \"/Game/Textures/T_Icon.T_Icon\",\n"
			"    \"Damage\": 50\n"
			"  }\n"
			"}"
		);
	}

	FString GetJsonContentWithNestedData()
	{
		return TEXT(
			"{\n"
			"  \"sgdtFormatVersion\": \"2.0.0\",\n"
			"  \"data\": {\n"
			"    \"VisualRef\": \"/Game/Textures/T_Icon.T_Icon\",\n"
			"    \"InstancedObj\": {\n"
			"      \"SG_INST_OBJ_CLASS\": \"/Script/SGDynamicTextAssetsEditor.SGDTABundleTestInstancedObject\",\n"
			"      \"InstancedSoftRef\": \"/Game/Meshes/SM_Test.SM_Test\"\n"
			"    },\n"
			"    \"StructWithBundle\": {\n"
			"      \"SG_STRUCT_TYPE\": \"SGDTABundleTestStruct\",\n"
			"      \"StructSoftRef\": \"/Game/Materials/M_Test.M_Test\",\n"
			"      \"PlainInt\": 42\n"
			"    }\n"
			"  }\n"
			"}"
		);
	}

	FString GetJsonContentWithMultiEntryData()
	{
		return TEXT(
			"{\n"
			"  \"sgdtFormatVersion\": \"2.0.0\",\n"
			"  \"data\": {\n"
			"    \"IconTexture\": \"/Game/Textures/T_Icon.T_Icon\",\n"
			"    \"DefaultMaterial\": \"/Game/Materials/M_Default.M_Default\",\n"
			"    \"MeshRef\": \"/Game/Meshes/SM_Sword.SM_Sword\",\n"
			"    \"HitSound\": \"/Game/Sounds/S_Hit.S_Hit\",\n"
			"    \"AmbientSound\": \"/Game/Sounds/S_Ambient.S_Ambient\",\n"
			"    \"PickupSound\": \"/Game/Sounds/S_Pickup.S_Pickup\",\n"
			"    \"PlainFloat\": 1.0\n"
			"  }\n"
			"}"
		);
	}

	FString GetXmlContentWithData()
	{
		return TEXT(
			"<sgDynamicTextAsset>\n"
			"    <sgdtFormatVersion>2.0.0</sgdtFormatVersion>\n"
			"    <data>\n"
			"        <VisualRef>/Game/Textures/T_Icon.T_Icon</VisualRef>\n"
			"        <Damage>50</Damage>\n"
			"    </data>\n"
			"</sgDynamicTextAsset>"
		);
	}

	FString GetXmlContentWithNestedData()
	{
		return TEXT(
			"<sgDynamicTextAsset>\n"
			"    <sgdtFormatVersion>2.0.0</sgdtFormatVersion>\n"
			"    <data>\n"
			"        <VisualRef>/Game/Textures/T_Icon.T_Icon</VisualRef>\n"
			"        <InstancedObj>\n"
			"            <SG_INST_OBJ_CLASS>/Script/SGDynamicTextAssetsEditor.SGDTABundleTestInstancedObject</SG_INST_OBJ_CLASS>\n"
			"            <InstancedSoftRef>/Game/Meshes/SM_Test.SM_Test</InstancedSoftRef>\n"
			"        </InstancedObj>\n"
			"        <StructWithBundle>\n"
			"            <SG_STRUCT_TYPE>SGDTABundleTestStruct</SG_STRUCT_TYPE>\n"
			"            <StructSoftRef>/Game/Materials/M_Test.M_Test</StructSoftRef>\n"
			"            <PlainInt>42</PlainInt>\n"
			"        </StructWithBundle>\n"
			"    </data>\n"
			"</sgDynamicTextAsset>"
		);
	}

	FString GetYamlContentWithData()
	{
		return TEXT(
			"sgdtFormatVersion: \"2.0.0\"\n"
			"data:\n"
			"  VisualRef: \"/Game/Textures/T_Icon.T_Icon\"\n"
			"  Damage: 50\n"
		);
	}

	FString GetYamlContentWithNestedData()
	{
		return TEXT(
			"sgdtFormatVersion: \"2.0.0\"\n"
			"data:\n"
			"  VisualRef: \"/Game/Textures/T_Icon.T_Icon\"\n"
			"  InstancedObj:\n"
			"    SG_INST_OBJ_CLASS: \"/Script/SGDynamicTextAssetsEditor.SGDTABundleTestInstancedObject\"\n"
			"    InstancedSoftRef: \"/Game/Meshes/SM_Test.SM_Test\"\n"
			"  StructWithBundle:\n"
			"    SG_STRUCT_TYPE: \"SGDTABundleTestStruct\"\n"
			"    StructSoftRef: \"/Game/Materials/M_Test.M_Test\"\n"
			"    PlainInt: 42\n"
		);
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
	FPerPropertyExtender_Serialize_WritesInlineAssetBundles,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.Serialize.WritesInlineAssetBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_Serialize_WritesInlineAssetBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData bundleData = MakeFlatBundleData();

	FString content = GetJsonContentWithData();
	extender->NotifyPostSerialize(bundleData, content, format);

	// The wrapped property should contain "value" and "assetBundles" keys
	TestTrue(TEXT("Content should contain 'value' key for wrapped property"),
		content.Contains(TEXT("\"value\"")));
	TestTrue(TEXT("Content should contain 'assetBundles' key for wrapped property"),
		content.Contains(TEXT("\"assetBundles\"")));
	TestTrue(TEXT("Content should contain 'Visual' bundle name"),
		content.Contains(TEXT("\"Visual\"")));

	// The original asset path should still be present (inside the "value" field)
	TestTrue(TEXT("Original asset path should be preserved"),
		content.Contains(TEXT("/Game/Textures/T_Icon.T_Icon")));

	// Non-bundled property should remain unwrapped
	TestTrue(TEXT("Non-bundled property should remain in content"),
		content.Contains(TEXT("\"Damage\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_Serialize_NoRootLevelBundleBlock,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.Serialize.NoRootLevelBundleBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_Serialize_NoRootLevelBundleBlock::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData bundleData = MakeFlatBundleData();

	FString content = GetJsonContentWithData();
	extender->NotifyPostSerialize(bundleData, content, format);

	// Per-property extender should NOT write the root-level bundle block
	TestFalse(TEXT("Should NOT contain root-level sgdtAssetBundles key"),
		content.Contains(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_Deserialize_ReadsInlineFormat,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.Deserialize.ReadsInlineFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_Deserialize_ReadsInlineFormat::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData original = MakeFlatBundleData();

	// First serialize to produce inline-wrapped content
	FString content = GetJsonContentWithData();
	extender->NotifyPostSerialize(original, content, format);

	// Then deserialize from the inline-wrapped content
	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);
	TestTrue(TEXT("Should have bundles"), deserialized.HasBundles());

	const FSGDynamicTextAssetBundle* visualBundle = deserialized.FindBundle(FName(TEXT("Visual")));
	TestNotNull(TEXT("Visual bundle should exist"), visualBundle);
	if (visualBundle)
	{
		TestEqual(TEXT("Visual bundle should have 1 entry"), visualBundle->Entries.Num(), 1);
		TestEqual(TEXT("Entry asset path should match"),
			visualBundle->Entries[0].AssetPath.ToString(),
			TEXT("/Game/Textures/T_Icon.T_Icon"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_JsonRoundTrip_FlatBundles,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.JsonRoundTrip.FlatBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_JsonRoundTrip_FlatBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData original = MakeFlatBundleData();

	FString content = GetJsonContentWithData();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("JSON Flat RoundTrip"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_XmlRoundTrip_FlatBundles,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.XmlRoundTrip.FlatBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_XmlRoundTrip_FlatBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = XmlFormat();
	const FSGDynamicTextAssetBundleData original = MakeFlatBundleData();

	FString content = GetXmlContentWithData();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("XML Flat RoundTrip"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_YamlRoundTrip_FlatBundles,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.YamlRoundTrip.FlatBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_YamlRoundTrip_FlatBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = YamlFormat();
	const FSGDynamicTextAssetBundleData original = MakeFlatBundleData();

	FString content = GetYamlContentWithData();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("YAML Flat RoundTrip"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_JsonRoundTrip_NestedBundles,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.JsonRoundTrip.NestedBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_JsonRoundTrip_NestedBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData original = MakeNestedBundleData();

	FString content = GetJsonContentWithNestedData();
	extender->NotifyPostSerialize(original, content, format);

	// Verify inline wrapping occurred inside nested objects
	TestTrue(TEXT("Content should contain assetBundles inside nested data"),
		content.Contains(TEXT("\"assetBundles\"")));

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("JSON Nested RoundTrip"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_XmlRoundTrip_NestedBundles,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.XmlRoundTrip.NestedBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_XmlRoundTrip_NestedBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = XmlFormat();
	const FSGDynamicTextAssetBundleData original = MakeNestedBundleData();

	FString content = GetXmlContentWithNestedData();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("XML Nested RoundTrip"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_YamlRoundTrip_NestedBundles,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.YamlRoundTrip.NestedBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_YamlRoundTrip_NestedBundles::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDTASerializerFormat format = YamlFormat();
	const FSGDynamicTextAssetBundleData original = MakeNestedBundleData();

	FString content = GetYamlContentWithNestedData();
	extender->NotifyPostSerialize(original, content, format);

	FSGDynamicTextAssetBundleData deserialized;
	const bool result = extender->NotifyPreDeserialize(content, deserialized, format);
	TestTrue(TEXT("Deserialization should succeed"), result);

	VerifyBundleDataMatches(this, original, deserialized, TEXT("YAML Nested RoundTrip"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_FileSize_SmallerThanDefault,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.FileSize.SmallerThanDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_FileSize_SmallerThanDefault::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* perPropertyExtender = CreateExtender();
	USGDTADefaultAssetBundleExtender* defaultExtender = CreateDefaultExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData bundleData = MakeMultiEntryBundleData();

	// Serialize with per-property extender
	FString perPropertyContent = GetJsonContentWithMultiEntryData();
	perPropertyExtender->NotifyPostSerialize(bundleData, perPropertyContent, format);

	// Serialize with default extender (uses root-level block)
	FString defaultContent = GetJsonContentWithMultiEntryData();
	defaultExtender->NotifyPostSerialize(bundleData, defaultContent, format);

	// Per-property should produce smaller output because it avoids duplicating
	// property names and asset paths in a separate block
	TestTrue(
		FString::Printf(TEXT("Per-property output (%d chars) should be smaller than default (%d chars)"),
			perPropertyContent.Len(), defaultContent.Len()),
		perPropertyContent.Len() < defaultContent.Len());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_CrossExtender_DefaultCannotReadInline,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.CrossExtender.DefaultCannotReadInline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_CrossExtender_DefaultCannotReadInline::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* perPropertyExtender = CreateExtender();
	USGDTADefaultAssetBundleExtender* defaultExtender = CreateDefaultExtender();
	const FSGDTASerializerFormat format = JsonFormat();
	const FSGDynamicTextAssetBundleData bundleData = MakeFlatBundleData();

	// Serialize with per-property extender (inline format, no root block)
	FString content = GetJsonContentWithData();
	perPropertyExtender->NotifyPostSerialize(bundleData, content, format);

	// Attempt to deserialize with default extender
	// The default extender looks for a root-level sgdtAssetBundles block
	// which does not exist in per-property format
	FSGDynamicTextAssetBundleData deserialized;
	const bool result = defaultExtender->NotifyPreDeserialize(content, deserialized, format);

	// Default extender should fail to find bundles (no root-level block)
	TestFalse(TEXT("Default extender should return false for per-property content"), result);
	TestFalse(TEXT("Deserialized data should have no bundles"), deserialized.HasBundles());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_Serialize_EmptyBundlesNoOp,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.Serialize.EmptyBundlesNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_Serialize_EmptyBundlesNoOp::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDynamicTextAssetBundleData emptyData;

	const FString originalContent = GetJsonContentWithData();
	FString content = originalContent;
	extender->NotifyPostSerialize(emptyData, content, JsonFormat());

	TestEqual(TEXT("Content should be unchanged when no bundles exist"),
		content, originalContent);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerPropertyExtender_Serialize_UnrecognizedFormatLogsWarning,
	"SGDynamicTextAssets.Runtime.Serialization.PerPropertyAssetBundleExtender.Serialize.UnrecognizedFormatLogsWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerPropertyExtender_Serialize_UnrecognizedFormatLogsWarning::RunTest(const FString& Parameters)
{
	using namespace SGDTAPerPropertyExtenderTestHelpers;

	USGDTAPerPropertyAssetBundleExtender* extender = CreateExtender();
	const FSGDynamicTextAssetBundleData bundleData = MakeFlatBundleData();
	const FSGDTASerializerFormat unknownFormat = FSGDTASerializerFormat::FromTypeId(99);

	AddExpectedError(TEXT("Unrecognized format"), EAutomationExpectedErrorFlags::Contains, 1);

	const FString originalContent = GetJsonContentWithData();
	FString content = originalContent;
	extender->NotifyPostSerialize(bundleData, content, unknownFormat);

	TestEqual(TEXT("Content should be unchanged for unrecognized format"),
		content, originalContent);

	return true;
}
