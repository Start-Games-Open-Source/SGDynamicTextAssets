// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDynamicTextAssetDuplicateDialog.h"

#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Utilities/SGDynamicTextAssetSourceControl.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

#include "Utilities/SGDynamicTextAssetEditorStatics.h"

#define LOCTEXT_NAMESPACE "SSGDynamicTextAssetDuplicateDialog"

void SSGDynamicTextAssetDuplicateDialog::Construct(const FArguments& InArgs)
{
	SourceFilePath = InArgs._SourceFilePath;
	SourceUserFacingId = InArgs._SourceUserFacingId;
	bWasConfirmed = false;
	bIsNameValid = true;

	// Default name suggestion: append "_Copy" to the source name
	EnteredName = SourceUserFacingId + TEXT("_Copy");

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(400.0f)
		.Padding(16.0f)
		[
			SNew(SVerticalBox)

			// Source info
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(FText::Format(INVTEXT("Duplicating: {0}"), FText::FromString(SourceUserFacingId)))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			// Name label
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(INVTEXT("New Name:"))
			]

			// Name input
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(NameTextBox, SEditableTextBox)
				.Text(FText::FromString(EnteredName))
				.OnTextChanged(this, &SSGDynamicTextAssetDuplicateDialog::OnNameTextChanged)
				.SelectAllTextWhenFocused(true)
			]

			// Error text
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SAssignNew(ErrorText, STextBlock)
				.ColorAndOpacity(FLinearColor::Red)
				.Visibility(EVisibility::Collapsed)
			]

			// Buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 16.0f, 0.0f, 0.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(4.0f, 0.0f))

				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(INVTEXT("Duplicate"))
					.IsEnabled(this, &SSGDynamicTextAssetDuplicateDialog::IsDuplicateButtonEnabled)
					.OnClicked(this, &SSGDynamicTextAssetDuplicateDialog::OnDuplicateClicked)
				]

				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(INVTEXT("Cancel"))
					.OnClicked(this, &SSGDynamicTextAssetDuplicateDialog::OnCancelClicked)
				]
			]
		]
	];

	// Trigger initial validation to show error if default name is already taken
	OnNameTextChanged(FText::FromString(EnteredName));

	// Focus name text box
	USGDynamicTextAssetEditorStatics::FrameDelayedKeyboardFocusToWidget(NameTextBox);
}

void SSGDynamicTextAssetDuplicateDialog::OnNameTextChanged(const FText& NewText)
{
	EnteredName = NewText.ToString();

	// Validate the name in real-time
	if (EnteredName.IsEmpty())
	{
		bIsNameValid = false;
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(INVTEXT("Please enter a name."));
			ErrorText->SetVisibility(EVisibility::Visible);
		}
		return;
	}

	// Check for invalid characters
	FString sanitized = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(EnteredName);
	if (sanitized.IsEmpty())
	{
		bIsNameValid = false;
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(INVTEXT("Name contains invalid characters."));
			ErrorText->SetVisibility(EVisibility::Visible);
		}
		return;
	}

	// Check if name is already used globally
	if (IsNameAlreadyUsed(sanitized))
	{
		bIsNameValid = false;
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(INVTEXT("A dynamic text asset with this name already exists."));
			ErrorText->SetVisibility(EVisibility::Visible);
		}
		return;
	}

	// Name is valid
	bIsNameValid = true;
	if (ErrorText.IsValid())
	{
		ErrorText->SetVisibility(EVisibility::Collapsed);
	}
}

bool SSGDynamicTextAssetDuplicateDialog::ValidateInput() const
{
	if (EnteredName.IsEmpty())
	{
		return false;
	}

	// Check for invalid characters
	FString sanitized = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(EnteredName);
	if (sanitized.IsEmpty())
	{
		return false;
	}

	// Respect the real-time validation result (includes duplicate name check)
	if (!bIsNameValid)
	{
		return false;
	}

	return true;
}

FReply SSGDynamicTextAssetDuplicateDialog::OnDuplicateClicked()
{
	if (!ValidateInput())
	{
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(INVTEXT("Please enter a valid name."));
			ErrorText->SetVisibility(EVisibility::Visible);
		}
		return FReply::Handled();
	}

	// Sanitize the name
	FString sanitizedName = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(EnteredName);

	// Check if a file with this name already exists
	// We need to get the class from the source to check the path
	FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(SourceFilePath);
	if (fileInfo.bIsValid && (fileInfo.AssetTypeId.IsValid() || !fileInfo.ClassName.IsEmpty()))
	{
		// Resolve class via Asset Type ID, with fallback to class name for legacy files
		UClass* dataObjectClass = nullptr;
		if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
		{
			if (fileInfo.AssetTypeId.IsValid())
			{
				dataObjectClass = registry->ResolveClassForTypeId(fileInfo.AssetTypeId);
			}

			// Fallback: resolve by class name for legacy files without a valid Asset Type ID
			if (!dataObjectClass && !fileInfo.ClassName.IsEmpty())
			{
				TArray<UClass*> allClasses;
				registry->GetAllInstantiableClasses(allClasses);
				for (UClass* registeredClass : allClasses)
				{
					if (registeredClass && registeredClass->GetName() == fileInfo.ClassName)
					{
						dataObjectClass = registeredClass;
						break;
					}
				}
			}
		}

		if (dataObjectClass)
		{
			FString proposedPath = FSGDynamicTextAssetFileManager::BuildFilePath(dataObjectClass, sanitizedName);
			if (FSGDynamicTextAssetFileManager::FileExists(proposedPath))
			{
				if (ErrorText.IsValid())
				{
					ErrorText->SetText(INVTEXT("A dynamic text asset with this name already exists."));
					ErrorText->SetVisibility(EVisibility::Visible);
				}
				return FReply::Handled();
			}
		}
	}

	// Perform the duplication
	if (!FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(SourceFilePath, sanitizedName, CreatedId, CreatedFilePath))
	{
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(INVTEXT("Failed to duplicate dynamic text asset."));
			ErrorText->SetVisibility(EVisibility::Visible);
		}
		return FReply::Handled();
	}

	// Auto-add to source control
	if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
	{
		if (!FSGDynamicTextAssetSourceControl::MarkForAdd(CreatedFilePath))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to mark duplicated file for add in source control: %s"), *CreatedFilePath);
		}
	}

	bWasConfirmed = true;

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Duplicated dynamic text asset: %s -> %s (ID: %s)"),
		*SourceUserFacingId, *sanitizedName, *CreatedId.ToString());

	// Close the window
	if (ParentWindow.IsValid())
	{
		ParentWindow.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SSGDynamicTextAssetDuplicateDialog::OnCancelClicked()
{
	bWasConfirmed = false;

	// Close the window
	if (ParentWindow.IsValid())
	{
		ParentWindow.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}

bool SSGDynamicTextAssetDuplicateDialog::ShowModal(const FString& SourceFilePath, const FString& SourceUserFacingId, FString& OutFilePath, FSGDynamicTextAssetId& OutId)
{
	TSharedRef<SSGDynamicTextAssetDuplicateDialog> dialog = SNew(SSGDynamicTextAssetDuplicateDialog)
		.SourceFilePath(SourceFilePath)
		.SourceUserFacingId(SourceUserFacingId);

	TSharedRef<SWindow> window = SNew(SWindow)
		.Title(INVTEXT("Duplicate Dynamic Text Asset"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			dialog
		];

	dialog->ParentWindow = window;

	FSlateApplication::Get().AddModalWindow(window, FSlateApplication::Get().GetActiveTopLevelWindow());

	if (dialog->WasConfirmed())
	{
		OutFilePath = dialog->GetCreatedFilePath();
		OutId = dialog->GetCreatedId();
		return true;
	}

	return false;
}

bool SSGDynamicTextAssetDuplicateDialog::IsDuplicateButtonEnabled() const
{
	return bIsNameValid;
}

bool SSGDynamicTextAssetDuplicateDialog::IsNameAlreadyUsed(const FString& Name) const
{
	if (Name.IsEmpty())
	{
		return false;
	}

	// Scan all dynamic text asset files and check for matching UserFacingId
	TArray<FString> allFilePaths;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFilePaths);

	for (const FString& filePath : allFilePaths)
	{
		FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);
		if (fileInfo.bIsValid && fileInfo.UserFacingId.Equals(Name, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
