// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STextBlock;
class SWindow;

/**
 * Modal dialog for renaming a dynamic text asset.
 * Prompts the user for a new name and performs the rename operation.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetRenameDialog : public SCompoundWidget
{
public:
	SSGDynamicTextAssetRenameDialog();

	SLATE_BEGIN_ARGS(SSGDynamicTextAssetRenameDialog)
		: _SourceFilePath()
		, _SourceUserFacingId()
	{ }
		/** The file path of the dynamic text asset to rename */
		SLATE_ARGUMENT(FString, SourceFilePath)

		/** The current user-facing ID of the dynamic text asset */
		SLATE_ARGUMENT(FString, SourceUserFacingId)
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Returns true if the user confirmed the rename */
	bool WasConfirmed() const { return bWasConfirmed; }

	/** Returns the new name entered by the user */
	FString GetEnteredName() const { return EnteredName; }

	/** Returns the new file path after rename */
	FString GetNewFilePath() const { return NewFilePath; }

	/**
	 * Shows the dialog as a modal window.
	 *
	 * @param SourceFilePath The file path of the dynamic text asset to rename
	 * @param SourceUserFacingId The current user-facing ID
	 * @param OutNewFilePath Output for the new file path after rename
	 * @return True if the user confirmed and rename succeeded
	 */
	static bool ShowModal(const FString& SourceFilePath, const FString& SourceUserFacingId, FString& OutNewFilePath);

private:

	/** Called when the name text changes */
	void OnNameTextChanged(const FText& NewText);

	/** Called when the name text is committed (Enter key) */
	void OnNameTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	/** Called when the Rename button is clicked */
	FReply OnRenameClicked();

	/** Called when the Cancel button is clicked */
	FReply OnCancelClicked();

	/** Validates the entered name */
	bool ValidateInput() const;

	/** Returns true if the Rename button should be enabled */
	bool IsRenameButtonEnabled() const;

	/** Checks if a name is already used by any existing dynamic text asset */
	bool IsNameAlreadyUsed(const FString& Name) const;

	/** Source file path */
	FString SourceFilePath;

	/** Source user-facing ID */
	FString SourceUserFacingId;

	/** The entered name for the rename */
	FString EnteredName;

	/** The new file path after rename */
	FString NewFilePath;

	/** Whether the user confirmed the dialog */
	uint8 bWasConfirmed : 1;

	/** Whether the current name is valid */
	uint8 bIsNameValid : 1;

	/** Name input text box */
	TSharedPtr<SEditableTextBox> NameTextBox = nullptr;

	/** Error text display */
	TSharedPtr<STextBlock> ErrorText = nullptr;

	/** Parent window reference */
	TWeakPtr<SWindow> ParentWindow = nullptr;
};
