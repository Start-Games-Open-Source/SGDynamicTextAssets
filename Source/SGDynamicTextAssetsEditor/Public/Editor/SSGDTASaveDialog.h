// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"

/**
 * Result of the DTA save dialog.
 */
enum class ESGDTASaveDialogResult : uint8
{
    SaveSelected,
    DontSave,
    Cancelled
};

/**
 * A single item in the DTA save dialog list.
 */
struct FSGDTASaveDialogItem
{
    /** Human-readable asset name (UserFacingId) */
    FString AssetName;

    /** Relative file path from Content/ */
    FString FilePath;

    /** Class name of the DTA type */
    FString TypeName;

    /** Absolute file path for save operations */
    FString AbsoluteFilePath;

    /** Whether this item is checked for saving */
    ECheckBoxState CheckState = ECheckBoxState::Checked;
};

/**
 * Modal save dialog for unsaved DTA changes, mimicking the UE "Save Content" dialog.
 *
 * Shows a checkbox list of dirty DTA files with Asset, File, and Type columns,
 * and Save Selected / Don't Save / Cancel buttons.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetSaveDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetSaveDialog) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Adds a dirty DTA item to the list. Call before showing the dialog. */
    void AddItem(const FString& InAssetName, const FString& InFilePath,
                 const FString& InTypeName, const FString& InAbsoluteFilePath);

    /** Returns the result chosen by the user. */
    ESGDTASaveDialogResult GetResult() const { return Result; }

    /** Returns all items that were checked when the user clicked Save Selected. */
    TArray<TSharedPtr<FSGDTASaveDialogItem>> GetCheckedItems() const;

    /** Sets the parent window reference so the dialog can close itself. */
    void SetParentWindow(TSharedPtr<SWindow> InWindow) { ParentWindow = InWindow; }

private:

    TSharedRef<ITableRow> GenerateRow(TSharedPtr<FSGDTASaveDialogItem> Item,
                                      const TSharedRef<STableViewBase>& OwnerTable);

    /** Returns the tri-state of the header checkbox based on all items. */
    ECheckBoxState GetHeaderCheckState() const;

    /** Toggles all item checkboxes. */
    void OnHeaderCheckStateChanged(ECheckBoxState NewState);

    void OnSaveSelectedClicked();
    void OnDontSaveClicked();
    void OnCancelClicked();

    /** Closes the parent modal window. */
    void CloseDialog();

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    TArray<TSharedPtr<FSGDTASaveDialogItem>> Items;
    TSharedPtr<SListView<TSharedPtr<FSGDTASaveDialogItem>>> ItemListView;
    ESGDTASaveDialogResult Result = ESGDTASaveDialogResult::Cancelled;

    /** Weak reference to the parent modal window for closing. */
    TWeakPtr<SWindow> ParentWindow;
};

/**
 * Multi-column row widget for SSGDynamicTextAssetSaveDialog.
 */
class SSGDTASaveDialogRow : public SMultiColumnTableRow<TSharedPtr<FSGDTASaveDialogItem>>
{
public:
    SLATE_BEGIN_ARGS(SSGDTASaveDialogRow) {}
        SLATE_ARGUMENT(TSharedPtr<FSGDTASaveDialogItem>, Item)
        SLATE_ARGUMENT(TWeakPtr<SSGDynamicTextAssetSaveDialog>, Dialog)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs,
                   const TSharedRef<STableViewBase>& InOwnerTable);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FSGDTASaveDialogItem> Item;
    TWeakPtr<SSGDynamicTextAssetSaveDialog> Dialog;
};
