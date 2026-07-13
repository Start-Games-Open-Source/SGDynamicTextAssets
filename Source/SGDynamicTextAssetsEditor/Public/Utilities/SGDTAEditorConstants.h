// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A centralized place for this plugin's editor constant values for
 * reuse throughout the editor module of the plugin.
 */
class SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetEditorConstants
{
public:

	/**
	 * Editor color for Blueprint assets.
	 * Grabbing this color from AssetTypeActions_Blueprint & AssetDefinition_Blueprint.
	 */
	static const FLinearColor BLUEPRINT_ASSET_COLOR;
	
	/**
	 * Size of the corner badges overlaid on the tile type icon.
	 * Matches the UE 5.6 SAssetTileItem source control icon size (16.0f). The engine's
	 * Small-thumbnail fallback is 11.0f; switch to that if visual QA finds 16.0f heavy
	 * over the compact type icon.
	 */
	static constexpr float SCC_BADGE_SIZE = 16.0f;

	/**
	 * Size of the unsaved-change star badge overlaid on the top-right of the tile type icon.
	 * A dedicated constant so the two corner badges (SCC top-left, dirty top-right) can be
	 * tuned independently. Matches SCC_BADGE_SIZE (16.0f) today for visual symmetry.
	 */
	static constexpr float DIRTY_BADGE_SIZE = 16.0f;
};