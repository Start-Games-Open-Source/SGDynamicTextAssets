// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * A runtime module accessor for slate style's,
 * registration/deregister occurs within FSGDynamicTextAssetsRuntimeModule.
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetSlateStyles
{
public:
	/** Call from FSGDynamicTextAssetsRuntimeModule::StartupModule() */
	static void Initialize();

	/** Call from FSGDynamicTextAssetsRuntimeModule::ShutdownModule() */
	static void Shutdown();

	/** Access the registered style set */
	static const ISlateStyle& Get();

	/**
	 * Retrieves a brush from the registered style set using the specified property name and optional specifier.
	 *
	 * @param PropertyName The name of the property associated with the desired brush.
	 * @param Specifier An optional specifier to further qualify the property lookup. Default is nullptr.
	 * @return A pointer to the FSlateBrush associated with the given property name and optional specifier, or nullptr if no brush is found.
	 */
	static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = NULL);

	/** The name of the registered style set, use this with FAppStyle style lookups */
	static FName GetStyleSetName();

	/** The name of the style set registered by for this plugin. */
	static const FName STYLE_SET_NAME;

	/** JSON brush name that this plugin uses. */
	static const FName BRUSH_NAME_JSON;

	/** XML brush name that this plugin uses. */
	static const FName BRUSH_NAME_XML;

	/** YAML brush name that this plugin uses. */
	static const FName BRUSH_NAME_YAML;

private:

	/** The style set associated with this plugin */
	static TSharedPtr<FSlateStyleSet> StyleSet;
};