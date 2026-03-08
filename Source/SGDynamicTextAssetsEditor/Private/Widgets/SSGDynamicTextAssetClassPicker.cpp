// Copyright Start Games, Inc. All Rights Reserved.

#include "Widgets/SSGDynamicTextAssetClassPicker.h"

#include "Management/SGDynamicTextAssetRegistry.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

void SSGDynamicTextAssetClassPicker::Construct(const FArguments& InArgs)
{
	bShowNoneOption = InArgs._bShowNoneOption;
	OnClassSelectedDelegate = InArgs._OnClassSelected;

	BuildEntries();

	// Apply initial selection
	if (InArgs._InitialClass)
	{
		for (const TSharedPtr<FClassPickerEntry>& entry : AllEntries)
		{
			if (entry->Class.IsValid() && entry->Class.Get() == InArgs._InitialClass)
			{
				SelectedClass = entry->Class;
				break;
			}
		}
	}

	// Default to first non-None entry if nothing selected
	if (!SelectedClass.IsValid() && !bShowNoneOption)
	{
		for (const TSharedPtr<FClassPickerEntry>& entry : AllEntries)
		{
			if (entry->Class.IsValid())
			{
				SelectedClass = entry->Class;
				break;
			}
		}
	}

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SSGDynamicTextAssetClassPicker::GenerateMenuContent)
		.ButtonContent()
		[
			SAssignNew(DisplayTextBlock, STextBlock)
			.Text(GetSelectedDisplayText())
		]
	];
}

SSGDynamicTextAssetClassPicker::~SSGDynamicTextAssetClassPicker()
{
	// Clean up any pending filter timer to prevent dangling this pointer
	if (FilterTimerHandle.IsValid() && PickerListView.IsValid())
	{
		PickerListView->UnRegisterActiveTimer(FilterTimerHandle.ToSharedRef());
		FilterTimerHandle.Reset();
	}
}

void SSGDynamicTextAssetClassPicker::SetSelectedClass(UClass* InClass)
{
	SelectedClass = InClass;

	if (DisplayTextBlock.IsValid())
	{
		DisplayTextBlock->SetText(GetSelectedDisplayText());
	}
}

UClass* SSGDynamicTextAssetClassPicker::GetSelectedClass() const
{
	return SelectedClass.Get();
}

FText SSGDynamicTextAssetClassPicker::GetSelectedDisplayText() const
{
	if (SelectedClass.IsValid() && SelectedClass.Get())
	{
		return SelectedClass.Get()->GetDisplayNameText();
	}
	return bShowNoneOption ? INVTEXT("None") : INVTEXT("Select a class...");
}

void SSGDynamicTextAssetClassPicker::BuildEntries()
{
	AllEntries.Reset();

	// Add "None" entry if requested
	if (bShowNoneOption)
	{
		TSharedPtr<FClassPickerEntry> noneEntry = MakeShared<FClassPickerEntry>();
		noneEntry->Class = nullptr;
		noneEntry->TypeId = FSGDynamicTextAssetTypeId::INVALID_TYPE_ID;
		noneEntry->DisplayName = INVTEXT("None");
		AllEntries.Add(noneEntry);
	}

	if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
	{
		TArray<UClass*> classes;
		registry->GetAllInstantiableClasses(classes);

		for (UClass* cls : classes)
		{
			if (!cls)
			{
				continue;
			}

			TSharedPtr<FClassPickerEntry> entry = MakeShared<FClassPickerEntry>();
			entry->Class = cls;
			entry->DisplayName = cls->GetDisplayNameText();
			entry->TypeId = registry->GetTypeIdForClass(cls);
			AllEntries.Add(entry);
		}
	}

	// Sort: None first (if present), then alphabetical by DisplayName
	AllEntries.Sort([](const TSharedPtr<FClassPickerEntry>& A, const TSharedPtr<FClassPickerEntry>& B)
	{
		// None (null class) always comes first
		if (!A->Class.IsValid())
		{
			return true;
		}
		if (!B->Class.IsValid())
		{
			return false;
		}
		return A->DisplayName.CompareTo(B->DisplayName, ETextComparisonLevel::Default) < 0;
	});

	ApplyFilter();
}

void SSGDynamicTextAssetClassPicker::ApplyFilter()
{
	FilteredEntries.Reset();

	for (const TSharedPtr<FClassPickerEntry>& entry : AllEntries)
	{
		if (!SearchText.IsEmpty())
		{
			const FString searchString = SearchText.ToString();
			bool bMatchesSearch =
				entry->DisplayName.ToString().Contains(searchString, ESearchCase::IgnoreCase) ||
				(entry->Class.IsValid() && entry->Class->GetName().Contains(searchString, ESearchCase::IgnoreCase)) ||
				entry->TypeId.ToString().Contains(searchString, ESearchCase::IgnoreCase);

			if (!bMatchesSearch)
			{
				continue;
			}
		}

		FilteredEntries.Add(entry);
	}

	if (PickerListView.IsValid())
	{
		PickerListView->RequestListRefresh();
	}
}

TSharedRef<SWidget> SSGDynamicTextAssetClassPicker::GenerateMenuContent()
{
	// Reset search state when menu opens
	SearchText = FText::GetEmpty();

	// Rebuild entries in case registry changed since last open
	BuildEntries();

	return SNew(SBox)
		.MinDesiredWidth(300.0f)
		.MaxDesiredHeight(400.0f)
		[
			SNew(SVerticalBox)

			// Search box
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SNew(SSearchBox)
				.HintText(INVTEXT("Search classes..."))
				.OnTextChanged(this, &SSGDynamicTextAssetClassPicker::OnSearchTextChanged)
			]

			// Virtualized list view
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(PickerListView, SListView<TSharedPtr<FClassPickerEntry>>)
				.ListItemsSource(&FilteredEntries)
				.OnGenerateRow(this, &SSGDynamicTextAssetClassPicker::GenerateRow)
				.OnSelectionChanged(this, &SSGDynamicTextAssetClassPicker::OnListSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
			]
		];
}

TSharedRef<ITableRow> SSGDynamicTextAssetClassPicker::GenerateRow(TSharedPtr<FClassPickerEntry> Entry,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	// "None" entry gets simpler styling
	if (!Entry->Class.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FClassPickerEntry>>, OwnerTable)
			[
				SNew(SBox)
				.Padding(FMargin(4.0f, 4.0f))
				[
					SNew(STextBlock)
					.Text(INVTEXT("None"))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			];
	}

	return SNew(STableRow<TSharedPtr<FClassPickerEntry>>, OwnerTable)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SVerticalBox)
				.ToolTipText(FText::Format(INVTEXT("{0}\nAsset Type ID: {1}"),
					Entry->DisplayName,
					FText::FromString(Entry->TypeId.ToString())))

				// Display name (primary, bold)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(Entry->DisplayName)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				]

				// Asset Type ID (secondary, subdued)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry->TypeId.ToString()))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
}

void SSGDynamicTextAssetClassPicker::OnSearchTextChanged(const FText& NewText)
{
	SearchText = NewText;

	// Cancel any pending filter timer
	if (FilterTimerHandle.IsValid() && PickerListView.IsValid())
	{
		PickerListView->UnRegisterActiveTimer(FilterTimerHandle.ToSharedRef());
		FilterTimerHandle.Reset();
	}

	// Start a new 150ms debounce timer for throttled refresh
	if (PickerListView.IsValid())
	{
		FilterTimerHandle = PickerListView->RegisterActiveTimer(
			0.15f,
			FWidgetActiveTimerDelegate::CreateLambda(
				[this](double /*InCurrentTime*/, float /*InDeltaTime*/) -> EActiveTimerReturnType
			{
				OnFilterTimerExpired();
				return EActiveTimerReturnType::Stop;
			}));
	}
	else
	{
		// Fallback: apply immediately if list view not available
		ApplyFilter();
	}
}

void SSGDynamicTextAssetClassPicker::OnFilterTimerExpired()
{
	ApplyFilter();
	FilterTimerHandle.Reset();
}

void SSGDynamicTextAssetClassPicker::OnListSelectionChanged(TSharedPtr<FClassPickerEntry> NewEntry, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (!NewEntry.IsValid())
	{
		return;
	}

	SelectedClass = NewEntry->Class;

	if (DisplayTextBlock.IsValid())
	{
		DisplayTextBlock->SetText(GetSelectedDisplayText());
	}

	// Close the combo button menu
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	// Fire the delegate
	OnClassSelectedDelegate.ExecuteIfBound(SelectedClass.Get());

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Class picker: selected %s (%s)"),
		*NewEntry->DisplayName.ToString(),
		*NewEntry->TypeId.ToString());
}
