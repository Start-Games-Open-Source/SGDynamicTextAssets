// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAssetEnums.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"

class ISGDynamicTextAssetSerializer;

/**
 * A reusable simple icon widget for displaying a serializer's Icon and Format Name.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetIcon : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSGDynamicTextAssetIcon)
		: _Serializer(nullptr)
		, _FontStyle(FCoreStyle::GetDefaultFontStyle("Italic", 8))
		, _ReferenceType(ESGReferenceType::DynamicTextAsset)
		, _Label(FText::GetEmpty())
	{}

		/** For Dynamic Text Asset's only, the serializer to receive information from for this widget. */
		SLATE_ARGUMENT(TWeakPtr<ISGDynamicTextAssetSerializer>, Serializer)

		/** Sets the font used to draw the label. */
		SLATE_ARGUMENT(FSlateFontInfo, FontStyle)

		/** The object type that this icon will use. */
		SLATE_ARGUMENT(ESGReferenceType, ReferenceType)

		/** Label override of the icon for non-Dynamic Text Asset types. */
		SLATE_ARGUMENT(FText, Label)

		/** Icon brush override for non-Dynamic Text Asset types. */
		SLATE_ARGUMENT(TOptional<const FSlateBrush*>, IconBrushOverride)

		/** Color override of the icon. */
		SLATE_ARGUMENT(TOptional<FSlateColor>, IconColorOverride)

		/** Color override of the label. */
		SLATE_ARGUMENT(TOptional<FSlateColor>, LabelColorOverride)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

protected:

	void ConstructErrorIcon();
	void ConstructDynamicTextAssetIcon(const FArguments& InArgs);
	void ConstructLevelIcon(const FArguments& InArgs);
	void ConstructAssetIcon(const FArguments& InArgs);
};
