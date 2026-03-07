// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Framework/Commands/UICommandList.h"
#include "Widgets/SCompoundWidget.h"
#include "Browser/SGDynamicTextAssetBrowserCommands.h"

class SButton;
class SCheckBox;
class SSplitter;
class SSGDynamicTextAssetTypeTree;
class SSGDynamicTextAssetTileView;
class STextBlock;

class FMenuBuilder;
struct FSGDynamicTextAssetListItem;
struct FSGDynamicTextAssetId;

/**
 * Main browser window for viewing and managing dynamic text assets.
 * 
 * Layout:
 * +------------------+------------------------+
 * |   Type Tree      |     Asset Grid         |
 * |   (Left Panel)   |     (Right Panel)      |
 * |                  |                        |
 * +------------------+------------------------+
 * 
 * Features:
 * - Class hierarchy tree for filtering
 * - Grid/list view of dynamic text asset files
 * - Create/delete/edit actions
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetBrowser)
    { }
    SLATE_END_ARGS()

    /** Constructs this widget */
    void Construct(const FArguments& InArgs);

    // SWidget overrides
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    // ~SWidget overrides

    /** Returns the unique identifier for this browser tab */
    static FName GetTabId() { return FName("SGDynamicTextAssetBrowser"); }

    /** Returns the display name for this browser */
    static FText GetTabDisplayName() { return INVTEXT("DTA Browser"); }

    /** Returns the tooltip for this browser tab */
    static FText GetTabTooltip() { return INVTEXT("Dynamic Text Asset(DTA) Browser\n\nBrowse and manage dynamic text assets"); }

    /** Opens the browser and selects the given dynamic text asset by ID */
    static void OpenAndSelect(const FSGDynamicTextAssetId& DynamicTextAssetId);

    /** Selects a specific dynamic text asset in the current view by ID */
    void SelectDynamicTextAssetById(const FSGDynamicTextAssetId& DynamicTextAssetId);

    /** Validates a list of dynamic text asset files and shows results */
    void ValidateDynamicTextAssets(const TArray<FString>& FilePaths);

private:

    /** Creates the toolbar widget */
    TSharedRef<SWidget> CreateToolbar();

    /** Creates the left panel (type tree) */
    TSharedRef<SWidget> CreateTypeTreePanel();

    /** Creates the right panel (asset grid) */
    TSharedRef<SWidget> CreateAssetGridPanel();

    /** Called when a type is selected in the tree */
    void OnTypeSelected(UClass* SelectedClass);

    /** Called when an item is selected in the tile view */
    void OnItemSelected(TSharedPtr<FSGDynamicTextAssetListItem> Item);

    /** Called when an item is double-clicked in the tile view */
    void OnItemDoubleClicked(TSharedPtr<FSGDynamicTextAssetListItem> Item);

    /** Called when the tile view selection changes - updates the status bar */
    void OnTileViewSelectionChanged(const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& SelectedItems);

    /** Pushes the current selection/count text to the status bar */
    void RefreshStatusBar();

    /** Opens all selected dynamic text assets in their own editor tabs */
    void OpenSelectedDynamicTextAssets();

    /** Returns true when 1+ items are selected (enables Open and Enter shortcut) */
    bool IsOpenButtonEnabled() const;

    /** Refreshes the tile view based on the selected type */
    void RefreshTileViewForSelectedType();

    /** Called when New button is clicked */
    FReply OnNewButtonClicked();

    /** Called when Refresh button is clicked */
    FReply OnRefreshButtonClicked();

    /** Called when Validate All button is clicked */
    FReply OnValidateAllButtonClicked();

    /** Returns whether the Reference Viewer button should be enabled */
    bool IsReferenceViewerButtonEnabled() const;

    /** Called when Reference Viewer button is clicked */
    FReply OnReferenceViewerButtonClicked();

    /** Returns whether the Show in Explorer button should be enabled */
    bool IsShowInExplorerButtonEnabled() const;

    /** Called when Show in Explorer button is clicked */
    FReply OnShowInExplorerButtonClicked();

    /** Returns whether the Rename button should be enabled */
    bool IsRenameButtonEnabled() const;

    /** Called when Rename button is clicked */
    FReply OnRenameButtonClicked();

    /** Renames the selected dynamic text asset with a dialog */
    void RenameSelectedDynamicTextAsset();

    /** Returns whether the Delete button should be enabled */
    bool IsDeleteButtonEnabled() const;

    /** Called when Delete button is clicked */
    FReply OnDeleteButtonClicked();

    /** Deletes the selected dynamic text asset(s) with confirmation */
    void DeleteSelectedDynamicTextAssets();

    /** Returns whether the Duplicate button should be enabled */
    bool IsDuplicateButtonEnabled() const;

    /** Called when Duplicate button is clicked */
    FReply OnDuplicateButtonClicked();

    /** Duplicates the selected dynamic text asset with a name dialog */
    void DuplicateSelectedDynamicTextAsset();

    /** Validates the selected dynamic text asset(s) and shows results */
    void ValidateSelectedDynamicTextAssets();

    /** Copies the selected item's GUID to the clipboard (single-item only) */
    void CopySelectedItemGuid();

    /** Returns whether exactly one item is selected (for single-item actions) */
    bool HasExactlyOneItemSelected() const;

    /** Called by the tile view to allow the browser to add additional context menu entries */
    void OnBuildContextMenuExtension(FMenuBuilder& MenuBuilder, const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& SelectedItems);

    /**
     * Converts the given items to the target file format.
     * Shows a notification with success/failure counts and refreshes the tile view.
     *
     * @param Items The items to convert
     * @param TargetExtension The target file extension (e.g., ".dta.xml")
     */
    void ConvertSelectedItems(const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& Items, const FString& TargetExtension);

    /** Binds keyboard shortcuts to the command list */
    void BindCommands();

    /** Updates toolbar button enabled states based on current tile view selection */
    void RefreshToolbarButtonStates();

    /** Returns the checkbox state for Include Child Types */
    ECheckBoxState GetIncludeChildTypesCheckState() const;

    /** Called when Include Child Types checkbox is toggled */
    void OnIncludeChildTypesChanged(ECheckBoxState NewState);

    /** Loads the Include Child Types preference from GConfig */
    void LoadIncludeChildTypesPreference();

    /** Saves the Include Child Types preference to GConfig */
    void SaveIncludeChildTypesPreference();

    // Guard against accidental bulk open: confirm if more than 10 items selected
    static constexpr int32 BULK_OPEN_WARNING_THRESHOLD = 10;

    /** The splitter between type tree and asset grid */
    TSharedPtr<SSplitter> MainSplitter = nullptr;

    /** The type tree widget */
    TSharedPtr<SSGDynamicTextAssetTypeTree> TypeTree = nullptr;

    /** Currently selected class for filtering */
    TWeakObjectPtr<UClass> SelectedTypeClass = nullptr;

    /** The tile view widget */
    TSharedPtr<SSGDynamicTextAssetTileView> TileView = nullptr;

    /** The command list for keyboard shortcut bindings */
    TSharedPtr<FUICommandList> CommandList = nullptr;

    /** The status bar text widget at the bottom of the asset grid */
    TSharedPtr<STextBlock> StatusBarText = nullptr;

    /** Whether to include child types in the tile view when a type is selected */
    uint8 bIncludeChildTypes : 1 = 0;

    /** Cached: whether the currently selected type is non-instantiable (abstract) */
    uint8 bIsSelectedTypeNonInstantiable : 1 = 0;

    /** Checkbox widget reference for direct enable/disable control */
    TSharedPtr<SCheckBox> IncludeChildTypesCheckBox = nullptr;

    /** Toolbar button widget references for event-based enable/disable control */
    TSharedPtr<SButton> RenameButton = nullptr;
    TSharedPtr<SButton> DeleteButton = nullptr;
    TSharedPtr<SButton> DuplicateButton = nullptr;
    TSharedPtr<SButton> ShowInExplorerButton = nullptr;
    TSharedPtr<SButton> ReferenceViewerButton = nullptr;
};
