// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"

/**
 * Minimal test serializer for testing the registry.
 * Implements ISGDynamicTextAssetSerializer with no-op methods.
 */
class FSGTestSerializer : public ISGDynamicTextAssetSerializer
{
public:

	FSGTestSerializer() = default;

	// ISGDynamicTextAssetSerializer overrides
	virtual FString GetFileExtension() const override { return TEXT(".dta.test"); }
	virtual FText GetFormatName() const override { return INVTEXT("Test"); }
#if !UE_BUILD_SHIPPING
	virtual FText GetFormatDescription() const override { return FText::GetEmpty(); }
#endif
	virtual uint32 GetSerializerTypeId() const override { return 99; }
	virtual FSGDynamicTextAssetVersion GetFileFormatVersion() const override { return FSGDynamicTextAssetVersion(1, 0, 0); }

	virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider,
		FString& OutString) const override
	{
		OutString = TEXT("test_serialized");
		return true;
	}

	virtual bool DeserializeProvider(const FString& InString,
		ISGDynamicTextAssetProvider* OutProvider,
		bool& bOutMigrated) const override
	{
		bOutMigrated = false;
		return true;
	}

	virtual bool ValidateStructure(const FString& InString, FString& OutErrorMessage) const override
	{
		return true;
	}

	virtual bool ExtractMetadata(const FString& InString, FSGDynamicTextAssetFileMetadata& OutMetadata) const override
	{
		OutMetadata.AssetTypeId.Invalidate();
		OutMetadata.bIsValid = false;
		return false;
	}
	virtual bool UpdateFieldsInPlace(FString& InOutContents,
		const TMap<FString, FString>& FieldUpdates) const override
	{
		return true;
	}
	virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const override
	{
		return FString();
	}
	// ~ISGDynamicTextAssetSerializer overrides
};

/**
 * Second test serializer with a different extension for multi-registration tests.
 */
class FSGTestAltSerializer : public ISGDynamicTextAssetSerializer
{
public:

	FSGTestAltSerializer() = default;

	// ISGDynamicTextAssetSerializer overrides
	virtual FString GetFileExtension() const override { return TEXT(".dta.test.alt"); }
	virtual FText GetFormatName() const override { return INVTEXT("Test Alt"); }
#if !UE_BUILD_SHIPPING
	virtual FText GetFormatDescription() const override { return FText::GetEmpty(); }
#endif
	virtual uint32 GetSerializerTypeId() const override { return 98; }
	virtual FSGDynamicTextAssetVersion GetFileFormatVersion() const override { return FSGDynamicTextAssetVersion(1, 0, 0); }

	virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider,
		FString& OutString) const override
	{
		OutString = TEXT("test_alt_serialized");
		return true;
	}

	virtual bool DeserializeProvider(const FString& InString,
		ISGDynamicTextAssetProvider* OutProvider,
		bool& bOutMigrated) const override
	{
		bOutMigrated = false;
		return true;
	}

	virtual bool ValidateStructure(const FString& InString,
		FString& OutErrorMessage) const override
	{
		return true;
	}

	virtual bool ExtractMetadata(const FString& InString, FSGDynamicTextAssetFileMetadata& OutMetadata) const override
	{
		OutMetadata.AssetTypeId.Invalidate();
		OutMetadata.bIsValid = false;
		return false;
	}
	virtual bool UpdateFieldsInPlace(FString& InOutContents,
		const TMap<FString, FString>& FieldUpdates) const override
	{
		return true;
	}
	virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const override
	{
		return FString();
	}
	// ~ISGDynamicTextAssetSerializer overrides
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerRegistry_JsonRegisteredByDefault,
	"SGDynamicTextAssets.Runtime.Management.SerializerRegistry.JsonRegisteredByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerRegistry_JsonRegisteredByDefault::RunTest(const FString& Parameters)
{
	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.json"));

	TestTrue(TEXT("JSON serializer should be registered by default"), serializer.IsValid());

	if (serializer.IsValid())
	{
		TestEqual(TEXT("JSON serializer format name should be correct"),
			serializer->GetFormatName_String(), FSGDynamicTextAssetJsonSerializer().GetFormatName_String());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerRegistry_RegisterAndFindCustomSerializer,
	"SGDynamicTextAssets.Runtime.Management.SerializerRegistry.RegisterAndFindCustomSerializer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerRegistry_RegisterAndFindCustomSerializer::RunTest(const FString& Parameters)
{
	// Register the test serializer
	FSGDynamicTextAssetFileManager::RegisterSerializer<FSGTestSerializer>();

	TSharedPtr<ISGDynamicTextAssetSerializer> found = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.test"));
	TestTrue(TEXT("Test serializer should be found after registration"), found.IsValid());

	if (found.IsValid())
	{
		TestEqual(TEXT("Found serializer should have correct format name"),
			found->GetFormatName_String(), FString(TEXT("Test")));
	}

	// Cleanup
	FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGTestSerializer>();

	TSharedPtr<ISGDynamicTextAssetSerializer> afterUnregister = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.test"));
	TestFalse(TEXT("Test serializer should not be found after unregistration"), afterUnregister.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerRegistry_FindSerializerForFile_DoubleExtension,
	"SGDynamicTextAssets.Runtime.Management.SerializerRegistry.FindSerializerForFile.DoubleExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerRegistry_FindSerializerForFile_DoubleExtension::RunTest(const FString& Parameters)
{
	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(
		TEXT("Content/SGDynamicTextAssets/WeaponData/excalibur.dta.json"));

	TestTrue(TEXT("Should find JSON serializer for .dta.json file path"), serializer.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerRegistry_FindSerializerForExtension_CaseInsensitive,
	"SGDynamicTextAssets.Runtime.Management.SerializerRegistry.FindSerializerForExtension.CaseInsensitive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerRegistry_FindSerializerForExtension_CaseInsensitive::RunTest(const FString& Parameters)
{
	TSharedPtr<ISGDynamicTextAssetSerializer> lower = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.json"));
	TSharedPtr<ISGDynamicTextAssetSerializer> upper = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".DTA.JSON"));

	TestTrue(TEXT("Should find serializer with lowercase extension"), lower.IsValid());
	TestTrue(TEXT("Should find serializer with uppercase extension"), upper.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerRegistry_FindUnknownExtension_ReturnsNull,
	"SGDynamicTextAssets.Runtime.Management.SerializerRegistry.FindUnknownExtension.ReturnsNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerRegistry_FindUnknownExtension_ReturnsNull::RunTest(const FString& Parameters)
{
	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT("dta.xml"));

	TestFalse(TEXT("Should not find serializer for unregistered extension"), serializer.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerRegistry_GetAllRegisteredExtensions,
	"SGDynamicTextAssets.Runtime.Management.SerializerRegistry.GetAllRegisteredExtensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerRegistry_GetAllRegisteredExtensions::RunTest(const FString& Parameters)
{
	TArray<FString> extensions;
	FSGDynamicTextAssetFileManager::GetAllRegisteredExtensions(extensions);

	TestTrue(TEXT("Should have at least one registered extension"), extensions.Num() >= 1);

	bool bFoundJson = false;
	for (const FString& ext : extensions)
	{
		if (ext.Equals(TEXT(".dta.json"), ESearchCase::IgnoreCase))
		{
			bFoundJson = true;
			break;
		}
	}
	TestTrue(TEXT("ddo.json should be in the registered extensions list"), bFoundJson);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSerializerRegistry_MultipleSerializers_IndependentLookup,
	"SGDynamicTextAssets.Runtime.Management.SerializerRegistry.MultipleSerializers.IndependentLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializerRegistry_MultipleSerializers_IndependentLookup::RunTest(const FString& Parameters)
{
	// Register two custom test serializers
	FSGDynamicTextAssetFileManager::RegisterSerializer<FSGTestSerializer>();
	FSGDynamicTextAssetFileManager::RegisterSerializer<FSGTestAltSerializer>();

	TSharedPtr<ISGDynamicTextAssetSerializer> test = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.test"));
	TSharedPtr<ISGDynamicTextAssetSerializer> alt = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.test.alt"));
	TSharedPtr<ISGDynamicTextAssetSerializer> json = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TEXT(".dta.json"));

	TestTrue(TEXT("Test serializer should be found"), test.IsValid());
	TestTrue(TEXT("Test Alt serializer should be found"), alt.IsValid());
	TestTrue(TEXT("JSON serializer should still be found"), json.IsValid());

	if (test.IsValid() && alt.IsValid())
	{
		TestNotEqual(TEXT("Test and Test Alt should be different serializers"),
			test->GetFormatName_String(), alt->GetFormatName_String());
	}

	// Cleanup
	FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGTestSerializer>();
	FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGTestAltSerializer>();

	return true;
}
