// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SSGDTASaveDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "SGDTAEditorLogs.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace SGDTASaveDialogColumns
{
    static const FName CheckBox("CheckBox");
    static const FName Asset("Asset");
    static const FName File("File");
    static const FName Type("Type");
}

void SSGDynamicTextAssetSaveDialog::Construct(const FArguments& InArgs)
{
    TSharedRef<SHeaderRow> headerRow = SNew(SHeaderRow)

        + SHeaderRow::Column(SGDTASaveDialogColumns::CheckBox)
        [
            SNew(SBox)
            .Padding(FMargin(6, 3, 6, 3))
            .HAlign(HAlign_Center)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SSGDynamicTextAssetSaveDialog::GetHeaderCheckState)
                .OnCheckStateChanged(this, &SSGDynamicTextAssetSaveDialog::OnHeaderCheckStateChanged)
            ]
        ]
        .FixedWidth(38.0f)

        + SHeaderRow::Column(SGDTASaveDialogColumns::Asset)
        .DefaultLabel(INVTEXT("Asset"))
        .FillWidth(5.0f)

        + SHeaderRow::Column(SGDTASaveDialogColumns::File)
        .DefaultLabel(INVTEXT("File"))
        .FillWidth(7.0f)

        + SHeaderRow::Column(SGDTASaveDialogColumns::Type)
        .DefaultLabel(INVTEXT("Type"))
        .FillWidth(2.0f);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
        .Padding(FMargin(16))
        [
            SNew(SVerticalBox)

            // Header message
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(INVTEXT("Select Content to Save"))
                .AutoWrapText(true)
            ]

            // Item list
            + SVerticalBox::Slot()
            .FillHeight(0.8f)
            [
                SAssignNew(ItemListView, SListView<TSharedPtr<FSGDTASaveDialogItem>>)
                .ListItemsSource(&Items)
                .OnGenerateRow(this, &SSGDynamicTextAssetSaveDialog::GenerateRow)
                .SelectionMode(ESelectionMode::None)
                .HeaderRow(headerRow)
            ]

            // Button bar
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 16.0f, 0.0f, 0.0f)
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Bottom)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(5, 0)
                [
                    SNew(SButton)
                    .ButtonStyle(&FAppStyle::Get(), "PrimaryButton")
                    .TextStyle(&FAppStyle::Get(), "PrimaryButtonText")
                    .Text(INVTEXT("Save Selected"))
                    .ToolTipText(INVTEXT("Save the selected dynamic text assets"))
                    .OnClicked_Lambda([this]() { OnSaveSelectedClicked(); return FReply::Handled(); })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(5, 0)
                [
                    SNew(SButton)
                    .ButtonStyle(&FAppStyle::Get(), "Button")
                    .TextStyle(&FAppStyle::Get(), "ButtonText")
                    .Text(INVTEXT("Don't Save"))
                    .ToolTipText(INVTEXT("Discard all unsaved dynamic text asset changes"))
                    .OnClicked_Lambda([this]() { OnDontSaveClicked(); return FReply::Handled(); })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(5, 0)
                [
                    SNew(SButton)
                    .ButtonStyle(&FAppStyle::Get(), "Button")
                    .TextStyle(&FAppStyle::Get(), "ButtonText")
                    .Text(INVTEXT("Cancel"))
                    .ToolTipText(INVTEXT("Cancel the close operation"))
                    .OnClicked_Lambda([this]() { OnCancelClicked(); return FReply::Handled(); })
                ]
            ]
        ]
    ];
}

void SSGDynamicTextAssetSaveDialog::AddItem(const FString& InAssetName, const FString& InFilePath,
    const FString& InTypeName, const FString& InAbsoluteFilePath)
{
    TSharedPtr<FSGDTASaveDialogItem> item = MakeShared<FSGDTASaveDialogItem>();
    item->AssetName = InAssetName;
    item->FilePath = InFilePath;
    item->TypeName = InTypeName;
    item->AbsoluteFilePath = InAbsoluteFilePath;
    item->CheckState = ECheckBoxState::Checked;
    Items.Add(item);

    if (ItemListView.IsValid())
    {
        ItemListView->RequestListRefresh();
    }
}


TArray<TSharedPtr<FSGDTASaveDialogItem>> SSGDynamicTextAssetSaveDialog::GetCheckedItems() const
{
    TArray<TSharedPtr<FSGDTASaveDialogItem>> checkedItems;
    for (const TSharedPtr<FSGDTASaveDialogItem>& item : Items)
    {
        if (item.IsValid() && item->CheckState == ECheckBoxState::Checked)
        {
            checkedItems.Add(item);
        }
    }
    return checkedItems;
}

TSharedRef<ITableRow> SSGDynamicTextAssetSaveDialog::GenerateRow(
    TSharedPtr<FSGDTASaveDialogItem> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SSGDTASaveDialogRow, OwnerTable)
        .Item(Item)
        .Dialog(SharedThis(this));
}

ECheckBoxState SSGDynamicTextAssetSaveDialog::GetHeaderCheckState() const
{
    if (Items.Num() == 0)
    {
        return ECheckBoxState::Unchecked;
    }

    int32 checkedCount = 0;
    for (const TSharedPtr<FSGDTASaveDialogItem>& item : Items)
    {
        if (item.IsValid() && item->CheckState == ECheckBoxState::Checked)
        {
            ++checkedCount;
        }
    }

    if (checkedCount == 0)
    {
        return ECheckBoxState::Unchecked;
    }
    if (checkedCount == Items.Num())
    {
        return ECheckBoxState::Checked;
    }
    return ECheckBoxState::Undetermined;
}

void SSGDynamicTextAssetSaveDialog::OnHeaderCheckStateChanged(ECheckBoxState NewState)
{
    ECheckBoxState targetState = (NewState == ECheckBoxState::Checked)
        ? ECheckBoxState::Checked
        : ECheckBoxState::Unchecked;

    for (const TSharedPtr<FSGDTASaveDialogItem>& item : Items)
    {
        if (item.IsValid())
        {
            item->CheckState = targetState;
        }
    }

    if (ItemListView.IsValid())
    {
        ItemListView->RequestListRefresh();
    }
}

void SSGDynamicTextAssetSaveDialog::OnSaveSelectedClicked()
{
    Result = ESGDTASaveDialogResult::SaveSelected;
    CloseDialog();
}

void SSGDynamicTextAssetSaveDialog::OnDontSaveClicked()
{
    Result = ESGDTASaveDialogResult::DontSave;
    CloseDialog();
}

void SSGDynamicTextAssetSaveDialog::OnCancelClicked()
{
    Result = ESGDTASaveDialogResult::Cancelled;
    CloseDialog();
}

void SSGDynamicTextAssetSaveDialog::CloseDialog()
{
    if (TSharedPtr<SWindow> window = ParentWindow.Pin())
    {
        window->RequestDestroyWindow();
    }
}

FReply SSGDynamicTextAssetSaveDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        OnCancelClicked();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SSGDTASaveDialogRow::Construct(const FArguments& InArgs,
    const TSharedRef<STableViewBase>& InOwnerTable)
{
    Item = InArgs._Item;
    Dialog = InArgs._Dialog;

    SMultiColumnTableRow<TSharedPtr<FSGDTASaveDialogItem>>::Construct(
        SMultiColumnTableRow<TSharedPtr<FSGDTASaveDialogItem>>::FArguments(),
        InOwnerTable);
}

TSharedRef<SWidget> SSGDTASaveDialogRow::GenerateWidgetForColumn(const FName& ColumnName)
{
    static const FMargin rowPadding(3, 3, 3, 3);

    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == SGDTASaveDialogColumns::CheckBox)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .Padding(FMargin(10, 3, 6, 3))
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this]() { return Item->CheckState; })
                .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                {
                    Item->CheckState = NewState;
                    if (TSharedPtr<SSGDynamicTextAssetSaveDialog> dialogPinned = Dialog.Pin())
                    {
                        // Force header checkbox to re-evaluate tri-state
                    }
                })
            ];
    }

    if (ColumnName == SGDTASaveDialogColumns::Asset)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .Padding(rowPadding)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->AssetName))
            ];
    }

    if (ColumnName == SGDTASaveDialogColumns::File)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .Padding(rowPadding)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->FilePath))
                .ToolTipText(FText::FromString(Item->AbsoluteFilePath))
            ];
    }

    if (ColumnName == SGDTASaveDialogColumns::Type)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .Padding(rowPadding)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->TypeName))
            ];
    }

    return SNullWidget::NullWidget;
}
