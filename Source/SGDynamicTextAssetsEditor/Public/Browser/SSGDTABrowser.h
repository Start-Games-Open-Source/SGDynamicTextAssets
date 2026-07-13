// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Framework/Commands/UICommandList.h"
#include "ISourceControlProvider.h"
#include "Widgets/SCompoundWidget.h"
#include "Browser/SGDTABrowserCommands.h"

class SButton;
class SCheckBox;
class SSplitter;
class SSGDynamicTextAssetTypeTree;
class SSGDynamicTextAssetTileView;
class STextBlock;

class FMenuBuilder;
struct FSGDTAAssetListItem;
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

    /** Removes the app-activation subscription, guarded against Slate being torn down first during shutdown. */
    ~SSGDynamicTextAssetBrowser();

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
    void OnItemSelected(TSharedPtr<FSGDTAAssetListItem> Item);

    /** Called when an item is double-clicked in the tile view */
    void OnItemDoubleClicked(TSharedPtr<FSGDTAAssetListItem> Item);

    /** Called when the tile view selection changes - updates the status bar */
    void OnTileViewSelectionChanged(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems);

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

    /**
     * Kicks off an async FUpdateStatus for the files currently listed in the tile view
     * and invalidates the SCC status cache on completion, so externally staged/changed
     * provider state (e.g. git add outside the editor) becomes visible.
     * Fire-and-forget: no-op when source control is disabled, the cache is unavailable,
     * or the view has no files; a failed update leaves cached state as-is.
     */
    void RequestSourceControlStatusUpdate();

    /**
     * Completion handler for the async FUpdateStatus kicked off by RequestSourceControlStatusUpdate.
     * On success, invalidates the SCC status cache so the next paint re-queries the now-fresh
     * engine state; a failed or cancelled update leaves cached state as-is.
     * Bound via CreateSP, so it safely no-ops if this widget is destroyed while in flight.
     *
     * @param Operation The completed source control operation
     * @param Result Whether the operation succeeded, failed, or was cancelled
     */
    void OnSourceControlStatusUpdateComplete(const FSourceControlOperationRef& Operation, ECommandResult::Type Result);

    /**
     * App-activation handler: when the editor regains focus (for example the author returns
     * from an external terminal after a git operation), automatically re-runs the source
     * control status update for the listed files so externally changed provider state converges
     * without a manual refresh click. No-ops on deactivation and when the browser tab is not the
     * foreground tab; the update itself no-ops when source control is disabled, the cache is
     * unavailable, the view is empty, or a prior update is still in flight.
     *
     * @param bIsActive True when the application became active, false when it was deactivated.
     */
    void HandleApplicationActivationChanged(const bool bIsActive);

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
    void OnBuildContextMenuExtension(FMenuBuilder& MenuBuilder, const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems);

    /**
     * Converts the given items to the target file format.
     * Shows a notification with success/failure counts and refreshes the tile view.
     *
     * @param Items The items to convert
     * @param TargetExtension The target file extension (e.g., ".dta.xml")
     */
    void ConvertSelectedItems(const TArray<TSharedPtr<FSGDTAAssetListItem>>& Items, const FString& TargetExtension);

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

    /** True while an async source control status update is in flight, so rapid triggers coalesce instead of stacking. */
    uint8 bSourceControlStatusUpdateInFlight : 1 = 0;

    /** Handle for the app-activation subscription driving the automatic source control refresh; removed in the destructor. */
    FDelegateHandle ApplicationActivationChangedHandle;

    /** Checkbox widget reference for direct enable/disable control */
    TSharedPtr<SCheckBox> IncludeChildTypesCheckBox = nullptr;

    /** Toolbar button widget references for event-based enable/disable control */
    TSharedPtr<SButton> RenameButton = nullptr;
    TSharedPtr<SButton> DeleteButton = nullptr;
    TSharedPtr<SButton> DuplicateButton = nullptr;
    TSharedPtr<SButton> ShowInExplorerButton = nullptr;
    TSharedPtr<SButton> ReferenceViewerButton = nullptr;
};
