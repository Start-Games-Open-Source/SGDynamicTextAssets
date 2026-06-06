// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Centralized constants for the SGDynamicTextAssets plugin.
 *
 * Cross-cutting values that are referenced by multiple modules live here
 * to avoid circular header dependencies and provide a single source of truth.
 */
namespace SGDynamicTextAssetConstants
{
	/** Invalid serializer type ID (no format assigned). */
	static constexpr uint32 INVALID_SERIALIZER_TYPE_ID = 0;

	/** JSON serializer format ID. */
	static constexpr uint32 JSON_SERIALIZER_TYPE_ID = 1;

	/** XML serializer format ID. */
	static constexpr uint32 XML_SERIALIZER_TYPE_ID = 2;

	/** YAML serializer format ID. */
	static constexpr uint32 YAML_SERIALIZER_TYPE_ID = 3;

	/** Minimum ID for third-party custom serializers. */
	static constexpr uint32 MIN_CUSTOM_SERIALIZER_TYPE_ID = 100;

	/** File extension for binary dynamic text asset files (includes dot). */
	inline const FString BINARY_FILE_EXTENSION = TEXT(".dta.bin");

	/** File extension for JSON dynamic text asset files (includes dot). */
	inline const FString JSON_FILE_EXTENSION = TEXT(".dta.json");

	/** File extension for YAML dynamic text asset files (includes dot). */
	inline const FString YAML_FILE_EXTENSION = TEXT(".dta.yaml");

	/** File extension for XML dynamic text asset files (includes dot). */
	inline const FString XML_FILE_EXTENSION = TEXT(".dta.xml");

	/** XML root element tag name used for DTA documents. */
	inline const FString XML_ROOT_TAG = TEXT("DynamicTextAsset");

	/** Framework key for the asset bundle extender manifest. */
	static const FName ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY = FName(TEXT("AssetBundles"));
}
