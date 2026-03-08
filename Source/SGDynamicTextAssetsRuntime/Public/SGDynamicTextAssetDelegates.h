// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SGDynamicTextAssetDelegates.generated.h"

class ISGDynamicTextAssetProvider;

UDELEGATE()
/** Dynamic Delegate for async dynamic text asset load completion. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnDynamicTextAssetRefLoaded, const TScriptInterface<ISGDynamicTextAssetProvider>&, LoadedProvider, bool, bSuccess);
/** Delegate for async dynamic text asset load completion. */
DECLARE_DELEGATE_TwoParams(FOnDynamicTextAssetLoaded, const TScriptInterface<ISGDynamicTextAssetProvider>& /*LoadedProvider*/, bool /*bSuccess*/);

UDELEGATE()
/** Dynamic Multicast Delegate that broadcasts with a constant ISGDynamicTextAssetProvider. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDynamicTextAssetProvider_Dynamic_Multi, const TScriptInterface<ISGDynamicTextAssetProvider>&, Provider);

/** Multicast Delegate that broadcasts with a copied ISGDynamicTextAssetProvider. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDynamicTextAssetProvider_Multi, TScriptInterface<ISGDynamicTextAssetProvider> /*Provider*/);