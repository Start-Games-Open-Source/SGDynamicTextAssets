// Copyright Start Games, Inc. All Rights Reserved.

#include "ReferenceViewer/SSGDynamicTextAssetReferenceViewer.h"

#include "ReferenceViewer/SGDynamicTextAssetReferenceSubsystem.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/FSGDynamicTextAssetEditorToolkit.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "Editor/SSGDynamicTextAssetIcon.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Framework/Docking/TabManager.h"
#include "Statics/SGDynamicTextAssetStatics.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Notifications/NotificationManager.h"

#include "Utilities/SGDynamicTextAssetEditorConstants.h"

#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SSGDynamicTextAssetReferenceViewer"

// Static storage for the open viewer tab
static TWeakPtr<SDockTab> GLOBAL_REFERENCE_VIEWER_TAB;
static TWeakPtr<SSGDynamicTextAssetReferenceViewer> GLOBAL_OPEN_REFERENCE_VIEWER;

void SSGDynamicTextAssetReferenceViewer::Construct(const FArguments& InArgs)
{
	ViewedId = InArgs._DynamicTextAssetId;
	ViewedUserFacingId = InArgs._UserFacingId;

	// Subscribe to scan delegates
	if (USGDynamicTextAssetReferenceSubsystem* refSubsystem = GetReferenceSubsystem())
	{
		refSubsystem->OnReferenceScanComplete.AddSP(this, &SSGDynamicTextAssetReferenceViewer::OnReferenceScanComplete);
		refSubsystem->OnReferenceScanProgress.AddSP(this, &SSGDynamicTextAssetReferenceViewer::OnReferenceScanProgress);
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		// Header bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreateHeaderBar()
		]

		// Main content area - switches between loading and content
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &SSGDynamicTextAssetReferenceViewer::GetContentWidgetIndex)

			// Index 0: Loading overlay
			+ SWidgetSwitcher::Slot()
			[
				CreateLoadingOverlay()
			]

			// Index 1: Main content
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHorizontalBox)

				// Left - Referencers panel (always expanded)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					CreateReferencersPanel()
				]

				// Right - Dependencies collapsed sidebar tab
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreateDependenciesCollapsedTab()
				]

				// Right - Dependencies expanded panel
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(DependenciesExpandedBox, SBox)
					.Visibility(EVisibility::Collapsed)
					[
						CreateDependenciesPanel()
					]
				]
			]
		]
	];

	// Initial refresh if we have a valid ID and cache is ready
	if (ViewedId.IsValid())
	{
		RefreshReferences();
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Reference Viewer constructed for: %s"), *ViewedUserFacingId);
}

TSharedRef<SWidget> SSGDynamicTextAssetReferenceViewer::CreateHeaderBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f))
		[
			SNew(SHorizontalBox)

			// Title text
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(TitleTextBlock, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				.Text(GetTitleText())
			]

			// Clear Cache and Rescan button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(10.0f, 5.0f))
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Clear cached reference data and perform a full rescan of all assets"))
				.OnClicked(this, &SSGDynamicTextAssetReferenceViewer::OnRefreshClicked)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Refresh"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}

FText SSGDynamicTextAssetReferenceViewer::GetTitleText() const
{
	if (ViewedUserFacingId.IsEmpty())
	{
		return INVTEXT("No Dynamic Text Asset Selected");
	}
	
	return FText::Format(INVTEXT("Viewing: {0}"), FText::FromString(ViewedUserFacingId));
}

TSharedRef<SWidget> SSGDynamicTextAssetReferenceViewer::CreateReferencersPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(4.0f))
		[
			SNew(SVerticalBox)

			// Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SNew(SHorizontalBox)

				// Help icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Help"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.ToolTipText(INVTEXT("Lists all Blueprints, Levels, and other Dynamic Text Assets that contain a reference to this Dynamic Text Asset. These are assets that depend on this Dynamic Text Asset."))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(INVTEXT("References to this Dynamic Text Asset"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(ReferencersCountText, STextBlock)
					.Text(GetReferencersCountText())
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			// Search bar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 0.0f, 4.0f, 4.0f)
			[
				SAssignNew(ReferencersSearchBox, SSearchBox)
				.HintText(INVTEXT("Search referencers..."))
				.OnTextChanged(this, &SSGDynamicTextAssetReferenceViewer::OnReferencersSearchTextChanged)
			]

			// List view
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(ReferencersListView, SListView<TSharedPtr<FSGDynamicTextAssetReferenceEntry>>)
				.ListItemsSource(&FilteredReferencerItems)
				.OnGenerateRow(this, &SSGDynamicTextAssetReferenceViewer::GenerateReferencerRow)
				.OnMouseButtonDoubleClick(this, &SSGDynamicTextAssetReferenceViewer::OnReferencerDoubleClicked)
				.SelectionMode(ESelectionMode::Single)
			]
		];
}

FText SSGDynamicTextAssetReferenceViewer::GetReferencersCountText() const
{
	if (ReferencersSearchText.IsEmpty())
	{
		return FText::Format(INVTEXT("({0})"), FText::AsNumber(AllReferencerItems.Num()));
	}
	return FText::Format(INVTEXT("({0}/{1})"),
		FText::AsNumber(FilteredReferencerItems.Num()),
		FText::AsNumber(AllReferencerItems.Num()));
}

TSharedRef<SWidget> SSGDynamicTextAssetReferenceViewer::CreateDependenciesPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(4.0f))
		[
			SNew(SVerticalBox)

			// Clickable header
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.OnClicked(this, &SSGDynamicTextAssetReferenceViewer::OnDependenciesHeaderClicked)
				[
					SNew(SHorizontalBox)

					// Expander arrow
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SAssignNew(DependenciesExpanderArrow, SImage)
						.Image(GetExpanderBrush(bDependenciesExpanded))
					]

					// Help icon
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Help"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.ToolTipText(INVTEXT("Lists all other Dynamic Text Assets that this Dynamic Text Asset references. These are the dependencies that this Dynamic Text Asset requires to function properly."))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(INVTEXT("Referenced by this Dynamic Text Asset"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SAssignNew(DependenciesCountText, STextBlock)
						.Text(GetDependenciesCountText())
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]

			// Search bar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 0.0f, 4.0f, 4.0f)
			[
				SAssignNew(DependenciesSearchBox, SSearchBox)
				.HintText(INVTEXT("Search dependencies..."))
				.OnTextChanged(this, &SSGDynamicTextAssetReferenceViewer::OnDependenciesSearchTextChanged)
			]

			// List view (collapsible)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(DependenciesListView, SListView<TSharedPtr<FSGDynamicTextAssetDependencyEntry>>)
				.ListItemsSource(&FilteredDependencyItems)
				.OnGenerateRow(this, &SSGDynamicTextAssetReferenceViewer::GenerateDependencyRow)
				.OnMouseButtonDoubleClick(this, &SSGDynamicTextAssetReferenceViewer::OnDependencyDoubleClicked)
				.SelectionMode(ESelectionMode::Single)
			]
		];
}

FText SSGDynamicTextAssetReferenceViewer::GetDependenciesCountText() const
{
	if (DependenciesSearchText.IsEmpty())
	{
		return FText::Format(INVTEXT("({0})"), FText::AsNumber(AllDependencyItems.Num()));
	}
	return FText::Format(INVTEXT("({0}/{1})"),
		FText::AsNumber(FilteredDependencyItems.Num()),
		FText::AsNumber(AllDependencyItems.Num()));
}

TSharedRef<SWidget> SSGDynamicTextAssetReferenceViewer::CreateLoadingOverlay()
{
	ScanStatusText = INVTEXT("Preparing to scan...");

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(16.0f))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				// Throbber
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[
					SNew(SThrobber)
				]

				// Status text (dynamic)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SAssignNew(ProgressStatusText, STextBlock)
					.Text(this, &SSGDynamicTextAssetReferenceViewer::GetScanProgressText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				]

				// Progress percentage
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						int32 percentage = FMath::RoundToInt(ScanProgress * 100.0f);
						return FText::Format(INVTEXT("{0}% complete"), FText::AsNumber(percentage));
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				// Description text
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(INVTEXT("The editor remains responsive during scanning."))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
}

FText SSGDynamicTextAssetReferenceViewer::GetScanProgressText() const
{
	return ScanStatusText;
}

int32 SSGDynamicTextAssetReferenceViewer::GetContentWidgetIndex() const
{
	USGDynamicTextAssetReferenceSubsystem* refSubsystem = const_cast<SSGDynamicTextAssetReferenceViewer*>(this)->GetReferenceSubsystem();
	if (refSubsystem && refSubsystem->IsScanningInProgress())
	{
		return 0; // Show loading overlay
	}
	return 1; // Show content
}

EVisibility SSGDynamicTextAssetReferenceViewer::GetLoadingOverlayVisibility() const
{
	USGDynamicTextAssetReferenceSubsystem* refSubsystem = const_cast<SSGDynamicTextAssetReferenceViewer*>(this)->GetReferenceSubsystem();
	if (refSubsystem && refSubsystem->IsScanningInProgress())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

void SSGDynamicTextAssetReferenceViewer::OnReferenceScanComplete()
{
	// Refresh references now that scan is complete
	if (ViewedId.IsValid())
	{
		RefreshReferences();
	}

	ScanProgress = 1.0f;
	ScanStatusText = INVTEXT("Scan complete!");

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Reference Viewer received scan complete notification"));
}

void SSGDynamicTextAssetReferenceViewer::OnReferenceScanProgress(int32 Current, int32 Total, const FText& StatusText)
{
	// Update progress for UI
	ScanProgress = (Total > 0) ? static_cast<float>(Current) / static_cast<float>(Total) : 0.0f;
	ScanStatusText = StatusText;
}

TSharedRef<ITableRow> SSGDynamicTextAssetReferenceViewer::GenerateReferencerRow(
	TSharedPtr<FSGDynamicTextAssetReferenceEntry> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FString displayText = Item->SourceDisplayName;
	if (displayText.IsEmpty())
	{
		displayText = Item->SourceAsset.GetAssetName();
	}

	TWeakPtr<ISGDynamicTextAssetSerializer> weakSerializer = nullptr;
	FText typeText;
	TOptional<FSlateColor> overrideColor;
	TOptional<const FSlateBrush*> overrideIcon;
	switch (Item->ReferenceType)
	{
	case ESGReferenceType::Blueprint:
		{
			overrideIcon = FAppStyle::GetBrush("Persona.AssetClass.Blueprint");
			typeText = INVTEXT("Blueprint");
			// Grabbing this color from AssetTypeActions_Blueprint & AssetDefinition_Blueprint
			overrideColor = FSGDynamicTextAssetEditorConstants::BLUEPRINT_ASSET_COLOR;
			break;
		}
	case ESGReferenceType::DynamicTextAsset:
		{
			weakSerializer = USGDynamicTextAssetStatics::FindSerializerForDynamicTextAssetId(Item->ReferencedId);
			typeText = INVTEXT("Dynamic Text Asset");
			break;
		}
	case ESGReferenceType::Level:
		{
			typeText = INVTEXT("Level");
			break;
		}
	// ESGReferenceType::Other
	default:
		{
			typeText = INVTEXT("Asset");
			break;
		}
	}

	// Path shown in tooltip: file path for DynamicTextAssets, asset path for everything else
	FString pathText;
	if (Item->ReferenceType == ESGReferenceType::DynamicTextAsset && !Item->SourceFilePath.IsEmpty())
	{
		FString relativeSourceFilePath = Item->SourceFilePath;
		FPaths::MakePathRelativeTo(relativeSourceFilePath, *FPaths::ProjectContentDir());
		pathText = "/Game/" + relativeSourceFilePath;
	}
	else
	{
		pathText = Item->SourceAsset.ToString();
	}

	const FText tooltipText = FText::Format(
		INVTEXT("Name: {0}\nPath: {1}\nType: {2}"),
		FText::FromString(displayText),
		FText::FromString(pathText),
		typeText
	);

	return SNew(STableRow<TSharedPtr<FSGDynamicTextAssetReferenceEntry>>, OwnerTable)
		.Padding(FMargin(4.0f, 2.0f))
		.ToolTipText(tooltipText)
		[
			SNew(SHorizontalBox)

			// Type
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)
				.ToolTipText(GetReferencerAssetTypeTooltip(Item->ReferenceType))

				+ SVerticalBox::Slot()
				[
					SNew(SSGDynamicTextAssetIcon)
					.ReferenceType(Item->ReferenceType)
					.Serializer(weakSerializer)
					.Label(typeText)
					.IconBrushOverride(overrideIcon)
					.IconColorOverride(overrideColor)
				]
			]

			// Asset
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				// Main line: Asset name
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
						.Text(FText::FromString(displayText))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				]

				// Property path
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(16.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::Format(INVTEXT("└─ {0}"), FText::FromString(Item->PropertyPath)))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
}

TSharedRef<ITableRow> SSGDynamicTextAssetReferenceViewer::GenerateDependencyRow(
	TSharedPtr<FSGDynamicTextAssetDependencyEntry> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FString topText = Item->PropertyPath;
	FString middleText;
	FString bottomText;

	if (Item->bIsAssetReference)
	{
		middleText = Item->AssetPath.GetAssetName();
		bottomText = FString::Printf(TEXT("└─ %s"), *Item->AssetPath.ToString());
	}
	else
	{
		middleText = Item->UserFacingId;
		bottomText = FString::Printf(TEXT("└─ %s"), *Item->DependencyId.ToString());
	}

	TWeakPtr<ISGDynamicTextAssetSerializer> weakSerializer = nullptr;
	if (!Item->bIsAssetReference)
	{
		weakSerializer = USGDynamicTextAssetStatics::FindSerializerForDynamicTextAssetId(Item->DependencyId);
	}

	const ESGReferenceType iconReferenceType = Item->bIsAssetReference ? Item->ReferenceType : ESGReferenceType::DynamicTextAsset;

	return SNew(STableRow<TSharedPtr<FSGDynamicTextAssetDependencyEntry>>, OwnerTable)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			// Type icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SSGDynamicTextAssetIcon)
				.ReferenceType(iconReferenceType)
				.Serializer(weakSerializer)
			]

			// Asset details
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				// Main line
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(topText))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				]

				// Middle Details
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(middleText))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				// Bottom Details
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(16.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(bottomText))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
}

void SSGDynamicTextAssetReferenceViewer::SetDynamicTextAsset(const FSGDynamicTextAssetId& InId, const FString& InUserFacingId)
{
	ViewedId = InId;
	ViewedUserFacingId = InUserFacingId;

	// Update title text (event-based, not polled)
	if (TitleTextBlock.IsValid())
	{
		TitleTextBlock->SetText(GetTitleText());
	}

	RefreshReferences();
}

void SSGDynamicTextAssetReferenceViewer::RefreshReferences()
{
	// Clear existing data
	AllReferencerItems.Empty();
	FilteredReferencerItems.Empty();
	AllDependencyItems.Empty();
	FilteredDependencyItems.Empty();

	// Clear search text on refresh
	ReferencersSearchText.Empty();
	DependenciesSearchText.Empty();
	if (ReferencersSearchBox.IsValid())
	{
		ReferencersSearchBox->SetText(FText::GetEmpty());
	}
	if (DependenciesSearchBox.IsValid())
	{
		DependenciesSearchBox->SetText(FText::GetEmpty());
	}

	if (!ViewedId.IsValid())
	{
		if (ReferencersListView.IsValid())
		{
			ReferencersListView->RequestListRefresh();
		}
		if (DependenciesListView.IsValid())
		{
			DependenciesListView->RequestListRefresh();
		}
		return;
	}

	USGDynamicTextAssetReferenceSubsystem* refSubsystem = GetReferenceSubsystem();
	if (!refSubsystem)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Reference subsystem not available"));
		return;
	}

	// Get referencers
	TArray<FSGDynamicTextAssetReferenceEntry> referencers;
	refSubsystem->FindReferencers(ViewedId, referencers, false);

	for (const FSGDynamicTextAssetReferenceEntry& entry : referencers)
	{
		AllReferencerItems.Add(MakeShared<FSGDynamicTextAssetReferenceEntry>(entry));
	}

	// Get dependencies
	TArray<FSGDynamicTextAssetDependencyEntry> dependencies;
	refSubsystem->FindDependencies(ViewedId, dependencies);

	for (const FSGDynamicTextAssetDependencyEntry& entry : dependencies)
	{
		AllDependencyItems.Add(MakeShared<FSGDynamicTextAssetDependencyEntry>(entry));
	}

	// Apply filters (with empty search text, these just copy All* to Filtered*)
	ApplyReferencersFilter();
	ApplyDependenciesFilter();

	// Refresh the list views
	if (ReferencersListView.IsValid())
	{
		ReferencersListView->RequestListRefresh();
	}
	if (DependenciesListView.IsValid())
	{
		DependenciesListView->RequestListRefresh();
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Refreshed references for %s: %d referencers, %d dependencies"),
		*ViewedUserFacingId, AllReferencerItems.Num(), AllDependencyItems.Num());
}

FReply SSGDynamicTextAssetReferenceViewer::OnRefreshClicked()
{
	// Clear cache and force a full rescan
	USGDynamicTextAssetReferenceSubsystem* refSubsystem = GetReferenceSubsystem();
	if (refSubsystem)
	{
		refSubsystem->ClearCacheAndRescan();
	}

	return FReply::Handled();
}

void SSGDynamicTextAssetReferenceViewer::OnReferencerDoubleClicked(TSharedPtr<FSGDynamicTextAssetReferenceEntry> Item)
{
	if (!Item.IsValid())
	{
		return;
	}

	switch (Item->ReferenceType)
	{
		case ESGReferenceType::DynamicTextAsset:
		{
			if (!Item->SourceFilePath.IsEmpty())
			{
				FSGDynamicTextAssetEditorToolkit::OpenEditorForFile(Item->SourceFilePath);
			}
			break;
		}
		case ESGReferenceType::Level:
		{
			OpenLevel(Item->SourceAsset, Item->SourceDisplayName);
			break;
		}
		// ESGReferenceType::Blueprint
		// ESGReferenceType::Other
		default:
		{
			OpenAsset(Item->SourceAsset);
			break;
		}
	}
}

void SSGDynamicTextAssetReferenceViewer::OnDependencyDoubleClicked(TSharedPtr<FSGDynamicTextAssetDependencyEntry> Item)
{
	if (!Item.IsValid())
	{
		return;
	}

	if (Item->bIsAssetReference)
	{
		OpenAsset(Item->AssetPath);
	}
	else
	{
		OpenDynamicTextAssetEditor(Item->DependencyId);
	}
}

void SSGDynamicTextAssetReferenceViewer::OpenDynamicTextAssetEditor(const FSGDynamicTextAssetId& Id)
{
	if (!Id.IsValid())
	{
		return;
	}

	// Find the file path for this ID
	FString filePath;
	if (!FSGDynamicTextAssetFileManager::FindFileForId(Id, filePath))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Could not find file for ID: %s"), *Id.ToString());
		return;
	}

	// Open in our custom editor
	FSGDynamicTextAssetEditorToolkit::OpenEditorForFile(filePath);
}

void SSGDynamicTextAssetReferenceViewer::OpenLevel(const FSoftObjectPath& LevelPath, const FString& SourceDisplayName)
{
	if (!LevelPath.IsValid())
	{
		return;
	}

	const FString packagePath = LevelPath.GetLongPackageName();
	FString filePath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(packagePath, filePath, FPackageName::GetMapPackageExtension()))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Could not resolve level path: %s"), *LevelPath.ToString());
		return;
	}

	FEditorFileUtils::LoadMap(filePath);

	// Parse actor label from SourceDisplayName ("LevelName > ActorLabel" or "LevelName > ActorLabel > ComponentName")
	TArray<FString> parts;
	SourceDisplayName.ParseIntoArray(parts, TEXT(" > "), true);

	if (parts.Num() < 2 || !GWorld || !GWorld->PersistentLevel)
	{
		return;
	}

	const FString actorLabel = parts[1];

	// Search for the actor by label in the loaded level
	AActor* foundActor = nullptr;
	for (AActor* actor : GWorld->PersistentLevel->Actors)
	{
		if (IsValid(actor) && actor->GetActorNameOrLabel() == actorLabel)
		{
			foundActor = actor;
			break;
		}
	}

	if (foundActor)
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(foundActor, true, true);
	}
	else
	{
		FNotificationInfo info(FText::Format(
			INVTEXT("Could not find actor '{0}' in level"),
			FText::FromString(actorLabel)
		));
		info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(info);
	}
}

void SSGDynamicTextAssetReferenceViewer::OpenAsset(const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return;
	}

	IAssetRegistry& assetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// TSoftClassPtr<> stores its path as the generated class path (e.g. /Game/.../BP_Foo.BP_Foo_C).
	// The asset registry indexes Blueprint assets at the asset path without the _C suffix.
	// Strip the suffix so GetAssetByObjectPath can locate the Blueprint asset.
	FSoftObjectPath resolvedPath = AssetPath;
	const FString pathStr = AssetPath.GetAssetPathString();
	if (pathStr.EndsWith(TEXT("_C")))
	{
		resolvedPath = FSoftObjectPath(pathStr.LeftChop(2));
	}

	const FAssetData assetData = assetRegistry.GetAssetByObjectPath(resolvedPath);
	if (!assetData.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Asset not found in registry: %s"), *AssetPath.ToString());
		return;
	}

	UObject* asset = assetData.GetAsset();
	if (!asset)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Could not load asset: %s"), *AssetPath.ToString());
		return;
	}

	// If it's a class generated by a Blueprint, open the Blueprint asset instead
	if (UClass* classObj = Cast<UClass>(asset))
	{
		if (classObj->ClassGeneratedBy)
		{
			asset = classObj->ClassGeneratedBy;
		}
	}

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(asset);
}

const FSlateBrush* SSGDynamicTextAssetReferenceViewer::GetReferencerAssetTypeIcon(const ESGReferenceType& Type)
{
	switch (Type)
	{
	case ESGReferenceType::Level:
		{
			return FAppStyle::GetBrush("Icons.Level");
		}
	case ESGReferenceType::DynamicTextAsset:
		{
			return FAppStyle::GetBrush("ClassIcon.Object");
		}
	default:
		{
			return FAppStyle::GetBrush("Persona.AssetClass.Blueprint");
		}
	}
}

FText SSGDynamicTextAssetReferenceViewer::GetReferencerAssetTypeTooltip(const ESGReferenceType& Type)
{
	switch (Type)
	{
	case ESGReferenceType::Level:
		{
			return INVTEXT("This dynamic text asset is being referenced by this level(or actor in this level)");
		}
	case ESGReferenceType::DynamicTextAsset:
		{
			return INVTEXT("This dynamic text asset is being referenced by this other dynamic text asset.");
		}
	default:
		{
			return INVTEXT("This dynamic text asset is being referenced by this asset.");
		}
	}
}

FLinearColor SSGDynamicTextAssetReferenceViewer::GetReferencerAssetTypeColor(const ESGReferenceType& Type)
{
	switch (Type)
	{
	case ESGReferenceType::Level:
		{
			return FAppStyle::Get().GetColor("LevelEditor.AssetColor");
		}
	default:
		{
			return FLinearColor::White;
		}
	}
}

USGDynamicTextAssetReferenceSubsystem* SSGDynamicTextAssetReferenceViewer::GetReferenceSubsystem()
{
	if (CachedReferenceSubsystem.IsValid())
	{
		return CachedReferenceSubsystem.Get();
	}

	CachedReferenceSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetReferenceSubsystem>();
	return CachedReferenceSubsystem.Get();
}

void SSGDynamicTextAssetReferenceViewer::OpenViewer(const FSGDynamicTextAssetId& Id, const FString& UserFacingId)
{
	// If viewer is already open, just update it with the new dynamic text asset
	if (GLOBAL_REFERENCE_VIEWER_TAB.IsValid() && GLOBAL_OPEN_REFERENCE_VIEWER.IsValid())
	{
		TSharedPtr<SDockTab> existingTab = GLOBAL_REFERENCE_VIEWER_TAB.Pin();
		TSharedPtr<SSGDynamicTextAssetReferenceViewer> existingViewer = GLOBAL_OPEN_REFERENCE_VIEWER.Pin();
		
		if (existingTab.IsValid() && existingViewer.IsValid())
		{
			existingViewer->SetDynamicTextAsset(Id, UserFacingId);
			existingTab->ActivateInParent(ETabActivationCause::SetDirectly);
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Updated Reference Viewer for: %s"), *UserFacingId);
			return;
		}
	}

	// Create new viewer widget
	TSharedRef<SSGDynamicTextAssetReferenceViewer> viewerWidget = SNew(SSGDynamicTextAssetReferenceViewer)
		.DynamicTextAssetId(Id)
		.UserFacingId(UserFacingId);

	// Spawn the tab
	TSharedRef<SDockTab> newTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(FText::Format(INVTEXT("References: {0}"), FText::FromString(UserFacingId)))
		.OnTabClosed_Lambda([](TSharedRef<SDockTab>)
		{
			GLOBAL_REFERENCE_VIEWER_TAB.Reset();
			GLOBAL_OPEN_REFERENCE_VIEWER.Reset();
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Closed Reference Viewer"));
		})
		[
			viewerWidget
		];

	// Store references
	GLOBAL_REFERENCE_VIEWER_TAB = newTab;
	GLOBAL_OPEN_REFERENCE_VIEWER = viewerWidget;

	// Invoke the tab in the global tab manager
	FGlobalTabmanager::Get()->InsertNewDocumentTab(
		GetTabId(),
		FTabManager::ESearchPreference::PreferLiveTab,
		newTab
	);

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Opened Reference Viewer for: %s"), *UserFacingId);
}

TSharedRef<SWidget> SSGDynamicTextAssetReferenceViewer::CreateDependenciesCollapsedTab()
{
	return SAssignNew(DependenciesCollapsedBox, SBox)
		.Visibility(EVisibility::Visible)
		.WidthOverride(28.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(2.0f))
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.OnClicked(this, &SSGDynamicTextAssetReferenceViewer::OnDependenciesHeaderClicked)
				.ToolTipText(INVTEXT("Expand dependencies panel"))
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Center)
				[
					SNew(SVerticalBox)

					// Expander arrow at top
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 4.0f, 0.0f, 4.0f)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("TreeArrow_Collapsed"))
					]

					// Count badge
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 4.0f, 0.0f, 4.0f)
					[
						SAssignNew(DependenciesCollapsedCountText, STextBlock)
						.Text(GetDependenciesCountText())
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
		];
}

FReply SSGDynamicTextAssetReferenceViewer::OnDependenciesHeaderClicked()
{
	bDependenciesExpanded = !bDependenciesExpanded;

	// Update expanded/collapsed panel visibility (event-based, not polled)
	if (DependenciesExpandedBox.IsValid())
	{
		DependenciesExpandedBox->SetVisibility(bDependenciesExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (DependenciesCollapsedBox.IsValid())
	{
		DependenciesCollapsedBox->SetVisibility(bDependenciesExpanded ? EVisibility::Collapsed : EVisibility::Visible);
	}
	if (DependenciesExpanderArrow.IsValid())
	{
		DependenciesExpanderArrow->SetImage(GetExpanderBrush(bDependenciesExpanded));
	}

	return FReply::Handled();
}

const FSlateBrush* SSGDynamicTextAssetReferenceViewer::GetExpanderBrush(bool bExpanded) const
{
	return FAppStyle::GetBrush(bExpanded ? TEXT("TreeArrow_Expanded") : TEXT("TreeArrow_Collapsed"));
}

void SSGDynamicTextAssetReferenceViewer::OnReferencersSearchTextChanged(const FText& NewText)
{
	ReferencersSearchText = NewText.ToString();
	ApplyReferencersFilter();

	if (ReferencersListView.IsValid())
	{
		ReferencersListView->RequestListRefresh();
	}
}

void SSGDynamicTextAssetReferenceViewer::OnDependenciesSearchTextChanged(const FText& NewText)
{
	DependenciesSearchText = NewText.ToString();
	ApplyDependenciesFilter();

	if (DependenciesListView.IsValid())
	{
		DependenciesListView->RequestListRefresh();
	}
}

void SSGDynamicTextAssetReferenceViewer::ApplyReferencersFilter()
{
	FilteredReferencerItems.Empty();

	if (ReferencersSearchText.IsEmpty())
	{
		FilteredReferencerItems = AllReferencerItems;
	}
	else
	{
		for (const TSharedPtr<FSGDynamicTextAssetReferenceEntry>& item : AllReferencerItems)
		{
			if (!item.IsValid())
			{
				continue;
			}

			// Match against display name / asset name
			FString displayName = item->SourceDisplayName;
			if (displayName.IsEmpty())
			{
				displayName = item->SourceAsset.GetAssetName();
			}

			if (displayName.Contains(ReferencersSearchText, ESearchCase::IgnoreCase))
			{
				FilteredReferencerItems.AddUnique(item);
				continue;
			}

			// Match against property path
			if (item->PropertyPath.Contains(ReferencersSearchText, ESearchCase::IgnoreCase))
			{
				FilteredReferencerItems.AddUnique(item);
				continue;
			}

			// Match against full asset path (covers class name searches)
			if (item->SourceAsset.GetAssetPathString().Contains(ReferencersSearchText, ESearchCase::IgnoreCase))
			{
				FilteredReferencerItems.AddUnique(item);
			}
		}
	}

	// Update count text (event-based, not polled)
	if (ReferencersCountText.IsValid())
	{
		ReferencersCountText->SetText(GetReferencersCountText());
	}
}

void SSGDynamicTextAssetReferenceViewer::ApplyDependenciesFilter()
{
	FilteredDependencyItems.Empty();

	if (DependenciesSearchText.IsEmpty())
	{
		FilteredDependencyItems = AllDependencyItems;
	}
	else
	{
		for (const TSharedPtr<FSGDynamicTextAssetDependencyEntry>& item : AllDependencyItems)
		{
			if (!item.IsValid())
			{
				continue;
			}

			// Match against user-facing ID
			if (!item->UserFacingId.IsEmpty() &&
				item->UserFacingId.Contains(DependenciesSearchText, ESearchCase::IgnoreCase))
			{
				FilteredDependencyItems.Add(item);
				continue;
			}

			// Match against property path
			if (item->PropertyPath.Contains(DependenciesSearchText, ESearchCase::IgnoreCase))
			{
				FilteredDependencyItems.Add(item);
				continue;
			}

			// Match against raw dependency ID (GUID)
			if (item->DependencyId.ToString().Contains(DependenciesSearchText, ESearchCase::IgnoreCase))
			{
				FilteredDependencyItems.Add(item);
				continue;
			}

			// Match against asset path
			if (item->bIsAssetReference && item->AssetPath.ToString().Contains(DependenciesSearchText, ESearchCase::IgnoreCase))
			{
				FilteredDependencyItems.Add(item);
			}
		}
	}

	// Sort to create a categorized appearance (Dynamic Text Assets first, then Asset References)
	FilteredDependencyItems.Sort([](const TSharedPtr<FSGDynamicTextAssetDependencyEntry>& A, const TSharedPtr<FSGDynamicTextAssetDependencyEntry>& B)
	{
		// Dynamic Text Assets first
		if (A->bIsAssetReference != B->bIsAssetReference)
		{
			return !A->bIsAssetReference;
		}

		// Sort alphabetically by name
		if (A->bIsAssetReference)
		{
			return A->AssetPath.GetAssetName().Compare(B->AssetPath.GetAssetName()) < 0;
		}
		FString aName = A->UserFacingId.IsEmpty() ? A->DependencyId.ToString() : A->UserFacingId;
		FString bName = B->UserFacingId.IsEmpty() ? B->DependencyId.ToString() : B->UserFacingId;

		return aName.Compare(bName) < 0;
	});

	// Update count texts (event-based, not polled)
	const FText countText = GetDependenciesCountText();
	if (DependenciesCountText.IsValid())
	{
		DependenciesCountText->SetText(countText);
	}
	if (DependenciesCollapsedCountText.IsValid())
	{
		DependenciesCollapsedCountText->SetText(countText);
	}
}

#undef LOCTEXT_NAMESPACE
