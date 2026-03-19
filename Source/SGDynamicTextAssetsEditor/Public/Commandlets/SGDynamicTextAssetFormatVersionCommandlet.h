// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Core/SGDynamicTextAssetVersion.h"

#include "SGDynamicTextAssetFormatVersionCommandlet.generated.h"

/**
 * Result of validating a single DTA file's format version against the
 * serializer's current version.
 */
struct SGDYNAMICTEXTASSETSEDITOR_API FSGFormatVersionValidationResult
{
	/** Absolute path to the DTA file */
	FString FilePath;

	/** The format version embedded in the file */
	FSGDynamicTextAssetVersion FileVersion;

	/** The serializer's current format version */
	FSGDynamicTextAssetVersion SerializerVersion;

	/** The serializer type ID that owns this file */
	uint32 SerializerTypeId = 0;

	/** True if the major version matches but minor/patch differs */
	uint8 bIsMinorMismatch : 1;

	/** True if the major version differs (breaking change) */
	uint8 bIsMajorMismatch : 1;

	FSGFormatVersionValidationResult()
		: bIsMinorMismatch(false)
		, bIsMajorMismatch(false)
	{ }
};

/**
 * Result of migrating a single DTA file's format version.
 */
struct SGDYNAMICTEXTASSETSEDITOR_API FSGFormatVersionMigrationResult
{
	/** Absolute path to the DTA file */
	FString FilePath;

	/** The format version before migration */
	FSGDynamicTextAssetVersion OldVersion;

	/** The format version after migration */
	FSGDynamicTextAssetVersion NewVersion;

	/** True if the migration succeeded */
	uint8 bSuccess : 1;

	/** Error message if the migration failed */
	FString ErrorMessage;

	FSGFormatVersionMigrationResult()
		: bSuccess(false)
	{ }
};

/**
 * Commandlet for validating and migrating DTA file format versions.
 *
 * This commandlet is the single source of truth for format version validation
 * and migration logic. The editor UI (T100.8) delegates to the static utility
 * functions on this class rather than duplicating the logic.
 *
 * Two modes of operation:
 *
 * 1. Validate: Scans all DTA files, compares embedded format versions against
 *    current serializer versions. Reports minor mismatches as warnings and
 *    major mismatches as errors. Does not modify any files.
 *
 * 2. Migrate: Scans all DTA files, runs MigrateFileFormat() on any files with
 *    outdated format versions, and re-saves them.
 *
 * Command Line Usage:
 *   UnrealEditor-Cmd.exe <ProjectPath> -run=SGDynamicTextAssetFormatVersion [options]
 *
 * Options:
 *   -validate    Scan and report version mismatches (default if no mode specified)
 *   -migrate     Migrate all outdated files to current format versions
 *
 * Examples:
 *   // Validate all files
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetFormatVersion -validate
 *
 *   // Migrate all outdated files
 *   UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetFormatVersion -migrate
 *
 * Return Codes:
 *   0 - Success (validate found no errors, or migrate completed successfully)
 *   1 - Failure (validate found major mismatches, or migrate had failures)
 *
 * @see ISGDynamicTextAssetSerializer::MigrateFileFormat
 * @see ISGDynamicTextAssetSerializer::GetFileFormatVersion
 */
UCLASS(ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetFormatVersionCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	USGDynamicTextAssetFormatVersionCommandlet();

	// UCommandlet overrides
	virtual int32 Main(const FString& Params) override;
	// ~UCommandlet overrides

	/**
	 * Scans all DTA files and returns validation results for files whose
	 * embedded format version does not match the serializer's current version.
	 * Files that are already up-to-date are not included in the results.
	 *
	 * @param OutResults Array populated with one entry per mismatched file
	 */
	static void ValidateAllFiles(TArray<FSGFormatVersionValidationResult>& OutResults);

	/**
	 * Migrates all DTA files with outdated format versions to the serializer's
	 * current version. Calls MigrateFileFormat() on each file and re-saves it.
	 *
	 * @param OutResults Array populated with one entry per migrated file
	 */
	static void MigrateAllFiles(TArray<FSGFormatVersionMigrationResult>& OutResults);

	/**
	 * Migrates a single DTA file to its serializer's current format version.
	 *
	 * @param FilePath Absolute path to the DTA file
	 * @param OutError Populated with an error message on failure
	 * @return True if migration succeeded or no migration was needed
	 */
	static bool MigrateSingleFile(const FString& FilePath, FString& OutError);
};
