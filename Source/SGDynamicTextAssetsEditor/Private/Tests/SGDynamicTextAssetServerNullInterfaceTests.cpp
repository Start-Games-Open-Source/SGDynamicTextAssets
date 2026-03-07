// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Server/SGDynamicTextAssetServerNullInterface.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetVersion.h"

// ============================================================================
// USGDynamicTextAssetServerNullInterface Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNullInterface_FetchAllDynamicTextAssets_SucceedsWithEmptyUpdates,
	"SGDynamicTextAssets.Runtime.Server.NullInterface.FetchAllDynamicTextAssets.SucceedsWithEmptyUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNullInterface_FetchAllDynamicTextAssets_SucceedsWithEmptyUpdates::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetServerNullInterface* nullInterface = NewObject<USGDynamicTextAssetServerNullInterface>();

	bool bCallbackFired = false;
	bool bCallbackSuccess = false;
	int32 callbackUpdateCount = -1;

	TArray<FSGServerDynamicTextAssetRequest> requests;
	requests.Add(FSGServerDynamicTextAssetRequest(FSGDynamicTextAssetId::NewGeneratedId(), FSGDynamicTextAssetVersion(1, 0, 0)));
	requests.Add(FSGServerDynamicTextAssetRequest(FSGDynamicTextAssetId::NewGeneratedId(), FSGDynamicTextAssetVersion(2, 1, 0)));

	FOnServerFetchComplete onComplete;
	onComplete.BindLambda([&](bool bSuccess, const TArray<FSGServerDynamicTextAssetResponse>& Updates)
	{
		bCallbackFired = true;
		bCallbackSuccess = bSuccess;
		callbackUpdateCount = Updates.Num();
	});

	nullInterface->FetchAllDynamicTextAssets(requests, onComplete);

	TestTrue(TEXT("Callback should have fired immediately"), bCallbackFired);
	TestTrue(TEXT("Callback should report success"), bCallbackSuccess);
	TestEqual(TEXT("Updates array should be empty"), callbackUpdateCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNullInterface_FetchAllDynamicTextAssets_EmptyRequestSucceeds,
	"SGDynamicTextAssets.Runtime.Server.NullInterface.FetchAllDynamicTextAssets.EmptyRequestSucceeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNullInterface_FetchAllDynamicTextAssets_EmptyRequestSucceeds::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetServerNullInterface* nullInterface = NewObject<USGDynamicTextAssetServerNullInterface>();

	bool bCallbackFired = false;
	bool bCallbackSuccess = false;

	TArray<FSGServerDynamicTextAssetRequest> emptyRequests;

	FOnServerFetchComplete onComplete;
	onComplete.BindLambda([&](bool bSuccess, const TArray<FSGServerDynamicTextAssetResponse>& Updates)
	{
		bCallbackFired = true;
		bCallbackSuccess = bSuccess;
	});

	nullInterface->FetchAllDynamicTextAssets(emptyRequests, onComplete);

	TestTrue(TEXT("Callback should fire even with empty requests"), bCallbackFired);
	TestTrue(TEXT("Callback should report success"), bCallbackSuccess);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNullInterface_CheckDynamicTextAssetExists_ReturnsFalse,
	"SGDynamicTextAssets.Runtime.Server.NullInterface.CheckDynamicTextAssetExists.ReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNullInterface_CheckDynamicTextAssetExists_ReturnsFalse::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetServerNullInterface* nullInterface = NewObject<USGDynamicTextAssetServerNullInterface>();

	bool bCallbackFired = false;
	bool bCallbackSuccess = false;
	bool bCallbackExists = true;
	FString callbackJson;

	const FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();

	FOnServerExistsComplete onComplete;
	onComplete.BindLambda([&](bool bSuccess, bool bExists, const FString& UpdatedJson)
	{
		bCallbackFired = true;
		bCallbackSuccess = bSuccess;
		bCallbackExists = bExists;
		callbackJson = UpdatedJson;
	});

	nullInterface->CheckDynamicTextAssetExists(testId, onComplete);

	TestTrue(TEXT("Callback should have fired immediately"), bCallbackFired);
	TestTrue(TEXT("Callback should report success"), bCallbackSuccess);
	TestFalse(TEXT("Null interface should report object does not exist"), bCallbackExists);
	TestTrue(TEXT("Updated JSON should be empty"), callbackJson.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNullInterface_FetchAllDynamicTextAssets_UnboundDelegateNoOp,
	"SGDynamicTextAssets.Runtime.Server.NullInterface.FetchAllDynamicTextAssets.UnboundDelegateNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNullInterface_FetchAllDynamicTextAssets_UnboundDelegateNoOp::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetServerNullInterface* nullInterface = NewObject<USGDynamicTextAssetServerNullInterface>();

	TArray<FSGServerDynamicTextAssetRequest> requests;
	FOnServerFetchComplete unboundDelegate;

	// Should not crash when delegate is unbound (ExecuteIfBound handles this)
	nullInterface->FetchAllDynamicTextAssets(requests, unboundDelegate);

	TestTrue(TEXT("No crash with unbound delegate"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNullInterface_CheckDynamicTextAssetExists_UnboundDelegateNoOp,
	"SGDynamicTextAssets.Runtime.Server.NullInterface.CheckDynamicTextAssetExists.UnboundDelegateNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNullInterface_CheckDynamicTextAssetExists_UnboundDelegateNoOp::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetServerNullInterface* nullInterface = NewObject<USGDynamicTextAssetServerNullInterface>();

	const FSGDynamicTextAssetId testId = FSGDynamicTextAssetId::NewGeneratedId();
	FOnServerExistsComplete unboundDelegate;

	// Should not crash when delegate is unbound (ExecuteIfBound handles this)
	nullInterface->CheckDynamicTextAssetExists(testId, unboundDelegate);

	TestTrue(TEXT("No crash with unbound delegate"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNullInterface_FetchTypeManifestOverrides_SucceedsWithNullData,
	"SGDynamicTextAssets.Runtime.Server.NullInterface.FetchTypeManifestOverrides.SucceedsWithNullData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNullInterface_FetchTypeManifestOverrides_SucceedsWithNullData::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetServerNullInterface* nullInterface = NewObject<USGDynamicTextAssetServerNullInterface>();

	bool bCallbackFired = false;
	bool bCallbackSuccess = false;
	TSharedPtr<FJsonObject> callbackData;

	FOnServerTypeOverridesComplete onComplete;
	onComplete.BindLambda([&](bool bSuccess, TSharedPtr<FJsonObject> OverrideData)
	{
		bCallbackFired = true;
		bCallbackSuccess = bSuccess;
		callbackData = OverrideData;
	});

	nullInterface->FetchTypeManifestOverrides(onComplete);

	TestTrue(TEXT("Callback should have fired immediately"), bCallbackFired);
	TestTrue(TEXT("Callback should report success"), bCallbackSuccess);
	TestFalse(TEXT("Override data should be null (no overrides)"), callbackData.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNullInterface_FetchTypeManifestOverrides_UnboundDelegateNoOp,
	"SGDynamicTextAssets.Runtime.Server.NullInterface.FetchTypeManifestOverrides.UnboundDelegateNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNullInterface_FetchTypeManifestOverrides_UnboundDelegateNoOp::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetServerNullInterface* nullInterface = NewObject<USGDynamicTextAssetServerNullInterface>();

	FOnServerTypeOverridesComplete unboundDelegate;

	// Should not crash when delegate is unbound (ExecuteIfBound handles this)
	nullInterface->FetchTypeManifestOverrides(unboundDelegate);

	TestTrue(TEXT("No crash with unbound delegate"), true);

	return true;
}
