// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDynamicTextAssetRefCustomization.h"

#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "DetailWidgetRow.h"
#include "Editor/FSGDynamicTextAssetEditorToolkit.h"
#include "Editor/SSGDynamicTextAssetIcon.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/Reply.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SGDynamicTextAssetRefCustomization"

TSharedRef<IPropertyTypeCustomization> FSGDynamicTextAssetRefCustomization::MakeInstance()
{
	return MakeShareable(new FSGDynamicTextAssetRefCustomization());
}

void FSGDynamicTextAssetRefCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = PropertyHandle;

	// Get child handle for the ID field
	IdHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSGDynamicTextAssetRef, Id));

	// Build picker entries first so AllEntries is populated when ReadCurrentValues resolves display name
	RebuildPickerEntries();

	// Read current values (resolves UserFacingId from AllEntries)
	ReadCurrentValues();

	// Build available class names for the editor filter
	AvailableClassNames.Reset();
	AvailableClassNames.Add(MakeShared<FString>(TEXT("All Types")));

	if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
	{
		TArray<UClass*> allClasses;
		registry->GetAllInstantiableClasses(allClasses);
		for (UClass* registeredClass : allClasses)
		{
			if (registeredClass)
			{
				AvailableClassNames.Add(MakeShared<FString>(registeredClass->GetName()));
			}
		}
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(300.0f)
		.MaxDesiredWidth(600.0f)
		[
			SNew(SHorizontalBox)

			// Searchable combo button picker
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ComboButton, SComboButton)
				.OnGetMenuContent(this, &FSGDynamicTextAssetRefCustomization::GeneratePickerMenu)
				.ButtonContent()
				[
					SAssignNew(DisplayTextBlock, STextBlock)
					.Text(GetCurrentDisplayText())
					.ToolTipText(GetCurrentTooltipText())
				]
			]

			// Open Editor button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(OpenEditorButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Open dynamic text asset in editor"))
				.OnClicked(this, &FSGDynamicTextAssetRefCustomization::OnOpenEditorClicked)
				.IsEnabled(CurrentId.IsValid())
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Edit"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Copy ID button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(CopyIdButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Copy ID to clipboard"))
				.OnClicked(this, &FSGDynamicTextAssetRefCustomization::OnCopyIdClicked)
				.IsEnabled(CurrentId.IsValid())
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("GenericCommands.Copy"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Clear button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(ClearButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Clear reference"))
				.OnClicked(this, &FSGDynamicTextAssetRefCustomization::OnClearClicked)
				.IsEnabled(CurrentId.IsValid())
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.X"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}

void FSGDynamicTextAssetRefCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Hide all children - the header handles everything
}

TSharedRef<SWidget> FSGDynamicTextAssetRefCustomization::GeneratePickerMenu()
{
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
				.HintText(INVTEXT("Search dynamic text assets..."))
				.OnTextChanged(this, &FSGDynamicTextAssetRefCustomization::OnSearchTextChanged)
			]

			// Editor-local class filter
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 0.0f, 4.0f, 4.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(INVTEXT("Filter:"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&AvailableClassNames)
					.OnSelectionChanged(this, &FSGDynamicTextAssetRefCustomization::OnEditorFilterClassChanged)
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(FText::FromString(*Item));
					})
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return EditorFilterClassName.IsEmpty() 
								? INVTEXT("All Types")
								: FText::FromString(EditorFilterClassName);
						})
					]
				]
			]

			// List view
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(PickerListView, SListView<TSharedPtr<FPickerEntry>>)
				.ListItemsSource(&FilteredEntries)
				.OnGenerateRow(this, &FSGDynamicTextAssetRefCustomization::GenerateListRow)
				.OnSelectionChanged(this, &FSGDynamicTextAssetRefCustomization::OnListSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
			]
		];
}

FText FSGDynamicTextAssetRefCustomization::FPickerEntry::GetDisplayText() const
{
	if (UserFacingId.IsEmpty())
	{
		return FText::FromString(Id.ToString());
	}
	return FText::FromString(FString::Printf(TEXT("%s (%s)"), *UserFacingId, *ClassName));
}

void FSGDynamicTextAssetRefCustomization::RebuildPickerEntries()
{
	AllEntries.Reset();

	// Determine which class to filter by from UPROPERTY metadata
	UClass* metaFilterClass = GetTypeFilterClass();

	// Scan for all dynamic text asset files
	TArray<FString> filePaths;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(filePaths);

	// Build picker entries from file metadata
	for (const FString& filePath : filePaths)
	{
		FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (!metadata.bIsValid)
		{
			continue;
		}

		// Check class filter from UPROPERTY metadata
		if (metaFilterClass != nullptr && !metadata.ClassName.IsEmpty())
		{
			USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
			if (registry)
			{
				TArray<UClass*> allClasses;
				registry->GetAllInstantiableClasses(allClasses);

				UClass* fileClass = nullptr;
				for (UClass* registeredClass : allClasses)
				{
					if (registeredClass && registeredClass->GetName() == metadata.ClassName)
					{
						fileClass = registeredClass;
						break;
					}
				}

				if (fileClass && !fileClass->IsChildOf(metaFilterClass))
				{
					continue;
				}
			}
		}

		TSharedPtr<FPickerEntry> entry = MakeShared<FPickerEntry>();
		entry->Id = metadata.Id;
		entry->UserFacingId = metadata.UserFacingId;
		entry->ClassName = metadata.ClassName;
		entry->FilePath = filePath;
		AllEntries.Add(entry);
	}

	// Sort by UserFacingId
	AllEntries.Sort([](const TSharedPtr<FPickerEntry>& A, const TSharedPtr<FPickerEntry>& B)
	{
		return A->UserFacingId < B->UserFacingId;
	});

	// Apply current filter
	ApplyFilter();

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Rebuilt ref picker with %d entries (%d filtered)"), 
		AllEntries.Num(), FilteredEntries.Num());
}

void FSGDynamicTextAssetRefCustomization::ApplyFilter()
{
	FilteredEntries.Reset();

	for (const TSharedPtr<FPickerEntry>& entry : AllEntries)
	{
		// Apply editor-local class filter
		if (!EditorFilterClassName.IsEmpty() && EditorFilterClassName != TEXT("All Types"))
		{
			if (entry->ClassName != EditorFilterClassName)
			{
				continue;
			}
		}

		// Apply search text filter
		if (!SearchText.IsEmpty())
		{
			bool bMatchesSearch = entry->UserFacingId.Contains(SearchText, ESearchCase::IgnoreCase) ||
				entry->ClassName.Contains(SearchText, ESearchCase::IgnoreCase) ||
				entry->Id.ToString().Contains(SearchText, ESearchCase::IgnoreCase);

			if (!bMatchesSearch)
			{
				continue;
			}
		}

		FilteredEntries.Add(entry);
	}

	// Refresh the list view if it exists
	if (PickerListView.IsValid())
	{
		PickerListView->RequestListRefresh();
	}
}

FText FSGDynamicTextAssetRefCustomization::GetCurrentDisplayText() const
{
	if (!CurrentId.IsValid())
	{
		return INVTEXT("[No Dynamic Text Asset Selected]");
	}

	if (!CurrentUserFacingId.IsEmpty())
	{
		return FText::FromString(CurrentUserFacingId);
	}

	return FText::FromString(CurrentId.ToString());
}

FText FSGDynamicTextAssetRefCustomization::GetCurrentTooltipText() const
{
	if (!CurrentId.IsValid())
	{
		return INVTEXT("No Dynamic Text Asset selected");
	}

	return FText::Format(INVTEXT("{0}\nID: {1}"),
		FText::FromString(CurrentUserFacingId), FText::FromString(CurrentId.ToString()));
}

void FSGDynamicTextAssetRefCustomization::OnEntrySelected(TSharedPtr<FPickerEntry> NewEntry)
{
	if (!NewEntry.IsValid())
	{
		return;
	}

	WriteValues(NewEntry->Id, NewEntry->UserFacingId);

	// Close the combo button menu
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Selected dynamic text asset ref: %s (%s)"),
		*NewEntry->UserFacingId, *NewEntry->Id.ToString());
}

void FSGDynamicTextAssetRefCustomization::OnListSelectionChanged(TSharedPtr<FPickerEntry> NewEntry, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		OnEntrySelected(NewEntry);
	}
}

TSharedRef<ITableRow> FSGDynamicTextAssetRefCustomization::GenerateListRow(TSharedPtr<FPickerEntry> Entry, 
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPickerEntry>>, OwnerTable)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				.ToolTipText(FText::Format(INVTEXT("{0}\nClass:{1}\nID:{2}"),
					FText::FromString(Entry->UserFacingId),
					FText::FromString(Entry->ClassName),
					FText::FromString(Entry->Id.ToString())))

				// Icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(SSGDynamicTextAssetIcon)
					.Serializer(FSGDynamicTextAssetFileManager::FindSerializerForFile(Entry->FilePath))
				]

				// Name and class
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					// User facing ID text
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Entry->UserFacingId))
					]

					// Class text
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 1.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Entry->ClassName))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]

					// ID text
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Entry->Id.ToString()))
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
		];
}

void FSGDynamicTextAssetRefCustomization::OnSearchTextChanged(const FText& NewText)
{
	SearchText = NewText.ToString();
	ApplyFilter();
}

void FSGDynamicTextAssetRefCustomization::OnEditorFilterClassChanged(TSharedPtr<FString> NewClass, ESelectInfo::Type SelectInfo)
{
	if (NewClass.IsValid())
	{
		EditorFilterClassName = *NewClass;
		if (EditorFilterClassName == TEXT("All Types"))
		{
			EditorFilterClassName.Empty();
		}
		ApplyFilter();
	}
}

FReply FSGDynamicTextAssetRefCustomization::OnClearClicked()
{
	FSGDynamicTextAssetId invalidId;
	invalidId.Invalidate();
	WriteValues(invalidId, FString());

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Cleared dynamic text asset ref"));
	return FReply::Handled();
}

FReply FSGDynamicTextAssetRefCustomization::OnCopyIdClicked()
{
	if (CurrentId.IsValid())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CurrentId.ToString());
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied ref ID to clipboard: %s"), *CurrentId.ToString());
	}
	return FReply::Handled();
}

FReply FSGDynamicTextAssetRefCustomization::OnPasteClicked()
{
	// Read text from system clipboard
	FString clipboardText;
	FPlatformApplicationMisc::ClipboardPaste(clipboardText);
	clipboardText.TrimStartAndEndInline();

	if (clipboardText.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Paste failed: clipboard is empty"));
		return FReply::Handled();
	}

	// First, try to parse as a GUID
	FSGDynamicTextAssetId parsedId = FSGDynamicTextAssetId::FromString(clipboardText);
	if (parsedId.IsValid())
	{
		// Look up UserFacingId from entries if a matching GUID exists
		FString resolvedUserFacingId;
		for (const TSharedPtr<FPickerEntry>& entry : AllEntries)
		{
			if (entry->Id == parsedId)
			{
				resolvedUserFacingId = entry->UserFacingId;
				break;
			}
		}

		WriteValues(parsedId, resolvedUserFacingId);
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Pasted GUID ref(%s), UserFacingId(%s)"),
			*parsedId.ToString(), *resolvedUserFacingId);
		return FReply::Handled();
	}

	// Not a valid GUID - try to match as a UserFacingId
	for (const TSharedPtr<FPickerEntry>& entry : AllEntries)
	{
		if (entry->UserFacingId == clipboardText)
		{
			WriteValues(entry->Id, entry->UserFacingId);
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Pasted UserFacingId ref: %s (%s)"),
				*entry->UserFacingId, *entry->Id.ToString());
			return FReply::Handled();
		}
	}

	// Neither format matched - log a warning
	UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Paste failed: clipboard content '%s' is not a valid GUID or known UserFacingId"),
		*clipboardText);

	return FReply::Handled();
}

FReply FSGDynamicTextAssetRefCustomization::OnOpenEditorClicked()
{
	if (!CurrentId.IsValid())
	{
		return FReply::Handled();
	}

	// Find the entry to get the file path and class
	TSharedPtr<FPickerEntry> currentEntry = FindCurrentEntry();
	if (currentEntry.IsValid())
	{
		// Look up the class and type ID from the registry
		UClass* dataObjectClass = nullptr;
		FSGDynamicTextAssetTypeId typeId;
		if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
		{
			TArray<UClass*> allClasses;
			registry->GetAllInstantiableClasses(allClasses);
			for (UClass* registeredClass : allClasses)
			{
				if (registeredClass && registeredClass->GetName() == currentEntry->ClassName)
				{
					dataObjectClass = registeredClass;
					break;
				}
			}

			if (dataObjectClass)
			{
				typeId = registry->GetTypeIdForClass(dataObjectClass);
			}
		}

		// Open the dynamic text asset editor
		FSGDynamicTextAssetEditorToolkit::OpenEditor(currentEntry->FilePath, dataObjectClass, typeId);

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Opening dynamic text asset editor for: %s"), *currentEntry->UserFacingId);
	}

	return FReply::Handled();
}

void FSGDynamicTextAssetRefCustomization::ReadCurrentValues()
{
	CurrentId.Invalidate();
	CurrentUserFacingId.Empty();

	if (!IdHandle.IsValid())
	{
		return;
	}

	// Read ID since it's an FSGDynamicTextAssetId struct, so we read its child properties
	void* idValuePtr = nullptr;
	if (IdHandle->GetValueData(idValuePtr) == FPropertyAccess::Success && idValuePtr)
	{
		CurrentId = *static_cast<FSGDynamicTextAssetId*>(idValuePtr);
	}

	// Resolve display name from the pre-scanned file metadata (no world context needed)
	for (const TSharedPtr<FPickerEntry>& entry : AllEntries)
	{
		if (entry->Id == CurrentId)
		{
			CurrentUserFacingId = entry->UserFacingId;
			break;
		}
	}
}

void FSGDynamicTextAssetRefCustomization::RefreshDisplayState()
{
	// Update display text and tooltip (event-based, not polled)
	if (DisplayTextBlock.IsValid())
	{
		DisplayTextBlock->SetText(GetCurrentDisplayText());
		DisplayTextBlock->SetToolTipText(GetCurrentTooltipText());
	}

	// Update action button enabled states (event-based, not polled)
	const bool bHasValidId = CurrentId.IsValid();
	if (OpenEditorButton.IsValid())
	{
		OpenEditorButton->SetEnabled(bHasValidId);
	}
	if (CopyIdButton.IsValid())
	{
		CopyIdButton->SetEnabled(bHasValidId);
	}
	if (ClearButton.IsValid())
	{
		ClearButton->SetEnabled(bHasValidId);
	}
}

void FSGDynamicTextAssetRefCustomization::WriteValues(const FSGDynamicTextAssetId& NewId, const FString& NewUserFacingId)
{
	if (!StructPropertyHandle.IsValid() || !IdHandle.IsValid())
	{
		return;
	}

	// Single pre-change notification on the struct handle
	StructPropertyHandle->NotifyPreChange();

	// Write the ID (the only field persisted on the struct)
	void* idValuePtr = nullptr;
	if (IdHandle->GetValueData(idValuePtr) == FPropertyAccess::Success && idValuePtr)
	{
		*static_cast<FSGDynamicTextAssetId*>(idValuePtr) = NewId;
	}

	// Single post change notification
	StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

	// Update local display cache (may not execute if panel was reconstructed above)
	CurrentId = NewId;
	CurrentUserFacingId = NewUserFacingId;

	RefreshDisplayState();
}

UClass* FSGDynamicTextAssetRefCustomization::GetTypeFilterClass() const
{
	if (!StructPropertyHandle.IsValid())
	{
		return nullptr;
	}

	// Check for "DynamicTextAssetType" meta tag on the UPROPERTY
	FString typeFilterName;
	if (StructPropertyHandle->GetProperty())
	{
		typeFilterName = StructPropertyHandle->GetProperty()->GetMetaData(TEXT("DynamicTextAssetType"));
	}

	if (typeFilterName.IsEmpty())
	{
		return nullptr;
	}

	// Search registered classes by name
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	if (!registry)
	{
		return nullptr;
	}

	TArray<UClass*> allClasses;
	registry->GetAllInstantiableClasses(allClasses);

	for (UClass* registeredClass : allClasses)
	{
		if (registeredClass && registeredClass->GetName() == typeFilterName)
		{
			return registeredClass;
		}
	}

	return nullptr;
}

TSharedPtr<FSGDynamicTextAssetRefCustomization::FPickerEntry> FSGDynamicTextAssetRefCustomization::FindCurrentEntry() const
{
	if (!CurrentId.IsValid())
	{
		return nullptr;
	}

	for (const TSharedPtr<FPickerEntry>& entry : AllEntries)
	{
		if (entry->Id == CurrentId)
		{
			return entry;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
