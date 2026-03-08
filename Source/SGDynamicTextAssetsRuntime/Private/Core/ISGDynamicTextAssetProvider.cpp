// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/ISGDynamicTextAssetProvider.h"

#if WITH_EDITOR

void ISGDynamicTextAssetProvider::BroadcastDynamicTextAssetChanged()
{
	OnDynamicTextAssetChanged.Broadcast(TScriptInterface<ISGDynamicTextAssetProvider>(_getUObject()));
}

#endif