// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Core/SGDynamicTextAssetId.h"

#include "SGDynamicTextAssetValidationCommandlet.generated.h"

class USGDynamicTextAsset;
struct FSGDynamicTextAssetRef;

/**
 * Pre-cook validation commandlet that scans all assets for FSGDynamicTextAssetRef
 * properties and verifies that every referenced ID has a corresponding
 * dynamic text asset file on disk.
 *
 * Designed to run before the cook commandlet to catch broken references
 * early, preventing runtime load failures in packaged builds.
 *
 * Command Line Usage:
 *   UnrealEditor-Cmd.exe <ProjectPath> -run=SGDynamicTextAssetValidation [options]
 *
 * Options:
 *   -strict          Treat warnings as errors (e.g., empty refs in non-optional properties)
 *   -class=Name      Only validate references within assets of a specific Blueprint class
 *   -verbose         Log all scanned assets and found references, not just failures
 *
 * Examples:
 *   // Validate all references across the entire project
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetValidation
 *
 *   // Strict mode - fail on warnings too
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetValidation -strict
 *
 *   // Verbose output for debugging reference issues
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetValidation -verbose
 *
 * Integration with Build Pipeline:
 *   Chain this commandlet before SGDynamicTextAssetCook in your build script:
 *     UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetValidation
 *     UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -output=Content/SGDynamicTextAssets
 *     RunUAT.bat BuildCookRun -project=MyProject.uproject -platform=Win64 ...
 *
 * Return Codes:
 *   0 - Success (all references are valid)
 *   1 - Failure (broken references found)
 */
UCLASS(ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetValidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	USGDynamicTextAssetValidationCommandlet();

	// UCommandlet overrides
	virtual int32 Main(const FString& Params) override;
	// ~UCommandlet overrides

private:

	/**
	 * Describes a single broken reference found during validation.
	 */
	struct FBrokenReference
	{
		/** The ID that was referenced but could not be resolved */
		FSGDynamicTextAssetId ReferencedId;

		/** The asset containing the broken reference */
		FString SourceAsset;

		/** The property path where the broken reference was found */
		FString PropertyPath;

		/** Human-readable display name for the source asset */
		FString SourceDisplayName;
	};

	/**
	 * Describes a warning found during validation (non-fatal).
	 */
	struct FValidationWarning
	{
		/** Human-readable description of the warning */
		FString Message;

		/** The asset where the warning originated */
		FString SourceAsset;

		/** The property path related to the warning */
		FString PropertyPath;
	};

	/**
	 * Builds a set of all known dynamic text asset IDs by scanning
	 * all .dta.json files on disk.
	 *
	 * @param OutKnownIds Output set of valid IDs
	 */
	void CollectKnownIds(TSet<FSGDynamicTextAssetId>& OutKnownIds) const;

	/**
	 * Scans all Blueprint assets for FSGDynamicTextAssetRef properties
	 * and validates that referenced IDs exist in the known set.
	 *
	 * @param KnownIds Set of IDs that exist on disk
	 * @param OutBrokenRefs Output array of broken references
	 * @param OutWarnings Output array of validation warnings
	 * @param bVerbose If true, log all scanned assets
	 */
	void ValidateBlueprintAssets(const TSet<FSGDynamicTextAssetId>& KnownIds,
	                             TArray<FBrokenReference>& OutBrokenRefs,
	                             TArray<FValidationWarning>& OutWarnings,
	                             bool bVerbose) const;

	/**
	 * Scans all dynamic text asset JSON files for FSGDynamicTextAssetRef properties
	 * and validates that referenced IDs exist in the known set.
	 *
	 * @param KnownIds Set of IDs that exist on disk
	 * @param OutBrokenRefs Output array of broken references
	 * @param OutWarnings Output array of validation warnings
	 * @param bVerbose If true, log all scanned files
	 */
	void ValidateDynamicTextAssetFiles(const TSet<FSGDynamicTextAssetId>& KnownIds,
	                              TArray<FBrokenReference>& OutBrokenRefs,
	                              TArray<FValidationWarning>& OutWarnings,
	                              bool bVerbose) const;

	/**
	 * Recursively inspects a property for FSGDynamicTextAssetRef values and
	 * validates each found ID against the known set.
	 *
	 * @param Property The property to inspect
	 * @param ContainerPtr Pointer to the containing object's data
	 * @param PropertyPath Dot-separated path built during recursion
	 * @param KnownIds Set of IDs that exist on disk
	 * @param SourceAsset The asset path for reporting
	 * @param SourceDisplayName Display name for the source asset
	 * @param OutBrokenRefs Output array of broken references
	 * @param OutWarnings Output array of validation warnings
	 */
	void ValidateRefsInProperty(const FProperty* Property,
	                            const void* ContainerPtr,
	                            const FString& PropertyPath,
	                            const TSet<FSGDynamicTextAssetId>& KnownIds,
	                            const FString& SourceAsset,
	                            const FString& SourceDisplayName,
	                            TArray<FBrokenReference>& OutBrokenRefs,
	                            TArray<FValidationWarning>& OutWarnings) const;
};
