// Copyright Start Games, Inc. All Rights Reserved.

#include "Statics/SGDynamicTextAssetSlateStyles.h"

#include "Interfaces/IPluginManager.h"
#include "SGDynamicTextAssetLogs.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"

const FName FSGDynamicTextAssetSlateStyles::STYLE_SET_NAME = "SGDynamicTextAssetsStyle";
const FName FSGDynamicTextAssetSlateStyles::BRUSH_NAME_JSON = "SGDynamicTextAssets.Icon.JSON";
const FName FSGDynamicTextAssetSlateStyles::BRUSH_NAME_XML = "SGDynamicTextAssets.Icon.XML";
const FName FSGDynamicTextAssetSlateStyles::BRUSH_NAME_YAML = "SGDynamicTextAssets.Icon.YAML";
const FName FSGDynamicTextAssetSlateStyles::BRUSH_NAME_ASSET_BUNDLE = "SGDynamicTextAssets.Icon.AssetBundle";

TSharedPtr<FSlateStyleSet> FSGDynamicTextAssetSlateStyles::StyleSet = nullptr;

// Shorthand macro for creating a raw FSlateImageBrush from a PNG image at the inputted relative plugin path
#define IMAGE_BRUSH_PNG(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

namespace FSGDynamicTextAssetSlateStylesInternal
{
	// Using 16x16 for regular icon's for consistency
	static constexpr float REGULAR_ICON_SIZE = 16.0f;
}

void FSGDynamicTextAssetSlateStyles::Initialize()
{
	if (StyleSet.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetEditorSlateStyles::Initialize: StyleSet already created and registered."));
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	const TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin(TEXT("SGDynamicTextAssets"));
	check(plugin.IsValid());
	// Point to Resources folder for the raw image path
	StyleSet->SetContentRoot(plugin->GetBaseDir() / TEXT("Resources"));

	// Create regular style's
	StyleSet->Set(BRUSH_NAME_JSON, new IMAGE_BRUSH_PNG(TEXT("json_icon"),
		FVector2D(FSGDynamicTextAssetSlateStylesInternal::REGULAR_ICON_SIZE)));

	StyleSet->Set(BRUSH_NAME_XML, new IMAGE_BRUSH_PNG(TEXT("xml_icon"),
		FVector2D(FSGDynamicTextAssetSlateStylesInternal::REGULAR_ICON_SIZE)));

	StyleSet->Set(BRUSH_NAME_YAML, new IMAGE_BRUSH_PNG(TEXT("yaml_icon"),
		FVector2D(FSGDynamicTextAssetSlateStylesInternal::REGULAR_ICON_SIZE)));

	StyleSet->Set(BRUSH_NAME_ASSET_BUNDLE, new IMAGE_BRUSH_PNG(TEXT("asset_bundles_icon"),
		FVector2D(FSGDynamicTextAssetSlateStylesInternal::REGULAR_ICON_SIZE)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

#undef IMAGE_BRUSH_PNG

void FSGDynamicTextAssetSlateStyles::Shutdown()
{
	if (!StyleSet.IsValid())
	{
		return;
	}
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}

const ISlateStyle& FSGDynamicTextAssetSlateStyles::Get()
{
	check(StyleSet.IsValid());
	return *StyleSet;
}

const FSlateBrush* FSGDynamicTextAssetSlateStyles::GetBrush(FName PropertyName, const ANSICHAR* Specifier)
{
	return Get().GetBrush(PropertyName, Specifier);
}

FName FSGDynamicTextAssetSlateStyles::GetStyleSetName()
{
	return STYLE_SET_NAME;
}
