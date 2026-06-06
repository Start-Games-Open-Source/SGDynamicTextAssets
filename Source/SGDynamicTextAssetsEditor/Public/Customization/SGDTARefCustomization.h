// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Views/SListView.h"

class USGDynamicTextAsset;

class SComboButton;
class SSearchBox;
class SWidget;

class ITableRow;

/**
 * Property type customization for FSGDynamicTextAssetRef.
 *
 * Replaces the default struct view with a searchable picker widget:
 * +----------------------------------------------------------------------+
 * | [Search...                         v] [Open] [Copy] [Paste] [X] |
 * +----------------------------------------------------------------------+
 *
 * Features:
 * - Searchable dropdown listing all available dynamic text assets
 * - Filtered by DynamicTextAssetType UPROPERTY metadata if set
 * - Additional editor-local class filter (not stored in struct)
 * - Open button launches the dynamic text asset editor for the selection
 * - Copy button copies the ID to clipboard
 * - Paste button reads clipboard and auto-detects GUID vs UserFacingId
 * - Clear button resets the reference
 */
class FSGDynamicTextAssetRefCustomization : public IPropertyTypeCustomization
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

	/** Entry in the picker dropdown */
	struct FSGDTAPickerEntry
	{
		FSGDynamicTextAssetId Id;
		FString UserFacingId;
		FString ClassName;
		FString FilePath;

		/** Returns display text for the combo box */
		FText GetDisplayText() const;
	};

	/** Rebuilds the full list of available dynamic text assets */
	void RebuildPickerEntries();

	/** Applies search text and class filter to the full list */
	void ApplyFilter();

	/** Returns the display text for the currently selected reference */
	FText GetCurrentDisplayText() const;

	/** Returns the tooltip for the currently selected reference */
	FText GetCurrentTooltipText() const;

	/** Called when the user selects an entry from the list */
	void OnEntrySelected(TSharedPtr<FSGDTAPickerEntry> NewEntry);

	/** Called when list view selection changes */
	void OnListSelectionChanged(TSharedPtr<FSGDTAPickerEntry> NewEntry, ESelectInfo::Type SelectInfo);

	/** Generates the picker menu content (search box, filter, list) */
	TSharedRef<SWidget> GeneratePickerMenu();

	/** Generates a row widget for the list view */
	TSharedRef<ITableRow> GenerateListRow(TSharedPtr<FSGDTAPickerEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);

	/** Called when search text changes */
	void OnSearchTextChanged(const FText& NewText);

	/** Called when the editor class filter changes */
	void OnEditorFilterClassChanged(TSharedPtr<FString> NewClass, ESelectInfo::Type SelectInfo);

	/** Called when the Clear button is clicked */
	FReply OnClearClicked();

	/** Called when the Copy ID button is clicked */
	FReply OnCopyIdClicked();

	/** Called when the Paste ID button is clicked. Reads clipboard and auto-detects GUID vs UserFacingId */
	FReply OnPasteClicked();

	/** Called when the Open Editor button is clicked */
	FReply OnOpenEditorClicked();

	/** Updates display text, tooltip, and button enabled states from current values */
	void RefreshDisplayState();

	/** Reads the current FSGDynamicTextAssetRef values from the property handle */
	void ReadCurrentValues();

	/** Writes updated values back to the property handle */
	void WriteValues(const FSGDynamicTextAssetId& NewId, const FString& NewUserFacingId);

	/** Returns the TypeFilter class from the meta tag on the UPROPERTY */
	UClass* GetTypeFilterClass() const;

	/** Finds the FSGDTAPickerEntry matching the current ID */
	TSharedPtr<FSGDTAPickerEntry> FindCurrentEntry() const;

	/** The property handle for the FSGDynamicTextAssetRef struct */
	TSharedPtr<IPropertyHandle> StructPropertyHandle = nullptr;

	/** Child property handle for the GUID field */
	TSharedPtr<IPropertyHandle> IdHandle = nullptr;

	/** Currently selected ID */
	FSGDynamicTextAssetId CurrentId;

	/** Currently selected UserFacingId */
	FString CurrentUserFacingId;

	/** All available entries (unfiltered) */
	TArray<TSharedPtr<FSGDTAPickerEntry>> AllEntries;

	/** Filtered entries currently shown in the list */
	TArray<TSharedPtr<FSGDTAPickerEntry>> FilteredEntries;

	/** Current search text */
	FString SearchText;

	/** Editor-local class filter (not stored in struct) */
	FString EditorFilterClassName;

	/** Available class names for the editor filter dropdown */
	TArray<TSharedPtr<FString>> AvailableClassNames;

	/** The list view widget inside the combo button */
	TSharedPtr<SListView<TSharedPtr<FSGDTAPickerEntry>>> PickerListView = nullptr;

	/** The combo button wrapping the searchable list */
	TSharedPtr<SComboButton> ComboButton = nullptr;

	/** Display text block inside the combo button for event-based text/tooltip updates */
	TSharedPtr<STextBlock> DisplayTextBlock = nullptr;

	/** Action button references for event-based enable/disable control */
	TSharedPtr<SButton> OpenEditorButton = nullptr;
	TSharedPtr<SButton> CopyIdButton = nullptr;
	TSharedPtr<SButton> ClearButton = nullptr;
};
