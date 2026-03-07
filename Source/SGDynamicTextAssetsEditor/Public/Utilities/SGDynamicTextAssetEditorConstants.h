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
};