// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SSGDynamicTextAssetIcon.h"

#include "SlateOptMacros.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

void SSGDynamicTextAssetIcon::Construct(const FArguments& InArgs)
{
	switch (InArgs._ReferenceType)
	{
	case ESGReferenceType::Blueprint:
		{
			ConstructAssetIcon(InArgs);
			break;
		}
	case ESGReferenceType::Level:
		{
			ConstructLevelIcon(InArgs);
			break;
		}
	case ESGReferenceType::DynamicTextAsset:
		{
			ConstructDynamicTextAssetIcon(InArgs);
			break;
		}
	default:
		{
			ConstructErrorIcon();
			break;
		}
	}
}

void SSGDynamicTextAssetIcon::ConstructErrorIcon()
{
	ChildSlot
	[
		SNew(SImage)
		.Image(FAppStyle::GetBrush("Icons.ErrorWithColor"))
	];
}

void SSGDynamicTextAssetIcon::ConstructDynamicTextAssetIcon(const FArguments& InArgs)
{
	TWeakPtr<ISGDynamicTextAssetSerializer> weakSerializer = InArgs._Serializer;

	if (!weakSerializer.Pin().IsValid())
	{
		ConstructErrorIcon();
		return;
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		.ToolTipText(weakSerializer.Pin()->GetFormatName())

		// Type Icon
		+ SVerticalBox::Slot()
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SImage)
				.Image(weakSerializer.Pin()->GetIconBrush())
				.ColorAndOpacity(InArgs._IconColorOverride.Get(FSlateColor::UseForeground()))
			]
		]

		// Type Label
		+ SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(weakSerializer.Pin()->GetFormatName())
			.Font(InArgs._FontStyle)
			.Justification(ETextJustify::Center)
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.ColorAndOpacity(InArgs._LabelColorOverride.Get(FSlateColor::UseForeground()))
		]
	];
}

void SSGDynamicTextAssetIcon::ConstructLevelIcon(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Type Icon
		+ SVerticalBox::Slot()
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Level"))
				.ColorAndOpacity(InArgs._IconColorOverride.Get(
					FAppStyle::Get().GetColor("LevelEditor.AssetColor")))
			]
		]

		// Type Label
		+ SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(InArgs._Label.IsEmpty() ? INVTEXT("Level") : InArgs._Label)
			.Font(InArgs._FontStyle)
			.Justification(ETextJustify::Center)
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.ColorAndOpacity(InArgs._LabelColorOverride.Get(
				FAppStyle::Get().GetColor("LevelEditor.AssetColor")))
		]
	];
}

void SSGDynamicTextAssetIcon::ConstructAssetIcon(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Type Icon
		+ SVerticalBox::Slot()
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SImage)
					.Image(InArgs._IconBrushOverride.Get(
						FAppStyle::GetBrush("ClassIcon.Object")))
					.ColorAndOpacity(InArgs._IconColorOverride.Get(FSlateColor::UseForeground()))
			]
		]

		// Type Label
		+ SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(InArgs._Label.IsEmpty() ? INVTEXT("Asset") : InArgs._Label)
			.Font(InArgs._FontStyle)
			.Justification(ETextJustify::Center)
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.ColorAndOpacity(InArgs._LabelColorOverride.Get(FLinearColor::White))
		]
	];
}
