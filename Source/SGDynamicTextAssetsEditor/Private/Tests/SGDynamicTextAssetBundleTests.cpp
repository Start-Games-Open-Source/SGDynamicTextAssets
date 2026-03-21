// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDynamicTextAssetBundleData.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Tests/SGDynamicTextAssetBundleTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_FindsSoftObjectAndClassRefs,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.FindsSoftObjectAndClassRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_FindsSoftObjectAndClassRefs::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->VisualRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/TestVisual.TestVisual")));
	asset->AudioRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/TestAudio.TestAudio")));
	asset->ClassRef = TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Game/TestClass.TestClass_C")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	TestTrue(TEXT("Should have bundles"), bundleData.HasBundles());

	const FSGDynamicTextAssetBundle* visualBundle = bundleData.FindBundle(FName(TEXT("Visual")));
	TestNotNull(TEXT("Visual bundle should exist"), visualBundle);
	if (visualBundle)
	{
		TestEqual(TEXT("Visual bundle should have 1 entry (VisualRef only, SharedRef has no value set)"),
			visualBundle->Entries.Num(), 1);
	}

	const FSGDynamicTextAssetBundle* audioBundle = bundleData.FindBundle(FName(TEXT("Audio")));
	TestNotNull(TEXT("Audio bundle should exist"), audioBundle);

	const FSGDynamicTextAssetBundle* classesBundle = bundleData.FindBundle(FName(TEXT("Classes")));
	TestNotNull(TEXT("Classes bundle should exist"), classesBundle);
	if (classesBundle)
	{
		TestEqual(TEXT("Classes bundle should have 1 entry"), classesBundle->Entries.Num(), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_CommaSeparatedBundleNames,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.CommaSeparatedBundleNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_CommaSeparatedBundleNames::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->SharedRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Shared.Shared")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	const FSGDynamicTextAssetBundle* visualBundle = bundleData.FindBundle(FName(TEXT("Visual")));
	const FSGDynamicTextAssetBundle* audioBundle = bundleData.FindBundle(FName(TEXT("Audio")));

	TestNotNull(TEXT("SharedRef should appear in Visual bundle"), visualBundle);
	TestNotNull(TEXT("SharedRef should appear in Audio bundle"), audioBundle);

	if (visualBundle)
	{
		bool bFound = false;
		for (const FSGDynamicTextAssetBundleEntry& entry : visualBundle->Entries)
		{
			if (entry.AssetPath.ToString().Contains(TEXT("Shared")))
			{
				bFound = true;
				break;
			}
		}
		TestTrue(TEXT("Visual bundle should contain the Shared path"), bFound);
	}

	if (audioBundle)
	{
		bool bFound = false;
		for (const FSGDynamicTextAssetBundleEntry& entry : audioBundle->Entries)
		{
			if (entry.AssetPath.ToString().Contains(TEXT("Shared")))
			{
				bFound = true;
				break;
			}
		}
		TestTrue(TEXT("Audio bundle should contain the Shared path"), bFound);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_UnbundledRefExcluded,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.UnbundledRefExcluded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_UnbundledRefExcluded::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->UnbundledRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Unbundled.Unbundled")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	// With only UnbundledRef set (no bundle tag), there should be no bundles
	TestFalse(TEXT("Unbundled soft ref should not create any bundles"), bundleData.HasBundles());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_StructNestedSoftRef,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.StructNestedSoftRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_StructNestedSoftRef::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->StructWithBundle.StructSoftRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/StructAsset.StructAsset")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	const FSGDynamicTextAssetBundle* structBundle = bundleData.FindBundle(FName(TEXT("StructBundle")));
	TestNotNull(TEXT("StructBundle should exist from nested struct property"), structBundle);
	if (structBundle)
	{
		TestEqual(TEXT("StructBundle should have 1 entry"), structBundle->Entries.Num(), 1);
		TestTrue(TEXT("Entry path should match"), structBundle->Entries[0].AssetPath.ToString().Contains(TEXT("StructAsset")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_ArraySoftRefs,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.ArraySoftRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_ArraySoftRefs::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->ArrayRefs.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Arr0.Arr0"))));
	asset->ArrayRefs.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Arr1.Arr1"))));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	const FSGDynamicTextAssetBundle* arrayBundle = bundleData.FindBundle(FName(TEXT("ArrayBundle")));
	TestNotNull(TEXT("ArrayBundle should exist"), arrayBundle);
	if (arrayBundle)
	{
		TestEqual(TEXT("ArrayBundle should have 2 entries"), arrayBundle->Entries.Num(), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_MapSoftRefs,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.MapSoftRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_MapSoftRefs::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->MapRefs.Add(FName(TEXT("Key1")), TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Map1.Map1"))));
	asset->MapRefs.Add(FName(TEXT("Key2")), TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Map2.Map2"))));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	const FSGDynamicTextAssetBundle* mapBundle = bundleData.FindBundle(FName(TEXT("MapBundle")));
	TestNotNull(TEXT("MapBundle should exist"), mapBundle);
	if (mapBundle)
	{
		TestEqual(TEXT("MapBundle should have 2 entries"), mapBundle->Entries.Num(), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_SetSoftRefs,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.SetSoftRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_SetSoftRefs::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->SetRefs.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Set0.Set0"))));
	asset->SetRefs.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Set1.Set1"))));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	const FSGDynamicTextAssetBundle* setBundle = bundleData.FindBundle(FName(TEXT("SetBundle")));
	TestNotNull(TEXT("SetBundle should exist"), setBundle);
	if (setBundle)
	{
		TestEqual(TEXT("SetBundle should have 2 entries"), setBundle->Entries.Num(), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_ExtractFromObject_InstancedSubObject,
	"SGDynamicTextAssets.Runtime.Bundle.ExtractFromObject.InstancedSubObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_ExtractFromObject_InstancedSubObject::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->InstancedObj = NewObject<USGBundleTestInstancedObject>(asset);
	asset->InstancedObj->InstancedSoftRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/InstancedAsset.InstancedAsset")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	const FSGDynamicTextAssetBundle* instancedBundle = bundleData.FindBundle(FName(TEXT("InstancedBundle")));
	TestNotNull(TEXT("InstancedBundle should exist from instanced sub-object"), instancedBundle);
	if (instancedBundle)
	{
		TestEqual(TEXT("InstancedBundle should have 1 entry"), instancedBundle->Entries.Num(), 1);
		TestTrue(TEXT("Entry path should match"),
			instancedBundle->Entries[0].AssetPath.ToString().Contains(TEXT("InstancedAsset")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_JsonRoundTrip_PreservesBundleMetadata,
	"SGDynamicTextAssets.Runtime.Bundle.JsonRoundTrip.PreservesBundleMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_JsonRoundTrip_PreservesBundleMetadata::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains);
	// Setup: create a DTA with bundled soft references
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->SetDynamicTextAssetId(FSGDynamicTextAssetId(FGuid::NewGuid()));
	asset->SetUserFacingId(TEXT("BundleTestAsset"));
	asset->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	asset->VisualRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/TestVisual.TestVisual")));
	asset->AudioRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/TestAudio.TestAudio")));

	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(asset);
	TestNotNull(TEXT("Provider cast should succeed"), provider);
	if (!provider) return false;

	// Serialize to JSON
	FSGDynamicTextAssetJsonSerializer serializer;
	FString jsonOutput;
	bool bSerialized = serializer.SerializeProvider(provider, jsonOutput);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);
	TestTrue(TEXT("JSON should contain sgdtAssetBundles key"),
		jsonOutput.Contains(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES));

	// Extract bundles from the serialized string
	FSGDynamicTextAssetBundleData extractedBundles;
	bool bExtracted = serializer.ExtractSGDTAssetBundles(jsonOutput, extractedBundles);
	TestTrue(TEXT("Bundle extraction should succeed"), bExtracted);
	TestTrue(TEXT("Extracted bundles should have data"), extractedBundles.HasBundles());

	const FSGDynamicTextAssetBundle* visualBundle = extractedBundles.FindBundle(FName(TEXT("Visual")));
	TestNotNull(TEXT("Visual bundle should be extractable from JSON"), visualBundle);
	if (visualBundle)
	{
		TestEqual(TEXT("Visual bundle should have 1 entry"), visualBundle->Entries.Num(), 1);
	}

	const FSGDynamicTextAssetBundle* audioBundle = extractedBundles.FindBundle(FName(TEXT("Audio")));
	TestNotNull(TEXT("Audio bundle should be extractable from JSON"), audioBundle);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_FindBundle_ReturnsNullForNonExistent,
	"SGDynamicTextAssets.Runtime.Bundle.FindBundle.ReturnsNullForNonExistent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_FindBundle_ReturnsNullForNonExistent::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->VisualRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test.Test")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	const FSGDynamicTextAssetBundle* nonExistent = bundleData.FindBundle(FName(TEXT("DoesNotExist")));
	TestNull(TEXT("FindBundle should return nullptr for non-existent bundle"), nonExistent);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_GetBundleNames_ReturnsAllNames,
	"SGDynamicTextAssets.Runtime.Bundle.GetBundleNames.ReturnsAllNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_GetBundleNames_ReturnsAllNames::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->VisualRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/V.V")));
	asset->AudioRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/A.A")));
	asset->ClassRef = TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Game/C.C_C")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	TArray<FName> bundleNames;
	bundleData.GetBundleNames(bundleNames);

	TestTrue(TEXT("Should have Visual bundle"), bundleNames.Contains(FName(TEXT("Visual"))));
	TestTrue(TEXT("Should have Audio bundle"), bundleNames.Contains(FName(TEXT("Audio"))));
	TestTrue(TEXT("Should have Classes bundle"), bundleNames.Contains(FName(TEXT("Classes"))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_GetPathsForBundle_CollectsPaths,
	"SGDynamicTextAssets.Runtime.Bundle.GetPathsForBundle.CollectsPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_GetPathsForBundle_CollectsPaths::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->VisualRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Visual.Visual")));
	asset->SharedRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Shared.Shared")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);

	TArray<FSoftObjectPath> visualPaths;
	bool bFound = bundleData.GetPathsForBundle(FName(TEXT("Visual")), visualPaths);
	TestTrue(TEXT("GetPathsForBundle should return true for Visual"), bFound);
	TestEqual(TEXT("Visual bundle should have 2 paths (VisualRef + SharedRef)"), visualPaths.Num(), 2);

	TArray<FSoftObjectPath> nonExistentPaths;
	bool bNotFound = bundleData.GetPathsForBundle(FName(TEXT("DoesNotExist")), nonExistentPaths);
	TestFalse(TEXT("GetPathsForBundle should return false for non-existent bundle"), bNotFound);
	TestEqual(TEXT("Should have 0 paths for non-existent bundle"), nonExistentPaths.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBundle_Reset_ClearsAllData,
	"SGDynamicTextAssets.Runtime.Bundle.Reset.ClearsAllData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBundle_Reset_ClearsAllData::RunTest(const FString& Parameters)
{
	USGBundleTestDynamicTextAsset* asset = NewObject<USGBundleTestDynamicTextAsset>();
	asset->VisualRef = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test.Test")));

	FSGDynamicTextAssetBundleData bundleData;
	bundleData.ExtractFromObject(asset);
	TestTrue(TEXT("Should have bundles before reset"), bundleData.HasBundles());

	bundleData.Reset();
	TestFalse(TEXT("Should have no bundles after reset"), bundleData.HasBundles());
	TestEqual(TEXT("Bundles array should be empty after reset"), bundleData.Bundles.Num(), 0);

	return true;
}
