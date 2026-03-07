// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetTypeId.h"
#include "Input/Reply.h"
#include "IPropertyTypeCustomization.h"

class SButton;
class SSGDynamicTextAssetClassPicker;

/**
 * Property type customization for FSGDynamicTextAssetTypeId.
 *
 * Replaces the default struct view with a searchable type picker widget:
 * +----------------------------------------------------------------------+
 * | [Search...                         v] [Copy] [Paste] [X]            |
 * +----------------------------------------------------------------------+
 *
 * Features:
 * - Searchable dropdown listing all registered dynamic text asset types
 * - SListView with virtualization for handling 100s of types performantly
 * - Display: {DisplayName} with Asset Type ID as secondary info
 * - Non-blocking keystroke-driven filtering with throttled refresh (150ms debounce)
 * - Search by display name, class name, and Asset Type ID
 * - 'None' option for clearing the value
 * - Copy button copies Asset Type ID to clipboard
 * - Paste button reads clipboard and resolves by Asset Type ID
 */
class FSGAssetTypeIdCustomization : public IPropertyTypeCustomization
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

	/** Returns the display text for the currently selected type */
	FText GetCurrentDisplayText() const;

	/** Returns the tooltip for the currently selected type */
	FText GetCurrentTooltipText() const;

	/** Called when the class picker selects a class (or nullptr for None) */
	void OnClassPicked(UClass* NewClass);

	/** Called when the Clear button is clicked */
	FReply OnClearClicked();

	/** Called when the Copy ID button is clicked */
	FReply OnCopyIdClicked();

	/** Called when the Paste ID button is clicked. Reads clipboard and resolves by Asset Type ID */
	FReply OnPasteClicked();

	/** Reads the current FSGDynamicTextAssetTypeId value from the property handle */
	void ReadCurrentValue();

	/** Writes an updated FSGDynamicTextAssetTypeId back to the property handle */
	void WriteValue(const FSGDynamicTextAssetTypeId& NewTypeId);

	/** Updates button enabled states from current values */
	void RefreshButtonStates();

	/** The property handle for the FSGDynamicTextAssetTypeId struct */
	TSharedPtr<IPropertyHandle> StructPropertyHandle = nullptr;

	/** Currently selected type ID */
	FSGDynamicTextAssetTypeId CurrentTypeId;

	/** The reusable class picker widget */
	TSharedPtr<SSGDynamicTextAssetClassPicker> ClassPicker = nullptr;

	/** Copy ID button reference for event-based enable/disable control */
	TSharedPtr<SButton> CopyIdButton = nullptr;

	/** Clear button reference for event-based enable/disable control */
	TSharedPtr<SButton> ClearButton = nullptr;
};
