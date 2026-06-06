// Copyright Start Games, Inc. All Rights Reserved.

#include "Settings/SGDTASettings.h"

#include "Engine/AssetManager.h"
#include "Management/SGDTAFileManager.h"
#include "Serialization/SGDTASerializer.h"
#include "SGDynamicTextAssetLogs.h"
#include "Statics/SGDTAConstants.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult USGDynamicTextAssetSettingsAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult result = Super::IsDataValid(Context);

	// Validate that every registered serializer format has an asset bundle extender
	TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> serializers =
		FSGDynamicTextAssetFileManager::GetAllRegisteredSerializers();

	// Track which formats are covered and check for overlaps
	TMap<uint32, uint32> formatCoverageCount;

	for (const FSGAssetBundleExtenderMapping& mapping : AssetBundleExtenderOverrides)
	{
		// Validate the extender class ID is set
		if (!mapping.ExtenderClassId.IsValid())
		{
			Context.AddError(INVTEXT("Asset bundle extender mapping has an invalid ExtenderClassId. Each mapping must specify an extender."));
			result = EDataValidationResult::Invalid;
		}

		// Check which formats this mapping covers
		const uint32 bitmask = mapping.AppliesTo.GetTypeId();
		for (const TSharedPtr<ISGDynamicTextAssetSerializer>& serializer : serializers)
		{
			if (!serializer.IsValid())
			{
				continue;
			}

			const uint32 formatBit = 1u << serializer->GetSerializerFormat().GetTypeId();
			if ((bitmask & formatBit) != 0)
			{
				formatCoverageCount.FindOrAdd(serializer->GetSerializerFormat().GetTypeId())++;
			}
		}
	}

	// Check for formats with duplicate coverage (overrides only, not baseline)
	for (const TSharedPtr<ISGDynamicTextAssetSerializer>& serializer : serializers)
	{
		if (!serializer.IsValid())
		{
			continue;
		}

		const uint32 typeId = serializer->GetSerializerFormat().GetTypeId();
		const uint32* countPtr = formatCoverageCount.Find(typeId);

		if (!countPtr)
		{
			// No override for this format is valid (baseline default handles it)
			continue;
		}
		const uint32& count = *countPtr;

		if (count > 1)
		{
			Context.AddError(FText::Format(
				INVTEXT("Serializer format '{0}' is assigned to multiple asset bundle extenders. Only one extender per format is allowed."),
				serializer->GetFormatName()));
			result = EDataValidationResult::Invalid;
		}
	}

	return result;
}
#endif

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
