// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class STextBlock;

/**
 * A reusable identity block widget that displays the Dynamic Text Asset ID, User Facing ID,
 * and Version exactly as they appear in the Details Panel.
 *
 * Displays identity information in this order:
 * - User Facing ID
 * - Dynamic Text Asset ID
 * - Version
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetIdentityBlock : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSGDynamicTextAssetIdentityBlock)
		: _DynamicTextAssetId(FText::GetEmpty())
		, _UserFacingId(FText::GetEmpty())
		, _Version(FText::GetEmpty())
	{}

		/** The unique GUID of the dynamic text asset. */
		SLATE_ARGUMENT(FText, DynamicTextAssetId)

		/** The human-readable name of the dynamic text asset. */
		SLATE_ARGUMENT(FText, UserFacingId)

		/** The data schema version. */
		SLATE_ARGUMENT(FText, Version)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Updates the properties displayed. */
	void SetIdentityProperties(const FText& InDynamicTextAssetId, const FText& InUserFacingId, const FText& InVersion);

	/**
	 * Copied from PropertyEditorConstants::PropertyRowHeight
	 * The row height for this property.
	 */
	static constexpr float PROPERTY_ROW_HEIGHT = 26.0f;

private:

	TSharedRef<SWidget> CreateRowBorders(TSharedRef<SWidget> Child);

	TSharedPtr<STextBlock> IdTextblock = nullptr;
	TSharedPtr<STextBlock> UserFacingIdTextblock = nullptr;
	TSharedPtr<STextBlock> VersionTextblock = nullptr;
};
