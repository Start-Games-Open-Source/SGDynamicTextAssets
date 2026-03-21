// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDynamicTextAssetBundleRowExtension.h"

#include "PropertyHandle.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "Settings/SGDynamicTextAssetEditorSettings.h"
#include "Statics/SGDynamicTextAssetSlateStyles.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

namespace Internal_AssetBundleRowExtension
{
	/** Returns true if the property holds a soft reference type (TSoftObjectPtr or TSoftClassPtr). */
	bool IsSoftRefProperty(const FProperty* Property)
	{
		return Property->IsA<FSoftObjectProperty>() || Property->IsA<FSoftClassProperty>();
	}

	/** Returns true if the property is a container (TArray/TMap/TSet) whose inner elements are soft references. */
	bool IsContainerOfSoftRefs(const FProperty* Property)
	{
		if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(Property))
		{
			return IsSoftRefProperty(arrayProp->Inner);
		}
		if (const FMapProperty* mapProp = CastField<FMapProperty>(Property))
		{
			return IsSoftRefProperty(mapProp->ValueProp);
		}
		if (const FSetProperty* setProp = CastField<FSetProperty>(Property))
		{
			return IsSoftRefProperty(setProp->ElementProp);
		}
		return false;
	}
}

bool FSGDynamicTextAssetPropertyExtensionHandler::IsPropertyExtendable(
	const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const
{
	// Check editor setting
	const USGDynamicTextAssetEditorSettings* editorSettings = USGDynamicTextAssetEditorSettings::Get();
	if (editorSettings && !editorSettings->bShowAssetBundleIcons)
	{
		return false;
	}
	const FProperty* property = PropertyHandle.GetProperty();
	if (!property)
	{
		return false;
	}

	const bool bIsSoftRef = Internal_AssetBundleRowExtension::IsSoftRefProperty(property);
	const bool bIsContainerOfSoftRefs = Internal_AssetBundleRowExtension::IsContainerOfSoftRefs(property);
	return (bIsSoftRef || bIsContainerOfSoftRefs) && property->HasMetaData(TEXT("AssetBundles"));
}

void FSGDynamicTextAssetPropertyExtensionHandler::ExtendWidgetRow(
	FDetailWidgetRow& InWidgetRow,
	const IDetailLayoutBuilder& InDetailBuilder,
	const UClass* InObjectClass,
	TSharedPtr<IPropertyHandle> PropertyHandle)
{
	const FProperty* property = PropertyHandle->GetProperty();
	if (!property)
	{
		return;
	}
	// Grab the existing value widget and wrap it with the icon prepended on the left
	TSharedPtr<SWidget> existingValueWidget = InWidgetRow.ValueWidget.Widget;
	if (!existingValueWidget.IsValid())
	{
		return;
	}

	const FString& bundleMeta = property->GetMetaData(TEXT("AssetBundles"));

	// Format each bundle name as a list entry
	FString formattedBundles;
	TArray<FString> bundleNames;
	bundleMeta.ParseIntoArray(bundleNames, TEXT(","));
	for (FString& bundleName : bundleNames)
	{
		bundleName.TrimStartAndEndInline();
		formattedBundles += FString::Printf(TEXT("\n- %s"), *bundleName);
	}
	const FText tooltipText = FText::FromString(FString::Printf(TEXT("Asset Bundles for %s:%s"),
		*property->GetDisplayNameText().ToString(), *formattedBundles));

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
		TEXT("Adding asset bundle icon for property '%s' (bundles: %s)"),
		*property->GetName(), *bundleMeta);

	// Reorganize the value content so the icon is in front of the actual property
	InWidgetRow.ValueContent()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		// This gets placed within a constrained box so I can't get around the padding on the left side
		.Padding(0.0f, 0.0f, 3.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			.HeightOverride(16.0f)
			[
				SNew(SImage)
				.Image(FSGDynamicTextAssetSlateStyles::GetBrush(FSGDynamicTextAssetSlateStyles::BRUSH_NAME_ASSET_BUNDLE))
				.ToolTipText(tooltipText)
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			existingValueWidget.ToSharedRef()
		]
	];
}
