// Copyright Start Games, Inc. All Rights Reserved.

#include "Customization/SGDTAClassIdCustomization.h"

#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "DetailWidgetRow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/Reply.h"
#include "Management/SGDTAExtenderManifest.h"
#include "Management/SGDTASerializerExtenderRegistry.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "SGDTAEditorLogs.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGDTAClassIdCustomization"

/**
 * Class viewer filter that restricts displayed classes to subclasses
 * of a given base class. Used by FSGDTAClassIdCustomization to filter
 * the class picker based on the SGDTAClassType meta tag.
 */
class FSGDTAClassIdClassFilter : public IClassViewerFilter
{
public:

	/** The base class that all displayed classes must derive from */
	const UClass* AllowedBaseClass = nullptr;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
		const UClass* InClass,
		TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		if (!AllowedBaseClass || !InClass)
		{
			return false;
		}
		return InClass->IsChildOf(AllowedBaseClass) && InClass != AllowedBaseClass;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
		const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData,
		TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		if (!AllowedBaseClass)
		{
			return false;
		}
		return InUnloadedClassData->IsChildOf(AllowedBaseClass);
	}
};

TSharedRef<IPropertyTypeCustomization> FSGDTAClassIdCustomization::MakeInstance()
{
	return MakeShareable(new FSGDTAClassIdCustomization());
}

void FSGDTAClassIdCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = PropertyHandle;

	// Read current value
	ReadCurrentValue();

	// Resolve the base class filter from the SGDTAClassType meta tag
	FilterBaseClass = UObject::StaticClass();
	if (const FProperty* property = PropertyHandle->GetProperty())
	{
		if (property->HasMetaData(TEXT("SGDTAClassType")))
		{
			const FString baseClassName = property->GetMetaData(TEXT("SGDTAClassType"));
			if (!baseClassName.IsEmpty())
			{
				if (UClass* foundClass = FindFirstObject<UClass>(*baseClassName, EFindFirstObjectOptions::NativeFirst))
				{
					FilterBaseClass = foundClass;
				}
				else
				{
					UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
						TEXT("SGDTAClassType meta tag specifies '%s' but the class was not found. Defaulting to UObject."),
						*baseClassName);
				}
			}
		}
	}

	// Set up the class viewer options
	TSharedRef<FSGDTAClassIdClassFilter> classFilter = MakeShared<FSGDTAClassIdClassFilter>();
	classFilter->AllowedBaseClass = FilterBaseClass;

	FClassViewerInitializationOptions classViewerOptions;
	classViewerOptions.Mode = EClassViewerMode::ClassPicker;
	classViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	classViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	classViewerOptions.bShowNoneOption = true;
	classViewerOptions.bShowUnloadedBlueprints = true;
	classViewerOptions.ClassFilters.Add(classFilter);

	// Create the class viewer widget for use in the combo button menu
	FClassViewerModule& classViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	TSharedRef<SWidget> classViewerWidget = classViewerModule.CreateClassViewer(
		classViewerOptions,
		FOnClassPicked::CreateRaw(this, &FSGDTAClassIdCustomization::OnClassPicked));

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

			// Class picker combo button
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ClassPickerCombo, SComboButton)
				.OnGetMenuContent_Lambda([classViewerWidget]()
				{
					return SNew(SBox)
						.MinDesiredWidth(300.0f)
						.MaxDesiredHeight(400.0f)
						[
							classViewerWidget
						];
				})
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FSGDTAClassIdCustomization::GetCurrentDisplayText)
					.ToolTipText(this, &FSGDTAClassIdCustomization::GetCurrentTooltipText)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				]
			]

			// Copy ID button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(CopyIdButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Copy Class ID to clipboard"))
				.OnClicked(this, &FSGDTAClassIdCustomization::OnCopyIdClicked)
				.IsEnabled(CurrentClassId.IsValid())
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
				.ToolTipText(INVTEXT("Paste Class ID from clipboard"))
				.OnClicked(this, &FSGDTAClassIdCustomization::OnPasteClicked)
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
				.ToolTipText(INVTEXT("Clear class ID selection"))
				.OnClicked(this, &FSGDTAClassIdCustomization::OnClearClicked)
				.IsEnabled(CurrentClassId.IsValid())
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.X"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}

void FSGDTAClassIdCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Hide all children - the header handles everything
}

void FSGDTAClassIdCustomization::OnClassPicked(UClass* NewClass)
{
	if (NewClass)
	{
		// Resolve UClass -> FSGDTAClassId via the extender registry
		if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
		{
			const FSGDTAClassId resolvedId =
				registry->GetExtenderRegistry().FindExtenderIdByClass(NewClass);

			if (resolvedId.IsValid())
			{
				WriteValue(resolvedId);
			}
			else
			{
				UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
					TEXT("Class(%s) is not registered in any extender manifest. Cannot assign ClassId."),
					*NewClass->GetName());
			}
		}
	}
	else
	{
		WriteValue(FSGDTAClassId::INVALID_CLASS_ID);
	}

	if (ClassPickerCombo.IsValid())
	{
		ClassPickerCombo->SetIsOpen(false);
	}
}

FText FSGDTAClassIdCustomization::GetCurrentDisplayText() const
{
	if (!CurrentClassId.IsValid())
	{
		return INVTEXT("None");
	}

	// Resolve ClassId to UClass via the extender registry, then use its display name
	if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
	{
		for (const FName& key : registry->GetExtenderRegistry().GetAllManifestKeys())
		{
			if (TSharedPtr<FSGDTAExtenderManifest> manifest = registry->GetExtenderRegistry().GetManifest(key))
			{
				if (const FSGDTASerializerExtenderRegistryEntry* entry = manifest->FindByExtenderId(CurrentClassId))
				{
					if (UClass* resolvedClass = entry->Class.Get())
					{
						return resolvedClass->GetDisplayNameText();
					}

					if (!entry->ClassName.IsEmpty())
					{
						return FText::FromString(entry->ClassName);
					}
				}
			}
		}
	}

	// Fallback to GUID string if no manifest entry found
	return FText::FromString(CurrentClassId.ToString());
}

FText FSGDTAClassIdCustomization::GetCurrentTooltipText() const
{
	if (!CurrentClassId.IsValid())
	{
		return INVTEXT("No class ID selected");
	}

	return FText::Format(INVTEXT("Class ID: {0}"),
		FText::FromString(CurrentClassId.ToString()));
}

FReply FSGDTAClassIdCustomization::OnClearClicked()
{
	WriteValue(FSGDTAClassId::INVALID_CLASS_ID);

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Cleared DTA Class ID"));
	return FReply::Handled();
}

FReply FSGDTAClassIdCustomization::OnCopyIdClicked()
{
	if (CurrentClassId.IsValid())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CurrentClassId.ToString());
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Copied DTA Class ID to clipboard: %s"), *CurrentClassId.ToString());
	}
	return FReply::Handled();
}

FReply FSGDTAClassIdCustomization::OnPasteClicked()
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
	FSGDTAClassId parsedClassId = FSGDTAClassId::FromString(clipboardText);
	if (parsedClassId.IsValid())
	{
		WriteValue(parsedClassId);
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Pasted DTA Class ID: %s"), *parsedClassId.ToString());
		return FReply::Handled();
	}

	{
		const FString message = FString::Printf(TEXT("Paste failed: clipboard content\n'%s' is not a valid Class ID"), *clipboardText);
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("%s"), *message);
		FNotificationInfo info(FText::FromString(message));
		info.ExpireDuration = 5.0f;
		info.Image = FAppStyle::GetBrush("Icons.ErrorWithColor");
		FSlateNotificationManager::Get().AddNotification(info);
	}

	return FReply::Handled();
}

void FSGDTAClassIdCustomization::ReadCurrentValue()
{
	CurrentClassId = FSGDTAClassId::INVALID_CLASS_ID;

	if (!StructPropertyHandle.IsValid())
	{
		return;
	}

	// Read the FSGDTAClassId struct directly via raw data access
	TArray<void*> rawData;
	StructPropertyHandle->AccessRawData(rawData);

	if (rawData.Num() > 0 && rawData[0] != nullptr)
	{
		CurrentClassId = *static_cast<FSGDTAClassId*>(rawData[0]);
	}

	RefreshButtonStates();
}

void FSGDTAClassIdCustomization::WriteValue(const FSGDTAClassId& NewClassId)
{
	if (!StructPropertyHandle.IsValid())
	{
		return;
	}

	StructPropertyHandle->NotifyPreChange();

	TArray<void*> rawData;
	StructPropertyHandle->AccessRawData(rawData);

	if (!rawData.IsEmpty() && rawData[0] != nullptr)
	{
		*static_cast<FSGDTAClassId*>(rawData[0]) = NewClassId;
	}

	StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	CurrentClassId = NewClassId;

	RefreshButtonStates();
}

void FSGDTAClassIdCustomization::RefreshButtonStates()
{
	const bool bHasValidId = CurrentClassId.IsValid();

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
