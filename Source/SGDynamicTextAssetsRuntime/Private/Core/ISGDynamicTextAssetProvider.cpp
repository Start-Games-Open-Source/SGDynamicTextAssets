// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/ISGDynamicTextAssetProvider.h"

#include "Statics/SGDynamicTextAssetStatics.h"

bool ISGDynamicTextAssetProvider::Native_ValidateDynamicTextAsset(FSGDynamicTextAssetValidationResult& OutResult) const
{
	if (!GetDynamicTextAssetId().IsValid())
	{
		OutResult.AddError(
			INVTEXT("Dynamic text asset has invalid ID"),
			TEXT("DynamicTextAssetId"),
			INVTEXT("Recreate this dynamic text asset to generate a valid ID")
		);
	}

	if (GetUserFacingId().IsEmpty())
	{
		OutResult.AddError(
			INVTEXT("Dynamic text asset has empty UserFacingId"),
			TEXT("UserFacingId"),
			INVTEXT("Set a unique user-facing name for this dynamic text asset")
		);
	}

	if (!GetVersion().IsValid())
	{
		OutResult.AddError(
			INVTEXT("Dynamic text asset has invalid version numbers"),
			TEXT("Version"),
			INVTEXT("Version numbers must be non-negative")
		);
	}

	// Validate soft asset references internally
	for (TFieldIterator<FProperty> propertyItr(_getUObject()->GetClass()); propertyItr; ++propertyItr)
	{
		const FProperty* property = *propertyItr;
		USGDynamicTextAssetStatics::ValidateSoftPathsInProperty(property, _getUObject(), property->GetName(), OutResult);
	}

	return OutResult.IsValid();
}

bool ISGDynamicTextAssetProvider::HasSGDTAssetBundles() const
{
	return GetSGDTAssetBundleData().HasBundles();
}

#if WITH_EDITOR

void ISGDynamicTextAssetProvider::BroadcastDynamicTextAssetChanged()
{
	OnDynamicTextAssetChanged.Broadcast(TScriptInterface<ISGDynamicTextAssetProvider>(_getUObject()));
}

#endif