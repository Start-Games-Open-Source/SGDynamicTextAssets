// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SMultiLineEditableTextBox;
class SButton;

/**
 * A read-only raw text data viewer tab for the standalone dynamic text asset editor.
 * Displays the core identity properties at the top, and the raw text payload in a scrolling text box below.
 *
 * Currently supported text file types:
 * - JSON
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetRawView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetRawView)
        : _DynamicTextAssetId(FText::GetEmpty())
        , _UserFacingId(FText::GetEmpty())
        , _Version(FText::GetEmpty())
        , _JsonText(FText::GetEmpty())
        , _RawText(FText::GetEmpty())
    {}

        /** The unique GUID of the dynamic text asset. */
        SLATE_ARGUMENT(FText, DynamicTextAssetId)

        /** The human-readable name of the dynamic text asset. */
        SLATE_ARGUMENT(FText, UserFacingId)

        /** The data schema version. */
        SLATE_ARGUMENT(FText, Version)

        /** The raw JSON payload text. */
        SLATE_ARGUMENT(FText, JsonText)

        /** The raw payload text. */
        SLATE_ARGUMENT(FText, RawText)

        /**
         * The file format version (e.g., "2.0.0").
         * When empty, the collapsible section is not shown.
         */
        SLATE_ARGUMENT(FText, FileFormatVersion)

        /** Called when the user clicks the Refresh button to reload file contents from disk. */
        SLATE_EVENT(FSimpleDelegate, OnRefreshRequested)

    SLATE_END_ARGS()

    enum class ESGDTARawTextFormat
    {
        // Custom or raw text
        Raw,
        JSON
    };

    /** Constructs this widget with InArgs */
    void Construct(const FArguments& InArgs);

    /** Updates the displayed raw JSON block. */
    void SetJSONText(const FText& InJSONText);

    /** Updates the displayed raw text block. */
    void SetRawText(const FText& InRawText);

    /** Updates the identity properties displayed in the header. */
    void SetIdentityProperties(const FText& InDynamicTextAssetId, const FText& InUserFacingId, const FText& InVersion);

    /** Retrieves the text format type that this raw view uses. */
    ESGDTARawTextFormat GetTextFormat() const { return TextFormat; }

private:

    /** Provides the text version of the Text Format enum. */
    FText GetFormatText() const;

    /** Handler for the floating Copy button inside the text view. */
    FReply OnCopyButtonClicked();

    /** Handler for the floating Refresh button inside the text view. */
    FReply OnRefreshButtonClicked();

    /** The text format type that this raw view uses. */
    ESGDTARawTextFormat TextFormat = ESGDTARawTextFormat::Raw;

    /** Current full text, stored to allow copying. */
    FText CurrentText;

    /** Pointer to the underlying text box so we can update its contents later if needed. */
    TSharedPtr<SMultiLineEditableTextBox> TextBox = nullptr;

    /** Property text blocks */
    TSharedPtr<class SSGDynamicTextAssetIdentityBlock> IdentityBlock = nullptr;

    /** Delegate fired when the Refresh button is clicked */
    FSimpleDelegate OnRefreshRequested;
};
