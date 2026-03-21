// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/FSGDynamicTextAssetIdCustomization.h"

#include "Core/SGDynamicTextAssetId.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Input/Reply.h"

TSharedRef<IPropertyTypeCustomization> FSGDynamicTextAssetIdCustomization::MakeInstance()
{
	return MakeShareable(new FSGDynamicTextAssetIdCustomization());
}

void FSGDynamicTextAssetIdCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = PropertyHandle;

	HeaderRow
		.NameContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)

			// Property name label
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyHandle->CreatePropertyNameWidget()
			]

			// Spacer
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]

			// Copy button (on the name side, before the separator)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &FSGDynamicTextAssetIdCustomization::OnCopyClicked)
				.ToolTipText(INVTEXT("Copy ID to clipboard"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("GenericCommands.Copy"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]
		.ValueContent()
		.MinDesiredWidth(300.0f)
		.VAlign(VAlign_Center)
		[
			// Read-only ID text
			SNew(STextBlock)
			.Text(this, &FSGDynamicTextAssetIdCustomization::GetIdDisplayText)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		];
}

void FSGDynamicTextAssetIdCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// No children exposed  - the ID is displayed inline in the header row
}

FText FSGDynamicTextAssetIdCustomization::GetIdDisplayText() const
{
	if (!StructPropertyHandle.IsValid() || !StructPropertyHandle->IsValidHandle())
	{
		return INVTEXT("Invalid");
	}

	// Read the FSGDynamicTextAssetId struct directly via raw data access
	TArray<void*> rawData;
	StructPropertyHandle->AccessRawData(rawData);

	if (rawData.Num() > 0 && rawData[0] != nullptr)
	{
		const FSGDynamicTextAssetId& dataObjectId = *static_cast<FSGDynamicTextAssetId*>(rawData[0]);
		if (dataObjectId.IsValid())
		{
			return FText::FromString(dataObjectId.ToString());
		}
	}

	return INVTEXT("None");
}

FReply FSGDynamicTextAssetIdCustomization::OnCopyClicked()
{
	const FText displayText = GetIdDisplayText();
	FPlatformApplicationMisc::ClipboardCopy(*displayText.ToString());
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied SGDynamicTextAssetId to clipboard: %s"), *displayText.ToString());
	return FReply::Handled();
}
