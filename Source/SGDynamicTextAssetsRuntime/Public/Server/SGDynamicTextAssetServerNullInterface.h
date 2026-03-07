// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Server/ISGDynamicTextAssetServerInterface.h"

#include "SGDynamicTextAssetServerNullInterface.generated.h"

/**
 * Null Object pattern implementation for the server interface.
 *
 * Provides immediate no-op responses for all server operations.
 * When no real server backend is configured, the subsystem uses
 * this interface so that loading flows proceed uniformly without
 * null checks. All responses indicate success with empty data,
 * causing the subsystem to fall through to local file loading.
 *
 * @see USGDynamicTextAssetServerInterface
 * @see USGDynamicTextAssetSubsystem
 */
UCLASS()
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetServerNullInterface : public USGDynamicTextAssetServerInterface
{
    GENERATED_BODY()

public:

    // USGDynamicTextAssetServerInterface interface
    virtual void FetchAllDynamicTextAssets(
        const TArray<FSGServerDynamicTextAssetRequest>& LocalObjects,
        FOnServerFetchComplete OnComplete) override;

    virtual void CheckDynamicTextAssetExists(
        const FSGDynamicTextAssetId& Id,
        FOnServerExistsComplete OnComplete) override;

    virtual void FetchTypeManifestOverrides(FOnServerTypeOverridesComplete OnComplete) override;
    // ~USGDynamicTextAssetServerInterface interface
};
