// Copyright Start Games, Inc. All Rights Reserved.

#include "Server/SGDynamicTextAssetServerNullInterface.h"

#include "SGDynamicTextAssetsRuntimeModule.h"

void USGDynamicTextAssetServerNullInterface::FetchAllDynamicTextAssets(
    const TArray<FSGServerDynamicTextAssetRequest>& LocalObjects,
    FOnServerFetchComplete OnComplete)
{
    UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose, TEXT("Null server interface: FetchAllDynamicTextAssets returning empty updates"));
    OnComplete.ExecuteIfBound(true, TArray<FSGServerDynamicTextAssetResponse>());
}

void USGDynamicTextAssetServerNullInterface::CheckDynamicTextAssetExists(
    const FSGDynamicTextAssetId& Id,
    FOnServerExistsComplete OnComplete)
{
    UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose, TEXT("Null server interface: CheckDynamicTextAssetExists returning false for Id(%s)"), *Id.ToString());
    OnComplete.ExecuteIfBound(true, false, FString());
}

void USGDynamicTextAssetServerNullInterface::FetchTypeManifestOverrides(FOnServerTypeOverridesComplete OnComplete)
{
    UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose, TEXT("Null server interface: FetchTypeManifestOverrides returning no overrides"));
    OnComplete.ExecuteIfBound(true, nullptr);
}
