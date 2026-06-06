// Copyright Start Games, Inc. All Rights Reserved.

#include "SGDTAAssetBundleExtenderTests.h"

#include "Misc/AutomationTest.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDTAClassId.h"
#include "Core/SGDTASerializerFormat.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Dom/JsonObject.h"
#include "Management/SGDTAExtenderManifest.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/SGDTAJsonSerializer.h"
#include "Serialization/SGDTASerializer.h"
#include "Settings/SGDTASettings.h"
#include "Statics/SGDTAConstants.h"
#include "Statics/SGDynamicTextAssetStatics.h"
#include "Tests/SGDTABundleTestTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// Stable test GUIDs for registering test extender classes in the manifest
static const TCHAR* TEST_EXTENDER_A_GUID = TEXT("AAAAAAAA-AAAA-AAAA-AAAA-000000000001");
static const TCHAR* TEST_EXTENDER_B_GUID = TEXT("AAAAAAAA-AAAA-AAAA-AAAA-000000000002");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_GetOrCreate_NullClassReturnsNull,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.GetOrCreate.NullClassReturnsNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_GetOrCreate_NullClassReturnsNull::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry should be available"), registry);
	if (!registry)
	{
		return false;
	}

	AddExpectedError(TEXT("Inputted invalid ExtenderClassId"), EAutomationExpectedErrorFlags::Contains, 1);

	FSGDTAClassId invalidId;
	USGDTAAssetBundleExtender* result = registry->GetOrCreateAssetBundleExtender(invalidId);
	TestNull(TEXT("Invalid ClassId should return null instance"), result);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_GetOrCreate_CreatesInstance,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.GetOrCreate.CreatesInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_GetOrCreate_CreatesInstance::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry should be available"), registry);
	if (!registry)
	{
		return false;
	}

	// Register the test extender in the manifest so it can be resolved by ClassId
	TSharedPtr<FSGDTAExtenderManifest> manifest =
		registry->GetExtenderRegistry().GetOrCreateManifest(SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
	const FSGDTAClassId testIdA = FSGDTAClassId::FromString(TEST_EXTENDER_A_GUID);
	if (!manifest->FindByExtenderId(testIdA))
	{
		manifest->AddExtender(testIdA, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderA::StaticClass()));
	}

	USGDTAAssetBundleExtender* result = registry->GetOrCreateAssetBundleExtender(testIdA);
	TestNotNull(TEXT("Should create a valid instance"), result);

	if (result)
	{
		TestTrue(TEXT("Instance should be of the correct class"),
			result->IsA(USGDTATestAssetBundleExtenderA::StaticClass()));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_GetOrCreate_CacheReturnsSameInstance,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.GetOrCreate.CacheReturnsSameInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_GetOrCreate_CacheReturnsSameInstance::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry should be available"), registry);
	if (!registry)
	{
		return false;
	}

	TSharedPtr<FSGDTAExtenderManifest> manifest =
		registry->GetExtenderRegistry().GetOrCreateManifest(SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
	const FSGDTAClassId testIdA = FSGDTAClassId::FromString(TEST_EXTENDER_A_GUID);
	if (!manifest->FindByExtenderId(testIdA))
	{
		manifest->AddExtender(testIdA, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderA::StaticClass()));
	}

	USGDTAAssetBundleExtender* first = registry->GetOrCreateAssetBundleExtender(testIdA);
	USGDTAAssetBundleExtender* second = registry->GetOrCreateAssetBundleExtender(testIdA);

	TestNotNull(TEXT("First call should return valid instance"), first);
	TestNotNull(TEXT("Second call should return valid instance"), second);
	TestEqual(TEXT("Both calls should return the same cached instance"), first, second);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_GetOrCreate_DifferentClassesDifferentInstances,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.GetOrCreate.DifferentClassesDifferentInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_GetOrCreate_DifferentClassesDifferentInstances::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry should be available"), registry);
	if (!registry)
	{
		return false;
	}

	TSharedPtr<FSGDTAExtenderManifest> manifest =
		registry->GetExtenderRegistry().GetOrCreateManifest(SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
	const FSGDTAClassId testIdA = FSGDTAClassId::FromString(TEST_EXTENDER_A_GUID);
	const FSGDTAClassId testIdB = FSGDTAClassId::FromString(TEST_EXTENDER_B_GUID);
	if (!manifest->FindByExtenderId(testIdA))
	{
		manifest->AddExtender(testIdA, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderA::StaticClass()));
	}
	if (!manifest->FindByExtenderId(testIdB))
	{
		manifest->AddExtender(testIdB, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderB::StaticClass()));
	}

	USGDTAAssetBundleExtender* instanceA = registry->GetOrCreateAssetBundleExtender(testIdA);
	USGDTAAssetBundleExtender* instanceB = registry->GetOrCreateAssetBundleExtender(testIdB);

	TestNotNull(TEXT("Instance A should be valid"), instanceA);
	TestNotNull(TEXT("Instance B should be valid"), instanceB);
	TestNotEqual(TEXT("Different classes should produce different instances"), instanceA, instanceB);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Resolve_NullProviderReturnsNull,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Resolve.NullProviderReturnsNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Resolve_NullProviderReturnsNull::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Inputted NULL Provider"), EAutomationExpectedErrorFlags::Contains, 1);

	TScriptInterface<ISGDynamicTextAssetProvider> nullProvider;
	FSGDTASerializerFormat format = FSGDTASerializerFormat::FromTypeId(1);

	FSGDTAClassId result =
		USGDynamicTextAssetStatics::ResolveAssetBundleExtender(nullProvider, format);

	TestFalse(TEXT("Null provider should resolve to invalid ClassId"), result.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Resolve_PerDTAOverrideTakesPriority,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Resolve.PerDTAOverrideTakesPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Resolve_PerDTAOverrideTakesPriority::RunTest(const FString& Parameters)
{
	USGDTABundleTestDynamicTextAsset* asset = NewObject<USGDTABundleTestDynamicTextAsset>();
	TestNotNull(TEXT("Test asset should be created"), asset);
	if (!asset)
	{
		return false;
	}

	// Register ExtenderA in the manifest so the per-DTA ClassId can be resolved
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry should be available"), registry);
	if (!registry)
	{
		return false;
	}

	TSharedPtr<FSGDTAExtenderManifest> manifest =
		registry->GetExtenderRegistry().GetOrCreateManifest(SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
	const FSGDTAClassId testIdA = FSGDTAClassId::FromString(TEST_EXTENDER_A_GUID);
	if (!manifest->FindByExtenderId(testIdA))
	{
		manifest->AddExtender(testIdA, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderA::StaticClass()));
	}

	// Set per-DTA override to ExtenderA via property reflection (protected member)
	{
		const FProperty* prop = USGDTABundleTestDynamicTextAsset::StaticClass()->FindPropertyByName(
			TEXT("AssetBundleExtenderOverride"));
		TestNotNull(TEXT("AssetBundleExtenderOverride property should exist"), prop);
		if (prop)
		{
			prop->CopyCompleteValue(prop->ContainerPtrToValuePtr<void>(asset), &testIdA);
		}
	}

	TScriptInterface<ISGDynamicTextAssetProvider> provider(asset);
	FSGDTASerializerFormat format = FSGDTASerializerFormat::FromTypeId(1);

	FSGDTAClassId result =
		USGDynamicTextAssetStatics::ResolveAssetBundleExtender(provider, format);

	TestTrue(TEXT("Should resolve to a valid ClassId"), result.IsValid());
	TestEqual(TEXT("Should resolve to ExtenderA's ClassId"), result, testIdA);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Resolve_NoMatchReturnsNull,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Resolve.NoMatchReturnsNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Resolve_NoMatchReturnsNull::RunTest(const FString& Parameters)
{
	USGDTABundleTestDynamicTextAsset* asset = NewObject<USGDTABundleTestDynamicTextAsset>();
	TestNotNull(TEXT("Test asset should be created"), asset);
	if (!asset)
	{
		return false;
	}

	// No per-DTA override set, no settings override.
	// The baseline default (Tier 3) should resolve to the default extender
	// if the registry is available, or invalid if not.
	TScriptInterface<ISGDynamicTextAssetProvider> provider(asset);
	FSGDTASerializerFormat unusedFormat = FSGDTASerializerFormat::FromTypeId(31);

	FSGDTAClassId result =
		USGDynamicTextAssetStatics::ResolveAssetBundleExtender(provider, unusedFormat);

	if (USGDynamicTextAssetRegistry::Get())
	{
		// Registry available: Tier 3 baseline default should resolve
		TestTrue(TEXT("Should resolve to valid ClassId via baseline default"), result.IsValid());
	}
	else
	{
		// Registry unavailable: no extender can be resolved
		TestFalse(TEXT("Should resolve to invalid ClassId when registry is unavailable"), result.IsValid());
	}

	return true;
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Validation_FailsWithNullExtenderClass,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Validation.FailsWithNullExtenderClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Validation_FailsWithNullExtenderClass::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetSettingsAsset* settings = NewObject<USGDynamicTextAssetSettingsAsset>();
	TestNotNull(TEXT("Settings asset should be created"), settings);
	if (!settings)
	{
		return false;
	}

	// Add a mapping with an invalid (default-constructed) ExtenderClassId
	FSGAssetBundleExtenderMapping mapping;
	mapping.AppliesTo = FSGDTASerializerFormat::FromTypeId(1);
	// ExtenderClassId left default-constructed (invalid) to trigger validation failure
	settings->AssetBundleExtenderOverrides.Add(mapping);

	FDataValidationContext context;
	EDataValidationResult result = settings->IsDataValid(context);

	TestEqual(TEXT("Validation should fail with null extender class"),
		result, EDataValidationResult::Invalid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Validation_FailsWithDuplicateFormat,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Validation.FailsWithDuplicateFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Validation_FailsWithDuplicateFormat::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetSettingsAsset* settings = NewObject<USGDynamicTextAssetSettingsAsset>();
	TestNotNull(TEXT("Settings asset should be created"), settings);
	if (!settings)
	{
		return false;
	}

	// Add two mappings both covering format 1 (bit 1 << 1 = 2) with distinct valid ClassIds
	FSGAssetBundleExtenderMapping mappingA;
	mappingA.AppliesTo = FSGDTASerializerFormat::FromTypeId(1 << 1);
	mappingA.ExtenderClassId = FSGDTAClassId::FromString(TEST_EXTENDER_A_GUID);
	settings->AssetBundleExtenderOverrides.Add(mappingA);

	FSGAssetBundleExtenderMapping mappingB;
	mappingB.AppliesTo = FSGDTASerializerFormat::FromTypeId(1 << 1);
	mappingB.ExtenderClassId = FSGDTAClassId::FromString(TEST_EXTENDER_B_GUID);
	settings->AssetBundleExtenderOverrides.Add(mappingB);

	FDataValidationContext context;
	EDataValidationResult result = settings->IsDataValid(context);

	TestEqual(TEXT("Validation should fail with duplicate format coverage"),
		result, EDataValidationResult::Invalid);

	return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Serialization_RoundTripOverridePreserved,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Serialization.RoundTripOverridePreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Serialization_RoundTripOverridePreserved::RunTest(const FString& Parameters)
{
	// Test class is Hidden so it has no Asset Type ID; suppress the fallback-to-classname warning
	AddExpectedError(TEXT("No valid Asset Type ID found"), EAutomationExpectedErrorFlags::Contains, 1);

	const FSGDTAClassId testOverrideId = FSGDTAClassId::FromString(TEST_EXTENDER_A_GUID);

	// Register the test extender in the manifest so the serializer can resolve it
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	if (registry)
	{
		TSharedPtr<FSGDTAExtenderManifest> manifest =
			registry->GetExtenderRegistry().GetOrCreateManifest(SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
		if (!manifest->FindByExtenderId(testOverrideId))
		{
			manifest->AddExtender(testOverrideId, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderA::StaticClass()));
		}
	}

	// Set up source asset with override
	USGDTABundleTestDynamicTextAsset* source = NewObject<USGDTABundleTestDynamicTextAsset>();
	source->SetDynamicTextAssetId(FSGDynamicTextAssetId::NewGeneratedId());
	source->SetUserFacingId(TEXT("RoundTripTest"));
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->SetAssetBundleExtenderOverride(testOverrideId);

	// Serialize
	FSGDynamicTextAssetJsonSerializer serializer;
	FString serializedContent;
	const bool bSerialized = serializer.SerializeProvider(source, serializedContent);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Deserialize into a fresh instance
	USGDTABundleTestDynamicTextAsset* target = NewObject<USGDTABundleTestDynamicTextAsset>();
	bool bMigrated = false;
	const bool bDeserialized = serializer.DeserializeProvider(serializedContent, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	// Verify override preserved
	TestTrue(TEXT("Deserialized override should be valid"),
		target->GetAssetBundleExtenderOverride().IsValid());
	TestEqual(TEXT("Deserialized override should match original"),
		target->GetAssetBundleExtenderOverride(), testOverrideId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Serialization_NoOverrideKeyProducesInvalidClassId,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Serialization.NoOverrideKeyProducesInvalidClassId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Serialization_NoOverrideKeyProducesInvalidClassId::RunTest(const FString& Parameters)
{
	// Minimal JSON with no assetBundleExtender key
	const FString json = FString::Printf(
		TEXT("{ \"%s\": { \"%s\": \"%s\", \"%s\": \"1.0.0\", \"%s\": \"%s\", \"%s\": \"TestNoOverride\", \"%s\": \"2.0.0\" }, \"%s\": {} }"),
		*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*USGDTABundleTestDynamicTextAsset::StaticClass()->GetName(),
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*FSGDynamicTextAssetId::NewGeneratedId().ToString(),
		*ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID,
		*ISGDynamicTextAssetSerializer::KEY_FILE_FORMAT_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	USGDTABundleTestDynamicTextAsset* target = NewObject<USGDTABundleTestDynamicTextAsset>();
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bMigrated = false;
	const bool bDeserialized = serializer.DeserializeProvider(json, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestFalse(TEXT("Override should be invalid when key is absent"),
		target->GetAssetBundleExtenderOverride().IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Serialization_ValidOverrideKeySetsClassId,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Serialization.ValidOverrideKeySetsClassId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Serialization_ValidOverrideKeySetsClassId::RunTest(const FString& Parameters)
{
	const FSGDTAClassId expectedId = FSGDTAClassId::FromString(TEST_EXTENDER_B_GUID);

	// Register the test extender in the manifest so the serializer can resolve it
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	if (registry)
	{
		TSharedPtr<FSGDTAExtenderManifest> manifest =
			registry->GetExtenderRegistry().GetOrCreateManifest(SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
		if (!manifest->FindByExtenderId(expectedId))
		{
			manifest->AddExtender(expectedId, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderB::StaticClass()));
		}
	}

	// JSON with assetBundleExtender key set
	const FString json = FString::Printf(
		TEXT("{ \"%s\": { \"%s\": \"%s\", \"%s\": \"1.0.0\", \"%s\": \"%s\", \"%s\": \"TestWithOverride\", \"%s\": \"2.0.0\", \"%s\": \"%s\" }, \"%s\": {} }"),
		*ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION,
		*ISGDynamicTextAssetSerializer::KEY_TYPE,
		*USGDTABundleTestDynamicTextAsset::StaticClass()->GetName(),
		*ISGDynamicTextAssetSerializer::KEY_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_ID,
		*FSGDynamicTextAssetId::NewGeneratedId().ToString(),
		*ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID,
		*ISGDynamicTextAssetSerializer::KEY_FILE_FORMAT_VERSION,
		*ISGDynamicTextAssetSerializer::KEY_ASSET_BUNDLE_EXTENDER,
		*expectedId.ToString(),
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	USGDTABundleTestDynamicTextAsset* target = NewObject<USGDTABundleTestDynamicTextAsset>();
	FSGDynamicTextAssetJsonSerializer serializer;
	bool bMigrated = false;
	const bool bDeserialized = serializer.DeserializeProvider(json, target, bMigrated);
	TestTrue(TEXT("Deserialization should succeed"), bDeserialized);

	TestTrue(TEXT("Override should be valid"),
		target->GetAssetBundleExtenderOverride().IsValid());
	TestEqual(TEXT("Override should match expected ClassId"),
		target->GetAssetBundleExtenderOverride(), expectedId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Serialization_NoOverrideOmitsKey,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Serialization.NoOverrideOmitsKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Serialization_NoOverrideOmitsKey::RunTest(const FString& Parameters)
{
	// Test class is Hidden so it has no Asset Type ID; suppress the fallback-to-classname warning
	AddExpectedError(TEXT("No valid Asset Type ID found"), EAutomationExpectedErrorFlags::Contains, 1);

	// Create asset with no override (default invalid ClassId)
	USGDTABundleTestDynamicTextAsset* source = NewObject<USGDTABundleTestDynamicTextAsset>();
	source->SetDynamicTextAssetId(FSGDynamicTextAssetId::NewGeneratedId());
	source->SetUserFacingId(TEXT("NoOverrideTest"));
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));

	FSGDynamicTextAssetJsonSerializer serializer;
	FString serializedContent;
	const bool bSerialized = serializer.SerializeProvider(source, serializedContent);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Verify the key is not present in the output
	TestFalse(TEXT("Serialized JSON should not contain assetBundleExtender key"),
		serializedContent.Contains(ISGDynamicTextAssetSerializer::KEY_ASSET_BUNDLE_EXTENDER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetBundleExtender_Serialization_ValidOverrideIncludesKey,
	"SGDynamicTextAssets.Runtime.Serialization.AssetBundleExtender.Serialization.ValidOverrideIncludesKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetBundleExtender_Serialization_ValidOverrideIncludesKey::RunTest(const FString& Parameters)
{
	// Test class is Hidden so it has no Asset Type ID; suppress the fallback-to-classname warning
	AddExpectedError(TEXT("No valid Asset Type ID found"), EAutomationExpectedErrorFlags::Contains, 1);

	const FSGDTAClassId testOverrideId = FSGDTAClassId::FromString(TEST_EXTENDER_A_GUID);

	// Register the test extender in the manifest so the serializer can resolve it
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	if (registry)
	{
		TSharedPtr<FSGDTAExtenderManifest> manifest =
			registry->GetExtenderRegistry().GetOrCreateManifest(SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
		if (!manifest->FindByExtenderId(testOverrideId))
		{
			manifest->AddExtender(testOverrideId, TSoftClassPtr<UObject>(USGDTATestAssetBundleExtenderA::StaticClass()));
		}
	}

	USGDTABundleTestDynamicTextAsset* source = NewObject<USGDTABundleTestDynamicTextAsset>();
	source->SetDynamicTextAssetId(FSGDynamicTextAssetId::NewGeneratedId());
	source->SetUserFacingId(TEXT("WithOverrideTest"));
	source->SetVersion(FSGDynamicTextAssetVersion(1, 0, 0));
	source->SetAssetBundleExtenderOverride(testOverrideId);

	FSGDynamicTextAssetJsonSerializer serializer;
	FString serializedContent;
	const bool bSerialized = serializer.SerializeProvider(source, serializedContent);
	TestTrue(TEXT("Serialization should succeed"), bSerialized);

	// Verify the key is present with the correct GUID
	TestTrue(TEXT("Serialized JSON should contain assetBundleExtender key"),
		serializedContent.Contains(ISGDynamicTextAssetSerializer::KEY_ASSET_BUNDLE_EXTENDER));
	TestTrue(TEXT("Serialized JSON should contain the override GUID value"),
		serializedContent.Contains(testOverrideId.ToString()));

	return true;
}
