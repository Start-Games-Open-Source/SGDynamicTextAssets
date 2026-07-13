// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDTATileView.h"

#include "Browser/SSGDTADirtyBadge.h"
#include "Browser/SSGDTADuplicateDialog.h"
#include "Browser/SSGDTARenameDialog.h"
#include "Browser/SSGDTASCCBadge.h"
#include "Editor/SGDTAEditorToolkit.h"
#include "Management/SGDTAFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "ReferenceViewer/SSGDTAReferenceViewer.h"
#include "SGDTAEditorLogs.h"
#include "Editor/SGDTADetailCustomization.h"
#include "Editor/SSGDynamicTextAssetIcon.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/MessageDialog.h"
#include "ReferenceViewer/SGDTAReferenceSubsystem.h"
#include "Utilities/SGDTASCCStatusCache.h"
#include "Utilities/SGDTASourceControl.h"
#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "SourceControlOperations.h"
#include "SourceControlWindows.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/AppStyle.h"

FText FSGDTAAssetListItem::GetDisplayName() const
{
    return FText::FromString(UserFacingId.IsEmpty() ? ("ID: " + Id.ToString()) : UserFacingId);
}

void SSGDynamicTextAssetTileView::Construct(const FArguments& InArgs)
{
    OnItemSelected = InArgs._OnItemSelected;
    OnItemDoubleClicked = InArgs._OnItemDoubleClicked;
    OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
    OnDeleteRequested = InArgs._OnDeleteRequested;
    OnValidateRequested = InArgs._OnValidateRequested;
    OnOpenRequested = InArgs._OnOpenRequested;
    ExternalCommandList = InArgs._ExternalCommandList;
    OnContextMenuExtension = InArgs._OnContextMenuExtension;

    ChildSlot
    [
        SNew(SVerticalBox)

        // Search bar
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SAssignNew(InstanceSearchBox, SSearchBox)
            .HintText(INVTEXT("Search by name or GUID..."))
            .OnTextChanged(this, &SSGDynamicTextAssetTileView::OnInstanceSearchTextChanged)
        ]

        // Content area
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(ContentSwitcher, SWidgetSwitcher)
            .WidgetIndex_Lambda([this]() { return GetContentIndex(); })

            // Index 0: Empty state
            + SWidgetSwitcher::Slot()
            [
                SNew(SBox)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                .Padding(16.0f)
                [
                    SNew(STextBlock)
                    .Text(INVTEXT("No dynamic text assets found. Select a type from the tree or create a new dynamic text asset."))
                    .Justification(ETextJustify::Center)
                    .AutoWrapText(true)
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
            ]

            // Index 1: List view
            + SWidgetSwitcher::Slot()
            [
                SAssignNew(ListView, SListView<TSharedPtr<FSGDTAAssetListItem>>)
                .ListItemsSource(&FilteredItems)
                .OnGenerateRow(this, &SSGDynamicTextAssetTileView::GenerateRow)
                .OnSelectionChanged(this, &SSGDynamicTextAssetTileView::OnSelectionChanged)
                .OnMouseButtonDoubleClick(this, &SSGDynamicTextAssetTileView::OnDoubleClick)
                .OnContextMenuOpening(this, &SSGDynamicTextAssetTileView::GenerateContextMenu)
                .SelectionMode(ESelectionMode::Multi)
            ]
        ]
    ];
}

void SSGDynamicTextAssetTileView::SetItems(const TArray<TSharedPtr<FSGDTAAssetListItem>>& InItems)
{
    AllItems = InItems;
    ApplyInstanceFilter();

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }

    // Using verbose to avoid log spam
    UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Tile view updated with %d items (%d filtered)"), AllItems.Num(), FilteredItems.Num());
}

void SSGDynamicTextAssetTileView::ClearItems()
{
    AllItems.Empty();
    FilteredItems.Empty();
    InstanceSearchText.Empty();

    if (InstanceSearchBox.IsValid())
    {
        InstanceSearchBox->SetText(FText::GetEmpty());
    }

    if (ListView.IsValid())
    {
        ListView->ClearSelection();
        ListView->RequestListRefresh();
    }
}

TSharedPtr<FSGDTAAssetListItem> SSGDynamicTextAssetTileView::GetSelectedItem() const
{
    if (ListView.IsValid())
    {
        TArray<TSharedPtr<FSGDTAAssetListItem>> selected = ListView->GetSelectedItems();
        if (selected.Num() > 0)
        {
            return selected[0];
        }
    }
    return nullptr;
}

TArray<TSharedPtr<FSGDTAAssetListItem>> SSGDynamicTextAssetTileView::GetSelectedItems() const
{
    if (ListView.IsValid())
    {
        return ListView->GetSelectedItems();
    }
    return {};
}

int32 SSGDynamicTextAssetTileView::GetFilteredItemCount() const
{
    return FilteredItems.Num();
}

TArray<FString> SSGDynamicTextAssetTileView::GetAllItemFilePaths() const
{
    TArray<FString> filePaths;
    filePaths.Reserve(AllItems.Num());

    for (const TSharedPtr<FSGDTAAssetListItem>& item : AllItems)
    {
        if (item.IsValid() && !item->FilePath.IsEmpty())
        {
            filePaths.Add(item->FilePath);
        }
    }

    return filePaths;
}

void SSGDynamicTextAssetTileView::SelectAll()
{
    if (ListView.IsValid())
    {
        ListView->SetItemSelection(FilteredItems, true);
    }
}

void SSGDynamicTextAssetTileView::ClearSelection()
{
    if (ListView.IsValid())
    {
        ListView->ClearSelection();
    }
}

void SSGDynamicTextAssetTileView::SelectById(const FSGDynamicTextAssetId& Id)
{
    if (!ListView.IsValid())
    {
        return;
    }

    for (const TSharedPtr<FSGDTAAssetListItem>& item : FilteredItems)
    {
        if (item.IsValid() && item->Id == Id)
        {
            ListView->SetSelection(item);
            return;
        }
    }
}

TSharedRef<ITableRow> SSGDynamicTextAssetTileView::GenerateRow(TSharedPtr<FSGDTAAssetListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    const FString className = Item->DynamicTextAssetClass.IsValid() ? Item->DynamicTextAssetClass->GetName() : TEXT("Unknown");

    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = nullptr;
    if (Item->SerializerFormat.IsValid())
    {
        serializer = FSGDynamicTextAssetFileManager::FindSerializerForFormat(Item->SerializerFormat);
    }
    // Account for if we failed to get the serializer for it
    // We NEED this information for readability
    if (!serializer.IsValid())
    {
        return SNew(STableRow<TSharedPtr<FSGDTAAssetListItem>>, OwnerTable)
            .Padding(FMargin(4.0f, 2.0f))
            [
                SNew(SHorizontalBox)

                // Fail Icon
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("Icons.ErrorWithColor"))
                ]

                // Name and class
                + SHorizontalBox::Slot()
                .Padding(4.0f, 0.0f)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Left)
                [
                    SNew(STextBlock)
                    .Text(FText::Format(
                        INVTEXT("Failed to parse file: {0}"),
                        FText::FromString(Item->FilePath)))
                ]
            ];
    }

    // For lambda's
    TWeakPtr<ISGDynamicTextAssetSerializer> weakSerializer(serializer);

    return SNew(STableRow<TSharedPtr<FSGDTAAssetListItem>>, OwnerTable)
        .Padding(FMargin(4.0f, 2.0f))
        .ToolTipText_Lambda([Item, weakSerializer]() -> FText
        {
            FText baseText = FText::Format(INVTEXT("DDO: {0}{1}\nID: {2}\nType: {3}"),
                Item->GetDisplayName(),
                FText::FromString(weakSerializer.Pin().IsValid() ? weakSerializer.Pin()->GetFileExtension() : "[?Unknown File Type?]"),
                FText::FromString(Item->Id.ToString()),
                (weakSerializer.Pin().IsValid() ? weakSerializer.Pin()->GetFormatName() : INVTEXT("[?Unknown Type?]")));

            FText metaInformation;
            
            // Unsaved-changes line: independent of source-control state, matching the dirty badge's path-keyed query.
            if (FSGDynamicTextAssetEditorToolkit::HasOpenEditorWithUnsavedChanges(Item->FilePath))
            {
                metaInformation = INVTEXT("Has unsaved changes.");
            }
            
            // Source-control status line: skipped entirely when source control is disabled (no
            // provider to query). Reads the provider's own ISourceControlState::GetDisplayTooltip()
            // so the tooltip matches the native Content Browser and agrees with the tile badge,
            // which derives from GetIcon() on the same state.
            if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
            {
                const FSourceControlStatePtr sccState =
                    ISourceControlModule::Get().GetProvider().GetState(Item->FilePath, EStateCacheUsage::Use);

                const FText sccLine = sccState.IsValid() ? sccState->GetDisplayTooltip() : FText::GetEmpty();
                if (!sccLine.IsEmpty())
                {
                    // Account for unsaved changes line not being displayed.
                    if (metaInformation.IsEmpty())
                    {
                        metaInformation = sccLine;
                    }
                    else
                    {
                        metaInformation = FText::Format(INVTEXT("{0}\n{1}"), 
                            metaInformation, sccLine);
                    }
                }
            }
            
            // Collapse the referencer/dependency-augmented body into a single FText so the
            // source-control and unsaved-changes lines below append from one tail return.
            if (USGDynamicTextAssetReferenceSubsystem* refSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetReferenceSubsystem>())
            {
                int32 ReferencerCount = refSubsystem->GetReferencerCount(Item->Id);
                TArray<FSGDynamicTextAssetDependencyEntry> dependencies;
                refSubsystem->FindDependencies(Item->Id, dependencies);
                const FText message = FText::Format(INVTEXT("Referenced By: {1} assets\nDependencies: {2} assets."),
                    ReferencerCount, dependencies.Num());
                if (metaInformation.IsEmpty())
                {
                    metaInformation = message;
                }
                else
                {
                    metaInformation = FText::Format(INVTEXT("{0}\n{1}"),
                        metaInformation, message);
                }
            }
            
            if (!metaInformation.IsEmpty())
            {
                metaInformation = FText::Format(INVTEXT("\n--------------------\n{0}"), metaInformation);
            }

            FText bodyText = FText::Format(INVTEXT("{0}{1}"),
                baseText, metaInformation);
            return bodyText;
        })
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.0f, 0.0f)
            [
                SNew(SOverlay)

                + SOverlay::Slot()
                [
                    SNew(SSGDynamicTextAssetIcon)
                    .Serializer(weakSerializer)
                    .Visibility(EVisibility::HitTestInvisible)
                ]

                // Top-left source control status badge: self-contained event-based widget,
                // updates on construction and on the SCC status cache's OnStatusChanged only
                + SOverlay::Slot()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Top)
                [
                    SNew(SSGDynamicTextAssetSCCBadge)
                    .FilePath(Item->FilePath)
                    .Visibility(EVisibility::HitTestInvisible)
                ]

                // Top-right unsaved-change star badge: self-contained event-based widget,
                // updates on construction and on the toolkit's OnDirtyStateChanged only
                + SOverlay::Slot()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Top)
                [
                    SNew(SSGDynamicTextAssetDirtyBadge)
                    .FilePath(Item->FilePath)
                    .Visibility(EVisibility::HitTestInvisible)
                ]
            ]

            // Name and class
            + SHorizontalBox::Slot()
            .Padding(4.0f, 0.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Left)
            [
                SNew(SVerticalBox)

                // DDO name
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(Item->GetDisplayName())
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]

                // Add class below name
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(className))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]
            ]

            // ID copy button and text
            // We replace the boundless Spacer with a FillWidth right-aligned slot
            // so the text can gracefully shrink with an ellipsis when the panel is narrowed.
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Right)
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SHorizontalBox)

                // ID
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item->Id.ToString()))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .Justification(ETextJustify::InvariantRight)
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]

                // Copy button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    FSGDTADetailCustomization::CreateCopyButton(
                        FOnClicked::CreateLambda([Item]() -> FReply
                        {
                            if (Item.IsValid())
                            {
                                FPlatformApplicationMisc::ClipboardCopy(*Item->Id.ToString());
                                UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied ID to clipboard"));
                            }
                            return FReply::Handled();
                        }),
                        INVTEXT("Copy ID to clipboard"))
                ]
            ]
        ];
}

void SSGDynamicTextAssetTileView::OnSelectionChanged(TSharedPtr<FSGDTAAssetListItem> Item, ESelectInfo::Type SelectInfo)
{
    // Fire single-item delegate (backwards compatibility) with first selected item
    if (OnItemSelected.IsBound())
    {
        TSharedPtr<FSGDTAAssetListItem> firstSelected = GetSelectedItem();
        OnItemSelected.Execute(firstSelected);
    }

    // Fire multi-select delegate with all selected items
    if (OnSelectionChangedDelegate.IsBound())
    {
        TArray<TSharedPtr<FSGDTAAssetListItem>> selectedItems = GetSelectedItems();
        OnSelectionChangedDelegate.Execute(selectedItems);
    }
}

void SSGDynamicTextAssetTileView::OnDoubleClick(TSharedPtr<FSGDTAAssetListItem> Item)
{
    if (OnItemDoubleClicked.IsBound())
    {
        OnItemDoubleClicked.Execute(Item);
    }
}

TSharedPtr<SWidget> SSGDynamicTextAssetTileView::GenerateContextMenu()
{
    TArray<TSharedPtr<FSGDTAAssetListItem>> selectedItems = GetSelectedItems();
    if (selectedItems.Num() == 0)
    {
        return nullptr;
    }

    // Build the menu with ExternalCommandList so entries with FUICommandInfo show their key binding
    FMenuBuilder menuBuilder(true, ExternalCommandList);

    // Multi-select context menu
    if (selectedItems.Num() > 1)
    {
        // Open N items (Enter)
        menuBuilder.AddMenuEntry(
            FText::Format(INVTEXT("Open {0} items"), FText::AsNumber(selectedItems.Num())),
            INVTEXT("Open all selected dynamic text assets in the editor"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
            FUIAction(
                FExecuteAction::CreateLambda([this, selectedItems]()
                {
                    if (OnOpenRequested.IsBound())
                    {
                        OnOpenRequested.Execute(selectedItems);
                    }
                })
            ),
            NAME_None,
            EUserInterfaceActionType::Button,
            NAME_None,
            FSGDynamicTextAssetBrowserCommands::Get().Open->GetFirstValidChord()->GetInputText()
        );

        // Rename (disabled for multi-select)
        menuBuilder.AddMenuEntry(
            FGenericCommands::Get().Rename->GetLabel(),
            INVTEXT("Select a single item to rename"),
            FGenericCommands::Get().Rename->GetIcon(),
            FUIAction(
                FExecuteAction::CreateLambda([]() {}),
                FCanExecuteAction::CreateLambda([]() { return false; })
            ),
            NAME_None,
            EUserInterfaceActionType::Button,
            NAME_None,
            FGenericCommands::Get().Rename->GetFirstValidChord()->GetInputText()
        );

        menuBuilder.AddSeparator();

        // Delete N items (DELETE)
        menuBuilder.AddMenuEntry(
            FText::Format(INVTEXT("Delete {0} items"), FText::AsNumber(selectedItems.Num())),
            INVTEXT("Delete the selected dynamic text assets"),
            FGenericCommands::Get().Delete->GetIcon(),
            FUIAction(
                FExecuteAction::CreateLambda([this, selectedItems]()
                {
                    if (OnDeleteRequested.IsBound())
                    {
                        OnDeleteRequested.Execute(selectedItems);
                    }
                })
            ),
            NAME_None,
            EUserInterfaceActionType::Button,
            NAME_None,
            FGenericCommands::Get().Delete->GetFirstValidChord()->GetInputText()
        );

        // Duplicate (disabled for multi-select)
        menuBuilder.AddMenuEntry(
            FGenericCommands::Get().Duplicate->GetLabel(),
            INVTEXT("Select a single item to duplicate"),
            FGenericCommands::Get().Duplicate->GetIcon(),
            FUIAction(
                FExecuteAction::CreateLambda([]() {}),
                FCanExecuteAction::CreateLambda([]() { return false; })
            ),
            NAME_None,
            EUserInterfaceActionType::Button,
            NAME_None,
            FGenericCommands::Get().Duplicate->GetFirstValidChord()->GetInputText()
        );

        menuBuilder.AddSeparator();

        // Show in Explorer (disabled for multi-select)
        menuBuilder.AddMenuEntry(
            INVTEXT("Show in Explorer"),
            INVTEXT("Select a single item to show in Explorer"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
            FUIAction(
                FExecuteAction::CreateLambda([]() {}),
                FCanExecuteAction::CreateLambda([]() { return false; })
            )
        );

        // Validate N items
        menuBuilder.AddMenuEntry(
            FText::Format(INVTEXT("Validate {0} items"), FText::AsNumber(selectedItems.Num())),
            INVTEXT("Validate the selected dynamic text assets"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeveloperTools.MenuIcon"),
            FUIAction(FExecuteAction::CreateLambda([this, selectedItems]()
            {
                if (OnValidateRequested.IsBound())
                {
                    OnValidateRequested.Execute(selectedItems);
                }
            }))
        );

        // Revision Control submenu: added only when source control is enabled, so it is absent
        // (not a disabled stub) otherwise. Placed before the extension hook so extensions stay last.
        if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
        {
            menuBuilder.AddSubMenu(
                INVTEXT("Revision Control"),
                INVTEXT("Revision control actions for the selected dynamic text asset(s)"),
                FNewMenuDelegate::CreateSP(this, &SSGDynamicTextAssetTileView::FillRevisionControlSubMenu, selectedItems),
                false,
                FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Icon")
            );
        }

        // Extension point for additional menu entries (e.g., Change File Type)
        if (OnContextMenuExtension.IsBound())
        {
            menuBuilder.AddSeparator();
            OnContextMenuExtension.Execute(menuBuilder, selectedItems);
        }

        return menuBuilder.MakeWidget();
    }

    // Single-item context menu: full menu
    TSharedPtr<FSGDTAAssetListItem> selectedItem = selectedItems[0];

    // Open action (Enter)
    menuBuilder.AddMenuEntry(
        INVTEXT("Open"),
        INVTEXT("Open this dynamic text asset in the editor"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
        FUIAction(FExecuteAction::CreateLambda([selectedItem]()
        {
            if (selectedItem.IsValid())
            {
                FSGDynamicTextAssetEditorToolkit::OpenEditorForFile(selectedItem->FilePath);
            }
        })),
        NAME_None,
        EUserInterfaceActionType::Button,
        NAME_None,
        FSGDynamicTextAssetBrowserCommands::Get().Open->GetFirstValidChord()->GetInputText()
    );

    // Rename action (F2)
    menuBuilder.AddMenuEntry(
        FGenericCommands::Get().Rename->GetLabel(),
        FGenericCommands::Get().Rename->GetDescription(),
        FGenericCommands::Get().Rename->GetIcon(),
        FUIAction(FExecuteAction::CreateLambda([this, selectedItem]()
        {
            if (!selectedItem.IsValid() || selectedItem->FilePath.IsEmpty())
            {
                return;
            }

            FString oldFilePath = selectedItem->FilePath;
            FString newFilePath;
            if (SSGDynamicTextAssetRenameDialog::ShowModal(oldFilePath, selectedItem->UserFacingId, newFilePath))
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Renamed dynamic text asset via context menu: %s -> %s"),
                    *selectedItem->UserFacingId, *newFilePath);

                // Notify any open editor about the rename so it updates its file path and title
                FSGDynamicTextAssetEditorToolkit::NotifyFileRenamed(oldFilePath, newFilePath);

                // Extract file info from the new file to update the list item
                FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(newFilePath);
                if (fileInfo.bIsValid)
                {
                    // Update the selected item in-place
                    selectedItem->UserFacingId = fileInfo.UserFacingId;
                    selectedItem->FilePath = newFilePath;

                    // Re-sort AllItems by UserFacingId
                    AllItems.Sort([](const TSharedPtr<FSGDTAAssetListItem>& A, const TSharedPtr<FSGDTAAssetListItem>& B)
                    {
                        return A->UserFacingId < B->UserFacingId;
                    });

                    // Re-apply filter and refresh
                    ApplyInstanceFilter();

                    if (ListView.IsValid())
                    {
                        ListView->RebuildList();
                        ListView->SetSelection(selectedItem);
                    }
                }
            }
        })),
        NAME_None,
        EUserInterfaceActionType::Button,
        NAME_None,
        FGenericCommands::Get().Rename->GetFirstValidChord()->GetInputText()
    );

    menuBuilder.AddSeparator();

    // Delete action (DELETE) - route through FGenericCommands + CommandList for automatic shortcut hint
    menuBuilder.AddMenuEntry(
        FGenericCommands::Get().Delete
    );

    // Duplicate action (Ctrl+D)
    menuBuilder.AddMenuEntry(
        FGenericCommands::Get().Duplicate->GetLabel(),
        FGenericCommands::Get().Duplicate->GetDescription(),
        FGenericCommands::Get().Duplicate->GetIcon(),
        FUIAction(FExecuteAction::CreateLambda([this, selectedItem]()
        {
            if (!selectedItem.IsValid() || selectedItem->FilePath.IsEmpty())
            {
                return;
            }

            FString createdFilePath;
            FSGDynamicTextAssetId createdId;

            if (SSGDynamicTextAssetDuplicateDialog::ShowModal(selectedItem->FilePath, selectedItem->UserFacingId, createdFilePath, createdId))
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Duplicated dynamic text asset via context menu: %s"), *createdFilePath);

                // Extract file info to create the new list item
                FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(createdFilePath);
                if (fileInfo.bIsValid)
                {
                    // Resolve class via Asset Type ID (O(1) map lookup)
                    UClass* itemClass = nullptr;
                    if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
                    {
                        itemClass = registry->ResolveClassForTypeId(fileInfo.AssetTypeId);
                    }

                    // Add the new item to AllItems
                    TSharedPtr<FSGDTAAssetListItem> newItem = MakeShared<FSGDTAAssetListItem>(
                        createdId,
                        fileInfo.UserFacingId,
                        createdFilePath,
                        itemClass ? itemClass : selectedItem->DynamicTextAssetClass.Get(),
                        fileInfo.AssetTypeId,
                        fileInfo.SerializerFormat
                    );
                    AllItems.Add(newItem);

                    // Sort AllItems by UserFacingId
                    AllItems.Sort([](const TSharedPtr<FSGDTAAssetListItem>& A, const TSharedPtr<FSGDTAAssetListItem>& B)
                    {
                        return A->UserFacingId < B->UserFacingId;
                    });

                    // Re-apply filter and refresh
                    ApplyInstanceFilter();

                    if (ListView.IsValid())
                    {
                        ListView->RequestListRefresh();
                        ListView->SetSelection(newItem);
                    }
                }
            }
        })),
        NAME_None,
        EUserInterfaceActionType::Button,
        NAME_None,
        FGenericCommands::Get().Duplicate->GetFirstValidChord()->GetInputText()
    );

    menuBuilder.AddSeparator();

    // Copy ID action
    menuBuilder.AddMenuEntry(
        INVTEXT("Copy ID"),
        INVTEXT("Copy the ID to the clipboard"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
        FUIAction(FExecuteAction::CreateLambda([selectedItem]()
        {
            if (selectedItem.IsValid())
            {
                FPlatformApplicationMisc::ClipboardCopy(*selectedItem->Id.ToString());
                UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied ID to clipboard: %s"), *selectedItem->Id.ToString());
            }
        }))
    );

    // Copy User Facing ID action
    menuBuilder.AddMenuEntry(
        INVTEXT("Copy User Facing ID"),
        INVTEXT("Copy the user-facing ID to the clipboard"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
        FUIAction(FExecuteAction::CreateLambda([selectedItem]()
        {
            if (selectedItem.IsValid())
            {
                FPlatformApplicationMisc::ClipboardCopy(*selectedItem->UserFacingId);
                UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied User Facing ID to clipboard: %s"), *selectedItem->UserFacingId);
            }
        }))
    );

    menuBuilder.AddSeparator();

    // Reference Viewer action
    menuBuilder.AddMenuEntry(
        INVTEXT("Open Reference Viewer"),
        INVTEXT("View what references this dynamic text asset and what it depends on"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"),
        FUIAction(FExecuteAction::CreateLambda([selectedItem]()
        {
            if (selectedItem.IsValid())
            {
                SSGDynamicTextAssetReferenceViewer::OpenViewer(selectedItem->Id, selectedItem->UserFacingId);
            }
        }))
    );

    menuBuilder.AddSeparator();

    // Show in Explorer action
    menuBuilder.AddMenuEntry(
        INVTEXT("Show in Explorer"),
        INVTEXT("Open the file location in the system file browser"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
        FUIAction(FExecuteAction::CreateLambda([selectedItem]()
        {
            if (selectedItem.IsValid())
            {
                // Pass full file path to select the file in Explorer
                FPlatformProcess::ExploreFolder(*selectedItem->FilePath);
            }
        }))
    );

    // Validate action
    menuBuilder.AddMenuEntry(
        INVTEXT("Validate"),
        INVTEXT("Validate this dynamic text asset file"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeveloperTools.MenuIcon"),
        FUIAction(FExecuteAction::CreateLambda([this, selectedItems]()
        {
            if (OnValidateRequested.IsBound())
            {
                OnValidateRequested.Execute(selectedItems);
            }
        }))
    );

    // Revision Control submenu: added only when source control is enabled, so it is absent
    // (not a disabled stub) otherwise. Placed before the extension hook so extensions stay last.
    if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        menuBuilder.AddSubMenu(
            INVTEXT("Revision Control"),
            INVTEXT("Revision control actions for the selected dynamic text asset(s)"),
            FNewMenuDelegate::CreateSP(this, &SSGDynamicTextAssetTileView::FillRevisionControlSubMenu, selectedItems),
            false,
            FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Icon")
        );
    }

    // Extension point for additional menu entries (e.g., Change File Type)
    if (OnContextMenuExtension.IsBound())
    {
        menuBuilder.AddSeparator();
        OnContextMenuExtension.Execute(menuBuilder, selectedItems);
    }

    return menuBuilder.MakeWidget();
}

void SSGDynamicTextAssetTileView::FillRevisionControlSubMenu(FMenuBuilder& MenuBuilder, TArray<TSharedPtr<FSGDTAAssetListItem>> SelectedItems)
{
    // Refresh: always enabled while source control is on. Runs an async FUpdateStatus for the
    // selected paths and invalidates the SCC status cache per path on completion, mirroring the
    // browser's RequestSourceControlStatusUpdate pattern so badges and menu enablement refresh.
    MenuBuilder.AddMenuEntry(
        INVTEXT("Refresh"),
        INVTEXT("Update the revision control status of the selected files"),
        FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Refresh"),
        FUIAction(FExecuteAction::CreateLambda([this, SelectedItems]()
        {
            if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
            {
                return;
            }

            TArray<FString> paths;
            paths.Reserve(SelectedItems.Num());
            for (const TSharedPtr<FSGDTAAssetListItem>& item : SelectedItems)
            {
                if (item.IsValid() && !item->FilePath.IsEmpty())
                {
                    paths.Add(item->FilePath);
                }
            }

            if (paths.IsEmpty())
            {
                return;
            }

            // Request modified-state detection so Perforce populates IsModified(); the Git provider
            // reports working-tree edits from a plain status regardless, so the flag is harmless there.
            // Matches the option set on the browser refresh and the write-triggered cache refresh.
            TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> updateOperation = ISourceControlOperation::Create<FUpdateStatus>();
            updateOperation->SetUpdateModifiedState(true);

            // Fire-and-forget async status update. The completion is bound via CreateSPLambda so it
            // no-ops safely if this widget is destroyed while the operation is in flight.
            ISourceControlProvider& sourceControlProvider = ISourceControlModule::Get().GetProvider();
            sourceControlProvider.Execute(
                updateOperation,
                paths,
                EConcurrency::Asynchronous,
                FSourceControlOperationComplete::CreateSPLambda(this,
                    [paths](const FSourceControlOperationRef& Operation, ECommandResult::Type Result)
                    {
                        if (Result != ECommandResult::Succeeded)
                        {
                            // Failed or cancelled update: leave cached state as-is
                            return;
                        }

                        // The engine's state cache is now fresh; clear our cached entries per path so
                        // the next query repopulates and any visible badges repaint via OnStatusChanged.
                        if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
                        {
                            FSGDynamicTextAssetSCCStatusCache& cache = FSGDynamicTextAssetSCCStatusCache::Get();
                            for (const FString& path : paths)
                            {
                                cache.InvalidateCachePath(path);
                            }
                        }
                    })
            );
        }))
    );

    // Mark For Add: enabled only when every selected file is untracked (NotInSourceControl).
    // On invoke, marks the files for add and invalidates the cache per path so state refreshes.
    MenuBuilder.AddMenuEntry(
        INVTEXT("Mark For Add"),
        INVTEXT("Mark the selected files for add in revision control"),
        FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Add"),
        FUIAction(
            FExecuteAction::CreateLambda([this, SelectedItems]()
            {
                TArray<FString> paths;
                paths.Reserve(SelectedItems.Num());
                for (const TSharedPtr<FSGDTAAssetListItem>& item : SelectedItems)
                {
                    if (item.IsValid() && !item->FilePath.IsEmpty())
                    {
                        paths.Add(item->FilePath);
                    }
                }

                if (paths.IsEmpty())
                {
                    return;
                }

                FSGDynamicTextAssetSourceControl::MarkFilesForAdd(paths);

                if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
                {
                    FSGDynamicTextAssetSCCStatusCache& cache = FSGDynamicTextAssetSCCStatusCache::Get();
                    for (const FString& path : paths)
                    {
                        cache.InvalidateCachePath(path);
                    }
                }
            }),
            FCanExecuteAction::CreateLambda([this, SelectedItems]()
            {
                return DoesSelectionMatchStatus(SelectedItems,
                    ESGDynamicTextAssetSourceControlStatus::NotInSourceControl, /*bRequireAll*/ true);
            })
        )
    );

    // Check In: enabled when at least one selected file is in a checked-in-able state (checked out,
    // added, locally modified, or marked for delete). Delegates to the engine check-in dialog for the
    // selected files' raw absolute paths; DTA files are plain files, so the filenames are passed
    // directly despite the parameter being named for package names.
    MenuBuilder.AddMenuEntry(
        INVTEXT("Check In"),
        INVTEXT("Check in the selected files to revision control"),
        FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Submit"),
        FUIAction(
            FExecuteAction::CreateLambda([this, SelectedItems]()
            {
                TArray<FString> paths;
                paths.Reserve(SelectedItems.Num());
                for (const TSharedPtr<FSGDTAAssetListItem>& item : SelectedItems)
                {
                    if (item.IsValid() && !item->FilePath.IsEmpty())
                    {
                        paths.Add(item->FilePath);
                    }
                }

                if (paths.IsEmpty())
                {
                    return;
                }

                // Opens the engine check-in dialog for exactly these paths. The result info is not
                // inspected here; the cache refreshes via the file manager's SCC change notifications.
                FCheckinResultInfo resultInfo;
                FSourceControlWindows::PromptForCheckin(resultInfo, paths);
            }),
            FCanExecuteAction::CreateLambda([this, SelectedItems]()
            {
                // Any-match "has local changes" over {CheckedOut, Added, ModifiedLocally, MarkedForDelete}.
                // Check In keeps this broader set, including a clean CheckedOut. Revert uses the tightened
                // IsSelectionAnyRevertable ({Added, ModifiedLocally, MarkedForDelete}), which excludes it.
                return IsSelectionAnyLocallyChanged(SelectedItems);
            })
        )
    );

    // History: enabled only when every selected file is under source control (tracked). Delegates to
    // the engine revision-history view for the selected files' raw absolute paths.
    MenuBuilder.AddMenuEntry(
        INVTEXT("History"),
        INVTEXT("View the revision history of the selected files"),
        FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.History"),
        FUIAction(
            FExecuteAction::CreateLambda([this, SelectedItems]()
            {
                TArray<FString> paths;
                paths.Reserve(SelectedItems.Num());
                for (const TSharedPtr<FSGDTAAssetListItem>& item : SelectedItems)
                {
                    if (item.IsValid() && !item->FilePath.IsEmpty())
                    {
                        paths.Add(item->FilePath);
                    }
                }

                if (paths.IsEmpty())
                {
                    return;
                }

                FSourceControlWindows::DisplayRevisionHistory(paths);
            }),
            FCanExecuteAction::CreateLambda([this, SelectedItems]()
            {
                return IsSelectionAllTracked(SelectedItems);
            })
        )
    );

    // Diff Against Depot: launches the configured external text-diff tool comparing the selected
    // file's local working copy against its head depot revision. This is an interim, format-agnostic
    // line-level diff: it fetches the depot revision to a temp file and shells out to the external
    // tool, both of which operate on plain files, unlike the engine history widget's asset-only diff
    // path that rejects .dta.* files. A DTA-aware semantic (property-level) diff is a separate,
    // later effort that supersedes or augments this raw text view; both can coexist as distinct views.
    //
    // A file diff is inherently pairwise, so this enables only for a single selected file. Multi-select
    // hard-disables it. It also requires the file to be tracked and the provider to allow diff against
    // depot; whether the file actually has depot history is confirmed in the action body, since that
    // needs a live provider query rather than a cheap cache read on every menu paint.
    MenuBuilder.AddMenuEntry(
        INVTEXT("Diff Against Depot"),
        INVTEXT("Diff the selected file's local working copy against its head depot revision using the external text-diff tool"),
        FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Diff"),
        FUIAction(
            FExecuteAction::CreateLambda([this, SelectedItems]()
            {
                // Single-select only: bail unless exactly one usable item is selected.
                if (SelectedItems.Num() != 1 || !SelectedItems[0].IsValid() || SelectedItems[0]->FilePath.IsEmpty())
                {
                    return;
                }

                if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
                {
                    return;
                }

                const FString localWorkingPath = SelectedItems[0]->FilePath;

                // Small helper to surface a graceful, explained failure toast instead of a silent no-op.
                auto reportFailure = [](const FText& Message)
                {
                    FNotificationInfo notification(Message);
                    notification.ExpireDuration = 4.0f;
                    FSlateNotificationManager::Get().AddNotification(notification);
                };

                ISourceControlProvider& provider = ISourceControlModule::Get().GetProvider();

                if (!provider.AllowsDiffAgainstDepot())
                {
                    reportFailure(INVTEXT("The active revision control provider does not support diffing against the depot."));
                    return;
                }

                // Make sure history is current so GetHistoryItem(0) resolves the head revision. This
                // mirrors the engine's own DiffAgainstDepot: a synchronous FUpdateStatus with history
                // enabled on the single file path. Synchronous is acceptable here because a single small
                // DTA's head revision is cheap to fetch and keeps the invoke self-contained; there is no
                // batch of files to amortise an async round-trip over.
                TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> updateStatusOperation =
                    ISourceControlOperation::Create<FUpdateStatus>();
                updateStatusOperation->SetUpdateHistory(true);
                provider.Execute(updateStatusOperation, localWorkingPath);

                FSourceControlStatePtr state = provider.GetState(localWorkingPath, EStateCacheUsage::Use);
                if (!state.IsValid() || !state->IsSourceControlled() || state->GetHistorySize() <= 0)
                {
                    reportFailure(INVTEXT("The selected file has no depot revision to diff against."));
                    return;
                }

                TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> revision = state->GetHistoryItem(0);
                if (!revision.IsValid())
                {
                    reportFailure(INVTEXT("Could not resolve the head depot revision of the selected file."));
                    return;
                }

                // Fetch the depot revision to a temp file. An empty in/out filename tells the provider to
                // generate a temp path under the engine diff area; no explicit cleanup is required here.
                FString tempDepotFile;
                if (!revision->Get(tempDepotFile) || tempDepotFile.IsEmpty())
                {
                    reportFailure(INVTEXT("Failed to fetch the depot revision of the selected file."));
                    return;
                }

                // CreateDiffProcess shells out to the configured external tool and, when none is set or it
                // cannot run, prompts the user to choose one and persists it. It returns false if the tool
                // could not be launched (including a declined prompt), which we surface as an explained toast.
                const FString diffCommand = GetDefault<UEditorLoadingSavingSettings>()->TextDiffToolPath.FilePath;
                const IAssetTools& assetTools = FAssetToolsModule::GetModule().Get();
                if (!assetTools.CreateDiffProcess(diffCommand, tempDepotFile, localWorkingPath))
                {
                    reportFailure(INVTEXT("Could not launch the external diff tool. Set a valid text-diff tool in Editor Preferences and try again."));
                }
            }),
            FCanExecuteAction::CreateLambda([this, SelectedItems]()
            {
                // Enable only for a single tracked file whose provider allows diff against depot.
                if (SelectedItems.Num() != 1)
                {
                    return false;
                }

                if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
                {
                    return false;
                }

                // IsSelectionAllTracked over the single-element selection confirms the file is under
                // source control (not untracked or unknown) using the cached status.
                if (!IsSelectionAllTracked(SelectedItems))
                {
                    return false;
                }

                return ISourceControlModule::Get().GetProvider().AllowsDiffAgainstDepot();
            })
        )
    );

    // Revert: enabled when at least one selected file has changes to discard, using the tightened
    // {Added, ModifiedLocally, MarkedForDelete} predicate (IsSelectionAnyRevertable). Unlike Check In,
    // this excludes a clean CheckedOut (checked out but unmodified), which has nothing to revert.
    // On invoke, prompts for confirmation, reverts each selected file synchronously, invalidates the
    // SCC status cache per reverted path, reloads any open editor for a reverted path so its content
    // and dirty star match disk, and reports the outcome in a result toast.
    //
    // No "Merge" entry is provided even though the native revision-control menu offers one: dynamic
    // text asset payloads are opaque to source control, so there is no semantic three-way merge to
    // perform. Reverting discards local changes wholesale, which is the only meaningful conflict
    // resolution for these files.
    MenuBuilder.AddMenuEntry(
        INVTEXT("Revert"),
        INVTEXT("Discard local changes to the selected files, restoring them to the depot revision"),
        FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Revert"),
        FUIAction(
            FExecuteAction::CreateLambda([this, SelectedItems]()
            {
                TArray<FString> paths;
                paths.Reserve(SelectedItems.Num());
                for (const TSharedPtr<FSGDTAAssetListItem>& item : SelectedItems)
                {
                    if (item.IsValid() && !item->FilePath.IsEmpty())
                    {
                        paths.Add(item->FilePath);
                    }
                }

                if (paths.IsEmpty())
                {
                    return;
                }

                // Confirmation dialog naming the count; bail if the user declines.
                const FText confirmMessage = FText::Format(
                    INVTEXT("Revert {0} file(s)? This discards all local changes and cannot be undone."),
                    FText::AsNumber(paths.Num()));
                const EAppReturnType::Type choice = FMessageDialog::Open(EAppMsgType::YesNo,
                    confirmMessage, INVTEXT("Revert Files"));
                if (choice != EAppReturnType::Yes)
                {
                    return;
                }

                int32 successCount = 0;
                int32 failureCount = 0;
                for (const FString& path : paths)
                {
                    if (FSGDynamicTextAssetSourceControl::RevertFile(path))
                    {
                        ++successCount;

                        // Invalidate the cached status so badges and menu enablement refresh.
                        if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
                        {
                            FSGDynamicTextAssetSCCStatusCache::Get().InvalidateCachePath(path);
                        }

                        // Reload any open editor for this path so its content and dirty star match disk.
                        FSGDynamicTextAssetEditorToolkit::NotifyFileReverted(path);
                    }
                    else
                    {
                        ++failureCount;
                    }
                }

                // Result toast: success summary, or a summary naming the failure count.
                FNotificationInfo notification(
                    failureCount == 0
                        ? FText::Format(INVTEXT("Reverted {0} file(s)."), FText::AsNumber(successCount))
                        : FText::Format(INVTEXT("Reverted {0} file(s); {1} failed."),
                            FText::AsNumber(successCount), FText::AsNumber(failureCount)));
                notification.ExpireDuration = 4.0f;
                FSlateNotificationManager::Get().AddNotification(notification);
            }),
            FCanExecuteAction::CreateLambda([this, SelectedItems]()
            {
                return IsSelectionAnyRevertable(SelectedItems);
            })
        )
    );
}

bool SSGDynamicTextAssetTileView::IsSelectionAllTracked(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems) const
{
    // Guard: no cache to read (module torn down or never created) means no enablement.
    if (!FSGDynamicTextAssetSCCStatusCache::IsAvailable() || SelectedItems.IsEmpty())
    {
        return false;
    }

    FSGDynamicTextAssetSCCStatusCache& cache = FSGDynamicTextAssetSCCStatusCache::Get();

    for (const TSharedPtr<FSGDTAAssetListItem>& item : SelectedItems)
    {
        if (!item.IsValid() || item->FilePath.IsEmpty())
        {
            // An unusable item cannot be confirmed as tracked, so it fails the all-match.
            return false;
        }

        const ESGDynamicTextAssetSourceControlStatus status = cache.GetCachedStatus(item->FilePath);
        if (status == ESGDynamicTextAssetSourceControlStatus::NotInSourceControl
            || status == ESGDynamicTextAssetSourceControlStatus::Unknown)
        {
            // A single untracked or unknown file disqualifies the whole selection from having history.
            return false;
        }
    }

    return true;
}

bool SSGDynamicTextAssetTileView::IsSelectionAnyLocallyChanged(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems) const
{
    // Any-match over the "has local changes" status set, composed as OR of single-status any-match
    // calls. DoesSelectionMatchStatus is single-status, so this OR is logically equivalent to an
    // any-match over {CheckedOut, Added, ModifiedLocally, MarkedForDelete}. Its own guards cover the
    // cache-unavailable and empty-selection cases.
    return DoesSelectionMatchStatus(SelectedItems,
            ESGDynamicTextAssetSourceControlStatus::CheckedOut, /*bRequireAll*/ false)
        || DoesSelectionMatchStatus(SelectedItems,
            ESGDynamicTextAssetSourceControlStatus::Added, /*bRequireAll*/ false)
        || DoesSelectionMatchStatus(SelectedItems,
            ESGDynamicTextAssetSourceControlStatus::ModifiedLocally, /*bRequireAll*/ false)
        || DoesSelectionMatchStatus(SelectedItems,
            ESGDynamicTextAssetSourceControlStatus::MarkedForDelete, /*bRequireAll*/ false);
}

bool SSGDynamicTextAssetTileView::IsSelectionAnyRevertable(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems) const
{
    // Any-match over the "has changes Revert can discard" status set, composed as OR of single-status
    // any-match calls. This is a tightened version of IsSelectionAnyLocallyChanged that excludes a clean
    // CheckedOut (checked out but unmodified): that state has nothing to revert. MarkedForDelete stays in
    // the set because reverting a marked-for-delete file un-deletes it, which is a real local change. The
    // OR is logically equivalent to an any-match over {Added, ModifiedLocally, MarkedForDelete}. Its own
    // guards cover the cache-unavailable and empty-selection cases.
    return DoesSelectionMatchStatus(SelectedItems,
            ESGDynamicTextAssetSourceControlStatus::Added, /*bRequireAll*/ false)
        || DoesSelectionMatchStatus(SelectedItems,
            ESGDynamicTextAssetSourceControlStatus::ModifiedLocally, /*bRequireAll*/ false)
        || DoesSelectionMatchStatus(SelectedItems,
            ESGDynamicTextAssetSourceControlStatus::MarkedForDelete, /*bRequireAll*/ false);
}

bool SSGDynamicTextAssetTileView::DoesSelectionMatchStatus(const TArray<TSharedPtr<FSGDTAAssetListItem>>& SelectedItems,
    ESGDynamicTextAssetSourceControlStatus Status, bool bRequireAll) const
{
    // Guard: no cache to read (module torn down or never created) means no enablement.
    if (!FSGDynamicTextAssetSCCStatusCache::IsAvailable() || SelectedItems.IsEmpty())
    {
        return false;
    }

    FSGDynamicTextAssetSCCStatusCache& cache = FSGDynamicTextAssetSCCStatusCache::Get();

    for (const TSharedPtr<FSGDTAAssetListItem>& item : SelectedItems)
    {
        if (!item.IsValid() || item->FilePath.IsEmpty())
        {
            // Treat an unusable item as a non-match: it cannot satisfy all-match and does not
            // contribute to any-match either.
            if (bRequireAll)
            {
                return false;
            }
            continue;
        }

        const bool bMatches = (cache.GetCachedStatus(item->FilePath) == Status);

        if (bRequireAll && !bMatches)
        {
            // All-match mode: first mismatch fails the whole selection.
            return false;
        }

        if (!bRequireAll && bMatches)
        {
            // Any-match mode: first match satisfies the selection.
            return true;
        }
    }

    // All-match: reached here with no mismatch, so every file matched.
    // Any-match: reached here with no match found.
    return bRequireAll;
}

void SSGDynamicTextAssetTileView::OnInstanceSearchTextChanged(const FText& NewText)
{
    InstanceSearchText = NewText.ToString();
    ApplyInstanceFilter();

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

void SSGDynamicTextAssetTileView::ApplyInstanceFilter()
{
    FilteredItems.Empty();

    if (InstanceSearchText.IsEmpty())
    {
        // No filter active, show all
        FilteredItems = AllItems;
        return;
    }

    // Filter AllItems by UserFacingId or GUID substring match
    for (const TSharedPtr<FSGDTAAssetListItem>& item : AllItems)
    {
        if (!item.IsValid())
        {
            continue;
        }

        if (item->UserFacingId.Contains(InstanceSearchText, ESearchCase::IgnoreCase) ||
            item->Id.ToString().Contains(InstanceSearchText, ESearchCase::IgnoreCase))
        {
            FilteredItems.Add(item);
        }
    }
}

int32 SSGDynamicTextAssetTileView::GetContentIndex() const
{
    return FilteredItems.Num() > 0 ? 1 : 0;
}
