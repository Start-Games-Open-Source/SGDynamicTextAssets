// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "SGDynamicTextAssetCookCommandlet.generated.h"

/**
 * Commandlet for validating and cooking dynamic text asset files.
 *
 * NOTE: For editor packaging (File > Package Project), the cook delegate
 * automatically invokes cooking - you typically do not need to run this
 * commandlet manually. It remains available for CI/CD pipelines and
 * custom workflows.
 *
 * This commandlet:
 * - Scans all .dta.json files in Content/SGDynamicTextAssets/
 * - Optionally validates them (with -validate flag)
 * - Converts them to compressed binary .dta.bin format
 * - Outputs flat ID-named files + dta_manifest.bin
 *
 * Default output: Content/SGDynamicTextAssetsCooked/
 * The -output flag overrides this for custom workflows.
 *
 * Command Line Usage:
 *   UnrealEditor-Cmd.exe <ProjectPath> -run=SGDynamicTextAssetCook [options]
 *
 * Options:
 *   -validate    Run validation only, do not cook
 *   -clean       Clean only: delete all cooked files without cooking
 *   -noclean     Skip pre-cook directory cleaning (for build machine iterative builds)
 *   -output=Path Output directory for cooked files (relative to project root)
 *                Overrides default Content/SGDynamicTextAssetsCooked/
 *   -class=Name  Only process files for a specific class (e.g., UWeaponData)
 *
 * Examples:
 *   // Validate all dynamic text assets without cooking
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -validate
 *
 *   // Cook all dynamic text assets to default location
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook
 *
 *   // Cook to custom staging directory
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -output=Saved/Cooked/DynamicTextAssets
 *
 *   // Cook only weapon dynamic text assets
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -class=UWeaponData
 *
 *   // Clean cooked directory without cooking (CI/CD cleanup step)
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -clean
 *
 *   // Cook without pre-clean (iterative build machine reuse)
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -noclean
 *
 * Integration:
 *   - Cook Delegate: For editor packaging, the editor module automatically
 *     subscribes to UE::Cook::FDelegates::CookStarted and invokes cooking.
 *     Manual invocation is not required.
 *
 *   - CI/CD Pipeline: Chain this commandlet before BuildCookRun:
 *       UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook
 *       RunUAT.bat BuildCookRun -project=MyProject.uproject -platform=Win64 ...
 *
 * Return Codes:
 *   0 - Success (all files validated/cooked)
 *   1 - Failure (validation errors or cook failures)
 *
 * @see FSGDynamicTextAssetCookUtils
 * @see FSGDynamicTextAssetCookManifest
 */
UCLASS(ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetCookCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	USGDynamicTextAssetCookCommandlet();

	// UCommandlet overrides
	virtual int32 Main(const FString& Params) override;
	// ~UCommandlet overrides
};
