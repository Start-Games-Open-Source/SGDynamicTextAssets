// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STextBlock;
class SWindow;

/**
 * Modal dialog for duplicating a dynamic text asset.
 * Prompts the user for a new name for the duplicate.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetDuplicateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSGDynamicTextAssetDuplicateDialog)
		: _SourceFilePath()
		, _SourceUserFacingId()
	{ }
		/** The file path of the dynamic text asset to duplicate */
		SLATE_ARGUMENT(FString, SourceFilePath)

		/** The user-facing ID of the source dynamic text asset (for display) */
		SLATE_ARGUMENT(FString, SourceUserFacingId)
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Returns true if the user confirmed the duplication */
	bool WasConfirmed() const { return bWasConfirmed; }

	/** Returns the entered name for the duplicate */
	FString GetEnteredName() const { return EnteredName; }

	/** Returns the ID of the created duplicate */
	FSGDynamicTextAssetId GetCreatedId() const { return CreatedId; }

	/** Returns the file path of the created duplicate */
	FString GetCreatedFilePath() const { return CreatedFilePath; }

	/**
	 * Shows the dialog as a modal window.
	 * 
	 * @param SourceFilePath The file path of the dynamic text asset to duplicate
	 * @param SourceUserFacingId The user-facing ID of the source
	 * @param OutFilePath Output for the created file path
	 * @param OutId Output for the created ID
	 * @return True if the user confirmed and duplication succeeded
	 */
	static bool ShowModal(const FString& SourceFilePath, const FString& SourceUserFacingId, FString& OutFilePath, FSGDynamicTextAssetId& OutId);

private:

	/** Called when the name text changes */
	void OnNameTextChanged(const FText& NewText);

	/** Called when the Duplicate button is clicked */
	FReply OnDuplicateClicked();

	/** Called when the Cancel button is clicked */
	FReply OnCancelClicked();

	/** Validates the entered name */
	bool ValidateInput() const;

	/** Returns true if the Duplicate button should be enabled */
	bool IsDuplicateButtonEnabled() const;

	/** Checks if a name is already used by any existing dynamic text asset */
	bool IsNameAlreadyUsed(const FString& Name) const;

	/** Source file path */
	FString SourceFilePath;

	/** Source user-facing ID */
	FString SourceUserFacingId;

	/** The entered name for the duplicate */
	FString EnteredName;

	/** The ID of the created duplicate */
	FSGDynamicTextAssetId CreatedId;

	/** The file path of the created duplicate */
	FString CreatedFilePath;

	/** Whether the user confirmed the dialog */
	uint8 bWasConfirmed : 1;

	/** Whether the current name is valid (not already taken) */
	uint8 bIsNameValid : 1;

	/** Name input text box */
	TSharedPtr<SEditableTextBox> NameTextBox;

	/** Error text display */
	TSharedPtr<STextBlock> ErrorText;

	/** Parent window reference */
	TWeakPtr<SWindow> ParentWindow;
};
