// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDynamicTextAssetBrowser.h"

#include "Browser/SSGDynamicTextAssetCreateDialog.h"
#include "Browser/SSGDynamicTextAssetDuplicateDialog.h"
#include "Browser/SSGDynamicTextAssetRenameDialog.h"
#include "Browser/SSGDynamicTextAssetTileView.h"
#include "Browser/SSGDynamicTextAssetTypeTree.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Editor/FSGDynamicTextAssetEditorToolkit.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "ReferenceViewer/SSGDynamicTextAssetReferenceViewer.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "Utilities/SGDynamicTextAssetCookUtils.h"
#include "Utilities/SGDynamicTextAssetSourceControl.h"
#include "Browser/SGDynamicTextAssetBrowserCommands.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "SPositiveActionButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Statics/SGDynamicTextAssetStatics.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SSGDynamicTextAssetBrowser"

void SSGDynamicTextAssetBrowser::Construct(const FArguments& InArgs)
{
    BindCommands();
    LoadIncludeChildTypesPreference();

    ChildSlot
    [
        SNew(SVerticalBox)

        // Toolbar
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            CreateToolbar()
        ]

        // Main content area
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(MainSplitter, SSplitter)
            .Orientation(Orient_Horizontal)

            // Left panel - Type Tree
            + SSplitter::Slot()
            .Value(0.25f)
            [
                CreateTypeTreePanel()
            ]

            // Right panel - Asset Grid
            + SSplitter::Slot()
            .Value(0.75f)
            [
                CreateAssetGridPanel()
            ]
        ]
    ];

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Dynamic Text Asset Browser constructed"));
}

void SSGDynamicTextAssetBrowser::OpenAndSelect(const FSGDynamicTextAssetId& DynamicTextAssetId)
{
    TSharedPtr<SDockTab> browserTab = FGlobalTabmanager::Get()->TryInvokeTab(GetTabId());
    if (!browserTab.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("SSGDynamicTextAssetBrowser::OpenAndSelect: Failed to get Dynamic Text Asset Browser Tab"));
        return;
    }
    TSharedRef<SWidget> tabContent = browserTab->GetContent();
    TSharedRef<SSGDynamicTextAssetBrowser> browserWidget = StaticCastSharedRef<SSGDynamicTextAssetBrowser>(tabContent);
    browserWidget->SelectDynamicTextAssetById(DynamicTextAssetId);
}

void SSGDynamicTextAssetBrowser::SelectDynamicTextAssetById(const FSGDynamicTextAssetId& DynamicTextAssetId)
{
    UClass* dataObjectType = USGDynamicTextAssetStatics::GetDynamicTextAssetTypeFromId(DynamicTextAssetId);
    if (!dataObjectType)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                INVTEXT("Failed to open browser for dynamic text asset at ID({0}) due to NULL dynamic text asset type."),
                FText::FromString(DynamicTextAssetId.ToString())),
            INVTEXT("Browser to Dynamic Text Asset Failed")
        );
        return;
    }
    if (TypeTree.IsValid())
    {
        TypeTree->SelectClass(dataObjectType);
    }
    if (TileView.IsValid())
    {
        TileView->SelectById(DynamicTextAssetId);
    }
}

TSharedRef<SWidget> SSGDynamicTextAssetBrowser::CreateToolbar()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin(4.0f))
        [
            SNew(SHorizontalBox)

            // New button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(5.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SPositiveActionButton)
                .Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
                .Text(INVTEXT("New Dynamic Text Asset"))
                .ToolTipText(INVTEXT("Create a new Dynamic Text Asset"))
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnNewButtonClicked)
            ]

            // Rename button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5.0f, 0.0f, 0.0f, 0.0f)
            [
                SAssignNew(RenameButton, SButton)
                .ContentPadding(FMargin(10.0f, 10.0f))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(INVTEXT("Rename the selected dynamic text asset"))
                .IsEnabled(false)
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnRenameButtonClicked)
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush("GenericCommands.Rename"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]

            // Delete button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5.0f, 0.0f, 0.0f, 0.0f)
            [
                SAssignNew(DeleteButton, SButton)
                .ContentPadding(FMargin(10.0f, 10.0f))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(INVTEXT("Delete the selected dynamic text asset"))
                .IsEnabled(false)
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnDeleteButtonClicked)
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Delete"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]

            // Duplicate button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5.0f, 0.0f, 0.0f, 0.0f)
            [
                SAssignNew(DuplicateButton, SButton)
                .ContentPadding(FMargin(10.0f, 10.0f))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(INVTEXT("Duplicate the selected dynamic text asset"))
                .IsEnabled(false)
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnDuplicateButtonClicked)
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Duplicate"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]

            // Show in Explorer button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5.0f, 0.0f, 0.0f, 0.0f)
            [
                SAssignNew(ShowInExplorerButton, SButton)
                .ContentPadding(FMargin(10.0f, 10.0f))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(INVTEXT("Open the selected dynamic text asset's file location in the system file browser"))
                .IsEnabled(false)
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnShowInExplorerButtonClicked)
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.FolderOpen"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]

            // Reference Viewer button
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(ReferenceViewerButton, SButton)
                .ContentPadding(FMargin(10.0f, 10.0f))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(INVTEXT("Open Reference Viewer for the selected dynamic text asset"))
                .IsEnabled(false)
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnReferenceViewerButtonClicked)
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Search"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]

            // Spacer
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .HAlign(HAlign_Fill)
            [
                SNew(SSpacer)
            ]

            // Validate All button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Right)
            .Padding(0.0f, 0.0f, 5.0f, 0.0f)
            [
                SNew(SButton)
                .ContentPadding(FMargin(10.0f, 10.0f))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(INVTEXT("Validate all dynamic text assets"))
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnValidateAllButtonClicked)
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush("DeveloperTools.MenuIcon"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]
            
            // Refresh button(right side
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Right)
            .Padding(0.0f, 0.0f, 5.0f, 0.0f)
            [
                SNew(SButton)
                .ContentPadding(FMargin(10.0f, 10.0f))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(INVTEXT("Refresh the file list"))
                .OnClicked(this, &SSGDynamicTextAssetBrowser::OnRefreshButtonClicked)
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Refresh"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]
        ];
}

TSharedRef<SWidget> SSGDynamicTextAssetBrowser::CreateTypeTreePanel()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin(4.0f))
        [
            SNew(SBox)
            .MinDesiredWidth(200.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    SNew(STextBlock)
                    .Text(INVTEXT("Dynamic Text Asset Types"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f, 2.0f)
                [
                    SAssignNew(IncludeChildTypesCheckBox, SCheckBox)
                    .IsChecked(bIncludeChildTypes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                    .OnCheckStateChanged(this, &SSGDynamicTextAssetBrowser::OnIncludeChildTypesChanged)
                    [
                        SNew(STextBlock)
                        .Text(INVTEXT("Include Child Types"))
                    ]
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(TypeTree, SSGDynamicTextAssetTypeTree)
                    .OnTypeSelected(this, &SSGDynamicTextAssetBrowser::OnTypeSelected)
                ]
            ]
        ];
}

void SSGDynamicTextAssetBrowser::OnTypeSelected(UClass* SelectedClass)
{
    SelectedTypeClass = SelectedClass;

    // Determine if selected type is abstract (non-instantiable)
    bIsSelectedTypeNonInstantiable = false;
    if (SelectedClass)
    {
        if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
        {
            bIsSelectedTypeNonInstantiable = !registry->IsInstantiableClass(SelectedClass);
        }
    }

    // Update checkbox enabled state, tooltip, and checked state directly (event-based, not polled)
    if (IncludeChildTypesCheckBox.IsValid())
    {
        IncludeChildTypesCheckBox->SetEnabled(!bIsSelectedTypeNonInstantiable);
        IncludeChildTypesCheckBox->SetToolTipText(
            bIsSelectedTypeNonInstantiable
                ? INVTEXT("Include Child Types is always on for abstract types.")
                : FText::GetEmpty()
        );
        IncludeChildTypesCheckBox->SetIsChecked(GetIncludeChildTypesCheckState());
    }

    if (SelectedClass)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Type selected: %s (Instantiable: %d)"),
            *GetNameSafe(SelectedClass), !bIsSelectedTypeNonInstantiable);
        RefreshTileViewForSelectedType();
    }
    else
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Type selection cleared"));
        if (TileView.IsValid())
        {
            TileView->ClearItems();
            RefreshStatusBar();
            RefreshToolbarButtonStates();
        }
    }
}

ECheckBoxState SSGDynamicTextAssetBrowser::GetIncludeChildTypesCheckState() const
{
    if (bIsSelectedTypeNonInstantiable)
    {
        return ECheckBoxState::Checked;
    }
    return bIncludeChildTypes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SSGDynamicTextAssetBrowser::OnIncludeChildTypesChanged(ECheckBoxState NewState)
{
    bIncludeChildTypes = (NewState == ECheckBoxState::Checked);
    SaveIncludeChildTypesPreference();

    if (SelectedTypeClass.IsValid())
    {
        RefreshTileViewForSelectedType();
    }
}

void SSGDynamicTextAssetBrowser::LoadIncludeChildTypesPreference()
{
    bool bValue = false;
    GConfig->GetBool(TEXT("SGDynamicTextAssetsEditor"), TEXT("bIncludeChildTypes"), bValue, GEditorPerProjectIni);
    bIncludeChildTypes = bValue;
}

void SSGDynamicTextAssetBrowser::SaveIncludeChildTypesPreference()
{
    GConfig->SetBool(TEXT("SGDynamicTextAssetsEditor"), TEXT("bIncludeChildTypes"), bIncludeChildTypes != 0, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

TSharedRef<SWidget> SSGDynamicTextAssetBrowser::CreateAssetGridPanel()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin(4.0f))
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(INVTEXT("Dynamic Text Assets"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(TileView, SSGDynamicTextAssetTileView)
                .OnItemSelected(this, &SSGDynamicTextAssetBrowser::OnItemSelected)
                .OnItemDoubleClicked(this, &SSGDynamicTextAssetBrowser::OnItemDoubleClicked)
                .OnSelectionChanged(this, &SSGDynamicTextAssetBrowser::OnTileViewSelectionChanged)
                .ExternalCommandList(CommandList)
                .OnContextMenuExtension(this, &SSGDynamicTextAssetBrowser::OnBuildContextMenuExtension)
                .OnOpenRequested_Lambda([this](const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& Items)
                {
                    OpenSelectedDynamicTextAssets();
                })
                .OnDeleteRequested_Lambda([this](const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& Items)
                {
                    DeleteSelectedDynamicTextAssets();
                })
                .OnValidateRequested_Lambda([this](const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& Items)
                {
                    ValidateSelectedDynamicTextAssets();
                })
            ]

            // Status bar
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                .Padding(FMargin(6.0f, 3.0f))
                [
                    SAssignNew(StatusBarText, STextBlock)
                    .Text(INVTEXT("0 items"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
            ]
        ];
}

void SSGDynamicTextAssetBrowser::OnItemSelected(TSharedPtr<FSGDynamicTextAssetListItem> Item)
{
    if (Item.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Item selected: %s"), *Item->UserFacingId);
    }
    else
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Item selection cleared"));
    }
}

void SSGDynamicTextAssetBrowser::OnItemDoubleClicked(TSharedPtr<FSGDynamicTextAssetListItem> Item)
{
    if (Item.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Item double-clicked: %s"), *Item->UserFacingId);
        // Open all selected items so multi-select double-click opens all, not just the clicked one
        OpenSelectedDynamicTextAssets();
    }
}

void SSGDynamicTextAssetBrowser::RefreshTileViewForSelectedType()
{
    if (!TileView.IsValid())
    {
        return;
    }

    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> items;

    // Get files for the selected class (or all if none selected)
    TArray<FString> filePaths;
    if (SelectedTypeClass.IsValid())
    {
        FSGDynamicTextAssetFileManager::FindAllFilesForClass(SelectedTypeClass.Get(), filePaths, bIncludeChildTypes || bIsSelectedTypeNonInstantiable);
    }
    else
    {
        FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(filePaths);
    }

    // Extract file info from each file and build list items
    for (const FString& filePath : filePaths)
    {
        FSGDynamicTextAssetFileInfo fileInfo =
            FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);

        if (fileInfo.bIsValid)
        {
            // Resolve class via Asset Type ID (O(1) map lookup)
            UClass* itemClass = nullptr;
            if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                itemClass = registry->ResolveClassForTypeId(fileInfo.AssetTypeId);
            }

            TSharedPtr<FSGDynamicTextAssetListItem> item = MakeShared<FSGDynamicTextAssetListItem>(
                fileInfo.Id,
                fileInfo.UserFacingId,
                filePath,
                itemClass ? itemClass : SelectedTypeClass.Get(),
                fileInfo.AssetTypeId,
                fileInfo.SerializerTypeId
            );
            items.Add(item);
        }
    }

    // Sort items by UserFacingId
    items.Sort([](const TSharedPtr<FSGDynamicTextAssetListItem>& A, const TSharedPtr<FSGDynamicTextAssetListItem>& B)
    {
        return A->UserFacingId < B->UserFacingId;
    });

    TileView->SetItems(items);

    // Update status bar to reflect new item count (selection is cleared on refresh)
    RefreshStatusBar();
    RefreshToolbarButtonStates();

    UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Refreshed tile view with %d items"), items.Num());
}

FReply SSGDynamicTextAssetBrowser::OnNewButtonClicked()
{
    UClass* preselectedClass = SelectedTypeClass.Get();

    FString createdFilePath;
    FSGDynamicTextAssetId createdId;

    if (SSGDynamicTextAssetCreateDialog::ShowModal(preselectedClass, createdFilePath, createdId))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Created new dynamic text asset: %s"), *createdFilePath);

        // Refresh the view to show the new file
        RefreshTileViewForSelectedType();
    }

    return FReply::Handled();
}

FReply SSGDynamicTextAssetBrowser::OnRefreshButtonClicked()
{
    // Refresh the type tree
    if (TypeTree.IsValid())
    {
        TypeTree->RefreshTree();
    }

    // Refresh the tile view
    RefreshTileViewForSelectedType();

    return FReply::Handled();
}

FReply SSGDynamicTextAssetBrowser::OnValidateAllButtonClicked()
{
    TArray<FString> allFiles;
    FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

    ValidateDynamicTextAssets(allFiles);

    return FReply::Handled();
}

bool SSGDynamicTextAssetBrowser::IsReferenceViewerButtonEnabled() const
{
    if (TileView.IsValid())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
        if (selectedItems.Num() == 1)
        {
            return selectedItems[0].IsValid() && selectedItems[0]->Id.IsValid();
        }
    }
    return false;
}

FReply SSGDynamicTextAssetBrowser::OnReferenceViewerButtonClicked()
{
    if (TileView.IsValid())
    {
        TSharedPtr<FSGDynamicTextAssetListItem> selectedItem = TileView->GetSelectedItem();
        if (selectedItem.IsValid() && selectedItem->Id.IsValid())
        {
            SSGDynamicTextAssetReferenceViewer::OpenViewer(selectedItem->Id, selectedItem->UserFacingId);
        }
    }
    return FReply::Handled();
}

bool SSGDynamicTextAssetBrowser::IsShowInExplorerButtonEnabled() const
{
    if (TileView.IsValid())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
        if (selectedItems.Num() == 1)
        {
            return selectedItems[0].IsValid() && !selectedItems[0]->FilePath.IsEmpty();
        }
    }
    return false;
}

FReply SSGDynamicTextAssetBrowser::OnShowInExplorerButtonClicked()
{
    if (TileView.IsValid())
    {
        TSharedPtr<FSGDynamicTextAssetListItem> selectedItem = TileView->GetSelectedItem();
        if (selectedItem.IsValid() && !selectedItem->FilePath.IsEmpty())
        {
            // Pass full file path to select the file in Explorer
            FPlatformProcess::ExploreFolder(*selectedItem->FilePath);
        }
    }
    return FReply::Handled();
}

bool SSGDynamicTextAssetBrowser::IsRenameButtonEnabled() const
{
    if (TileView.IsValid())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
        if (!selectedItems.IsEmpty())
        {
            return selectedItems[0].IsValid() && !selectedItems[0]->FilePath.IsEmpty();
        }
    }
    return false;
}

FReply SSGDynamicTextAssetBrowser::OnRenameButtonClicked()
{
    RenameSelectedDynamicTextAsset();
    return FReply::Handled();
}

void SSGDynamicTextAssetBrowser::RenameSelectedDynamicTextAsset()
{
    if (!TileView.IsValid())
    {
        return;
    }

    TSharedPtr<FSGDynamicTextAssetListItem> selectedItem = TileView->GetSelectedItem();
    if (!selectedItem.IsValid() || selectedItem->FilePath.IsEmpty())
    {
        return;
    }

    FString oldFilePath = selectedItem->FilePath;
    FString newFilePath;
    if (SSGDynamicTextAssetRenameDialog::ShowModal(oldFilePath, selectedItem->UserFacingId, newFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Renamed dynamic text asset: %s -> %s"), *selectedItem->UserFacingId, *newFilePath);

        // Notify any open editor about the rename so it updates its file path and title
        FSGDynamicTextAssetEditorToolkit::NotifyFileRenamed(oldFilePath, newFilePath);

        // Refresh the tile view to reflect the rename
        RefreshTileViewForSelectedType();
    }
}

bool SSGDynamicTextAssetBrowser::IsDeleteButtonEnabled() const
{
    if (TileView.IsValid())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
        for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : selectedItems)
        {
            if (item.IsValid() && !item->FilePath.IsEmpty())
            {
                return true;
            }
        }
    }
    return false;
}

FReply SSGDynamicTextAssetBrowser::OnDeleteButtonClicked()
{
    DeleteSelectedDynamicTextAssets();
    return FReply::Handled();
}

void SSGDynamicTextAssetBrowser::DeleteSelectedDynamicTextAssets()
{
    if (!TileView.IsValid())
    {
        return;
    }

    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
    if (selectedItems.IsEmpty())
    {
        return;
    }

    // Build confirmation message
    FText confirmTitle = INVTEXT("Delete Dynamic Text Asset");
    FText confirmMessage;

    if (selectedItems.Num() == 1)
    {
        // Single item: show name
        confirmMessage = FText::Format(
            INVTEXT("Are you sure you want to delete '{0}'?\n\nThis action cannot be undone."),
            FText::FromString(selectedItems[0]->UserFacingId)
        );
    }
    else
    {
        // Multiple items: count only
        confirmMessage = FText::Format(
            INVTEXT("Delete {0} dynamic text assets?\n\nThis action cannot be undone."),
            FText::AsNumber(selectedItems.Num())
        );
    }

    EAppReturnType::Type result = FMessageDialog::Open(
        EAppMsgType::YesNo,
        confirmMessage,
        confirmTitle
    );

    if (result != EAppReturnType::Yes)
    {
        return;
    }

    int32 successCount = 0;
    int32 failCount = 0;

    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : selectedItems)
    {
        if (!item.IsValid() || item->FilePath.IsEmpty())
        {
            continue;
        }

        // Mark for delete in source control if enabled
        if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
        {
            if (!FSGDynamicTextAssetSourceControl::MarkForDelete(item->FilePath))
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to mark file for delete in source control: %s"), *item->FilePath);
            }
        }

        // Close any open editor for this file before deleting
        FSGDynamicTextAssetEditorToolkit::NotifyFileDeleted(item->FilePath);

        // Delete the file
        if (FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFile(item->FilePath))
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Deleted dynamic text asset: %s (%s)"), *item->UserFacingId, *item->FilePath);
            successCount++;
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to delete dynamic text asset: %s (%s)"), *item->UserFacingId, *item->FilePath);
            failCount++;
        }
    }

    // Single refresh after all deletions
    RefreshTileViewForSelectedType();

    if (failCount > 0)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                INVTEXT("Failed to delete {0} of {1} dynamic text assets. Check the log for details."),
                FText::AsNumber(failCount),
                FText::AsNumber(selectedItems.Num())
            ),
            INVTEXT("Delete Failed")
        );
    }
}

bool SSGDynamicTextAssetBrowser::IsDuplicateButtonEnabled() const
{
    if (TileView.IsValid())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
        if (!selectedItems.IsEmpty())
        {
            return selectedItems[0].IsValid() && !selectedItems[0]->FilePath.IsEmpty();
        }
    }
    return false;
}

FReply SSGDynamicTextAssetBrowser::OnDuplicateButtonClicked()
{
    DuplicateSelectedDynamicTextAsset();
    return FReply::Handled();
}

void SSGDynamicTextAssetBrowser::DuplicateSelectedDynamicTextAsset()
{
    if (!TileView.IsValid())
    {
        return;
    }

    TSharedPtr<FSGDynamicTextAssetListItem> selectedItem = TileView->GetSelectedItem();
    if (!selectedItem.IsValid() || selectedItem->FilePath.IsEmpty())
    {
        return;
    }

    FString createdFilePath;
    FSGDynamicTextAssetId createdId;

    if (SSGDynamicTextAssetDuplicateDialog::ShowModal(selectedItem->FilePath, selectedItem->UserFacingId, createdFilePath, createdId))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Duplicated dynamic text asset: %s"), *createdFilePath);

        // Refresh the tile view to show the new file
        RefreshTileViewForSelectedType();

        // Select the new item
        TileView->SelectById(createdId);
    }
}

void SSGDynamicTextAssetBrowser::ValidateSelectedDynamicTextAssets()
{
    if (!TileView.IsValid())
    {
        return;
    }
    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
    if (selectedItems.IsEmpty())
    {
        return;
    }

    TArray<FString> filePaths;
    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : selectedItems)
    {
        if (item.IsValid() && !item->FilePath.IsEmpty())
        {
            filePaths.Add(item->FilePath);
        }
    }

    ValidateDynamicTextAssets(filePaths);
}

void SSGDynamicTextAssetBrowser::ValidateDynamicTextAssets(const TArray<FString>& FilePaths)
{
    if (FilePaths.IsEmpty())
    {
        FNotificationInfo notificationInfo(INVTEXT("No dynamic text assets found to validate."));
        notificationInfo.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(notificationInfo);
        return;
    }

    int32 passCount = 0;
    int32 failCount = 0;

    for (const FString& filePath : FilePaths)
    {
        TArray<FString> errors;
        if (FSGDynamicTextAssetCookUtils::ValidateDynamicTextAssetFile(filePath, errors))
        {
            passCount++;
        }
        else
        {
            failCount++;
            for (const FString& err : errors)
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Validation error in %s: %s"), *filePath, *err);
            }
        }
    }

    bool failedValidation = failCount != 0;

    // Build result dialog text
    FText resultMessage;
    if (failedValidation)
    {
        resultMessage = FText::Format(INVTEXT("Validation Failed!\n{0} passed, {1} failed.\nCheck Output Log."),
            passCount,
            failCount);
    }
    else
    {
        // Account for singular vs multiple
        if (passCount > 1)
        {
            resultMessage = FText::Format(INVTEXT("All {0} dynamic text assets passed validation."), passCount);
        }
        else
        {
            resultMessage = INVTEXT("Dynamic text asset passed validation.");
        }
    }

    FNotificationInfo notificationInfo(resultMessage);
    notificationInfo.ExpireDuration = 5.0f;

    if (failedValidation)
    {
        notificationInfo.Image = FAppStyle::GetBrush("Icons.ErrorWithColor");

        notificationInfo.Hyperlink = FSimpleDelegate::CreateLambda([]()
        {
            FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog"));
        });
        notificationInfo.HyperlinkText = INVTEXT("Open Output Log");
    }
    else
    {
        notificationInfo.Image = FAppStyle::GetBrush("Icons.SuccessWithColor");
    }
    FSlateNotificationManager::Get().AddNotification(notificationInfo);

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Validation complete: %d passed, %d failed"), passCount, failCount);
}

void SSGDynamicTextAssetBrowser::OpenSelectedDynamicTextAssets()
{
    if (!TileView.IsValid())
    {
        return;
    }

    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
    if (selectedItems.IsEmpty())
    {
        return;
    }

    if (selectedItems.Num() > BULK_OPEN_WARNING_THRESHOLD)
    {
        const FText message = FText::Format(
            INVTEXT("Open {0} dynamic text assets? This will open {0} editor tabs."),
            FText::AsNumber(selectedItems.Num()));

        const EAppReturnType::Type result = FMessageDialog::Open(
            EAppMsgType::OkCancel,
            message,
            INVTEXT("Open Multiple Dynamic Text Assets"));

        if (result != EAppReturnType::Ok)
        {
            return;
        }
    }

    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : selectedItems)
    {
        if (!item.IsValid() || item->FilePath.IsEmpty())
        {
            continue;
        }

        FSGDynamicTextAssetEditorToolkit::OpenEditor(item->FilePath, item->DynamicTextAssetClass.Get(), item->AssetTypeId);
    }

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Opened %d dynamic text asset editor(s)"), selectedItems.Num());
}

bool SSGDynamicTextAssetBrowser::IsOpenButtonEnabled() const
{
    if (!TileView.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : selectedItems)
    {
        if (item.IsValid() && !item->FilePath.IsEmpty())
        {
            return true;
        }
    }

    return false;
}

void SSGDynamicTextAssetBrowser::OnTileViewSelectionChanged(const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& SelectedItems)
{
    RefreshStatusBar();
    RefreshToolbarButtonStates();
}

void SSGDynamicTextAssetBrowser::RefreshStatusBar()
{
    if (!StatusBarText.IsValid() || !TileView.IsValid())
    {
        return;
    }

    const int32 totalCount = TileView->GetFilteredItemCount();
    const int32 selectedCount = TileView->GetSelectedItems().Num();

    FText statusText;
    if (selectedCount == 0)
    {
        // Nothing selected: show total count only
        statusText = FText::Format(INVTEXT("{0} {1}"),
            FText::AsNumber(totalCount),
            totalCount == 1 ? INVTEXT("item") : INVTEXT("items"));
    }
    else
    {
        // 1 or more selected: show selected of total
        statusText = FText::Format(INVTEXT("{0} of {1} selected"),
            FText::AsNumber(selectedCount),
            FText::AsNumber(totalCount));
    }

    StatusBarText->SetText(statusText);
}

void SSGDynamicTextAssetBrowser::BindCommands()
{
    // Safety net: no-op if already registered by the module's StartupModule().
    FSGDynamicTextAssetBrowserCommands::Register();

    CommandList = MakeShared<FUICommandList>();

    // Open (Enter) - 1+ items selected
    CommandList->MapAction(
        FSGDynamicTextAssetBrowserCommands::Get().Open,
        FExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::OpenSelectedDynamicTextAssets),
        FCanExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::IsOpenButtonEnabled)
    );

    // Delete - 1+ items selected
    CommandList->MapAction(
        FGenericCommands::Get().Delete,
        FExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::DeleteSelectedDynamicTextAssets),
        FCanExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::IsDeleteButtonEnabled)
    );

    // Rename (F2) - exactly 1 item selected
    CommandList->MapAction(
        FGenericCommands::Get().Rename,
        FExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::RenameSelectedDynamicTextAsset),
        FCanExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::IsRenameButtonEnabled)
    );

    // Duplicate (Ctrl+D) - exactly 1 item selected
    CommandList->MapAction(
        FGenericCommands::Get().Duplicate,
        FExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::DuplicateSelectedDynamicTextAsset),
        FCanExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::IsDuplicateButtonEnabled)
    );

    // Copy (Ctrl+C) - copy GUID, exactly 1 item selected
    CommandList->MapAction(
        FGenericCommands::Get().Copy,
        FExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::CopySelectedItemGuid),
        FCanExecuteAction::CreateSP(this, &SSGDynamicTextAssetBrowser::HasExactlyOneItemSelected)
    );

    // Select All (Ctrl+A)
    CommandList->MapAction(
        FGenericCommands::Get().SelectAll,
        FExecuteAction::CreateLambda([this]()
        {
            if (TileView.IsValid())
            {
                TileView->SelectAll();
            }
        }),
        FCanExecuteAction::CreateLambda([]() { return true; })
    );
}

FReply SSGDynamicTextAssetBrowser::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // Process command bindings first
    if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
    {
        return FReply::Handled();
    }

    // Escape - deselect all (not a standard FGenericCommands action)
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        if (TileView.IsValid())
        {
            TileView->ClearSelection();
            return FReply::Handled();
        }
    }

    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SSGDynamicTextAssetBrowser::CopySelectedItemGuid()
{
    if (!TileView.IsValid())
    {
        return;
    }

    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = TileView->GetSelectedItems();
    if (selectedItems.Num() == 1 && selectedItems[0].IsValid())
    {
        FPlatformApplicationMisc::ClipboardCopy(*selectedItems[0]->Id.ToString());
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied GUID to clipboard: %s"), *selectedItems[0]->Id.ToString());
    }
}

bool SSGDynamicTextAssetBrowser::HasExactlyOneItemSelected() const
{
    if (TileView.IsValid())
    {
        return TileView->GetSelectedItems().Num() == 1;
    }
    return false;
}

void SSGDynamicTextAssetBrowser::OnBuildContextMenuExtension(FMenuBuilder& MenuBuilder, const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& SelectedItems)
{
    // Check that all selected items have the same file extension
    FString uniformExtension;
    bool bAllSameExtension = true;
    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : SelectedItems)
    {
        if (!item.IsValid() || item->FilePath.IsEmpty())
        {
            continue;
        }

        FString ext = FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(item->FilePath);
        if (uniformExtension.IsEmpty())
        {
            uniformExtension = ext;
        }
        else if (ext != uniformExtension)
        {
            bAllSameExtension = false;
            break;
        }
    }

    // Mixed extensions: show disabled entry with explanation
    if (!bAllSameExtension)
    {
        MenuBuilder.AddMenuEntry(
            INVTEXT("Change File Type"),
            INVTEXT("All selected items must have the same file type to convert."),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateLambda([]() {}),
                FCanExecuteAction::CreateLambda([]() { return false; })
            )
        );
        return;
    }

    // Build Change File Type submenu dynamically from registered serializers
    MenuBuilder.AddSubMenu(
        INVTEXT("Change File Type"),
        INVTEXT("Convert the selected file(s) to a different format"),
        FNewMenuDelegate::CreateLambda([this, SelectedItems, uniformExtension](FMenuBuilder& SubMenuBuilder)
        {
            TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> serializers =
                FSGDynamicTextAssetFileManager::GetAllRegisteredSerializers();

            bool bAddedAny = false;
            for (const TSharedPtr<ISGDynamicTextAssetSerializer>& serializer : serializers)
            {
                if (!serializer.IsValid())
                {
                    continue;
                }

                const FString extension = serializer->GetFileExtension();

                // Exclude binary serializer (not a text format)
                if (extension == FSGDynamicTextAssetFileManager::BINARY_EXTENSION)
                {
                    continue;
                }

                // Exclude the current file's format
                if (extension == uniformExtension)
                {
                    continue;
                }

                bAddedAny = true;

                // Entry: "FormatName (.ext)" e.g. "XML (.dta.xml)"
                FText entryLabel = FText::Format(INVTEXT("{0} ({1})"),
                    serializer->GetFormatName(),
                    FText::FromString(extension));

                FString targetExtension = extension;

                SubMenuBuilder.AddMenuEntry(
                    entryLabel,
                    FText::Format(INVTEXT("Convert to {0} format"), serializer->GetFormatName()),
                    FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([this, SelectedItems, targetExtension]()
                    {
                        ConvertSelectedItems(SelectedItems, targetExtension);
                    }))
                );
            }

            if (!bAddedAny)
            {
                SubMenuBuilder.AddMenuEntry(
                    INVTEXT("No additional formats available"),
                    FText::GetEmpty(),
                    FSlateIcon(),
                    FUIAction(
                        FExecuteAction::CreateLambda([]() {}),
                        FCanExecuteAction::CreateLambda([]() { return false; })
                    )
                );
            }
        })
    );
}

void SSGDynamicTextAssetBrowser::ConvertSelectedItems(const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& Items, const FString& TargetExtension)
{
    if (Items.IsEmpty())
    {
        return;
    }

    // Resolve the target format's display name for the confirmation dialog
    FText targetFormatName = FText::FromString(TargetExtension);
    TSharedPtr<ISGDynamicTextAssetSerializer> targetSerializer = FSGDynamicTextAssetFileManager::FindSerializerForExtension(TargetExtension);
    if (targetSerializer.IsValid())
    {
        targetFormatName = targetSerializer->GetFormatName();
    }

    // Count how many selected items have open editors with unsaved changes
    int32 unsavedCount = 0;
    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : Items)
    {
        if (item.IsValid() && !item->FilePath.IsEmpty()
            && FSGDynamicTextAssetEditorToolkit::HasOpenEditorWithUnsavedChanges(item->FilePath))
        {
            unsavedCount++;
        }
    }

    // Build confirmation dialog text
    FText dialogMessage;
    if (Items.Num() == 1)
    {
        if (unsavedCount > 0)
        {
            dialogMessage = FText::Format(
                INVTEXT("Convert this file to {0} format?\n\nThe open editor has unsaved changes that will be saved first."),
                targetFormatName);
        }
        else
        {
            dialogMessage = FText::Format(
                INVTEXT("Convert this file to {0} format?\n\nThis will change the file type on disk."),
                targetFormatName);
        }
    }
    else
    {
        if (unsavedCount > 0)
        {
            dialogMessage = FText::Format(
                INVTEXT("Convert {0} file(s) to {1} format?\n\n{2} open editor(s) have unsaved changes that will be saved first."),
                FText::AsNumber(Items.Num()), targetFormatName, FText::AsNumber(unsavedCount));
        }
        else
        {
            dialogMessage = FText::Format(
                INVTEXT("Convert {0} file(s) to {1} format?\n\nThis will change the file type on disk."),
                FText::AsNumber(Items.Num()), targetFormatName);
        }
    }

    // Show confirmation dialog
    EAppReturnType::Type result = FMessageDialog::Open(EAppMsgType::YesNo, dialogMessage,
        INVTEXT("Change File Type"));

    if (result != EAppReturnType::Yes)
    {
        return;
    }

    int32 successCount = 0;
    int32 failCount = 0;

    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : Items)
    {
        if (!item.IsValid() || item->FilePath.IsEmpty())
        {
            continue;
        }

        // Save any open editor before conversion
        if (!FSGDynamicTextAssetEditorToolkit::SaveOpenEditor(item->FilePath))
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                TEXT("Failed to save open editor for %s before conversion, skipping."), *item->FilePath);
            failCount++;
            continue;
        }

        FString oldFilePath = item->FilePath;
        FString newFilePath;
        if (FSGDynamicTextAssetFileManager::ConvertFileFormat(oldFilePath, TargetExtension, newFilePath))
        {
            successCount++;

            // Source control: mark old file for delete, new file for add
            FSGDynamicTextAssetSourceControl::MarkForDelete(oldFilePath);
            FSGDynamicTextAssetSourceControl::MarkForAdd(newFilePath);

            // Notify any open editor so it updates its file path and title
            FSGDynamicTextAssetEditorToolkit::NotifyFileRenamed(oldFilePath, newFilePath);

            UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Converted file format: %s -> %s"), *oldFilePath, *newFilePath);
        }
        else
        {
            failCount++;
            UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to convert file format: %s"), *item->FilePath);
        }
    }

    // Show notification
    FText notificationText;
    if (failCount == 0)
    {
        if (successCount == 1)
        {
            notificationText = INVTEXT("File format converted successfully.");
        }
        else
        {
            notificationText = FText::Format(INVTEXT("Converted {0} files successfully."),
                FText::AsNumber(successCount));
        }
    }
    else
    {
        notificationText = FText::Format(INVTEXT("Converted {0} of {1} files. {2} failed. Check the log for details."),
            FText::AsNumber(successCount),
            FText::AsNumber(successCount + failCount),
            FText::AsNumber(failCount));
    }

    FNotificationInfo notificationInfo(notificationText);
    notificationInfo.ExpireDuration = 4.0f;
    notificationInfo.Image = failCount > 0
        ? FAppStyle::GetBrush("Icons.ErrorWithColor")
        : FAppStyle::GetBrush("Icons.SuccessWithColor");
    FSlateNotificationManager::Get().AddNotification(notificationInfo);

    // Refresh the tile view to show updated files
    RefreshTileViewForSelectedType();
}

void SSGDynamicTextAssetBrowser::RefreshToolbarButtonStates()
{
    const bool bRenameEnabled = IsRenameButtonEnabled();
    const bool bDeleteEnabled = IsDeleteButtonEnabled();
    const bool bDuplicateEnabled = IsDuplicateButtonEnabled();
    const bool bShowInExplorerEnabled = IsShowInExplorerButtonEnabled();
    const bool bReferenceViewerEnabled = IsReferenceViewerButtonEnabled();

    if (RenameButton.IsValid())
    {
        RenameButton->SetEnabled(bRenameEnabled);
    }
    if (DeleteButton.IsValid())
    {
        DeleteButton->SetEnabled(bDeleteEnabled);
    }
    if (DuplicateButton.IsValid())
    {
        DuplicateButton->SetEnabled(bDuplicateEnabled);
    }
    if (ShowInExplorerButton.IsValid())
    {
        ShowInExplorerButton->SetEnabled(bShowInExplorerEnabled);
    }
    if (ReferenceViewerButton.IsValid())
    {
        ReferenceViewerButton->SetEnabled(bReferenceViewerEnabled);
    }
}

#undef LOCTEXT_NAMESPACE
