// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Framework/SlateDelegates.h"
#include "IDetailCustomization.h"

class ISGDynamicTextAssetProvider;

class SWidget;
class FReply;
class SSGDynamicTextAssetIdentityBlock;

/**
 * Detail customization for USGDynamicTextAsset that adds copy-to-clipboard buttons
 * next to the DynamicTextAssetId and UserFacingId identity properties.
 * 
 * Displays the following properties:
 * - DynamicTextAssetId
 * - UserFacingId
 * - Version
 */
class FSGDynamicTextAssetIdentityCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization overrides
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// ~IDetailCustomization overrides

	/**
	 * Creates a copy button widget using the engine's copy icon.
	 *
	 * @param OnCopyClicked Callback invoked when the copy button is pressed
	 * @param Tooltip Tooltip text for the button
	 * @return The copy button widget
	 */
	static TSharedRef<SWidget> CreateCopyButton(FOnClicked OnCopyClicked, const FText& Tooltip);

private:

	void RefreshId(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset=nullptr);
	void RefreshUserFacingId(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset=nullptr);
	void RefreshVersion(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset=nullptr);

	FText GetIdText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const;
	FText GetUserFacingIdText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const;
	FText GetVersionText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const;

	TSharedPtr<SSGDynamicTextAssetIdentityBlock> IdentityBlock = nullptr;
};
