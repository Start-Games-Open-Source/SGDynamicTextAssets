// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SListView.h"

class SBox;
class SImage;
class USGDynamicTextAssetReferenceSubsystem;

struct FSGDynamicTextAssetReferenceEntry;
struct FSGDynamicTextAssetDependencyEntry;

enum class ESGDTAReferenceType : uint8;

/**
 * Reference viewer window for displaying what references a dynamic text asset and what it depends on.
 * 
 * Layout:
 * +------------------------------------------+
 * | [Title: Viewing: UserFacingId (ID)]    |
 * +---------------------+--------------------+
 * | Referencers (N):    | Dependencies (M):  |
 * | +---------------+   | +--------------+   |
 * | | Asset List    |   | | DataObj List |   |
 * | |   └─Property  |   | |   └─Property |   |
 * | +---------------+   | +--------------+   |
 * +---------------------+--------------------+
 * 
 * Features:
 * - Shows all assets that reference the viewed dynamic text asset
 * - Shows all dynamic text assets that the viewed object depends on
 * - Double-click to navigate to referencers/dependencies
 * - Refresh button to rescan references
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetReferenceViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSGDynamicTextAssetReferenceViewer)
		: _DynamicTextAssetId()
		, _UserFacingId()
	{ }
		/** The ID of the dynamic text asset to view references for */
		SLATE_ARGUMENT(FSGDynamicTextAssetId, DynamicTextAssetId)
		
		/** The user-facing ID for display */
		SLATE_ARGUMENT(FString, UserFacingId)
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Returns the unique identifier for this viewer tab */
	static FName GetTabId() { return FName("SGDynamicTextAssetReferenceViewer"); }

	/** Returns the display name for this viewer */
	static FText GetTabDisplayName() { return INVTEXT("Reference Viewer"); }

	/** Returns the tooltip for this viewer tab */
	static FText GetTabTooltip() { return INVTEXT("View dynamic text asset references and dependencies"); }

	/** Sets the dynamic text asset to view */
	void SetDynamicTextAsset(const FSGDynamicTextAssetId& InId, const FString& InUserFacingId);

	/** Refreshes the reference data */
	void RefreshReferences();

	/**
	 * Opens a Reference Viewer window for the specified dynamic text asset.
	 * If a viewer is already open, it will be updated to show the new dynamic text asset.
	 *
	 * @param Id The ID of the dynamic text asset to view
	 * @param UserFacingId The user-facing ID for display
	 */
	static void OpenViewer(const FSGDynamicTextAssetId& Id, const FString& UserFacingId);

	static const FSlateBrush* GetReferencerAssetTypeIcon(const ESGDTAReferenceType& Type);

	static FText GetReferencerAssetTypeTooltip(const ESGDTAReferenceType& Type);

	static FLinearColor GetReferencerAssetTypeColor(const ESGDTAReferenceType& Type);

private:

	/** Creates the header/title bar widget */
	TSharedRef<SWidget> CreateHeaderBar();

	/** Creates the left panel (referencers list) */
	TSharedRef<SWidget> CreateReferencersPanel();

	/** Creates the right panel (dependencies list) */
	TSharedRef<SWidget> CreateDependenciesPanel();

	/** Creates the loading overlay shown while scanning */
	TSharedRef<SWidget> CreateLoadingOverlay();

	/** Returns the visibility of the loading overlay */
	EVisibility GetLoadingOverlayVisibility() const;

	/** Returns the widget index for the content switcher (0=loading, 1=content) */
	int32 GetContentWidgetIndex() const;

	/** Called when the reference scan completes */
	void OnReferenceScanComplete();

	/** Called when scan progress updates */
	void OnReferenceScanProgress(int32 Current, int32 Total, const FText& StatusText);

	/** Called when refresh button is clicked */
	FReply OnRefreshClicked();

	/** Called when a referencer item is double-clicked */
	void OnReferencerDoubleClicked(TSharedPtr<FSGDynamicTextAssetReferenceEntry> Item);

	/** Called when a dependency item is double-clicked */
	void OnDependencyDoubleClicked(TSharedPtr<FSGDynamicTextAssetDependencyEntry> Item);

	/** Generates a row for the referencers list */
	TSharedRef<ITableRow> GenerateReferencerRow(TSharedPtr<FSGDynamicTextAssetReferenceEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Generates a row for the dependencies list */
	TSharedRef<ITableRow> GenerateDependencyRow(TSharedPtr<FSGDynamicTextAssetDependencyEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets the title text for display */
	FText GetTitleText() const;

	/** Gets the referencers count text */
	FText GetReferencersCountText() const;

	/** Gets the dependencies count text */
	FText GetDependenciesCountText() const;

	/** Opens a dynamic text asset in the editor by ID */
	void OpenDynamicTextAssetEditor(const FSGDynamicTextAssetId& Id);

	/** Opens a level in the editor using its package path and selects the referencing actor */
	void OpenLevel(const FSoftObjectPath& LevelPath, const FString& SourceDisplayName);

	/** Opens an asset in the appropriate editor using the asset registry */
	void OpenAsset(const FSoftObjectPath& AssetPath);

	/** Returns the current scan progress text */
	FText GetScanProgressText() const;

	/** Gets the reference subsystem */
	USGDynamicTextAssetReferenceSubsystem* GetReferenceSubsystem();

	/** Called when the dependencies panel header is clicked */
	FReply OnDependenciesHeaderClicked();

	/** Returns the appropriate tree arrow brush for expanded/collapsed state */
	const FSlateBrush* GetExpanderBrush(bool bExpanded) const;

	/** Creates the collapsed sidebar tab shown when the dependencies panel is collapsed */
	TSharedRef<SWidget> CreateDependenciesCollapsedTab();

	/** Called when the referencers search text changes */
	void OnReferencersSearchTextChanged(const FText& NewText);

	/** Called when the dependencies search text changes */
	void OnDependenciesSearchTextChanged(const FText& NewText);

	/** Rebuilds FilteredReferencerItems from AllReferencerItems based on current search text */
	void ApplyReferencersFilter();

	/** Rebuilds FilteredDependencyItems from AllDependencyItems based on current search text */
	void ApplyDependenciesFilter();

	/** The ID of the dynamic text asset being viewed */
	FSGDynamicTextAssetId ViewedId;

	/** The user-facing ID for display */
	FString ViewedUserFacingId;

	/** Title text block */
	TSharedPtr<STextBlock> TitleTextBlock = nullptr;

	/** Referencers count text block */
	TSharedPtr<STextBlock> ReferencersCountText = nullptr;

	/** Dependencies count text block */
	TSharedPtr<STextBlock> DependenciesCountText = nullptr;

	/** All referencer items - unfiltered source of truth */
	TArray<TSharedPtr<FSGDynamicTextAssetReferenceEntry>> AllReferencerItems;

	/** Filtered referencer items displayed by the list view */
	TArray<TSharedPtr<FSGDynamicTextAssetReferenceEntry>> FilteredReferencerItems;

	/** All dependency items - unfiltered source of truth */
	TArray<TSharedPtr<FSGDynamicTextAssetDependencyEntry>> AllDependencyItems;

	/** Filtered dependency items displayed by the list view */
	TArray<TSharedPtr<FSGDynamicTextAssetDependencyEntry>> FilteredDependencyItems;

	/** Search box for filtering referencers */
	TSharedPtr<SSearchBox> ReferencersSearchBox;

	/** Search box for filtering dependencies */
	TSharedPtr<SSearchBox> DependenciesSearchBox;

	/** Current search text for referencers panel */
	FString ReferencersSearchText;

	/** Current search text for dependencies panel */
	FString DependenciesSearchText;

	/** The referencers list view */
	TSharedPtr<SListView<TSharedPtr<FSGDynamicTextAssetReferenceEntry>>> ReferencersListView = nullptr;

	/** The dependencies list view */
	TSharedPtr<SListView<TSharedPtr<FSGDynamicTextAssetDependencyEntry>>> DependenciesListView = nullptr;

	/** Cached pointer to the reference subsystem */
	TWeakObjectPtr<USGDynamicTextAssetReferenceSubsystem> CachedReferenceSubsystem = nullptr;

	/** Dependencies expanded panel wrapper for event-based visibility control */
	TSharedPtr<SBox> DependenciesExpandedBox = nullptr;

	/** Dependencies collapsed sidebar wrapper for event-based visibility control */
	TSharedPtr<SBox> DependenciesCollapsedBox = nullptr;

	/** Dependencies expander arrow image for event-based image updates */
	TSharedPtr<SImage> DependenciesExpanderArrow = nullptr;

	/** Dependencies count text in the collapsed sidebar tab */
	TSharedPtr<STextBlock> DependenciesCollapsedCountText = nullptr;

	/** Whether the dependencies panel is expanded */
	uint8 bDependenciesExpanded : 1 = 0;

	/** Current scan progress (0.0 to 1.0) */
	float ScanProgress = 0.0f;

	/** Current scan status text */
	FText ScanStatusText;

	/** Text block for progress status in loading overlay */
	TSharedPtr<STextBlock> ProgressStatusText = nullptr;
};
