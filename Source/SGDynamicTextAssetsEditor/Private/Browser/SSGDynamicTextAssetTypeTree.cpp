// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDynamicTextAssetTypeTree.h"

#include "SGDynamicTextAssetsEditorModule.h"
#include "Core/SGDynamicTextAsset.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Widgets/Views/STableRow.h"

void SSGDynamicTextAssetTypeTree::Construct(const FArguments& InArgs)
{
    OnTypeSelected = InArgs._OnTypeSelected;

    ChildSlot
    [
        SNew(SVerticalBox)

        // Search bar
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SAssignNew(TypeSearchBox, SSearchBox)
            .HintText(INVTEXT("Search types..."))
            .OnTextChanged(this, &SSGDynamicTextAssetTypeTree::OnTypeSearchTextChanged)
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
                    .Text(INVTEXT("No dynamic text asset types registered. Create a subclass of USGDynamicTextAsset or a new Dynamic Text Asset Provider and register it with the registry."))
                    .Justification(ETextJustify::Center)
                    .AutoWrapText(true)
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
            ]

            // Index 1: Tree view
            + SWidgetSwitcher::Slot()
            [
                SAssignNew(TreeView, STreeView<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>)
                .TreeItemsSource(&FilteredRootItems)
                .OnGenerateRow(this, &SSGDynamicTextAssetTypeTree::GenerateRow)
                .OnGetChildren(this, &SSGDynamicTextAssetTypeTree::GetTreeChildren)
                .OnSelectionChanged(this, &SSGDynamicTextAssetTypeTree::OnSelectionChanged)
                .SelectionMode(ESelectionMode::Single)
            ]
        ]
    ];

    RefreshTree();
}

void SSGDynamicTextAssetTypeTree::RefreshTree()
{
    RootItems.Empty();
    AllItems.Empty();

    BuildTreeHierarchy();
    ApplyTypeFilter();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();

        // Expand all items by default
        for (const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& item : FilteredRootItems)
        {
            TreeView->SetItemExpansion(item, true);
        }
    }

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Type tree refreshed with %d root items (%d filtered)"), RootItems.Num(), FilteredRootItems.Num());
}

UClass* SSGDynamicTextAssetTypeTree::GetSelectedClass() const
{
    if (TreeView.IsValid())
    {
        TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>> selected = TreeView->GetSelectedItems();
        if (selected.Num() > 0 && selected[0].IsValid())
        {
            return selected[0]->Class.Get();
        }
    }
    return nullptr;
}

void SSGDynamicTextAssetTypeTree::SelectClass(UClass* ClassToSelect)
{
    if (!ClassToSelect)
    {
        return;
    }
    if (!TreeView.IsValid())
    {
        return;
    }

    if (TSharedPtr<FSGDynamicTextAssetTypeTreeItem>* found = AllItems.Find(ClassToSelect))
    {
        TreeView->SetSelection(*found);

        // Expand parents
        TWeakPtr<FSGDynamicTextAssetTypeTreeItem> parent = (*found)->Parent;
        while (parent.IsValid())
        {
            TreeView->SetItemExpansion(parent.Pin(), true);
            parent = parent.Pin()->Parent;
        }
    }
}

TSharedRef<ITableRow> SSGDynamicTextAssetTypeTree::GenerateRow(TSharedPtr<FSGDynamicTextAssetTypeTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    const bool bIsInstantiable = Item.IsValid() && Item->bIsInstantiable;

    const FSlateFontInfo fontInfo = bIsInstantiable
        ? FAppStyle::GetFontStyle("NormalFont")
        : FCoreStyle::GetDefaultFontStyle("Italic", 9);

    const FSlateColor textColor = bIsInstantiable
        ? FSlateColor::UseForeground()
        : FSlateColor::UseSubduedForeground();

    const FText toolTipText = bIsInstantiable
        ? Item->Class->GetToolTipText(false)
        : FText::Format(INVTEXT("(This is a root dynamic text asset type that is abstract and cannot be instantiated directly)\n{0}"),
            Item->Class->GetToolTipText(false));

    return SNew(STableRow<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>, OwnerTable)
        .ToolTipText(toolTipText)
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f, 0.0f)
        .VAlign(VAlign_Center)
        [
            SNew(SImage)
            .Image(FAppStyle::GetBrush("ClassIcon.Object"))
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(2.0f, 0.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(Item->DisplayName)
            .Font(fontInfo)
            .ColorAndOpacity(textColor)
        ]
    ];
}

void SSGDynamicTextAssetTypeTree::GetTreeChildren(TSharedPtr<FSGDynamicTextAssetTypeTreeItem> Item, TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>& OutChildren)
{
    if (Item.IsValid())
    {
        OutChildren = Item->Children;
    }
}

void SSGDynamicTextAssetTypeTree::OnSelectionChanged(TSharedPtr<FSGDynamicTextAssetTypeTreeItem> Item, ESelectInfo::Type SelectInfo)
{
    if (OnTypeSelected.IsBound())
    {
        UClass* selectedClass = Item.IsValid() ? Item->Class.Get() : nullptr;
        OnTypeSelected.Execute(selectedClass);
    }
}

void SSGDynamicTextAssetTypeTree::BuildTreeHierarchy()
{
    USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
    if (!registry)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Registry not available for type tree"));
        return;
    }

    // Get all registered classes
    TArray<UClass*> allClasses;
    registry->GetAllRegisteredClasses(allClasses);

    // Build items for each class
    for (UClass* classObj : allClasses)
    {
        FindOrCreateItem(classObj, AllItems);
    }

    // Find root items (direct children of USGDynamicTextAsset and other providers)
    for (TPair<UClass*, TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>& kvp : AllItems)
    {
        TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& item = kvp.Value;
        if (!item->Parent.IsValid())
        {
            // This is a root item if its parent class is USGDynamicTextAsset or not in our map
            UClass* parentClass = kvp.Key->GetSuperClass();
            if (!AllItems.Contains(parentClass))
            {
                RootItems.Add(item);
            }
        }
    }

    // Sort root items alphabetically
    RootItems.Sort([]
        (const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& A, const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& B)
    {
        return A->DisplayName.ToString() < B->DisplayName.ToString();
    });
}

TSharedPtr<FSGDynamicTextAssetTypeTreeItem> SSGDynamicTextAssetTypeTree::FindOrCreateItem(UClass* InClass, TMap<UClass*, TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>& ItemMap)
{
    if (!InClass)
    {
        return nullptr;
    }
    // Check if already exists
    if (TSharedPtr<FSGDynamicTextAssetTypeTreeItem>* existing = ItemMap.Find(InClass))
    {
        return *existing;
    }
    // Create new item
    TSharedPtr<FSGDynamicTextAssetTypeTreeItem> newItem = MakeShared<FSGDynamicTextAssetTypeTreeItem>(InClass);
    ItemMap.Add(InClass, newItem);

    // Find/create parent
    UClass* parentClass = InClass->GetSuperClass();
    if (parentClass && parentClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        TSharedPtr<FSGDynamicTextAssetTypeTreeItem> parentItem = FindOrCreateItem(parentClass, ItemMap);
        if (parentItem.IsValid())
        {
            newItem->Parent = parentItem;
            parentItem->Children.Add(newItem);

            // Sort children alphabetically
            parentItem->Children.Sort([]
                (const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& A, const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& B)
            {
                return A->DisplayName.ToString() < B->DisplayName.ToString();
            });
        }
    }

    return newItem;
}

void SSGDynamicTextAssetTypeTree::OnTypeSearchTextChanged(const FText& NewText)
{
    TypeSearchText = NewText.ToString();
    ApplyTypeFilter();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();

        // Auto-expand all filtered items so matches are visible
        if (!TypeSearchText.IsEmpty())
        {
            TFunction<void(const TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>&)> expandAll =
                [this, &expandAll](const TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>& Items)
            {
                for (const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& item : Items)
                {
                    TreeView->SetItemExpansion(item, true);
                    expandAll(item->Children);
                }
            };
            expandAll(FilteredRootItems);
        }
    }
}

void SSGDynamicTextAssetTypeTree::ApplyTypeFilter()
{
    FilteredRootItems.Empty();

    if (TypeSearchText.IsEmpty())
    {
        // No filter active, show all
        FilteredRootItems = RootItems;
        return;
    }

    // Build filtered tree by cloning only matching branches
    for (const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& rootItem : RootItems)
    {
        TSharedPtr<FSGDynamicTextAssetTypeTreeItem> filteredItem = FilterTreeItem(rootItem, TypeSearchText);
        if (filteredItem.IsValid())
        {
            FilteredRootItems.Add(filteredItem);
        }
    }
}

bool SSGDynamicTextAssetTypeTree::DoesItemMatchFilter(const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& Item, const FString& Filter) const
{
    if (!Item.IsValid())
    {
        return false;
    }

    return Item->DisplayName.ToString().Contains(Filter, ESearchCase::IgnoreCase);
}

bool SSGDynamicTextAssetTypeTree::DoesItemOrDescendantMatchFilter(const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& Item, const FString& Filter) const
{
    if (!Item.IsValid())
    {
        return false;
    }

    if (DoesItemMatchFilter(Item, Filter))
    {
        return true;
    }

    for (const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& child : Item->Children)
    {
        if (DoesItemOrDescendantMatchFilter(child, Filter))
        {
            return true;
        }
    }

    return false;
}

TSharedPtr<FSGDynamicTextAssetTypeTreeItem> SSGDynamicTextAssetTypeTree::FilterTreeItem(const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& Source, const FString& Filter) const
{
    if (!Source.IsValid())
    {
        return nullptr;
    }

    // If this item matches, include it with all its children
    if (DoesItemMatchFilter(Source, Filter))
    {
        TSharedPtr<FSGDynamicTextAssetTypeTreeItem> copy = MakeShared<FSGDynamicTextAssetTypeTreeItem>(Source->Class.Get());
        copy->Children = Source->Children;
        return copy;
    }

    // Item doesn't match, but check if any descendant does
    TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>> filteredChildren;
    for (const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& child : Source->Children)
    {
        TSharedPtr<FSGDynamicTextAssetTypeTreeItem> filteredChild = FilterTreeItem(child, Filter);
        if (filteredChild.IsValid())
        {
            filteredChildren.Add(filteredChild);
        }
    }

    if (filteredChildren.IsEmpty())
    {
        return nullptr;
    }

    // Include this item as a parent of matching descendants
    TSharedPtr<FSGDynamicTextAssetTypeTreeItem> copy = MakeShared<FSGDynamicTextAssetTypeTreeItem>(Source->Class.Get());
    copy->Children = MoveTemp(filteredChildren);
    return copy;
}

int32 SSGDynamicTextAssetTypeTree::GetContentIndex() const
{
    return FilteredRootItems.Num() > 0 ? 1 : 0;
}
