// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDynamicTextAssetCreateDialog.h"

#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "Utilities/SGDynamicTextAssetSourceControl.h"
#include "Widgets/SSGDynamicTextAssetClassPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"

#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Serialization/SGDynamicTextAssetSerializerMetadata.h"

#include "Utilities/SGDynamicTextAssetEditorStatics.h"

#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SSGDynamicTextAssetCreateDialog"

void SSGDynamicTextAssetCreateDialog::Construct(const FArguments& InArgs)
{
    OnConfirm = InArgs._OnConfirm;
    OnCancel = InArgs._OnCancel;

    // Extension options are built after the widget tree is constructed so
    // ExtensionRow is valid when BuildExtensionOptions sets its visibility.

    // Set initial class selection
    SelectedClass = InArgs._InitialClass;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(400.0f)
        .Padding(16.0f)
        [
            SNew(SVerticalBox)

            // Title
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, 0.0f, 0.0f, 16.0f)
            [
                SNew(STextBlock)
                .Text(INVTEXT("Create New Dynamic Text Asset"))
                .Justification(ETextJustify::Center)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            ]

            // Class selection (searchable picker)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(INVTEXT("Class:"))
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(ClassPicker, SSGDynamicTextAssetClassPicker)
                    .InitialClass(InArgs._InitialClass)
                    .bShowNoneOption(false)
                    .OnClassSelected(this, &SSGDynamicTextAssetCreateDialog::OnClassPicked)
                ]
            ]

            // Name input
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(INVTEXT("Name:"))
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SBox)
                    [
                        SAssignNew(NameTextBox, SEditableTextBox)
                        .HintText(INVTEXT("Enter a unique name..."))
                        .OnTextChanged(this, &SSGDynamicTextAssetCreateDialog::OnNameTextChanged)
                        .OnTextCommitted(this, &SSGDynamicTextAssetCreateDialog::OnNameTextCommitted)
                        .SelectAllTextOnCommit(true)
                    ]
                ]
            ]

            // Format (extension) selection to hidden when only one serializer is registered
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SAssignNew(ExtensionRow, SHorizontalBox)
                .Visibility(EVisibility::Collapsed)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                         .Text(INVTEXT("Format:"))
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(ExtensionComboBox, SComboBox<TSharedPtr<FSGDynamicTextAssetSerializerMetadata>>)
                    .OptionsSource(&ExtensionOptions)
                    .OnSelectionChanged(this, &SSGDynamicTextAssetCreateDialog::OnExtensionSelectionChanged)
                    .OnGenerateWidget(this, &SSGDynamicTextAssetCreateDialog::GenerateExtensionComboItem)
                    .InitiallySelectedItem(SelectedExtension.Pin())
                    [
                        SAssignNew(ExtensionDisplayText, STextBlock)
                        .Text(GetSelectedExtensionText())
                    ]
                ]
            ]

            // Error message
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 16.0f)
            [
                SAssignNew(ErrorText, STextBlock)
                .ColorAndOpacity(FLinearColor::Red)
                .Visibility(EVisibility::Collapsed)
            ]

            // Buttons
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            [
                SNew(SUniformGridPanel)
                .SlotPadding(FMargin(4.0f, 0.0f))

                + SUniformGridPanel::Slot(0, 0)
                [
                    SAssignNew(CreateButton, SButton)
                    .Text(INVTEXT("Create"))
                    .OnClicked(this, &SSGDynamicTextAssetCreateDialog::OnCreateClicked)
                    .IsEnabled(bIsNameValid)
                ]

                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .Text(INVTEXT("Cancel"))
                    .OnClicked(this, &SSGDynamicTextAssetCreateDialog::OnCancelClicked)
                ]
            ]
        ]
    ];

    // Sync SelectedClass from the picker's default selection (first class if no InitialClass)
    if (!SelectedClass.IsValid() && ClassPicker.IsValid())
    {
        SelectedClass = ClassPicker->GetSelectedClass();
    }

    // Build extension options after the widget tree is constructed so
    // ExtensionRow is valid when BuildExtensionOptions sets its visibility.
    BuildExtensionOptions();

    // Focus name text box
    USGDynamicTextAssetEditorStatics::FrameDelayedKeyboardFocusToWidget(NameTextBox);
}

void SSGDynamicTextAssetCreateDialog::OnClassPicked(UClass* NewClass)
{
    SelectedClass = NewClass;

    // Re-validate to update error text and bIsNameValid for the new class path
    OnNameTextChanged(FText::FromString(EnteredName));
}

void SSGDynamicTextAssetCreateDialog::BuildExtensionOptions()
{
    ExtensionOptions.Empty();

    TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> serializers = FSGDynamicTextAssetFileManager::GetAllRegisteredSerializers();

    for (const TSharedPtr<ISGDynamicTextAssetSerializer>& serializer : serializers)
    {
        TSharedPtr<FSGDynamicTextAssetSerializerMetadata> metadata = MakeShared<FSGDynamicTextAssetSerializerMetadata>();
        metadata->SerializerTypeId = serializer->GetSerializerTypeId();
        metadata->FileExtension = serializer->GetFileExtension();
        metadata->FormatName = serializer->GetFormatName();
        ExtensionOptions.Add(metadata);
    }

    // Try and favor default extension type
    SelectedExtension = nullptr;
    for (const TSharedPtr<FSGDynamicTextAssetSerializerMetadata>& metadata : ExtensionOptions)
    {
        if (metadata->FileExtension == FSGDynamicTextAssetFileManager::DEFAULT_TEXT_EXTENSION)
        {
            SelectedExtension = metadata.ToWeakPtr();
            break;
        }
    }

    // Or fall back to index 0 if it exists
    if (!SelectedExtension.Pin().IsValid() && ExtensionOptions.IsValidIndex(0))
    {
        SelectedExtension = ExtensionOptions[0];
    }

    // Refresh the combo box selection to match SelectedExtension
    if (ExtensionComboBox.IsValid())
    {
        ExtensionComboBox->SetSelectedItem(SelectedExtension.Pin());
    }

    if (ExtensionRow.IsValid())
    {
        ExtensionRow->SetVisibility(ExtensionOptions.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed);
    }

    if (ExtensionDisplayText.IsValid())
    {
        ExtensionDisplayText->SetText(GetSelectedExtensionText());
    }
}

TSharedRef<SWidget> SSGDynamicTextAssetCreateDialog::GenerateExtensionComboItem(TSharedPtr<FSGDynamicTextAssetSerializerMetadata> InExtension)
{
    FText displayText = INVTEXT("[INVALID EXTENSION]");
    if (InExtension.IsValid() && InExtension->IsValidId())
    {
        displayText = FText::Format(INVTEXT("{0} ({1})"),
            InExtension->FormatName,
            FText::FromString(InExtension->FileExtension));
    }
    return SNew(STextBlock)
        .Text(displayText);
}

void SSGDynamicTextAssetCreateDialog::OnExtensionSelectionChanged(TSharedPtr<FSGDynamicTextAssetSerializerMetadata> NewSelection, ESelectInfo::Type SelectInfo)
{
    SelectedExtension = NewSelection;

    if (ExtensionDisplayText.IsValid())
    {
        ExtensionDisplayText->SetText(GetSelectedExtensionText());
    }

    // Re-validate since the file path depends on extension
    OnNameTextChanged(FText::FromString(EnteredName));
}

FText SSGDynamicTextAssetCreateDialog::GetSelectedExtensionText() const
{
    if (SelectedExtension.Pin().IsValid() && SelectedExtension.Pin()->IsValidId())
    {
        return FText::Format(INVTEXT("{0} ({1})"),
            SelectedExtension.Pin()->FormatName,
            FText::FromString(SelectedExtension.Pin()->FileExtension));
    }
    return INVTEXT("Please select a format...");
}

FString SSGDynamicTextAssetCreateDialog::GetSelectedExtensionString() const
{
    if (SelectedExtension.Pin().IsValid() && SelectedExtension.Pin()->IsValidId())
    {
        return SelectedExtension.Pin()->FileExtension;
    }
    return FSGDynamicTextAssetFileManager::DEFAULT_TEXT_EXTENSION;
}

void SSGDynamicTextAssetCreateDialog::OnNameTextChanged(const FText& NewText)
{
    EnteredName = NewText.ToString();

    if (!SelectedClass.IsValid())
    {
        bIsNameValid = false;
        if (ErrorText.IsValid())
        {
            ErrorText->SetText(INVTEXT("Please select a class."));
            ErrorText->SetVisibility(EVisibility::Visible);
        }
        if (CreateButton.IsValid()) { CreateButton->SetEnabled(false); }
        return;
    }

    if (EnteredName.IsEmpty())
    {
        bIsNameValid = false;
        if (ErrorText.IsValid())
        {
            ErrorText->SetText(INVTEXT("Please enter a name."));
            ErrorText->SetVisibility(EVisibility::Visible);
        }
        if (CreateButton.IsValid()) { CreateButton->SetEnabled(false); }
        return;
    }

    // Check for invalid characters
    const FString sanitized = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(EnteredName);
    if (sanitized.IsEmpty())
    {
        bIsNameValid = false;
        if (ErrorText.IsValid())
        {
            ErrorText->SetText(INVTEXT("Name contains invalid characters."));
            ErrorText->SetVisibility(EVisibility::Visible);
        }
        if (CreateButton.IsValid()) { CreateButton->SetEnabled(false); }
        return;
    }

    // Check if name is already used globally (across all classes and file formats)
    if (IsNameAlreadyUsed(sanitized))
    {
        bIsNameValid = false;
        if (ErrorText.IsValid())
        {
            ErrorText->SetText(INVTEXT("A dynamic text asset with this name already exists."));
            ErrorText->SetVisibility(EVisibility::Visible);
        }
        if (CreateButton.IsValid()) { CreateButton->SetEnabled(false); }
        return;
    }

    // All validations passed
    bIsNameValid = true;
    if (ErrorText.IsValid())
    {
        ErrorText->SetVisibility(EVisibility::Collapsed);
    }

    if (CreateButton.IsValid())
    {
        CreateButton->SetEnabled(bIsNameValid);
    }
}

bool SSGDynamicTextAssetCreateDialog::ValidateInputs() const
{
    // Check for valid class
    if (!SelectedClass.IsValid())
    {
        return false;
    }
    // Check if the name is empty
    if (EnteredName.IsEmpty())
    {
        return false;
    }
    // Check for invalid characters
    const FString sanitized = FSGDynamicTextAssetFileManager::SanitizeUserFacingId(EnteredName);
    if (sanitized.IsEmpty())
    {
        return false;
    }
    // Check if name is already used globally
    if (IsNameAlreadyUsed(FSGDynamicTextAssetFileManager::SanitizeUserFacingId(EnteredName)))
    {
        return false;
    }

    // Respect the real-time validation result
    if (!bIsNameValid)
    {
        return false;
    }

    return true;
}

bool SSGDynamicTextAssetCreateDialog::IsCreateButtonEnabled() const
{
    return bIsNameValid;
}

bool SSGDynamicTextAssetCreateDialog::IsNameAlreadyUsed(const FString& Name) const
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
        FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
        if (metadata.bIsValid && metadata.UserFacingId.Equals(Name, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }

    return false;
}

void SSGDynamicTextAssetCreateDialog::OnNameTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter)
    {
        CommitNewDynamicTextAsset();
    }
}

FReply SSGDynamicTextAssetCreateDialog::OnCreateClicked()
{
    CommitNewDynamicTextAsset();

    return FReply::Handled();
}

FReply SSGDynamicTextAssetCreateDialog::OnCancelClicked()
{
    bWasConfirmed = false;
    OnCancel.ExecuteIfBound();

    // Close the window
    if (ParentWindow.IsValid())
    {
        ParentWindow.Pin()->RequestDestroyWindow();
    }

    return FReply::Handled();
}

bool SSGDynamicTextAssetCreateDialog::CommitNewDynamicTextAsset()
{
    if (!ValidateInputs())
    {
        return false;
    }
    // Create the file
    if (!FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile(SelectedClass.Get(),
        EnteredName, GetSelectedExtensionString(), CreatedId, CreatedFilePath))
    {
        ErrorText->SetText(INVTEXT("Failed to create dynamic text asset file."));
        ErrorText->SetVisibility(EVisibility::Visible);
        return false;
    }
    // Auto-add to source control
    if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        if (!FSGDynamicTextAssetSourceControl::MarkForAdd(CreatedFilePath))
        {
            const FString message = FString::Printf(TEXT("Failed to mark file for add in source control: %s"), *CreatedFilePath);
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("%s"), *message);
            FNotificationInfo notificationInfo(FText::Format(INVTEXT("{0}"), FText::FromString(message)));

            notificationInfo.ExpireDuration = 10.0f;
            notificationInfo.Image = FAppStyle::GetBrush("Icons.WarningWithColor");

            FSlateNotificationManager::Get().AddNotification(notificationInfo);

            // Continue anyway - file was created successfully
        }
    }

    bWasConfirmed = true;

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Created dynamic text asset: %s (ID: %s)"), *CreatedFilePath, *CreatedId.ToString());

    OnConfirm.ExecuteIfBound();

    // Close the window
    if (ParentWindow.IsValid())
    {
        ParentWindow.Pin()->RequestDestroyWindow();
    }

    return true;
}

UClass* SSGDynamicTextAssetCreateDialog::GetSelectedClass() const
{
    return SelectedClass.Get();
}

FString SSGDynamicTextAssetCreateDialog::GetEnteredName() const
{
    return EnteredName;
}

bool SSGDynamicTextAssetCreateDialog::ShowModal(UClass* InitialClass, FString& OutFilePath, FSGDynamicTextAssetId& OutId)
{
    TSharedRef<SSGDynamicTextAssetCreateDialog> dialog = SNew(SSGDynamicTextAssetCreateDialog)
        .InitialClass(InitialClass);

    TSharedRef<SWindow> window = SNew(SWindow)
        .Title(INVTEXT("Create Dynamic Text Asset"))
        .SizingRule(ESizingRule::Autosized)
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        [
            dialog
        ];

    dialog->ParentWindow = window;

    FSlateApplication::Get().AddModalWindow(window, FSlateApplication::Get().GetActiveTopLevelWindow());

    if (dialog->bWasConfirmed)
    {
        OutFilePath = dialog->GetCreatedFilePath();
        OutId = dialog->GetCreatedId();
        return true;
    }

    return false;
}

#undef LOCTEXT_NAMESPACE
