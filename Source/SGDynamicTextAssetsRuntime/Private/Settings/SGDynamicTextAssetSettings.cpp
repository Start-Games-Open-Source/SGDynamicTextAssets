// Copyright Start Games, Inc. All Rights Reserved.

#include "Settings/SGDynamicTextAssetSettings.h"

#include "Engine/AssetManager.h"
#include "SGDynamicTextAssetsRuntimeModule.h"

FName USGDynamicTextAssetSettingsAsset::GetCustomCompressionName() const
{
	return CustomCompressionName;
}

USGDynamicTextAssetSettings* USGDynamicTextAssetSettings::Get()
{
	return GetMutableDefault<USGDynamicTextAssetSettings>();
}

USGDynamicTextAssetSettingsAsset* USGDynamicTextAssetSettings::GetSettings()
{
	USGDynamicTextAssetSettings* settings = Get();
	return settings ? settings->GetSettingsAsset() : nullptr;
}

USGDynamicTextAssetSettingsAsset* USGDynamicTextAssetSettings::GetSettingsAsset() const
{
	// Return cached asset if valid
	if (CachedSettingsAsset.IsValid())
	{
		return CachedSettingsAsset.Get();
	}

	// Try to load from soft reference
	if (!SettingsAsset.IsNull())
	{
		if (USGDynamicTextAssetSettingsAsset* loadedAsset = UAssetManager::Get().GetStreamableManager().LoadSynchronous<USGDynamicTextAssetSettingsAsset>(SettingsAsset))
		{
			CachedSettingsAsset = loadedAsset;
			return loadedAsset;
		}
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("SGDynamicTextAssetSettings: Failed to load SettingsAsset(%s)"), *SettingsAsset.ToString());
	return nullptr;
}
