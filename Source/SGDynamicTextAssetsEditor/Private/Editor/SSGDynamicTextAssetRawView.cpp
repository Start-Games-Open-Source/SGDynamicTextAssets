// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SSGDynamicTextAssetRawView.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "Editor/SSGDynamicTextAssetIdentityBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SSGDynamicTextAssetRawView"

void SSGDynamicTextAssetRawView::Construct(const FArguments& InArgs)
{
    OnRefreshRequested = InArgs._OnRefreshRequested;

    if (!InArgs._JsonText.IsEmpty())
    {
        CurrentText = InArgs._JsonText;
        TextFormat = ERawTextFormat::JSON;
    }
    else
    {
        CurrentText = InArgs._RawText;
        TextFormat = ERawTextFormat::Raw;
    }

    ChildSlot
    .Padding(0)
    [
        SNew(SVerticalBox)

        // Header Section
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(16.0f, 4.0f))
        [
            SNew(SVerticalBox)

            // Identity Properties
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SAssignNew(IdentityBlock, SSGDynamicTextAssetIdentityBlock)
                    .DynamicTextAssetId(InArgs._DynamicTextAssetId)
                    .UserFacingId(InArgs._UserFacingId)
                    .Version(InArgs._Version)
                    .FileFormatVersion(InArgs._FileFormatVersion)
                ]
            ]

        ]

        // Main Raw View Section
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(FMargin(0.0f, 0.0f, 4.0f, 4.0f))
        [
            SNew(SOverlay)

            // Background Raw Text
            + SOverlay::Slot()
            [
                SAssignNew(TextBox, SMultiLineEditableTextBox)
                .Text(CurrentText)
                .IsReadOnly(true)
                .AlwaysShowScrollbars(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
            ]

            // Floating Refresh + Copy Buttons
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            .Padding(FMargin(0.0f, 5.0f, 22.5f, 0.0f)) // Offset from scrollbars and text bounds
            [
                SNew(SHorizontalBox)

                // Refresh Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
                    .ContentPadding(FMargin(8.0f))
                    .ToolTipText(INVTEXT("Reload file contents from disk"))
                    .OnClicked(this, &SSGDynamicTextAssetRawView::OnRefreshButtonClicked)
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Refresh"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                ]

                // Copy Button
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
                    .ContentPadding(FMargin(8.0f))
                    .ToolTipText(FText::Format(INVTEXT("Copy {0} Text to Clipboard"), GetFormatText()))
                    .OnClicked(this, &SSGDynamicTextAssetRawView::OnCopyButtonClicked)
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("GenericCommands.Copy"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                ]
            ]
        ]
    ];
}

void SSGDynamicTextAssetRawView::SetJSONText(const FText& InJSONText)
{
    // Currently its directly inputted in, routing through this function incase intended behavior changes.
    SetRawText(InJSONText);
}

void SSGDynamicTextAssetRawView::SetRawText(const FText& InJsonText)
{
    CurrentText = InJsonText;
    if (TextBox.IsValid())
    {
        TextBox->SetText(InJsonText);
    }
}

void SSGDynamicTextAssetRawView::SetIdentityProperties(const FText& InDynamicTextAssetId, const FText& InUserFacingId, const FText& InVersion)
{
    if (IdentityBlock.IsValid())
    {
        IdentityBlock->SetIdentityProperties(InDynamicTextAssetId, InUserFacingId, InVersion);
    }
}

FText SSGDynamicTextAssetRawView::GetFormatText() const
{
    switch (TextFormat)
    {
    case ERawTextFormat::JSON:
        {
            return INVTEXT("JSON");
        }
    default:
        {
            return INVTEXT("Raw");
        }
    }
}

FReply SSGDynamicTextAssetRawView::OnCopyButtonClicked()
{
    if (!CurrentText.IsEmpty())
    {
        FPlatformApplicationMisc::ClipboardCopy(*CurrentText.ToString());
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied Raw JSON from Raw View tab to clipboard."));
    }
    return FReply::Handled();
}

FReply SSGDynamicTextAssetRawView::OnRefreshButtonClicked()
{
    OnRefreshRequested.ExecuteIfBound();
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
