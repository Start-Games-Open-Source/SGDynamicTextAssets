// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetTypeId.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SComboButton;
class SSearchBox;
class STextBlock;

DECLARE_DELEGATE_OneParam(FOnDTAClassSelected, UClass* /*SelectedClass*/);

/**
 * Reusable searchable class picker for dynamic text asset types.
 *
 * Presents a SComboButton containing a SSearchBox + virtualized SListView
 * of all registered dynamic text asset classes. Each row shows the class
 * display name (bold) and Asset Type ID (subdued).
 *
 * Features:
 * - Keystroke-driven filtering with 150ms debounce
 * - Matches against display name, class name, and Asset Type ID
 * - Optional "None" entry for clearing the selection
 * - Programmatic selection via SetSelectedClass()
 *
 * Used by FSGDTAAssetTypeIdCustomization and SSGDynamicTextAssetCreateDialog.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetClassPicker : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSGDynamicTextAssetClassPicker)
		: _InitialClass(nullptr)
		, _bShowNoneOption(false)
	{ }
		/** Optional: Pre-select a specific class on construction */
		SLATE_ARGUMENT(UClass*, InitialClass)

		/** Whether to show a "None" entry at the top of the list */
		SLATE_ARGUMENT(bool, bShowNoneOption)

		/** Called when the user selects a class (nullptr if None is selected) */
		SLATE_EVENT(FOnDTAClassSelected, OnClassSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSGDynamicTextAssetClassPicker() override;

	/** Programmatically sets the selected class and updates the display. */
	void SetSelectedClass(UClass* InClass);

	/** Returns the currently selected class (nullptr if None or nothing selected). */
	UClass* GetSelectedClass() const;

	/** Returns the display text for the currently selected class. */
	FText GetSelectedDisplayText() const;

private:

	/** Entry in the class picker dropdown */
	struct FClassPickerEntry
	{
		TWeakObjectPtr<UClass> Class;
		FSGDynamicTextAssetTypeId TypeId;
		FText DisplayName;
	};

	/** Builds the full list of entries from the registry */
	void BuildEntries();

	/** Applies the current search filter to the full list */
	void ApplyFilter();

	/** Generates the picker menu content (search box + list) */
	TSharedRef<SWidget> GenerateMenuContent();

	/** Generates a row widget for the list view */
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FClassPickerEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);

	/** Called when search text changes (starts debounce timer) */
	void OnSearchTextChanged(const FText& NewText);

	/** Called when the filter debounce timer expires */
	void OnFilterTimerExpired();

	/** Called when list view selection changes */
	void OnListSelectionChanged(TSharedPtr<FClassPickerEntry> NewEntry, ESelectInfo::Type SelectInfo);

	/** Whether to show the "None" option */
	uint8 bShowNoneOption : 1 = 0;

	/** Currently selected class */
	TWeakObjectPtr<UClass> SelectedClass = nullptr;

	/** Delegate fired when a class is selected */
	FOnDTAClassSelected OnClassSelectedDelegate;

	/** All available entries (unfiltered) */
	TArray<TSharedPtr<FClassPickerEntry>> AllEntries;

	/** Filtered entries currently shown in the list */
	TArray<TSharedPtr<FClassPickerEntry>> FilteredEntries;

	/** Current search text */
	FText SearchText;

	/** Display text block inside the combo button for event-based text updates */
	TSharedPtr<STextBlock> DisplayTextBlock = nullptr;

	/** The combo button wrapping the searchable list */
	TSharedPtr<SComboButton> ComboButton = nullptr;

	/** The list view widget inside the combo button */
	TSharedPtr<SListView<TSharedPtr<FClassPickerEntry>>> PickerListView = nullptr;

	/** Active timer handle for throttled search filtering */
	TSharedPtr<FActiveTimerHandle> FilterTimerHandle = nullptr;
};
