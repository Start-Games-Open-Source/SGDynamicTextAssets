// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Browser/SGDynamicTextAssetBrowserCommands.h"

#include "Serialization/SGDynamicTextAssetSerializer.h"

/**
 * Item representing a dynamic text asset file in the browser.
 */
struct FSGDynamicTextAssetListItem
{
    FSGDynamicTextAssetListItem() = default;

    FSGDynamicTextAssetListItem(const FSGDynamicTextAssetId& InId,
        const FString& InUserFacingId,
        const FString& InFilePath,
        UClass* InClass,
        const FSGDynamicTextAssetTypeId& InAssetTypeId,
        const uint32& InSerializerTypeId)
        : Id(InId)
        , UserFacingId(InUserFacingId)
        , FilePath(InFilePath)
        , DynamicTextAssetClass(InClass)
        , AssetTypeId(InAssetTypeId)
        , SerializerTypeId(InSerializerTypeId)
    { }

    /** Display name (derived from UserFacingId) */
    FText GetDisplayName() const;

    /** Unique ID of the dynamic text asset */
    FSGDynamicTextAssetId Id;

    /** Human-readable ID */
    FString UserFacingId;

    /** Full path to the text file */
    FString FilePath;

    /** The class of this dynamic text asset */
    TWeakObjectPtr<UClass> DynamicTextAssetClass = nullptr;

    /** The asset type ID for this item's class */
    FSGDynamicTextAssetTypeId AssetTypeId;

    /** The serializer associated with this item. */
    uint32 SerializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
};

DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetSelected, TSharedPtr<FSGDynamicTextAssetListItem> /*SelectedItem*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetDoubleClicked, TSharedPtr<FSGDynamicTextAssetListItem> /*ClickedItem*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetSelectionChanged, const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& /*SelectedItems*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetDeleteRequested, const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& /*Items*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetValidateRequested, const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& /*Items*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetOpenRequested, const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& /*Items*/);
DECLARE_DELEGATE_TwoParams(FOnDynamicTextAssetContextMenuExtension, FMenuBuilder& /*MenuBuilder*/, const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& /*SelectedItems*/);

/**
 * List/tile view widget displaying dynamic text asset files.
 * Used as the right panel in the Dynamic Text Asset Browser.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetTileView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetTileView)
    { }
        /** Called when an item is selected */
        SLATE_EVENT(FOnDynamicTextAssetSelected, OnItemSelected)

        /** Called when an item is double-clicked */
        SLATE_EVENT(FOnDynamicTextAssetDoubleClicked, OnItemDoubleClicked)

        /** Called when the selection changes (provides all selected items) */
        SLATE_EVENT(FOnDynamicTextAssetSelectionChanged, OnSelectionChanged)

        /** Called when delete is requested from the context menu */
        SLATE_EVENT(FOnDynamicTextAssetDeleteRequested, OnDeleteRequested)

        /** Called when validate is requested from the context menu */
        SLATE_EVENT(FOnDynamicTextAssetValidateRequested, OnValidateRequested)

        /** Called when open is requested from the multi-select context menu */
        SLATE_EVENT(FOnDynamicTextAssetOpenRequested, OnOpenRequested)

        /** Browser command list used to display keyboard shortcut hints in the context menu */
        SLATE_ARGUMENT(TSharedPtr<FUICommandList>, ExternalCommandList)

        /** Called to allow the parent widget to add additional context menu entries */
        SLATE_EVENT(FOnDynamicTextAssetContextMenuExtension, OnContextMenuExtension)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Refreshes the view with new items */
    void SetItems(const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& InItems);

    /** Clears all items */
    void ClearItems();

    /** Gets the currently selected item (first if multiple selected) */
    TSharedPtr<FSGDynamicTextAssetListItem> GetSelectedItem() const;

    /** Gets all currently selected items */
    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> GetSelectedItems() const;

    /** Returns the number of items currently displayed (after filtering) */
    int32 GetFilteredItemCount() const;

    /** Selects an item by ID */
    void SelectById(const FSGDynamicTextAssetId& Id);

    /** Selects all visible (filtered) items */
    void SelectAll();

    /** Clears the current selection */
    void ClearSelection();

private:

    /** Generates a row for the list view */
    TSharedRef<ITableRow> GenerateRow(TSharedPtr<FSGDynamicTextAssetListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

    /** Called when selection changes */
    void OnSelectionChanged(TSharedPtr<FSGDynamicTextAssetListItem> Item, ESelectInfo::Type SelectInfo);

    /** Called when an item is double-clicked */
    void OnDoubleClick(TSharedPtr<FSGDynamicTextAssetListItem> Item);

    /** Generates the context menu for right-click actions */
    TSharedPtr<SWidget> GenerateContextMenu();

    /** Called when the instance search text changes */
    void OnInstanceSearchTextChanged(const FText& NewText);

    /** Rebuilds FilteredItems from AllItems based on the current search text */
    void ApplyInstanceFilter();

    /** Returns the index to show in the switcher (0=empty, 1=list) */
    int32 GetContentIndex() const;

    /** The list view widget */
    TSharedPtr<SListView<TSharedPtr<FSGDynamicTextAssetListItem>>> ListView = nullptr;

    /** The instance search box widget */
    TSharedPtr<SSearchBox> InstanceSearchBox;

    /** Widget switcher for empty/populated states */
    TSharedPtr<SWidgetSwitcher> ContentSwitcher = nullptr;

    /** Current instance search filter text */
    FString InstanceSearchText;

    /** All items in the view - unfiltered source of truth */
    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> AllItems;

    /** Filtered items displayed by the list view */
    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> FilteredItems;

    /** Selection callback (single item, backwards compat) */
    FOnDynamicTextAssetSelected OnItemSelected;

    /** Selection changed callback (all selected items) */
    FOnDynamicTextAssetSelectionChanged OnSelectionChangedDelegate;

    /** Double-click callback */
    FOnDynamicTextAssetDoubleClicked OnItemDoubleClicked;

    /** Delete requested callback (from context menu) */
    FOnDynamicTextAssetDeleteRequested OnDeleteRequested;

    /** Validate requested callback (from context menu) */
    FOnDynamicTextAssetValidateRequested OnValidateRequested;

    /** Open requested callback (from multi-select context menu) */
    FOnDynamicTextAssetOpenRequested OnOpenRequested;

    /** Browser command list for shortcut hint display in context menu */
    TSharedPtr<FUICommandList> ExternalCommandList;

    /** Delegate for extending the context menu with additional entries */
    FOnDynamicTextAssetContextMenuExtension OnContextMenuExtension;
};
