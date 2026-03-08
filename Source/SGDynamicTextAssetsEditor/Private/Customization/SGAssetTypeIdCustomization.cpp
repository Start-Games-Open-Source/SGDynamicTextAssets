// Copyright Start Games, Inc. All Rights Reserved.

#include "Customization/SGAssetTypeIdCustomization.h"

#include "DetailWidgetRow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/Reply.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "Widgets/SSGDynamicTextAssetClassPicker.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGAssetTypeIdCustomization"

TSharedRef<IPropertyTypeCustomization> FSGAssetTypeIdCustomization::MakeInstance()
{
	return MakeShareable(new FSGAssetTypeIdCustomization());
}

void FSGAssetTypeIdCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = PropertyHandle;

	// Read current value to resolve initial class
	ReadCurrentValue();

	// Resolve the initial class from the current type ID
	UClass* initialClass = nullptr;
	if (CurrentTypeId.IsValid())
	{
		if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
		{
			initialClass = registry->ResolveClassForTypeId(CurrentTypeId);
		}
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(500.0f)
		[
			SNew(SHorizontalBox)

			// Searchable class picker
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ClassPicker, SSGDynamicTextAssetClassPicker)
				.InitialClass(initialClass)
				.bShowNoneOption(true)
				.OnClassSelected(this, &FSGAssetTypeIdCustomization::OnClassPicked)
			]

			// Copy ID button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(CopyIdButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Copy Asset Type ID to clipboard"))
				.OnClicked(this, &FSGAssetTypeIdCustomization::OnCopyIdClicked)
				.IsEnabled(CurrentTypeId.IsValid())
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("GenericCommands.Copy"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Paste ID button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Paste Asset Type ID from clipboard"))
				.OnClicked(this, &FSGAssetTypeIdCustomization::OnPasteClicked)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("GenericCommands.Paste"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Clear button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(ClearButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Clear type selection"))
				.OnClicked(this, &FSGAssetTypeIdCustomization::OnClearClicked)
				.IsEnabled(CurrentTypeId.IsValid())
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.X"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}

void FSGAssetTypeIdCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Hide all children - the header handles everything
}

void FSGAssetTypeIdCustomization::OnClassPicked(UClass* NewClass)
{
	FSGDynamicTextAssetTypeId newTypeId = FSGDynamicTextAssetTypeId::INVALID_TYPE_ID;

	if (NewClass)
	{
		if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
		{
			newTypeId = registry->GetTypeIdForClass(NewClass);
		}
	}

	WriteValue(newTypeId);

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Asset type customization: picked %s (%s)"),
		NewClass ? *NewClass->GetName() : TEXT("None"),
		*newTypeId.ToString());
}

FText FSGAssetTypeIdCustomization::GetCurrentDisplayText() const
{
	if (!CurrentTypeId.IsValid())
	{
		return INVTEXT("None");
	}

	if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
	{
		if (UClass* resolvedClass = registry->ResolveClassForTypeId(CurrentTypeId))
		{
			return resolvedClass->GetDisplayNameText();
		}
	}

	// Fallback to displaying the raw GUID
	return FText::FromString(CurrentTypeId.ToString());
}

FText FSGAssetTypeIdCustomization::GetCurrentTooltipText() const
{
	if (!CurrentTypeId.IsValid())
	{
		return INVTEXT("No asset type selected");
	}

	FText displayName = INVTEXT("Unknown");
	FString className = TEXT("Unknown");
	if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
	{
		if (UClass* resolvedClass = registry->ResolveClassForTypeId(CurrentTypeId))
		{
			displayName = resolvedClass->GetDisplayNameText();
			className = resolvedClass->GetName();
		}
	}

	return FText::Format(INVTEXT("{0}\nClass: {1}\nAsset Type ID: {2}"),
		displayName,
		FText::FromString(className),
		FText::FromString(CurrentTypeId.ToString()));
}

FReply FSGAssetTypeIdCustomization::OnClearClicked()
{
	WriteValue(FSGDynamicTextAssetTypeId::INVALID_TYPE_ID);

	// Update the picker to show None
	if (ClassPicker.IsValid())
	{
		ClassPicker->SetSelectedClass(nullptr);
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Cleared asset type ID"));
	return FReply::Handled();
}

FReply FSGAssetTypeIdCustomization::OnCopyIdClicked()
{
	if (CurrentTypeId.IsValid())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CurrentTypeId.ToString());
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Copied Asset Type ID to clipboard: %s"), *CurrentTypeId.ToString());
	}
	return FReply::Handled();
}

FReply FSGAssetTypeIdCustomization::OnPasteClicked()
{
	// Read text from system clipboard
	FString clipboardText;
	FPlatformApplicationMisc::ClipboardPaste(clipboardText);
	clipboardText.TrimStartAndEndInline();

	if (clipboardText.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Paste failed: clipboard is empty"));
		return FReply::Handled();
	}

	// Try to parse as a GUID
	FSGDynamicTextAssetTypeId parsedTypeId = FSGDynamicTextAssetTypeId::FromString(clipboardText);
	if (parsedTypeId.IsValid())
	{
		WriteValue(parsedTypeId);

		// Update the picker to reflect the pasted value
		if (ClassPicker.IsValid())
		{
			UClass* resolvedClass = nullptr;
			if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
			{
				resolvedClass = registry->ResolveClassForTypeId(parsedTypeId);
			}
			ClassPicker->SetSelectedClass(resolvedClass);
		}

		if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
		{
			if (registry->ResolveClassForTypeId(parsedTypeId))
			{
				UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Pasted Asset Type ID: %s"), *parsedTypeId.ToString());
				return FReply::Handled();
			}
		}

		// Valid GUID but not found in registry
		{
			const FString message = FString::Printf(TEXT("Pasted Asset Type ID (%s),\nnot found in current registry"), *parsedTypeId.ToString());
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("%s"), *message);
			FNotificationInfo info(FText::FromString(message));
			info.ExpireDuration = 3.0f;
			info.Image = FAppStyle::GetBrush("Icons.WarningWithColor");
			FSlateNotificationManager::Get().AddNotification(info);
		}
		return FReply::Handled();
	}

	{
		const FString message = FString::Printf(TEXT("Paste failed: clipboard content\n'%s' is not a valid Asset Type ID"), *clipboardText);
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("%s"), *message);
		FNotificationInfo info(FText::FromString(message));
		info.ExpireDuration = 3.0f;
		info.Image = FAppStyle::GetBrush("Icons.ErrorWithColor");
		FSlateNotificationManager::Get().AddNotification(info);
	}

	return FReply::Handled();
}

void FSGAssetTypeIdCustomization::ReadCurrentValue()
{
	CurrentTypeId = FSGDynamicTextAssetTypeId::INVALID_TYPE_ID;

	if (!StructPropertyHandle.IsValid())
	{
		return;
	}

	// Read the FSGDynamicTextAssetTypeId struct directly via raw data access
	TArray<void*> rawData;
	StructPropertyHandle->AccessRawData(rawData);

	if (rawData.Num() > 0 && rawData[0] != nullptr)
	{
		CurrentTypeId = *static_cast<FSGDynamicTextAssetTypeId*>(rawData[0]);
	}

	RefreshButtonStates();
}

void FSGAssetTypeIdCustomization::WriteValue(const FSGDynamicTextAssetTypeId& NewTypeId)
{
	if (!StructPropertyHandle.IsValid())
	{
		return;
	}

	StructPropertyHandle->NotifyPreChange();

	TArray<void*> rawData;
	StructPropertyHandle->AccessRawData(rawData);

	if (rawData.Num() > 0 && rawData[0] != nullptr)
	{
		*static_cast<FSGDynamicTextAssetTypeId*>(rawData[0]) = NewTypeId;
	}

	StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	CurrentTypeId = NewTypeId;

	RefreshButtonStates();
}

void FSGAssetTypeIdCustomization::RefreshButtonStates()
{
	const bool bHasValidId = CurrentTypeId.IsValid();

	if (CopyIdButton.IsValid())
	{
		CopyIdButton->SetEnabled(bHasValidId);
	}

	if (ClearButton.IsValid())
	{
		ClearButton->SetEnabled(bHasValidId);
	}
}

#undef LOCTEXT_NAMESPACE
