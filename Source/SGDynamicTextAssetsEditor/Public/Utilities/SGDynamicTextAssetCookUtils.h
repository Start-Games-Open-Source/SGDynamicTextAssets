// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Settings/SGDynamicTextAssetSettings.h"

class FSGDynamicTextAssetCookManifest;

/**
 * Static utility class for cooking dynamic text asset files.
 *
 * Provides shared cook logic used by both the cook commandlet (CI/CD)
 * and the cook delegate (editor packaging). Produces flat ID-named
 * binary files and a cook manifest in the output directory.
 *
 * Output format:
 *   {OutputDirectory}/
 *   ├── {Id1}.dta.bin
 *   ├── {Id2}.dta.bin
 *   ├── ...
 *   ├── dta_manifest.bin
 *   └── _TypeManifests/
 *       ├── {RootTypeId1}.json
 *       └── {RootTypeId2}.json
 *
 * @see USGDynamicTextAssetCookCommandlet
 * @see FSGDynamicTextAssetCookManifest
 */
class SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetCookUtils
{
public:

	/**
	 * Deletes all .dta.bin files and dta_manifest.bin from the cooked output directory.
	 * Logs each deleted file. Safe to call when the directory does not exist.
	 *
	 * @return True if the directory is clean after the operation (or did not exist)
	 */
	static bool CleanCookedDirectory();

	/**
	 * Validates a single .dta.json file's structure and content.
	 * Checks JSON structure, ID, class name, and version fields.
	 *
	 * @param FilePath Absolute path to the .dta.json file
	 * @param OutErrors Output array of error messages (empty if valid)
	 * @return True if validation passed (no errors)
	 */
	static bool ValidateDynamicTextAssetFile(const FString& FilePath, TArray<FString>& OutErrors);

	/**
	 * Cooks a single .dta.json file to flat ID-named binary format.
	 * Extracts file information (ID, ClassName, UserFacingId) from the JSON,
	 * converts to compressed binary, writes {Id}.dta.bin to OutputDirectory,
	 * and adds an entry to the manifest.
	 *
	 * @param JsonFilePath Absolute path to the source .dta.json file
	 * @param OutputDirectory Absolute path to the flat output directory
	 * @param CompressionMethod Compression algorithm to use
	 * @param OutManifest Manifest to add the cooked entry to
	 * @return True if conversion and manifest entry succeeded
	 */
	static bool CookDynamicTextAssetFile(
		const FString& JsonFilePath,
		const FString& OutputDirectory,
		ESGDynamicTextAssetCompressionMethod CompressionMethod,
		FSGDynamicTextAssetCookManifest& OutManifest);

	/**
	 * Cooks all .dta.json files to flat ID-named binary format with manifest.
	 * Validates each file, converts to binary, writes {Id}.dta.bin files,
	 * and saves the cook manifest to the output directory.
	 *
	 * When bSkipPreClean is false (default), calls CleanCookedDirectory() before
	 * cooking if the bCleanCookedDirectoryBeforeCook setting is enabled (or if the
	 * settings asset is null, defaults to true).
	 *
	 * @param OutputDirectory Absolute path to the output directory
	 * @param ClassFilter If non-empty, only cook files matching this class name
	 * @param OutErrors Output array of error messages from validation and cook failures
	 * @param bSkipPreClean If true, skips pre-cook directory cleaning regardless of settings (for commandlet -noclean override)
	 * @return True if all files cooked successfully (false if any failures)
	 */
	static bool CookAllDynamicTextAssets(
		const FString& OutputDirectory,
		const FString& ClassFilter,
		TArray<FString>& OutErrors,
		bool bSkipPreClean = false);

	/**
	 * Returns the root directory for cooked dynamic text assets.
	 * Delegates to FSGDynamicTextAssetFileManager::GetCookedDynamicTextAssetsRootPath().
	 *
	 * @return Absolute path to Content/SGDynamicTextAssetsCooked/
	 */
	static FString GetCookedOutputRootPath();

	/**
	 * Scans all DTA files and groups soft reference package names by their AssetBundles
	 * metadata tag value. Deserializes each DTA into a transient UObject and uses
	 * FSGDynamicTextAssetBundleData::ExtractFromObject() to collect bundle entries.
	 * Soft references without AssetBundles metadata are grouped under NAME_None.
	 *
	 * @param OutBundlePackages Map of bundle name to array of package names
	 * @return Total number of unique packages found across all bundles
	 */
	static int32 GatherSoftReferencesBySGDTBundle(TMap<FName, TArray<FName>>& OutBundlePackages);

	/**
	 * Scans all DTA files and gathers soft object/class references from their properties.
	 * Used during cook to ensure referenced assets are included in packaged builds.
	 *
	 * Deserializes each DTA into a transient UObject and recursively walks all UProperties
	 * to find FSoftObjectPath and FSoftClassPath values. Skips empty paths and /Script/ paths
	 * (native classes are always available).
	 *
	 * @param OutPackageNames Deduplicated package names (e.g., "/Game/Weapons/Sword")
	 * @return Number of unique packages found
	 */
	static int32 GatherSoftReferencesFromAllFiles(TArray<FName>& OutPackageNames);

private:

	/**
	 * Recursively walks a property and collects soft object/class reference package names.
	 * Handles FSoftObjectProperty, FSoftClassProperty, FStructProperty, FArrayProperty, FMapProperty, FSetProperty.
	 *
	 * @param Property The property to inspect
	 * @param ContainerPtr Pointer to the struct/object containing this property
	 * @param OutPackageNames Set to collect unique package names into
	 */
	static void GatherSoftReferencesFromProperty(const FProperty* Property, const void* ContainerPtr, TSet<FName>& OutPackageNames);
};
