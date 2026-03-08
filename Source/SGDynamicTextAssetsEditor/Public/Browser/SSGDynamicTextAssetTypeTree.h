// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#include "Management/SGDynamicTextAssetRegistry.h"

/** Tree item representing a dynamic text asset class */
struct FSGDynamicTextAssetTypeTreeItem
{
    FSGDynamicTextAssetTypeTreeItem() = default;

    explicit FSGDynamicTextAssetTypeTreeItem(UClass* InClass)
        : Class(InClass)
    {
        if (InClass)
        {
            DisplayName = FText::FromString(InClass->GetName());

            if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                bIsInstantiable = registry->IsInstantiableClass(InClass);
            }
        }
    }

    /** The class this item represents */
    TWeakObjectPtr<UClass> Class = nullptr;

    /** Display name for the class */
    FText DisplayName;

    /** Child items (subclasses) */
    TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>> Children;

    /** Parent item */
    TWeakPtr<FSGDynamicTextAssetTypeTreeItem> Parent = nullptr;

    /** Whether this class can be instantiated (non-abstract) */
    uint8 bIsInstantiable : 1 = 0;
};

DECLARE_DELEGATE_OneParam(FOnDynamicTextAssetTypeSelected, UClass* /*SelectedClass*/);

/**
 * Tree view widget displaying the hierarchy of registered dynamic text asset classes.
 * Used as the left panel in the Dynamic Text Asset Browser.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetTypeTree : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetTypeTree)
    { }
        /** Called when a type is selected */
        SLATE_EVENT(FOnDynamicTextAssetTypeSelected, OnTypeSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Refreshes the tree from the registry */
    void RefreshTree();

    /** Gets the currently selected class */
    UClass* GetSelectedClass() const;

    /** Selects a class in the tree */
    void SelectClass(UClass* ClassToSelect);

private:

    /** Generates a row for the tree view */
    TSharedRef<ITableRow> GenerateRow(TSharedPtr<FSGDynamicTextAssetTypeTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

    /** Gets children for a tree item */
    void GetTreeChildren(TSharedPtr<FSGDynamicTextAssetTypeTreeItem> Item, TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>& OutChildren);

    /** Called when selection changes */
    void OnSelectionChanged(TSharedPtr<FSGDynamicTextAssetTypeTreeItem> Item, ESelectInfo::Type SelectInfo);

    /** Builds the tree hierarchy from the registry */
    void BuildTreeHierarchy();

    /** Recursively finds or creates a tree item for a class */
    TSharedPtr<FSGDynamicTextAssetTypeTreeItem> FindOrCreateItem(UClass* InClass, TMap<UClass*, TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>& ItemMap);

    /** Called when the type search text changes */
    void OnTypeSearchTextChanged(const FText& NewText);

    /** Rebuilds FilteredRootItems from RootItems based on the current search text */
    void ApplyTypeFilter();

    /** Returns true if the item's DisplayName contains the filter substring (case-insensitive) */
    bool DoesItemMatchFilter(const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& Item, const FString& Filter) const;

    /** Returns true if the item or any descendant matches the filter */
    bool DoesItemOrDescendantMatchFilter(const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& Item, const FString& Filter) const;

    /**
     * Creates a filtered copy of a tree item, keeping only branches that contain matches.
     * If the item itself matches, all its children are included.
     * If a descendant matches, only the matching branch is included.
     *
     * @return Filtered copy, or nullptr if nothing in this branch matches.
     */
    TSharedPtr<FSGDynamicTextAssetTypeTreeItem> FilterTreeItem(const TSharedPtr<FSGDynamicTextAssetTypeTreeItem>& Source, const FString& Filter) const;

    /** Returns the index to show in the switcher (0=empty, 1=tree) */
    int32 GetContentIndex() const;

    /** The tree view widget */
    TSharedPtr<STreeView<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>>> TreeView = nullptr;

    /** The type search box widget */
    TSharedPtr<SSearchBox> TypeSearchBox = nullptr;

    /** Current type search filter text */
    FString TypeSearchText;

    /** Root items (direct children of USGDynamicTextAsset) - unfiltered source of truth */
    TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>> RootItems;

    /** Filtered root items displayed by the tree view */
    TArray<TSharedPtr<FSGDynamicTextAssetTypeTreeItem>> FilteredRootItems;

    /** All items by class */
    TMap<UClass*, TSharedPtr<FSGDynamicTextAssetTypeTreeItem>> AllItems;

    /** Callback when type is selected */
    FOnDynamicTextAssetTypeSelected OnTypeSelected;

    /** Widget switcher for empty/populated states */
    TSharedPtr<SWidgetSwitcher> ContentSwitcher = nullptr;
};
