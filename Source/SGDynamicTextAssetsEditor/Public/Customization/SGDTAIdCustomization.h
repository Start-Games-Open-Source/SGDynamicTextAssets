// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "IPropertyTypeCustomization.h"

class FReply;

/**
 * Property type customization for FSGDynamicTextAssetId.
 *
 * Displays the underlying ID as a read-only text field with a copy button
 * on the name side, matching the visual style of how IDs appear natively
 * in the editor. The value cannot be edited directly through the details panel.
 *
 * +------------------------------------------------------+
 * | DynamicTextAssetId [CopyBtn]  |  550E8400-E29B-41D4-...    |
 * +------------------------------------------------------+
 */
class FSGDynamicTextAssetIdCustomization : public IPropertyTypeCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization overrides
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	// ~IPropertyTypeCustomization overrides

private:

	/** Returns the ID string for display. */
	FText GetIdDisplayText() const;

	/** Called when the copy button is clicked. */
	FReply OnCopyClicked();

	/** The property handle for the FSGDynamicTextAssetId struct. */
	TSharedPtr<IPropertyHandle> StructPropertyHandle = nullptr;
};
