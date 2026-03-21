// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SSGDynamicTextAssetIdentityBlock.h"
#include "Editor/SGDynamicTextAssetIdentityCustomization.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Layout/SSpacer.h"

namespace SSGDynamicTextAssetIdentityBlockInternals
{
	constexpr float NameFillWidth = 0.2f;
	constexpr float ValueFillWidth = (1.0f - NameFillWidth);
	constexpr float ButtonRightPadding = 6.0;
}

void SSGDynamicTextAssetIdentityBlock::Construct(const FArguments& InArgs)
{
	TSharedRef<SVerticalBox> verticalBox = SNew(SVerticalBox)

		// User Facing ID Row
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			CreateRowBorders(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::NameFillWidth)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(INVTEXT("User Facing ID"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::ValueFillWidth)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					// Copy Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, SSGDynamicTextAssetIdentityBlockInternals::ButtonRightPadding, 0.0f)
					[
						FSGDynamicTextAssetIdentityCustomization::CreateCopyButton(
							FOnClicked::CreateLambda([this]() -> FReply
							{
								if (UserFacingIdTextblock.IsValid() && !UserFacingIdTextblock->GetText().IsEmpty())
								{
									FPlatformApplicationMisc::ClipboardCopy(*UserFacingIdTextblock->GetText().ToString());
									UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied User Facing ID to clipboard."));
								}
								return FReply::Handled();
							}),
							INVTEXT("Copy User Facing ID to clipboard.")
						)
					]
					// Text Value
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(UserFacingIdTextblock, STextBlock)
						.Text(InArgs._UserFacingId)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				])
		]

		// Dynamic Text Asset ID Row
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 2.0f)
		[
			CreateRowBorders(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::NameFillWidth)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(INVTEXT("Dynamic Text Asset ID"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::ValueFillWidth)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					// Copy Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, SSGDynamicTextAssetIdentityBlockInternals::ButtonRightPadding, 0.0f)
					[
						FSGDynamicTextAssetIdentityCustomization::CreateCopyButton(
							FOnClicked::CreateLambda([this]() -> FReply
							{
								if (IdTextblock.IsValid() && !IdTextblock->GetText().IsEmpty())
								{
									FPlatformApplicationMisc::ClipboardCopy(*IdTextblock->GetText().ToString());
									UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied ID to clipboard."));
								}
								return FReply::Handled();
							}),
							INVTEXT("Copy ID to clipboard.")
						)
					]
					// Text Value
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(IdTextblock, STextBlock)
						.Text(InArgs._DynamicTextAssetId)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				])
		]

		// Version Row
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			CreateRowBorders(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::NameFillWidth)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(INVTEXT("Version"))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(INVTEXT("(Major.Minor.Patch)"))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::ValueFillWidth)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					// Copy Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, SSGDynamicTextAssetIdentityBlockInternals::ButtonRightPadding, 0.0f)
					[
						FSGDynamicTextAssetIdentityCustomization::CreateCopyButton(
							FOnClicked::CreateLambda([this]() -> FReply
							{
								if (VersionTextblock.IsValid() && !VersionTextblock->GetText().IsEmpty())
								{
									FPlatformApplicationMisc::ClipboardCopy(*VersionTextblock->GetText().ToString());
									UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied Version to clipboard."));
								}
								return FReply::Handled();
							}),
							INVTEXT("Copy Version information to clipboard.")
						)
					]
					// Text Value
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(VersionTextblock, STextBlock)
						.Text(InArgs._Version)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				])
		];

	// Optional collapsible File Format Version section (only shown when data is provided)
	if (!InArgs._FileFormatVersion.IsEmpty())
	{
		verticalBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SAssignNew(CollapsibleArea, SExpandableArea)
				.InitiallyCollapsed(true)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.BorderBackgroundColor(FLinearColor::Transparent)
				.HeaderPadding(FMargin(0.0f))
				.Padding(FMargin(20.0f, 2.0f, 0.0f, 0.0f))
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(INVTEXT("Advanced"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.BodyContent()
				[
					CreateRowBorders(
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::NameFillWidth)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							.ToolTipText(INVTEXT("The file format version that this DTA file type is using.\nMay or may not be up to date with the file type's file format version."))

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(INVTEXT("File Format Version"))
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(INVTEXT("(Major.Minor.Patch)"))
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth(SSGDynamicTextAssetIdentityBlockInternals::ValueFillWidth)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							// Copy Button
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, SSGDynamicTextAssetIdentityBlockInternals::ButtonRightPadding, 0.0f)
							[
								FSGDynamicTextAssetIdentityCustomization::CreateCopyButton(
									FOnClicked::CreateLambda([this]() -> FReply
									{
										if (FileFormatVersionTextblock.IsValid() && !FileFormatVersionTextblock->GetText().IsEmpty())
										{
											FPlatformApplicationMisc::ClipboardCopy(*FileFormatVersionTextblock->GetText().ToString());
											UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied File Format Version to clipboard."));
										}
										return FReply::Handled();
									}),
									INVTEXT("Copy File Format Version to clipboard.")
								)
							]

							// Text Value
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SAssignNew(FileFormatVersionTextblock, STextBlock)
								.Text(InArgs._FileFormatVersion)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							]
						])
				]
			];
	}

	ChildSlot
	[
		verticalBox
	];
}

void SSGDynamicTextAssetIdentityBlock::SetIdentityProperties(const FText& InDynamicTextAssetId, const FText& InUserFacingId,
	const FText& InVersion, const FText& InFileFormatVersion)
{
	if (IdTextblock.IsValid())
	{
		IdTextblock->SetText(InDynamicTextAssetId);
	}
	if (UserFacingIdTextblock.IsValid())
	{
		UserFacingIdTextblock->SetText(InUserFacingId);
	}
	if (VersionTextblock.IsValid())
	{
		VersionTextblock->SetText(InVersion);
	}
	if (FileFormatVersionTextblock.IsValid() && !InFileFormatVersion.IsEmpty())
	{
		FileFormatVersionTextblock->SetText(InFileFormatVersion);
	}
}

TSharedRef<SWidget> SSGDynamicTextAssetIdentityBlock::CreateRowBorders(TSharedRef<SWidget> Child)
{
	return SNew(SBox)
			.Padding(FMargin(0.0f,0.0f,0.0f,1.0f))
			.Clipping(EWidgetClipping::ClipToBounds)
			.MinDesiredHeight(PROPERTY_ROW_HEIGHT)
			[
				SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						Child
					]
			];
}
