// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailPropertyExtensionHandler.h"

/**
 * Property extension handler for the DTA editor's IDetailsView.
 *
 * Adds a small asset bundle icon to the extension content area of soft reference
 * properties tagged with meta=(AssetBundles="..."). Hovering the icon shows a
 * tooltip with the bundle names for quick debugging.
 *
 * Set on the IDetailsView via SetExtensionHandler() during toolkit initialization.
 * This handler also covers properties inside instanced sub-objects since the
 * IDetailsView calls it for every property row it builds.
 *
 * To add additional property extensions, add new checks in IsPropertyExtendable()
 * and corresponding widget creation in ExtendWidgetRow().
 *
 * *NOTE*
 * We're only checking the key's for TMap's to keep it simple. Unintended behavior can occur
 * when trying to support both Key and Value's for TMaps with asset bundles in terms of intended design and intentional choice.
 * If you want to add that support, its recommended to introduce a new meta tag for values such as `AssetBundles_TMapValues`
 * to keep it explicit and clear. But then you're managing two types of asset bundle meta tags as a warning.
 */
class SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetPropertyExtensionHandler
	: public IDetailPropertyExtensionHandler
{
public:

	// IDetailPropertyExtensionHandler overrides
	virtual bool IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const override;
	virtual void ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> PropertyHandle) override;
	// ~IDetailPropertyExtensionHandler overrides
};
