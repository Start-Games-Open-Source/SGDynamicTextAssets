// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDTATypeTree.h"

#include "SGDTAEditorLogs.h"
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
                SAssignNew(TreeView, STreeView<TSharedPtr<FSGDTATypeTreeItem>>)
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
    // Capture the selection and expansion state before the rebuild; STreeView tracks
    // both by item pointer identity, so rebuilding the item arrays silently drops them
    // otherwise. The UClass keys survive the rebuild even though the items do not.
    UClass* previouslySelectedClass = GetSelectedClass();

    // First construction (no prior items) keeps the default root expansion below;
    // any later refresh restores exactly what the user had instead
    const bool bIsFirstBuild = RootItems.IsEmpty();

    TSet<UClass*> previouslyExpandedClasses;
    if (!bIsFirstBuild && TreeView.IsValid())
    {
        CaptureExpandedClasses(FilteredRootItems, previouslyExpandedClasses);
    }

    RootItems.Empty();
    AllItems.Empty();

    BuildTreeHierarchy();
    ApplyTypeFilter();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();

        if (bIsFirstBuild)
        {
            // Expand all root items by default on the first build
            for (const TSharedPtr<FSGDTATypeTreeItem>& item : FilteredRootItems)
            {
                TreeView->SetItemExpansion(item, true);
            }
        }
        else
        {
            // The captured set is authoritative on a refresh: expanded nodes stay
            // expanded and collapsed nodes (including roots the user collapsed) stay
            // collapsed, so the default root-expansion pass is intentionally skipped.
            // Newly built items default to collapsed, so only expansion needs applying.
            RestoreExpandedClasses(FilteredRootItems, previouslyExpandedClasses);
        }

        // Restore the pre-rebuild selection onto the newly built items. This runs after
        // the expansion restore so its ancestor-chain expansion is layered on top and
        // a restored child selection stays visible even if its parents were captured
        // collapsed or missed by the capture.
        RestoreSelectedClass(previouslySelectedClass);
    }

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Type tree refreshed with %d root items (%d filtered)"), RootItems.Num(), FilteredRootItems.Num());
}

UClass* SSGDynamicTextAssetTypeTree::GetSelectedClass() const
{
    if (TreeView.IsValid())
    {
        TArray<TSharedPtr<FSGDTATypeTreeItem>> selected = TreeView->GetSelectedItems();
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

    if (TSharedPtr<FSGDTATypeTreeItem>* found = AllItems.Find(ClassToSelect))
    {
        TreeView->SetSelection(*found);

        // Expand parents
        TWeakPtr<FSGDTATypeTreeItem> parent = (*found)->Parent;
        while (parent.IsValid())
        {
            TreeView->SetItemExpansion(parent.Pin(), true);
            parent = parent.Pin()->Parent;
        }
    }
}

void SSGDynamicTextAssetTypeTree::RestoreSelectedClass(UClass* ClassToRestore)
{
    if (!ClassToRestore)
    {
        return;
    }
    if (!TreeView.IsValid())
    {
        return;
    }

    // Search the visible (filtered) tree rather than AllItems: with a search filter
    // active the displayed items can be filtered clones that AllItems cannot see, and
    // a class that is filtered out or no longer registered must not be force-selected.
    // If the class is gone or hidden, keep the existing no-selection behavior.
    TArray<TSharedPtr<FSGDTATypeTreeItem>> ancestors;
    TSharedPtr<FSGDTATypeTreeItem> visibleItem = FindVisibleItemForClass(ClassToRestore, FilteredRootItems, ancestors);
    if (!visibleItem.IsValid())
    {
        return;
    }

    // Expand the visible ancestor chain so the restored selection is on screen
    for (const TSharedPtr<FSGDTATypeTreeItem>& ancestor : ancestors)
    {
        TreeView->SetItemExpansion(ancestor, true);
    }

    // Re-select without notifying: the selected type did not actually change across
    // the rebuild, and SetSelection always fires the selection-changed delegate, which
    // would trigger a redundant tile view rebuild in the browser
    bIsRestoringSelection = true;
    TreeView->SetSelection(visibleItem, ESelectInfo::Direct);
    bIsRestoringSelection = false;
}

TSharedPtr<FSGDTATypeTreeItem> SSGDynamicTextAssetTypeTree::FindVisibleItemForClass(UClass* InClass, const TArray<TSharedPtr<FSGDTATypeTreeItem>>& Items, TArray<TSharedPtr<FSGDTATypeTreeItem>>& OutAncestors) const
{
    for (const TSharedPtr<FSGDTATypeTreeItem>& item : Items)
    {
        if (!item.IsValid())
        {
            continue;
        }

        if (item->Class.Get() == InClass)
        {
            return item;
        }

        // Track the ancestor path while descending so the caller can expand it
        OutAncestors.Push(item);
        TSharedPtr<FSGDTATypeTreeItem> found = FindVisibleItemForClass(InClass, item->Children, OutAncestors);
        if (found.IsValid())
        {
            return found;
        }
        OutAncestors.Pop();
    }

    return nullptr;
}

void SSGDynamicTextAssetTypeTree::CaptureExpandedClasses(const TArray<TSharedPtr<FSGDTATypeTreeItem>>& Items, TSet<UClass*>& OutExpandedClasses) const
{
    for (const TSharedPtr<FSGDTATypeTreeItem>& item : Items)
    {
        if (!item.IsValid())
        {
            continue;
        }

        // Key by class, not item pointer: the rebuilt tree contains new item objects
        // (and filtered clones when a search is active) but the same classes
        if (UClass* itemClass = item->Class.Get())
        {
            if (TreeView->IsItemExpanded(item))
            {
                OutExpandedClasses.Add(itemClass);
            }
        }

        CaptureExpandedClasses(item->Children, OutExpandedClasses);
    }
}

void SSGDynamicTextAssetTypeTree::RestoreExpandedClasses(const TArray<TSharedPtr<FSGDTATypeTreeItem>>& Items, const TSet<UClass*>& ExpandedClasses)
{
    for (const TSharedPtr<FSGDTATypeTreeItem>& item : Items)
    {
        if (!item.IsValid())
        {
            continue;
        }

        // Captured classes no longer in the visible tree are skipped implicitly:
        // only items that actually exist after the rebuild are walked
        UClass* itemClass = item->Class.Get();
        if (itemClass && ExpandedClasses.Contains(itemClass))
        {
            TreeView->SetItemExpansion(item, true);
        }

        RestoreExpandedClasses(item->Children, ExpandedClasses);
    }
}

TSharedRef<ITableRow> SSGDynamicTextAssetTypeTree::GenerateRow(TSharedPtr<FSGDTATypeTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
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

    return SNew(STableRow<TSharedPtr<FSGDTATypeTreeItem>>, OwnerTable)
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

void SSGDynamicTextAssetTypeTree::GetTreeChildren(TSharedPtr<FSGDTATypeTreeItem> Item, TArray<TSharedPtr<FSGDTATypeTreeItem>>& OutChildren)
{
    if (Item.IsValid())
    {
        OutChildren = Item->Children;
    }
}

void SSGDynamicTextAssetTypeTree::OnSelectionChanged(TSharedPtr<FSGDTATypeTreeItem> Item, ESelectInfo::Type SelectInfo)
{
    // Skip the notify while RefreshTree restores the previous selection; the selected
    // type is unchanged and the browser refreshes its tile view for that flow already
    if (bIsRestoringSelection)
    {
        return;
    }

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
    for (TPair<UClass*, TSharedPtr<FSGDTATypeTreeItem>>& kvp : AllItems)
    {
        TSharedPtr<FSGDTATypeTreeItem>& item = kvp.Value;
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
        (const TSharedPtr<FSGDTATypeTreeItem>& A, const TSharedPtr<FSGDTATypeTreeItem>& B)
    {
        return A->DisplayName.ToString() < B->DisplayName.ToString();
    });
}

TSharedPtr<FSGDTATypeTreeItem> SSGDynamicTextAssetTypeTree::FindOrCreateItem(UClass* InClass, TMap<UClass*, TSharedPtr<FSGDTATypeTreeItem>>& ItemMap)
{
    if (!InClass)
    {
        return nullptr;
    }
    // Check if already exists
    if (TSharedPtr<FSGDTATypeTreeItem>* existing = ItemMap.Find(InClass))
    {
        return *existing;
    }
    // Create new item
    TSharedPtr<FSGDTATypeTreeItem> newItem = MakeShared<FSGDTATypeTreeItem>(InClass);
    ItemMap.Add(InClass, newItem);

    // Find/create parent
    UClass* parentClass = InClass->GetSuperClass();
    if (parentClass && parentClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        TSharedPtr<FSGDTATypeTreeItem> parentItem = FindOrCreateItem(parentClass, ItemMap);
        if (parentItem.IsValid())
        {
            newItem->Parent = parentItem;
            parentItem->Children.Add(newItem);

            // Sort children alphabetically
            parentItem->Children.Sort([]
                (const TSharedPtr<FSGDTATypeTreeItem>& A, const TSharedPtr<FSGDTATypeTreeItem>& B)
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
            TFunction<void(const TArray<TSharedPtr<FSGDTATypeTreeItem>>&)> expandAll =
                [this, &expandAll](const TArray<TSharedPtr<FSGDTATypeTreeItem>>& Items)
            {
                for (const TSharedPtr<FSGDTATypeTreeItem>& item : Items)
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
    for (const TSharedPtr<FSGDTATypeTreeItem>& rootItem : RootItems)
    {
        TSharedPtr<FSGDTATypeTreeItem> filteredItem = FilterTreeItem(rootItem, TypeSearchText);
        if (filteredItem.IsValid())
        {
            FilteredRootItems.Add(filteredItem);
        }
    }
}

bool SSGDynamicTextAssetTypeTree::DoesItemMatchFilter(const TSharedPtr<FSGDTATypeTreeItem>& Item, const FString& Filter) const
{
    if (!Item.IsValid())
    {
        return false;
    }

    return Item->DisplayName.ToString().Contains(Filter, ESearchCase::IgnoreCase);
}

bool SSGDynamicTextAssetTypeTree::DoesItemOrDescendantMatchFilter(const TSharedPtr<FSGDTATypeTreeItem>& Item, const FString& Filter) const
{
    if (!Item.IsValid())
    {
        return false;
    }

    if (DoesItemMatchFilter(Item, Filter))
    {
        return true;
    }

    for (const TSharedPtr<FSGDTATypeTreeItem>& child : Item->Children)
    {
        if (DoesItemOrDescendantMatchFilter(child, Filter))
        {
            return true;
        }
    }

    return false;
}

TSharedPtr<FSGDTATypeTreeItem> SSGDynamicTextAssetTypeTree::FilterTreeItem(const TSharedPtr<FSGDTATypeTreeItem>& Source, const FString& Filter) const
{
    if (!Source.IsValid())
    {
        return nullptr;
    }

    // If this item matches, include it with all its children
    if (DoesItemMatchFilter(Source, Filter))
    {
        TSharedPtr<FSGDTATypeTreeItem> copy = MakeShared<FSGDTATypeTreeItem>(Source->Class.Get());
        copy->Children = Source->Children;
        return copy;
    }

    // Item doesn't match, but check if any descendant does
    TArray<TSharedPtr<FSGDTATypeTreeItem>> filteredChildren;
    for (const TSharedPtr<FSGDTATypeTreeItem>& child : Source->Children)
    {
        TSharedPtr<FSGDTATypeTreeItem> filteredChild = FilterTreeItem(child, Filter);
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
    TSharedPtr<FSGDTATypeTreeItem> copy = MakeShared<FSGDTATypeTreeItem>(Source->Class.Get());
    copy->Children = MoveTemp(filteredChildren);
    return copy;
}

int32 SSGDynamicTextAssetTypeTree::GetContentIndex() const
{
    return FilteredRootItems.Num() > 0 ? 1 : 0;
}
