// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAssetId.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"

struct FSGDynamicTextAssetSerializerMetadata;

class SButton;
class STextBlock;
class SSGDynamicTextAssetClassPicker;
class USGDynamicTextAssetRegistry;

/**
 * Modal dialog for creating a new dynamic text asset.
 *
 * Allows the user to select a dynamic text asset class and enter a name.
 * On confirmation, creates the file via FSGDynamicTextAssetFileManager.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetCreateDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetCreateDialog)
    { }
        /** Optional: Pre-select a specific class */
        SLATE_ARGUMENT(UClass*, InitialClass)

        /** Called when dialog is confirmed with valid inputs */
        SLATE_EVENT(FSimpleDelegate, OnConfirm)

        /** Called when dialog is cancelled */
        SLATE_EVENT(FSimpleDelegate, OnCancel)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Gets the selected class */
    UClass* GetSelectedClass() const;

    /** Gets the entered name */
    FString GetEnteredName() const;

    /** Gets the created file path (valid after successful creation) */
    FString GetCreatedFilePath() const { return CreatedFilePath; }

    /** Gets the created ID (valid after successful creation) */
    FSGDynamicTextAssetId GetCreatedId() const { return CreatedId; }

    /** Gets the currently selected extension string (e.g. ".dta.json"). Falls back to JSON_EXTENSION if none selected. */
    FString GetSelectedExtensionString() const;

    /** Shows the dialog as a modal window. Returns true if confirmed. */
    static bool ShowModal(UClass* InitialClass, FString& OutFilePath, FSGDynamicTextAssetId& OutId);

private:

    /** Builds the file format/extension dropdown options from registered serializers */
    void BuildExtensionOptions();

    /** Generates a combo box row for an extension string */
    TSharedRef<SWidget> GenerateExtensionComboItem(TSharedPtr<FSGDynamicTextAssetSerializerMetadata> InExtension);

    /** Handles extension selection change */
    void OnExtensionSelectionChanged(TSharedPtr<FSGDynamicTextAssetSerializerMetadata> NewSelection, ESelectInfo::Type SelectInfo);

    /** Gets the text to display for the selected extension */
    FText GetSelectedExtensionText() const;

    /** Called when a class is selected from the class picker */
    void OnClassPicked(UClass* NewClass);

    /** Handles name text change */
    void OnNameTextChanged(const FText& NewText);

    /** Validates the current inputs */
    bool ValidateInputs() const;

    /** Returns whether the Create button should be enabled (cached validation state) */
    bool IsCreateButtonEnabled() const;

    /** Returns whether the given name is already used by any existing dynamic text asset (global check) */
    bool IsNameAlreadyUsed(const FString& Name) const;

    /** Called when the name text is committed (Enter key) */
    void OnNameTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

    /** Called when Create button is clicked */
    FReply OnCreateClicked();

    /** Called when Cancel button is clicked */
    FReply OnCancelClicked();

    bool CommitNewDynamicTextAsset();

    /** The searchable class picker */
    TSharedPtr<SSGDynamicTextAssetClassPicker> ClassPicker = nullptr;

    /** The name text box */
    TSharedPtr<SEditableTextBox> NameTextBox = nullptr;

    /** The format/extension dropdown */
    TSharedPtr<SComboBox<TSharedPtr<FSGDynamicTextAssetSerializerMetadata>>> ExtensionComboBox = nullptr;

    /** Currently selected class */
    TWeakObjectPtr<UClass> SelectedClass = nullptr;

    /** Available extension options */
    TArray<TSharedPtr<FSGDynamicTextAssetSerializerMetadata>> ExtensionOptions;

    /** Currently selected extension */
    TWeakPtr<FSGDynamicTextAssetSerializerMetadata> SelectedExtension = nullptr;

    /** The Format row widget — visibility is set imperatively in BuildExtensionOptions */
    TSharedPtr<SWidget> ExtensionRow = nullptr;

    /** Entered name */
    FString EnteredName;

    /** Error message text */
    TSharedPtr<STextBlock> ErrorText = nullptr;

    /** Created file path (after successful creation) */
    FString CreatedFilePath;

    /** Created ID (after successful creation) */
    FSGDynamicTextAssetId CreatedId;

    /** Confirm callback */
    FSimpleDelegate OnConfirm;

    /** Cancel callback */
    FSimpleDelegate OnCancel;

    /** Whether creation was successful */
    uint8 bWasConfirmed : 1 = 0;

    /** Whether the current inputs pass all validation checks */
    uint8 bIsNameValid : 1 = 0;

    /** Reference to the window hosting this dialog */
    TWeakPtr<SWindow> ParentWindow = nullptr;

    /** Create button reference for event-based enable/disable control */
    TSharedPtr<SButton> CreateButton = nullptr;

    /** Extension display text block for event-based text updates */
    TSharedPtr<STextBlock> ExtensionDisplayText = nullptr;
};
