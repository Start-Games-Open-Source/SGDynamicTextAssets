// Copyright Start Games, Inc. All Rights Reserved.

#include "Commandlets/SGDynamicTextAssetCookCommandlet.h"

#include "SGDynamicTextAssetsEditorModule.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Utilities/SGDynamicTextAssetCookUtils.h"
#include "Misc/Paths.h"

USGDynamicTextAssetCookCommandlet::USGDynamicTextAssetCookCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 USGDynamicTextAssetCookCommandlet::Main(const FString& Params)
{
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetCook Commandlet Started ==="));

	// Parse command line parameters
	TArray<FString> tokens;
	TArray<FString> switches;
	TMap<FString, FString> paramMap;
	ParseCommandLine(*Params, tokens, switches, paramMap);

	bool bValidateOnly = switches.Contains(TEXT("validate"));
	bool bCleanOnly = switches.Contains(TEXT("clean"));
	bool bNoClean = switches.Contains(TEXT("noclean"));
	FString outputPath = paramMap.FindRef(TEXT("output"));
	FString classFilter = paramMap.FindRef(TEXT("class"));

	// Mutual exclusion: -clean and -validate cannot be combined
	if (bCleanOnly && bValidateOnly)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("-clean and -validate are mutually exclusive"));
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetCook Commandlet Finished (error) ==="));
		return 1;
	}

	// Standalone clean mode: delete all cooked files and exit
	if (bCleanOnly)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Mode: Clean only (delete cooked files)"));
		bool bCleanSuccess = FSGDynamicTextAssetCookUtils::CleanCookedDirectory();
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetCook Commandlet Finished (clean) ==="));
		return bCleanSuccess ? 0 : 1;
	}

	if (bValidateOnly)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Mode: Validation only"));
	}
	else
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Mode: Cook (validate + convert to binary)"));
		if (bNoClean)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Pre-cook cleaning disabled (-noclean)"));
		}
	}

	if (!classFilter.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Class filter: %s"), *classFilter);
	}

	// Determine output directory
	FString outputDirectory;
	if (outputPath.IsEmpty())
	{
		// Default: flat cooked directory (ID-named files + manifest)
		outputDirectory = FSGDynamicTextAssetCookUtils::GetCookedOutputRootPath();
	}
	else
	{
		// Resolve relative to project root
		outputDirectory = FPaths::Combine(FPaths::ProjectDir(), outputPath);
		FPaths::NormalizeDirectoryName(outputDirectory);
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Output directory: %s"), *outputDirectory);

	// Validate-only mode: just validate files
	if (bValidateOnly)
	{
		// Collect JSON files with optional class filter
		TArray<FString> allFiles;
		FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

		TArray<FString> jsonFilePaths;
		for (const FString& filePath : allFiles)
		{
			if (FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(filePath).IsEmpty())
			{
				continue;
			}

			// Apply class filter if specified
			if (!classFilter.IsEmpty())
			{
				FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
				if (!metadata.bIsValid)
				{
					continue;
				}

				// Normalize: strip leading 'U' for comparison if present
				FString filterName = classFilter;
				FString className = metadata.ClassName;

				if (filterName.Len() > 1 && filterName[0] == TEXT('U') && FChar::IsUpper(filterName[1]))
				{
					filterName = filterName.RightChop(1);
				}
				if (className.Len() > 1 && className[0] == TEXT('U') && FChar::IsUpper(className[1]))
				{
					className = className.RightChop(1);
				}

				if (!className.Equals(filterName, ESearchCase::IgnoreCase))
				{
					continue;
				}
			}

			jsonFilePaths.Add(filePath);
		}

		if (jsonFilePaths.IsEmpty())
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("No .dta.json files found to validate"));
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetCook Commandlet Finished (0 files) ==="));
			return 0;
		}

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Found %d .dta.json file(s) to validate"), jsonFilePaths.Num());

		int32 totalErrors = 0;
		int32 filesWithErrors = 0;

		for (const FString& filePath : jsonFilePaths)
		{
			TArray<FString> errors;
			bool bValid = FSGDynamicTextAssetCookUtils::ValidateDynamicTextAssetFile(filePath, errors);

			if (!bValid)
			{
				filesWithErrors++;
				totalErrors += errors.Num();

				FString relativePath = filePath;
				FPaths::MakePathRelativeTo(relativePath, *FPaths::ProjectDir());

				UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Validation FAILED for: %s"), *relativePath);
				for (const FString& error : errors)
				{
					UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("  - %s"), *error);
				}
			}
		}

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Validation complete: %d/%d files passed, %d error(s)"),
			jsonFilePaths.Num() - filesWithErrors, jsonFilePaths.Num(), totalErrors);
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetCook Commandlet Finished (validate only) ==="));

		return filesWithErrors > 0 ? 1 : 0;
	}

	// Cook mode: use shared utility to cook all dynamic text assets
	TArray<FString> errors;
	bool bSuccess = FSGDynamicTextAssetCookUtils::CookAllDynamicTextAssets(outputDirectory, classFilter, errors, bNoClean);

	if (!bSuccess)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Cook completed with errors:"));
		for (const FString& error : errors)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("  - %s"), *error);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetCook Commandlet Finished ==="));

	return bSuccess ? 0 : 1;
}
