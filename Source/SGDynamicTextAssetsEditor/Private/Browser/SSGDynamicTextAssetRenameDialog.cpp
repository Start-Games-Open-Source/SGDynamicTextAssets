// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDynamicTextAssetRenameDialog.h"

#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "SGDynamicTextAssetEditorLogs.h"
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

#define LOCTEXT_NAMESPACE "SSGDynamicTextAssetRenameDialog"

SSGDynamicTextAssetRenameDialog::SSGDynamicTextAssetRenameDialog()
	: bWasConfirmed(false)
	, bIsNameValid(true)
{ }

void SSGDynamicTextAssetRenameDialog::Construct(const FArguments& InArgs)
{
	SourceFilePath = InArgs._SourceFilePath;
	SourceUserFacingId = InArgs._SourceUserFacingId;

	// Default to the current name
	EnteredName = SourceUserFacingId;

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
				.Text(FText::Format(INVTEXT("Renaming: {0}"), FText::FromString(SourceUserFacingId)))
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
				.OnTextChanged(this, &SSGDynamicTextAssetRenameDialog::OnNameTextChanged)
				.OnTextCommitted(this, &SSGDynamicTextAssetRenameDialog::OnNameTextCommitted)
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
					.Text(INVTEXT("Rename"))
					.IsEnabled(this, &SSGDynamicTextAssetRenameDialog::IsRenameButtonEnabled)
					.OnClicked(this, &SSGDynamicTextAssetRenameDialog::OnRenameClicked)
				]

				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(INVTEXT("Cancel"))
					.OnClicked(this, &SSGDynamicTextAssetRenameDialog::OnCancelClicked)
				]
			]
		]
	];

	// Trigger initial validation
	OnNameTextChanged(FText::FromString(EnteredName));

	// Focus name text box
	USGDynamicTextAssetEditorStatics::FrameDelayedKeyboardFocusToWidget(NameTextBox);
}

void SSGDynamicTextAssetRenameDialog::OnNameTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && bIsNameValid)
	{
		OnRenameClicked();
	}
}

void SSGDynamicTextAssetRenameDialog::OnNameTextChanged(const FText& NewText)
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

	// Same name as current is not a valid rename
	if (sanitized.Equals(FSGDynamicTextAssetFileManager::SanitizeUserFacingId(SourceUserFacingId), ESearchCase::IgnoreCase))
	{
		bIsNameValid = false;
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(INVTEXT("Name is the same as the current name."));
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

bool SSGDynamicTextAssetRenameDialog::ValidateInput() const
{
	if (EnteredName.IsEmpty())
	{
		return false;
	}

	FString sanitized = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(EnteredName);
	if (sanitized.IsEmpty())
	{
		return false;
	}

	return true;
}

FReply SSGDynamicTextAssetRenameDialog::OnRenameClicked()
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

	// Perform the rename
	if (!FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(SourceFilePath, sanitizedName, NewFilePath))
	{
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(INVTEXT("Failed to rename dynamic text asset."));
			ErrorText->SetVisibility(EVisibility::Visible);
		}
		return FReply::Handled();
	}

	// Source control: mark new file for add, old file for delete
	if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
	{
		if (!FSGDynamicTextAssetSourceControl::MarkForAdd(NewFilePath))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to mark renamed file for add in source control: %s"), *NewFilePath);
		}

		if (!FSGDynamicTextAssetSourceControl::MarkForDelete(SourceFilePath))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to mark old file for delete in source control: %s"), *SourceFilePath);
		}
	}

	bWasConfirmed = true;

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Renamed dynamic text asset: %s -> %s"), *SourceUserFacingId, *sanitizedName);

	// Close the window
	if (ParentWindow.IsValid())
	{
		ParentWindow.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SSGDynamicTextAssetRenameDialog::OnCancelClicked()
{
	bWasConfirmed = false;

	// Close the window
	if (ParentWindow.IsValid())
	{
		ParentWindow.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}

bool SSGDynamicTextAssetRenameDialog::ShowModal(const FString& SourceFilePath, const FString& SourceUserFacingId, FString& OutNewFilePath)
{
	TSharedRef<SSGDynamicTextAssetRenameDialog> dialog = SNew(SSGDynamicTextAssetRenameDialog)
		.SourceFilePath(SourceFilePath)
		.SourceUserFacingId(SourceUserFacingId);

	TSharedRef<SWindow> window = SNew(SWindow)
		.Title(INVTEXT("Rename Dynamic Text Asset"))
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
		OutNewFilePath = dialog->GetNewFilePath();
		return true;
	}

	return false;
}

bool SSGDynamicTextAssetRenameDialog::IsRenameButtonEnabled() const
{
	return bIsNameValid;
}

bool SSGDynamicTextAssetRenameDialog::IsNameAlreadyUsed(const FString& Name) const
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
