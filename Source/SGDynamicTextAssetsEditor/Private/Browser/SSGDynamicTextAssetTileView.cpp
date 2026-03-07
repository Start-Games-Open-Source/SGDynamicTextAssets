// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDynamicTextAssetTileView.h"

#include "Browser/SSGDynamicTextAssetDuplicateDialog.h"
#include "Browser/SSGDynamicTextAssetRenameDialog.h"
#include "Editor/FSGDynamicTextAssetEditorToolkit.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "ReferenceViewer/SSGDynamicTextAssetReferenceViewer.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "Core/SGDynamicTextAsset.h"
#include "Editor/SGDynamicTextAssetIdentityCustomization.h"
#include "Editor/SSGDynamicTextAssetIcon.h"
#include "Framework/Commands/GenericCommands.h"
#include "Utilities/SGDynamicTextAssetSourceControl.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/MessageDialog.h"
#include "ReferenceViewer/SGDynamicTextAssetReferenceSubsystem.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"

FText FSGDynamicTextAssetListItem::GetDisplayName() const
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
                SAssignNew(ListView, SListView<TSharedPtr<FSGDynamicTextAssetListItem>>)
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

void SSGDynamicTextAssetTileView::SetItems(const TArray<TSharedPtr<FSGDynamicTextAssetListItem>>& InItems)
{
    AllItems = InItems;
    ApplyInstanceFilter();

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Tile view updated with %d items (%d filtered)"), AllItems.Num(), FilteredItems.Num());
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

TSharedPtr<FSGDynamicTextAssetListItem> SSGDynamicTextAssetTileView::GetSelectedItem() const
{
    if (ListView.IsValid())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selected = ListView->GetSelectedItems();
        if (selected.Num() > 0)
        {
            return selected[0];
        }
    }
    return nullptr;
}

TArray<TSharedPtr<FSGDynamicTextAssetListItem>> SSGDynamicTextAssetTileView::GetSelectedItems() const
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

    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : FilteredItems)
    {
        if (item.IsValid() && item->Id == Id)
        {
            ListView->SetSelection(item);
            return;
        }
    }
}

TSharedRef<ITableRow> SSGDynamicTextAssetTileView::GenerateRow(TSharedPtr<FSGDynamicTextAssetListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    const FString className = Item->DynamicTextAssetClass.IsValid() ? Item->DynamicTextAssetClass->GetName() : TEXT("Unknown");

    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = nullptr;
    if (Item->SerializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
    {
        serializer = FSGDynamicTextAssetFileManager::FindSerializerForTypeId(Item->SerializerTypeId);
    }
    // Account for if we failed to get the serializer for it
    // We NEED this information for readability
    if (!serializer.IsValid())
    {
        return SNew(STableRow<TSharedPtr<FSGDynamicTextAssetListItem>>, OwnerTable)
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

    return SNew(STableRow<TSharedPtr<FSGDynamicTextAssetListItem>>, OwnerTable)
        .Padding(FMargin(4.0f, 2.0f))
        .ToolTipText_Lambda([Item, weakSerializer]() -> FText
        {
            FText baseText = FText::Format(INVTEXT("DDO: {0}{1}\nID: {2}"),
                Item->GetDisplayName(),
                FText::FromString(weakSerializer.Pin().IsValid() ? weakSerializer.Pin()->GetFileExtension() : "[Unknown File Type]"),
                FText::FromString(Item->Id.ToString()));

            if (USGDynamicTextAssetReferenceSubsystem* refSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetReferenceSubsystem>())
            {
                int32 ReferencerCount = refSubsystem->GetReferencerCount(Item->Id);
                TArray<FSGDynamicTextAssetDependencyEntry> dependencies;
                refSubsystem->FindDependencies(Item->Id, dependencies);
                return FText::Format(INVTEXT("{0}\nReferenced By: {1} assets\nDependencies: {2} assets"),
                    baseText, ReferencerCount, dependencies.Num());
            }
            return baseText;
        })
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.0f, 0.0f)
            [
                SNew(SSGDynamicTextAssetIcon)
                .Serializer(weakSerializer)
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
                    FSGDynamicTextAssetIdentityCustomization::CreateCopyButton(
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

void SSGDynamicTextAssetTileView::OnSelectionChanged(TSharedPtr<FSGDynamicTextAssetListItem> Item, ESelectInfo::Type SelectInfo)
{
    // Fire single-item delegate (backwards compatibility) with first selected item
    if (OnItemSelected.IsBound())
    {
        TSharedPtr<FSGDynamicTextAssetListItem> firstSelected = GetSelectedItem();
        OnItemSelected.Execute(firstSelected);
    }

    // Fire multi-select delegate with all selected items
    if (OnSelectionChangedDelegate.IsBound())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = GetSelectedItems();
        OnSelectionChangedDelegate.Execute(selectedItems);
    }
}

void SSGDynamicTextAssetTileView::OnDoubleClick(TSharedPtr<FSGDynamicTextAssetListItem> Item)
{
    if (OnItemDoubleClicked.IsBound())
    {
        OnItemDoubleClicked.Execute(Item);
    }
}

TSharedPtr<SWidget> SSGDynamicTextAssetTileView::GenerateContextMenu()
{
    TArray<TSharedPtr<FSGDynamicTextAssetListItem>> selectedItems = GetSelectedItems();
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

        // Extension point for additional menu entries (e.g., Change File Type)
        if (OnContextMenuExtension.IsBound())
        {
            menuBuilder.AddSeparator();
            OnContextMenuExtension.Execute(menuBuilder, selectedItems);
        }

        return menuBuilder.MakeWidget();
    }

    // Single-item context menu: full menu
    TSharedPtr<FSGDynamicTextAssetListItem> selectedItem = selectedItems[0];

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

                // Extract metadata from the new file to update the list item
                FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(newFilePath);
                if (metadata.bIsValid)
                {
                    // Update the selected item in-place
                    selectedItem->UserFacingId = metadata.UserFacingId;
                    selectedItem->FilePath = newFilePath;

                    // Re-sort AllItems by UserFacingId
                    AllItems.Sort([](const TSharedPtr<FSGDynamicTextAssetListItem>& A, const TSharedPtr<FSGDynamicTextAssetListItem>& B)
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

                // Extract metadata to create the new list item
                FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(createdFilePath);
                if (metadata.bIsValid)
                {
                    // Resolve class via Asset Type ID (O(1) map lookup)
                    UClass* itemClass = nullptr;
                    if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
                    {
                        itemClass = registry->ResolveClassForTypeId(metadata.AssetTypeId);
                    }

                    // Add the new item to AllItems
                    TSharedPtr<FSGDynamicTextAssetListItem> newItem = MakeShared<FSGDynamicTextAssetListItem>(
                        createdId,
                        metadata.UserFacingId,
                        createdFilePath,
                        itemClass ? itemClass : selectedItem->DynamicTextAssetClass.Get(),
                        metadata.AssetTypeId,
                        metadata.SerializerTypeId
                    );
                    AllItems.Add(newItem);

                    // Sort AllItems by UserFacingId
                    AllItems.Sort([](const TSharedPtr<FSGDynamicTextAssetListItem>& A, const TSharedPtr<FSGDynamicTextAssetListItem>& B)
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

    // Extension point for additional menu entries (e.g., Change File Type)
    if (OnContextMenuExtension.IsBound())
    {
        menuBuilder.AddSeparator();
        OnContextMenuExtension.Execute(menuBuilder, selectedItems);
    }

    return menuBuilder.MakeWidget();
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
    for (const TSharedPtr<FSGDynamicTextAssetListItem>& item : AllItems)
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
