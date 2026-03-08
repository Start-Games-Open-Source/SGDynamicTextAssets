// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Core/SGDynamicTextAssetValidationResult.h"
#include "Dom/JsonObject.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Tests/SGDynamicTextAssetUnitTest.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_USGDynamicTextAsset_ImplementsInterface,
	"SGDynamicTextAssets.Runtime.Core.Provider.USGDynamicTextAsset.ImplementsInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_USGDynamicTextAsset_ImplementsInterface::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();

	TestTrue(TEXT("USGDynamicTextAsset subclass should implement ISGDynamicTextAssetProvider"),
		testObject->GetClass()->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()));

	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);
	TestNotNull(TEXT("Cast to ISGDynamicTextAssetProvider should succeed"), provider);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_GetSetDynamicTextAssetId_RoundTrip,
	"SGDynamicTextAssets.Runtime.Core.Provider.GetSetDynamicTextAssetId.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_GetSetDynamicTextAssetId_RoundTrip::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();
	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);

	FSGDynamicTextAssetId newId = FSGDynamicTextAssetId::NewGeneratedId();
	provider->SetDynamicTextAssetId(newId);

	TestEqual(TEXT("GetDynamicTextAssetId should return the set ID"), provider->GetDynamicTextAssetId(), newId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_GetSetUserFacingId_RoundTrip,
	"SGDynamicTextAssets.Runtime.Core.Provider.GetSetUserFacingId.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_GetSetUserFacingId_RoundTrip::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();
	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);

	const FString testName = TEXT("TestWeapon_Sword");
	provider->SetUserFacingId(testName);

	TestEqual(TEXT("GetUserFacingId should return the set ID"), provider->GetUserFacingId(), testName);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_GetSetVersion_RoundTrip,
	"SGDynamicTextAssets.Runtime.Core.Provider.GetSetVersion.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_GetSetVersion_RoundTrip::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();
	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);

	const FSGDynamicTextAssetVersion testVersion(3, 2, 1);
	provider->SetVersion(testVersion);

	TestEqual(TEXT("GetVersion should return the set version"), provider->GetVersion(), testVersion);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_GetCurrentMajorVersion_ReturnsPositive,
	"SGDynamicTextAssets.Runtime.Core.Provider.GetCurrentMajorVersion.ReturnsPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_GetCurrentMajorVersion_ReturnsPositive::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();
	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);

	TestTrue(TEXT("GetCurrentMajorVersion should return a positive value"),
		provider->GetCurrentMajorVersion() >= 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_GetCurrentVersion_MatchesMajorVersion,
	"SGDynamicTextAssets.Runtime.Core.Provider.GetCurrentVersion.MatchesMajorVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_GetCurrentVersion_MatchesMajorVersion::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();
	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);

	const FSGDynamicTextAssetVersion currentVersion = provider->GetCurrentVersion();
	const int32 majorVersion = provider->GetCurrentMajorVersion();

	TestEqual(TEXT("GetCurrentVersion().Major should match GetCurrentMajorVersion()"),
		currentVersion.Major, majorVersion);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_GetCurrentVersion_DefaultMinorPatchZero,
	"SGDynamicTextAssets.Runtime.Core.Provider.GetCurrentVersion.DefaultMinorPatchZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_GetCurrentVersion_DefaultMinorPatchZero::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();
	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);

	const FSGDynamicTextAssetVersion currentVersion = provider->GetCurrentVersion();

	TestEqual(TEXT("Default GetCurrentVersion().Minor should be 0"), currentVersion.Minor, 0);
	TestEqual(TEXT("Default GetCurrentVersion().Patch should be 0"), currentVersion.Patch, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProvider_MigrateFromVersion_DefaultReturnsTrue,
	"SGDynamicTextAssets.Runtime.Core.Provider.MigrateFromVersion.DefaultReturnsTrue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProvider_MigrateFromVersion_DefaultReturnsTrue::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetUnitTest* testObject = NewObject<USGDynamicTextAssetUnitTest>();
	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(testObject);

	const FSGDynamicTextAssetVersion oldVersion(1, 0, 0);
	const FSGDynamicTextAssetVersion currentVersion(2, 0, 0);
	TSharedPtr<FJsonObject> emptyJson = MakeShared<FJsonObject>();

	bool bResult = provider->MigrateFromVersion(oldVersion, currentVersion, emptyJson);

	TestTrue(TEXT("Default MigrateFromVersion should return true"), bResult);

	return true;
}

// ============================================================================
// Registry Guard Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_RegisterNull_Fails,
	"SGDynamicTextAssets.Runtime.Management.Registry.RegisterNull.Fails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_RegisterNull_Fails::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry& registry = USGDynamicTextAssetRegistry::GetChecked();

	AddExpectedError(TEXT("Inputted NULL DynamicTextAssetClass"), EAutomationExpectedErrorFlags::Contains, 1);
	bool bResult = registry.RegisterDynamicTextAssetClass(nullptr);

	TestFalse(TEXT("Registering nullptr should fail"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_RegisterActor_Fails,
	"SGDynamicTextAssets.Runtime.Management.Registry.RegisterActor.Fails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_RegisterActor_Fails::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry& registry = USGDynamicTextAssetRegistry::GetChecked();

	AddExpectedError(TEXT("AActor subclasses cannot be dynamic text asset providers"), EAutomationExpectedErrorFlags::Contains, 1);
	bool bResult = registry.RegisterDynamicTextAssetClass(AActor::StaticClass());

	TestFalse(TEXT("Registering AActor should fail"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_RegisterComponent_Fails,
	"SGDynamicTextAssets.Runtime.Management.Registry.RegisterComponent.Fails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_RegisterComponent_Fails::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry& registry = USGDynamicTextAssetRegistry::GetChecked();

	AddExpectedError(TEXT("UActorComponent subclasses cannot be dynamic text asset providers"), EAutomationExpectedErrorFlags::Contains, 1);
	bool bResult = registry.RegisterDynamicTextAssetClass(UActorComponent::StaticClass());

	TestFalse(TEXT("Registering UActorComponent should fail"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_RegisterNonProvider_Fails,
	"SGDynamicTextAssets.Runtime.Management.Registry.RegisterNonProvider.Fails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_RegisterNonProvider_Fails::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry& registry = USGDynamicTextAssetRegistry::GetChecked();

	AddExpectedError(TEXT("does not implement ISGDynamicTextAssetProvider"), EAutomationExpectedErrorFlags::Contains, 1);
	bool bResult = registry.RegisterDynamicTextAssetClass(UObject::StaticClass());

	TestFalse(TEXT("Registering a class without ISGDynamicTextAssetProvider should fail"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_RegisterValidProvider_Succeeds,
	"SGDynamicTextAssets.Runtime.Management.Registry.RegisterValidProvider.Succeeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_RegisterValidProvider_Succeeds::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry& registry = USGDynamicTextAssetRegistry::GetChecked();

	// USGDynamicTextAsset is already registered during module startup, so unregister first to test fresh
	registry.UnregisterDynamicTextAssetClass(USGDynamicTextAsset::StaticClass());

	bool bResult = registry.RegisterDynamicTextAssetClass(USGDynamicTextAsset::StaticClass());
	TestTrue(TEXT("Registering USGDynamicTextAsset (valid provider) should succeed"), bResult);

	// Verify it's in the registry
	TestTrue(TEXT("USGDynamicTextAsset should be a valid dynamic text asset class after registration"),
		registry.IsValidDynamicTextAssetClass(USGDynamicTextAsset::StaticClass()));

	return true;
}
