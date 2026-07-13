// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Browser/SGDTABrowserCommands.h"
#include "Core/SGDTASerializerFormat.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Serialization/SGDTASerializer.h"
#include "Utilities/SGDTASourceControl.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

/**
 * Item representing a dynamic text asset file in the browser.
 */
struct FSGDTAAssetListItem
{
    FSGDTAAssetListItem() = default;

    FSGDTAAssetListItem(const FSGDynamicTextAssetId& InId,
        const FString& InUserFacingId,
        const FString& InFilePath,
        UClass* InClass,
        const FSGDynamicTextAssetTypeId& InAssetTypeId,
        const FSGDTASerializerFormat& InSerializerFormat)
        : Id(InId)
        , UserFacingId(InUserFacingId)
        , FilePath(InFilePath)
        , DynamicTextAssetClass(InClass)
        , AssetTypeId(InAssetTypeId)
        , SerializerFormat(InSerializerFormat)
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

    /** The serializer format associated with this item. */
    FSGDTASerializerFormat SerializerFormat;
};

DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetSelected, TSharedPtr<FSGDTAAssetListItem> /*SelectedItem*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetDoubleClicked, TSharedPtr<FSGDTAAssetListItem> /*ClickedItem*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetSelectionChanged, const TArray<TSharedPtr<FSGDTAAssetListItem>>& /*SelectedItems*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetDeleteRequested, const TArray<TSharedPtr<FSGDTAAssetListItem>>& /*Items*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetValidateRequested, const TArray<TSharedPtr<FSGDTAAssetListItem>>& /*Items*/);
DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetOpenRequested, const TArray<TSharedPtr<FSGDTAAssetListItem>>& /*Items*/);
DECLARE_DELEGATE_TwoParams(FOnDynamicTextAssetContextMenuExtension, FMenuBuilder& /*MenuBuilder*/, const TArray<TSharedPtr<FSGDTAAssetListItem>>& /*SelectedItems*/);

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
    void SetItems(const TArray<TSharedPtr<FSGDTAAssetListItem>>& InItems);

    /** Clears all items */
    void ClearItems();

    /** Gets the currently selected item (first if multiple selected) */
    TSharedPtr<FSGDTAAssetListItem> GetSelectedItem() const;

    /** Gets all currently selected items */
    TArray<TSharedPtr<FSGDTAAssetListItem>> GetSelectedItems() const;

    /** Returns the number of items currently displayed (after filtering) */
    int32 GetFilteredItemCount() const;

    /** Returns the file paths of all items currently in the view (unfiltered) */
    TArray<FString> GetAllItemFilePaths() const;

    /** Selects an item by ID */
    void SelectById(const FSGDynamicTextAssetId& Id);

    /** Selects all visible (filtered) items */
    void SelectAll();

    /** Clears the current selection */
    void ClearSelection();

private:

    /** Generates a row for the list view */
    TSharedRef<ITableRow> GenerateRow(TSharedPtr<FSGDTAAssetListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

    /** Called when selection changes */
    void OnSelectionChanged(TSharedPtr<FSGDTAAssetListItem> Item, ESelectInfo::Type SelectInfo);

    /** Called when an item is double-clicked */
    void OnDoubleClick(TSharedPtr<FSGDTAAssetListItem> Item);

    /** Generates the context menu for right-click actions */
    TSharedPtr<SWidget> GenerateContextMenu();

    /**
     * Fills the "Revision Control" submenu with its source-control entries.
     * Shared by both the single-select and multi-select context-menu branches; all entries are
     * selection-count-agnostic. Entries are appended in native Content Browser order: Refresh,
     * Mark For Add, Check In, History, Diff Against Depot, Revert. No "Merge" entry is provided
     * because opaque dynamic text asset payloads have no semantic three-way merge.
     *
     * @param MenuBuilder The submenu's builder, supplied by the AddSubMenu fill delegate.
     * @param SelectedItems The tiles selected when the context menu was opened.
     */
    void FillRevisionControlSubMenu(FMenuBuilder& MenuBuilder, TArray<TSharedPtr<FSGDTAAssetListItem>> SelectedItems);

    /**
     * Answers whether the current selection matches a source-control status, in either
     * all-match or any-match mode, reading the shared SCC status cache per selected file.
     * Returns false when the cache is unavailable or the selection is empty.
     *
     * @param SelectedItems The selected tiles to test.
     * @param Status The source-control status to compare each selected file's cached status against.
     * @param bRequireAll True to require every selected file to match (all-match); false to accept any (any-match).
     * @return True if the selection satisfies the requested match against Status.
     */
    bool DoesSelectionMatchStatus(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems,
        ESGDynamicTextAssetSourceControlStatus Status, bool bRequireAll) const;

    /**
     * Answers whether every selected file is under source control, reading the shared SCC status
     * cache per selected file. A file counts as tracked when its cached status is neither
     * NotInSourceControl nor Unknown. This is the negative-set condition the single-status
     * DoesSelectionMatchStatus cannot express, so it is a dedicated all-match helper. Returns false
     * when the cache is unavailable or the selection is empty.
     *
     * @param SelectedItems The selected tiles to test.
     * @return True if every selected file is tracked by source control.
     */
    bool IsSelectionAllTracked(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems) const;

    /**
     * Answers whether at least one selected file has local changes, reading the shared SCC status
     * cache per selected file. A file counts as locally changed when its cached status is CheckedOut,
     * Added, ModifiedLocally, or MarkedForDelete. This is the any-match "has local changes" predicate
     * used by the Check In entry (Revert uses the tightened IsSelectionAnyRevertable, which excludes a
     * clean CheckedOut). Returns false when the cache is unavailable or the selection is empty.
     *
     * @param SelectedItems The selected tiles to test.
     * @return True if any selected file has local changes.
     */
    bool IsSelectionAnyLocallyChanged(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems) const;

    /**
     * Answers whether at least one selected file has changes that Revert can discard, reading the shared
     * SCC status cache per selected file. This is the tightened Revert predicate: a file counts as
     * revertable when its cached status is Added, ModifiedLocally, or MarkedForDelete. It deliberately
     * excludes a clean CheckedOut (checked out but unmodified), which has nothing to revert. Returns false
     * when the cache is unavailable or the selection is empty.
     *
     * @param SelectedItems The selected tiles to test.
     * @return True if any selected file has changes Revert can discard.
     */
    bool IsSelectionAnyRevertable(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems) const;

    /** Called when the instance search text changes */
    void OnInstanceSearchTextChanged(const FText& NewText);

    /** Rebuilds FilteredItems from AllItems based on the current search text */
    void ApplyInstanceFilter();

    /** Returns the index to show in the switcher (0=empty, 1=list) */
    int32 GetContentIndex() const;

    /** The list view widget */
    TSharedPtr<SListView<TSharedPtr<FSGDTAAssetListItem>>> ListView = nullptr;

    /** The instance search box widget */
    TSharedPtr<SSearchBox> InstanceSearchBox;

    /** Widget switcher for empty/populated states */
    TSharedPtr<SWidgetSwitcher> ContentSwitcher = nullptr;

    /** Current instance search filter text */
    FString InstanceSearchText;

    /** All items in the view - unfiltered source of truth */
    TArray<TSharedPtr<FSGDTAAssetListItem>> AllItems;

    /** Filtered items displayed by the list view */
    TArray<TSharedPtr<FSGDTAAssetListItem>> FilteredItems;

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
