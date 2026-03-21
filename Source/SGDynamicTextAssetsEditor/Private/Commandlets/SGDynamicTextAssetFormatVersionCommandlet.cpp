// Copyright Start Games, Inc. All Rights Reserved.

#include "Commandlets/SGDynamicTextAssetFormatVersionCommandlet.h"

#include "SGDynamicTextAssetEditorLogs.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"

USGDynamicTextAssetFormatVersionCommandlet::USGDynamicTextAssetFormatVersionCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 USGDynamicTextAssetFormatVersionCommandlet::Main(const FString& Params)
{
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetFormatVersion Commandlet Started ==="));

	// Parse command line parameters
	TArray<FString> tokens;
	TArray<FString> switches;
	TMap<FString, FString> paramMap;
	ParseCommandLine(*Params, tokens, switches, paramMap);

	bool bMigrateMode = switches.Contains(TEXT("migrate"));
	bool bValidateMode = switches.Contains(TEXT("validate"));

	// Default to validate if no mode specified
	if (!bMigrateMode && !bValidateMode)
	{
		bValidateMode = true;
	}

	// Mutual exclusion
	if (bMigrateMode && bValidateMode)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("-validate and -migrate are mutually exclusive"));
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetFormatVersion Commandlet Finished (error) ==="));
		return 1;
	}

	if (bValidateMode)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Mode: Validate"));

		TArray<FSGFormatVersionValidationResult> results;
		ValidateAllFiles(results);

		// Report results
		int32 minorCount = 0;
		int32 majorCount = 0;

		for (const FSGFormatVersionValidationResult& result : results)
		{
			if (result.bIsMajorMismatch)
			{
				majorCount++;
				UE_LOG(LogSGDynamicTextAssetsEditor, Error,
					TEXT("  MAJOR mismatch: %s (file: %s, serializer: %s)"),
					*result.FilePath, *result.FileVersion.ToString(), *result.SerializerVersion.ToString());
			}
			else if (result.bIsMinorMismatch)
			{
				minorCount++;
				UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
					TEXT("  Minor mismatch: %s (file: %s, serializer: %s)"),
					*result.FilePath, *result.FileVersion.ToString(), *result.SerializerVersion.ToString());
			}
		}

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT(""));
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Validation Summary:"));
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  Major mismatches (errors): %d"), majorCount);
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  Minor mismatches (warnings): %d"), minorCount);

		if (results.Num() == 0)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  All files are up-to-date."));
		}

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetFormatVersion Commandlet Finished ==="));
		return majorCount > 0 ? 1 : 0;
	}

	// Migrate mode
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Mode: Migrate"));

	TArray<FSGFormatVersionMigrationResult> results;
	MigrateAllFiles(results);

	// Report results
	int32 successCount = 0;
	int32 failureCount = 0;

	for (const FSGFormatVersionMigrationResult& result : results)
	{
		if (result.bSuccess)
		{
			successCount++;
			UE_LOG(LogSGDynamicTextAssetsEditor, Log,
				TEXT("  Migrated: %s (%s -> %s)"),
				*result.FilePath, *result.OldVersion.ToString(), *result.NewVersion.ToString());
		}
		else
		{
			failureCount++;
			UE_LOG(LogSGDynamicTextAssetsEditor, Error,
				TEXT("  FAILED: %s - %s"),
				*result.FilePath, *result.ErrorMessage);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT(""));
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Migration Summary:"));
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  Succeeded: %d"), successCount);
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  Failed: %d"), failureCount);

	if (results.Num() == 0)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  No files needed migration."));
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetFormatVersion Commandlet Finished ==="));
	return failureCount > 0 ? 1 : 0;
}

void USGDynamicTextAssetFormatVersionCommandlet::ValidateAllFiles(TArray<FSGFormatVersionValidationResult>& OutResults)
{
	OutResults.Reset();

	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Scanning %d DTA files for format version mismatches..."), allFiles.Num());

	for (const FString& filePath : allFiles)
	{
		// Extract file info to get the file's format version
		FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);
		if (!fileInfo.bIsValid)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
				TEXT("  Skipping invalid file: %s"), *filePath);
			continue;
		}

		// Find the serializer for this file to get its current version
		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
		if (!serializer.IsValid())
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
				TEXT("  Skipping file with no serializer: %s"), *filePath);
			continue;
		}

		const FSGDynamicTextAssetVersion serializerVersion = serializer->GetFileFormatVersion();
		const FSGDynamicTextAssetVersion fileVersion = fileInfo.FileFormatVersion;

		// Skip files that are already at the current version
		if (fileVersion == serializerVersion)
		{
			continue;
		}

		FSGFormatVersionValidationResult result;
		result.FilePath = filePath;
		result.FileVersion = fileVersion;
		result.SerializerVersion = serializerVersion;
		result.SerializerTypeId = serializer->GetSerializerTypeId();

		if (fileVersion.Major != serializerVersion.Major)
		{
			result.bIsMajorMismatch = true;
		}
		else
		{
			result.bIsMinorMismatch = true;
		}

		OutResults.Add(MoveTemp(result));
	}
}

void USGDynamicTextAssetFormatVersionCommandlet::MigrateAllFiles(TArray<FSGFormatVersionMigrationResult>& OutResults)
{
	OutResults.Reset();

	// First validate to find files that need migration
	TArray<FSGFormatVersionValidationResult> validationResults;
	ValidateAllFiles(validationResults);

	if (validationResults.Num() == 0)
	{
		return;
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Migrating %d files with outdated format versions..."), validationResults.Num());

	for (const FSGFormatVersionValidationResult& validation : validationResults)
	{
		FSGFormatVersionMigrationResult result;
		result.FilePath = validation.FilePath;
		result.OldVersion = validation.FileVersion;
		result.NewVersion = validation.SerializerVersion;

		FString error;
		result.bSuccess = MigrateSingleFile(validation.FilePath, error);
		result.ErrorMessage = error;

		OutResults.Add(MoveTemp(result));
	}
}

bool USGDynamicTextAssetFormatVersionCommandlet::MigrateSingleFile(const FString& FilePath, FString& OutError)
{
	// Read the file contents
	FString fileContents;
	if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, fileContents))
	{
		OutError = FString::Printf(TEXT("Failed to read file: %s"), *FilePath);
		return false;
	}

	// Find the serializer
	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
	if (!serializer.IsValid())
	{
		OutError = FString::Printf(TEXT("No serializer found for: %s"), *FilePath);
		return false;
	}

	// Extract file info to get current file version
	FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(FilePath);
	if (!fileInfo.bIsValid)
	{
		OutError = FString::Printf(TEXT("Failed to extract file information from: %s"), *FilePath);
		return false;
	}

	const FSGDynamicTextAssetVersion serializerVersion = serializer->GetFileFormatVersion();
	const FSGDynamicTextAssetVersion fileVersion = fileInfo.FileFormatVersion;

	// No migration needed if already at current version
	if (fileVersion == serializerVersion)
	{
		return true;
	}

	// Run structural format migration
	if (!serializer->MigrateFileFormat(fileContents, fileVersion, serializerVersion))
	{
		OutError = FString::Printf(TEXT("MigrateFileFormat() failed for %s -> %s on: %s"),
			*fileVersion.ToString(), *serializerVersion.ToString(), *FilePath);
		return false;
	}

	// Stamp the new format version into the file contents
	if (!serializer->UpdateFileFormatVersion(fileContents, serializerVersion))
	{
		OutError = FString::Printf(TEXT("UpdateFileFormatVersion() failed for %s on: %s"),
			*serializerVersion.ToString(), *FilePath);
		return false;
	}

	// Write the migrated contents back to disk
	if (!FSGDynamicTextAssetFileManager::WriteRawFileContents(FilePath, fileContents))
	{
		OutError = FString::Printf(TEXT("Failed to write migrated file: %s"), *FilePath);
		return false;
	}

	return true;
}
