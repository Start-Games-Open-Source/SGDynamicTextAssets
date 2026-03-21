// Copyright Start Games, Inc. All Rights Reserved.

#include "Utilities/SGDynamicTextAssetCookUtils.h"

#include "SGDynamicTextAssetEditorLogs.h"
#include "UObject/Package.h"
#include "Management/SGDynamicTextAssetCookManifest.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Management/SGDynamicTextAssetTypeManifest.h"
#include "Serialization/SGBinaryEncodeParams.h"
#include "Serialization/SGDynamicTextAssetBinarySerializer.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Settings/SGDynamicTextAssetSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetBundleData.h"

bool FSGDynamicTextAssetCookUtils::CleanCookedDirectory()
{
	FString cookedDir = GetCookedOutputRootPath();
	IFileManager& fileManager = IFileManager::Get();

	// If the directory doesn't exist, it's already clean
	if (!fileManager.DirectoryExists(*cookedDir))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("FSGDynamicTextAssetCookUtils: Cooked directory does not exist, nothing to clean: %s"), *cookedDir);
		return true;
	}

	// Collect .dta.bin files
	TArray<FString> binFiles;
	fileManager.FindFiles(binFiles, *FPaths::Combine(cookedDir, TEXT("*") + FSGDynamicTextAssetFileManager::BINARY_EXTENSION), true, false);

	// Check for manifest file
	FString manifestPath = FPaths::Combine(cookedDir, FSGDynamicTextAssetCookManifest::MANIFEST_FILENAME);
	uint8 bManifestExists = fileManager.FileExists(*manifestPath) ? 1 : 0;

	int32 deletedCount = 0;

	// Delete .dta.bin files
	for (const FString& binFile : binFiles)
	{
		FString fullPath = FPaths::Combine(cookedDir, binFile);
		if (fileManager.Delete(*fullPath))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("  Deleted: %s"), *binFile);
			deletedCount++;
		}
		else
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FSGDynamicTextAssetCookUtils: Failed to delete: %s"), *fullPath);
		}
	}

	// Delete manifest
	if (bManifestExists)
	{
		if (fileManager.Delete(*manifestPath))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("  Deleted manifest file: %s"), FSGDynamicTextAssetCookManifest::MANIFEST_FILENAME);
			deletedCount++;
		}
		else
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FSGDynamicTextAssetCookUtils: Failed to delete manifest: %s"), *manifestPath);
		}
	}

	// Delete _TypeManifests/ subdirectory if it exists
	FString typeManifestsDir = FPaths::Combine(cookedDir, TEXT("_TypeManifests"));
	if (fileManager.DirectoryExists(*typeManifestsDir))
	{
		TArray<FString> typeManifestFiles;
		fileManager.FindFiles(typeManifestFiles, *FPaths::Combine(typeManifestsDir, TEXT("*.json")), true, false);

		for (const FString& typeManifestFile : typeManifestFiles)
		{
			FString fullPath = FPaths::Combine(typeManifestsDir, typeManifestFile);
			if (fileManager.Delete(*fullPath))
			{
				UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("  Deleted type manifest: %s"), *typeManifestFile);
				deletedCount++;
			}
		}

		fileManager.DeleteDirectory(*typeManifestsDir);
	}

	fileManager.DeleteDirectory(*cookedDir);

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("FSGDynamicTextAssetCookUtils: Cleaned cooked directory - %d file(s) deleted"), deletedCount);

	return true;
}

bool FSGDynamicTextAssetCookUtils::ValidateDynamicTextAssetFile(const FString& FilePath, TArray<FString>& OutErrors)
{
	OutErrors.Reset();

	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
	if (!serializer.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("No registered serializer found for file: %s"), *FilePath));
		return false;
	}

	// Read file contents
	FString fileContents;
	if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, fileContents))
	{
		OutErrors.Add(FString::Printf(TEXT("Failed to read file: %s"), *FilePath));
		return false;
	}

	// Validate structure
	FString structureError;
	if (!serializer->ValidateStructure(fileContents, structureError))
	{
		OutErrors.Add(FString::Printf(TEXT("Invalid file structure: %s"), *structureError));
		return false;
	}

	// Extract file information
	FSGDynamicTextAssetFileInfo fileInfo;
	if (!serializer->ExtractFileInfo(fileContents, fileInfo))
	{
		OutErrors.Add(TEXT("Failed to extract file information"));
	}
	else
	{
		if (!fileInfo.Id.IsValid())
		{
			OutErrors.Add(TEXT("ID is not valid"));
		}
		if (fileInfo.ClassName.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("Empty %s field"), *ISGDynamicTextAssetSerializer::KEY_TYPE));
		}
		if (!fileInfo.Version.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("Empty %s field"), *ISGDynamicTextAssetSerializer::KEY_VERSION));
		}
	}

    // Attempt to instantiate and deep validate if basic structure holds
    if (OutErrors.IsEmpty())
    {
        UClass* actualClass = FindFirstObject<UClass>(*fileInfo.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
        if (!actualClass)
        {
            OutErrors.Add(FString::Printf(TEXT("Failed to find UClass for type: %s"), *fileInfo.ClassName));
        }
        else
        {
            TScriptInterface<ISGDynamicTextAssetProvider> tempObject = NewObject<UObject>(GetTransientPackage(), actualClass);
            if (!tempObject)
            {
                OutErrors.Add(FString::Printf(TEXT("Failed to instantiate UClass: %s"), *fileInfo.ClassName));
            }
            else
            {
                bool bMigrated;
                if (!serializer->DeserializeProvider(fileContents, tempObject.GetInterface(), bMigrated))
                {
                    OutErrors.Add(TEXT("Failed to deserialize properties into object provider"));
                }
                else
                {
                    FSGDynamicTextAssetValidationResult validationResult;
                    if (!tempObject->Native_ValidateDynamicTextAsset(validationResult))
                    {
                        OutErrors.Add(validationResult.ToFormattedString());
                    }
                }
            }
        }
    }

	return OutErrors.IsEmpty();
}

bool FSGDynamicTextAssetCookUtils::CookDynamicTextAssetFile(
	const FString& JsonFilePath,
	const FString& OutputDirectory,
	ESGDynamicTextAssetCompressionMethod CompressionMethod,
	FSGDynamicTextAssetCookManifest& OutManifest)
{
	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(JsonFilePath);
	if (!serializer.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: No serializer found for: %s"), *JsonFilePath);
		return false;
	}

	// Read file
	FString fileContents;
	if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(JsonFilePath, fileContents))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: Failed to read file: %s"), *JsonFilePath);
		return false;
	}

	// Extract file information
	FSGDynamicTextAssetFileInfo cookFileInfo;
	if (!serializer->ExtractFileInfo(fileContents, cookFileInfo) || !cookFileInfo.Id.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: Failed to extract file info or ID from: %s"), *JsonFilePath);
		return false;
	}

	if (cookFileInfo.ClassName.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: Failed to extract ClassName from: %s"), *JsonFilePath);
		return false;
	}

	if (cookFileInfo.UserFacingId.IsEmpty())
	{
		cookFileInfo.UserFacingId = FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(JsonFilePath);
	}

	// Build flat ID-named output path: {OutputDirectory}/{Id}.dta.bin
	FString binaryFilePath = FPaths::Combine(
		OutputDirectory,
		cookFileInfo.Id.ToString() + FSGDynamicTextAssetFileManager::BINARY_EXTENSION);

	// Ensure output directory exists
	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!platformFile.DirectoryExists(*OutputDirectory))
	{
		if (!platformFile.CreateDirectoryTree(*OutputDirectory))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: Failed to create output directory: %s"), *OutputDirectory);
			return false;
		}
	}

	// Convert string to binary, storing the serializer's type ID and asset type ID for routing on load
	FSGBinaryEncodeParams encodeParams;
	encodeParams.Id = cookFileInfo.Id;
	encodeParams.SerializerTypeId = serializer->GetSerializerTypeId();
	encodeParams.AssetTypeId = cookFileInfo.AssetTypeId;
	encodeParams.CompressionMethod = CompressionMethod;

	TArray<uint8> binaryData;
	if (!FSGDynamicTextAssetBinarySerializer::StringToBinary(fileContents, encodeParams, binaryData))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: Failed to convert to binary: %s"), *JsonFilePath);
		return false;
	}

	// Write binary file
	if (!FSGDynamicTextAssetBinarySerializer::WriteBinaryFile(binaryFilePath, binaryData))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: Failed to write binary file: %s"), *binaryFilePath);
		return false;
	}

	// Add entry to manifest (strip UserFacingId if setting is enabled)
	FString manifestUserFacingId = cookFileInfo.UserFacingId;
	if (USGDynamicTextAssetSettingsAsset* settings = USGDynamicTextAssetSettings::GetSettings())
	{
		if (settings->ShouldStripUserFacingIdFromCookedManifest())
		{
			manifestUserFacingId.Empty();
		}
	}
	OutManifest.AddEntry(cookFileInfo.Id, cookFileInfo.ClassName, manifestUserFacingId, cookFileInfo.AssetTypeId);

	FString relativeOutput = binaryFilePath;
	FPaths::MakePathRelativeTo(relativeOutput, *FPaths::ProjectDir());
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  Cooked: %s"), *relativeOutput);

	return true;
}

bool FSGDynamicTextAssetCookUtils::CookAllDynamicTextAssets(
	const FString& OutputDirectory,
	const FString& ClassFilter,
	TArray<FString>& OutErrors,
	bool bSkipPreClean)
{
	OutErrors.Reset();

	// Pre-cook clean: delete stale cooked files unless explicitly skipped
	if (!bSkipPreClean)
	{
		bool bShouldClean = true;
		if (USGDynamicTextAssetSettingsAsset* settings = USGDynamicTextAssetSettings::GetSettings())
		{
			bShouldClean = settings->ShouldCleanCookedDirectoryBeforeCook();
		}

		if (bShouldClean)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("FSGDynamicTextAssetCookUtils: Pre-cook clean enabled, cleaning cooked directory..."));
			CleanCookedDirectory();
		}
	}
	else
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("FSGDynamicTextAssetCookUtils: Pre-cook clean skipped (-noclean)"));
	}

	// Collect all dynamic text asset files
	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	// Filter to supported files only and apply class filter
	TArray<FString> rawFilePaths;
	for (const FString& filePath : allFiles)
	{
		if (FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(filePath).IsEmpty())
		{
			continue;
		}

		// Apply class filter if specified
		if (!ClassFilter.IsEmpty())
		{
			FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);
			if (!fileInfo.bIsValid)
			{
				continue;
			}

			// Normalize: strip leading 'U' for comparison if present
			FString filterName = ClassFilter;
			FString fileInfoClassName = fileInfo.ClassName;

			if (filterName.Len() > 1 && filterName[0] == TEXT('U') && FChar::IsUpper(filterName[1]))
			{
				filterName = filterName.RightChop(1);
			}
			if (fileInfoClassName.Len() > 1 && fileInfoClassName[0] == TEXT('U') && FChar::IsUpper(fileInfoClassName[1]))
			{
				fileInfoClassName = fileInfoClassName.RightChop(1);
			}

			if (!fileInfoClassName.Equals(filterName, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		rawFilePaths.Add(filePath);
	}

	if (rawFilePaths.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FSGDynamicTextAssetCookUtils: No files found to cook"));
		return true;
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("FSGDynamicTextAssetCookUtils: Found %d file(s) to cook"), rawFilePaths.Num());

	// Validation pass
	int32 filesWithErrors = 0;
	for (const FString& filePath : rawFilePaths)
	{
		TArray<FString> errors;
		if (!ValidateDynamicTextAssetFile(filePath, errors))
		{
			filesWithErrors++;

			FString relativePath = filePath;
			FPaths::MakePathRelativeTo(relativePath, *FPaths::ProjectDir());

			for (const FString& error : errors)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: %s"), *relativePath, *error));
			}
		}
	}

	if (filesWithErrors > 0)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: %d file(s) failed validation, cook aborted"), filesWithErrors);
		return false;
	}

	// Get compression method from settings
	ESGDynamicTextAssetCompressionMethod compressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;
	if (USGDynamicTextAssetSettingsAsset* settings = USGDynamicTextAssetSettings::GetSettings())
	{
		compressionMethod = settings->GetDefaultCompressionMethod();
	}

	// Cook pass
	FSGDynamicTextAssetCookManifest manifest;
	int32 filesCooked = 0;
	int32 filesFailed = 0;

	for (const FString& filePath : rawFilePaths)
	{
		if (CookDynamicTextAssetFile(filePath, OutputDirectory, compressionMethod, manifest))
		{
			filesCooked++;
		}
		else
		{
			filesFailed++;

			FString relativePath = filePath;
			FPaths::MakePathRelativeTo(relativePath, *FPaths::ProjectDir());

			OutErrors.Add(FString::Printf(TEXT("Cook failed: %s"), *relativePath));
		}
	}

	// Save manifest to output directory
	if (filesCooked > 0)
	{
		if (!manifest.SaveToFileBinary(OutputDirectory))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetCookUtils: Failed to save cook manifest to: %s"), *OutputDirectory);
			OutErrors.Add(TEXT("Failed to save cook manifest"));
			return false;
		}

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("FSGDynamicTextAssetCookUtils: Manifest saved with %d entries"), manifest.Num());
	}

	// Copy _TypeManifest.json files to _TypeManifests/ subdirectory in cooked output
	if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
	{
		TArray<UClass*> rootClasses;
		registry->GetAllRegisteredBaseClasses(rootClasses);

		if (rootClasses.Num() > 0)
		{
			FString typeManifestsDir = FPaths::Combine(OutputDirectory, TEXT("_TypeManifests"));
			IPlatformFile& platformFileRef = FPlatformFileManager::Get().GetPlatformFile();
			platformFileRef.CreateDirectoryTree(*typeManifestsDir);

			int32 manifestsCopied = 0;
			for (const UClass* rootClass : rootClasses)
			{
				if (!rootClass)
				{
					continue;
				}

				const FSGDynamicTextAssetTypeManifest* typeManifest = registry->GetManifestForRootClass(rootClass);
				if (!typeManifest)
				{
					continue;
				}

				// Source file is {RootClassName}_TypeManifest.json alongside the class folder
				FString sourceManifestPath = USGDynamicTextAssetRegistry::GetRootClassManifestFilePath(rootClass);
				if (!platformFileRef.FileExists(*sourceManifestPath))
				{
					UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
						TEXT("FSGDynamicTextAssetCookUtils: Type manifest not found at '%s', skipping"), *sourceManifestPath);
					continue;
				}

				// Destination is _TypeManifests/{RootTypeId}.json
				FSGDynamicTextAssetTypeId rootTypeId = typeManifest->GetRootTypeId();
				if (!rootTypeId.IsValid())
				{
					UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
						TEXT("FSGDynamicTextAssetCookUtils: Root type ID not valid for class '%s', skipping type manifest copy"),
						*rootClass->GetName());
					continue;
				}

				FString destManifestPath = FPaths::Combine(typeManifestsDir, rootTypeId.ToString() + TEXT(".json"));
				if (platformFileRef.CopyFile(*destManifestPath, *sourceManifestPath))
				{
					manifestsCopied++;
					UE_LOG(LogSGDynamicTextAssetsEditor, Log,
						TEXT("  Copied type manifest: %s -> %s.json"), *rootClass->GetName(), *rootTypeId.ToString());
				}
				else
				{
					UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
						TEXT("FSGDynamicTextAssetCookUtils: Failed to copy type manifest for '%s'"), *rootClass->GetName());
				}
			}

			if (manifestsCopied > 0)
			{
				UE_LOG(LogSGDynamicTextAssetsEditor, Log,
					TEXT("FSGDynamicTextAssetCookUtils: Copied %d type manifest(s) to _TypeManifests/"), manifestsCopied);
			}
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("FSGDynamicTextAssetCookUtils: Cook complete - %d/%d succeeded, %d failed"),
		filesCooked, rawFilePaths.Num(), filesFailed);

	return filesFailed == 0;
}

FString FSGDynamicTextAssetCookUtils::GetCookedOutputRootPath()
{
	return FSGDynamicTextAssetFileManager::GetCookedDynamicTextAssetsRootPath();
}

void FSGDynamicTextAssetCookUtils::GatherSoftReferencesFromProperty(const FProperty* Property, const void* ContainerPtr, TSet<FName>& OutPackageNames)
{
	if (!Property || !ContainerPtr)
	{
		return;
	}

	// Instanced object properties own sub-objects whose properties may contain soft references.
	// Walk into the sub-object and recurse on its properties.
	if (const FObjectProperty* objectProp = CastField<FObjectProperty>(Property))
	{
		if (Property->HasAllPropertyFlags(CPF_InstancedReference))
		{
			const void* valuePtr = objectProp->ContainerPtrToValuePtr<void>(ContainerPtr);
			if (const UObject* subObject = objectProp->GetObjectPropertyValue(valuePtr))
			{
				for (TFieldIterator<FProperty> innerIt(subObject->GetClass()); innerIt; ++innerIt)
				{
					GatherSoftReferencesFromProperty(*innerIt, subObject, OutPackageNames);
				}
			}
			return;
		}
	}

	if (const FSoftObjectProperty* softObjProp = CastField<FSoftObjectProperty>(Property))
	{
		const void* valuePtr = softObjProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		if (const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr))
		{
			FSoftObjectPath path = softPtr->ToSoftObjectPath();
			if (path.IsValid() && !path.IsNull())
			{
				FName packageName = path.GetLongPackageFName();
				if (!packageName.IsNone())
				{
					FString packageStr = packageName.ToString();
					if (!packageStr.StartsWith(TEXT("/Script/")))
					{
						OutPackageNames.Add(packageName);
					}
				}
			}
		}
		return;
	}

	if (const FSoftClassProperty* softClassProp = CastField<FSoftClassProperty>(Property))
	{
		const void* valuePtr = softClassProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		if (const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr))
		{
			FSoftObjectPath path = softPtr->ToSoftObjectPath();
			if (path.IsValid() && !path.IsNull())
			{
				FName packageName = path.GetLongPackageFName();
				if (!packageName.IsNone())
				{
					FString packageStr = packageName.ToString();
					if (!packageStr.StartsWith(TEXT("/Script/")))
					{
						OutPackageNames.Add(packageName);
					}
				}
			}
		}
		return;
	}

	if (const FStructProperty* structProp = CastField<FStructProperty>(Property))
	{
		const void* structPtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		for (TFieldIterator<FProperty> innerIt(structProp->Struct); innerIt; ++innerIt)
		{
			GatherSoftReferencesFromProperty(*innerIt, structPtr, OutPackageNames);
		}
		return;
	}

	if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(Property))
	{
		const void* arrayPtr = arrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptArrayHelper arrayHelper(arrayProp, arrayPtr);

		for (int32 index = 0; index < arrayHelper.Num(); ++index)
		{
			GatherSoftReferencesFromProperty(arrayProp->Inner, arrayHelper.GetRawPtr(index), OutPackageNames);
		}
		return;
	}

	if (const FMapProperty* mapProp = CastField<FMapProperty>(Property))
	{
		const void* mapPtr = mapProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptMapHelper mapHelper(mapProp, mapPtr);

		for (FScriptMapHelper::FIterator itr = mapHelper.CreateIterator(); itr; ++itr)
		{
			GatherSoftReferencesFromProperty(mapProp->KeyProp, mapHelper.GetKeyPtr(itr.GetInternalIndex()), OutPackageNames);
			GatherSoftReferencesFromProperty(mapProp->ValueProp, mapHelper.GetValuePtr(itr.GetInternalIndex()), OutPackageNames);
		}
		return;
	}

	if (const FSetProperty* setProp = CastField<FSetProperty>(Property))
	{
		const void* setPtr = setProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptSetHelper setHelper(setProp, setPtr);

		for (FScriptSetHelper::FIterator itr = setHelper.CreateIterator(); itr; ++itr)
		{
			GatherSoftReferencesFromProperty(setProp->ElementProp, setHelper.GetElementPtr(itr.GetInternalIndex()), OutPackageNames);
		}
	}
}

int32 FSGDynamicTextAssetCookUtils::GatherSoftReferencesBySGDTBundle(TMap<FName, TArray<FName>>& OutBundlePackages)
{
	OutBundlePackages.Reset();

	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	if (allFiles.IsEmpty())
	{
		return 0;
	}

	// Track unique packages per bundle to avoid duplicates
	TMap<FName, TSet<FName>> bundlePackageSets;
	TSet<FName> allUniquePackages;
	int32 filesProcessed = 0;

	for (const FString& filePath : allFiles)
	{
		FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);
		if (!fileInfo.bIsValid || fileInfo.ClassName.IsEmpty())
		{
			continue;
		}

		UClass* resolvedClass = FindFirstObject<UClass>(*fileInfo.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
		if (!resolvedClass)
		{
			continue;
		}

		UObject* tempObject = NewObject<UObject>(GetTransientPackage(), resolvedClass);
		if (!tempObject)
		{
			continue;
		}

		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
		if (!serializer.IsValid())
		{
			continue;
		}

		FString fileContents;
		if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(filePath, fileContents))
		{
			continue;
		}

		TScriptInterface<ISGDynamicTextAssetProvider> provider(tempObject);
		if (!provider.GetInterface())
		{
			continue;
		}

		bool bMigrated = false;
		if (!serializer->DeserializeProvider(fileContents, provider.GetInterface(), bMigrated))
		{
			continue;
		}

		// Extract bundle data from the deserialized object
		FSGDynamicTextAssetBundleData bundleData;
		bundleData.ExtractFromObject(tempObject);

		// Collect bundled soft references by bundle name
		TSet<FName> bundledPackages;
		for (const FSGDynamicTextAssetBundle& bundle : bundleData.Bundles)
		{
			TSet<FName>& packageSet = bundlePackageSets.FindOrAdd(bundle.BundleName);
			for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
			{
				if (entry.AssetPath.IsValid() && !entry.AssetPath.IsNull())
				{
					FName packageName = entry.AssetPath.GetLongPackageFName();
					if (!packageName.IsNone())
					{
						FString packageStr = packageName.ToString();
						if (!packageStr.StartsWith(TEXT("/Script/")))
						{
							packageSet.Add(packageName);
							bundledPackages.Add(packageName);
							allUniquePackages.Add(packageName);
						}
					}
				}
			}
		}

		// Gather all soft references and put unbundled ones under NAME_None
		TSet<FName> allFilePackages;
		for (TFieldIterator<FProperty> propertyIt(resolvedClass); propertyIt; ++propertyIt)
		{
			GatherSoftReferencesFromProperty(*propertyIt, tempObject, allFilePackages);
		}

		TSet<FName>& unbundledSet = bundlePackageSets.FindOrAdd(NAME_None);
		for (const FName& packageName : allFilePackages)
		{
			if (!bundledPackages.Contains(packageName))
			{
				unbundledSet.Add(packageName);
				allUniquePackages.Add(packageName);
			}
		}

		filesProcessed++;
	}

	// Convert sets to output arrays
	for (const TPair<FName, TSet<FName>>& pair : bundlePackageSets)
	{
		if (!pair.Value.IsEmpty())
		{
			OutBundlePackages.Add(pair.Key, pair.Value.Array());
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("FSGDynamicTextAssetCookUtils: Gathered %d unique soft reference(s) across %d bundle(s) from %d/%d DTA file(s)"),
		allUniquePackages.Num(), OutBundlePackages.Num(), filesProcessed, allFiles.Num());

	return allUniquePackages.Num();
}

int32 FSGDynamicTextAssetCookUtils::GatherSoftReferencesFromAllFiles(TArray<FName>& OutPackageNames)
{
	OutPackageNames.Reset();

	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	if (allFiles.IsEmpty())
	{
		return 0;
	}

	TSet<FName> uniquePackages;
	int32 filesProcessed = 0;

	for (const FString& filePath : allFiles)
	{
		// Extract file info to get class name
		FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);
		if (!fileInfo.bIsValid || fileInfo.ClassName.IsEmpty())
		{
			continue;
		}

		// Resolve class
		UClass* resolvedClass = FindFirstObject<UClass>(*fileInfo.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
		if (!resolvedClass)
		{
			continue;
		}

		// Create transient instance
		UObject* tempObject = NewObject<UObject>(GetTransientPackage(), resolvedClass);
		if (!tempObject)
		{
			continue;
		}

		// Find serializer and deserialize
		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
		if (!serializer.IsValid())
		{
			continue;
		}

		FString fileContents;
		if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(filePath, fileContents))
		{
			continue;
		}

		TScriptInterface<ISGDynamicTextAssetProvider> provider(tempObject);
		if (!provider.GetInterface())
		{
			continue;
		}

		bool bMigrated = false;
		if (!serializer->DeserializeProvider(fileContents, provider.GetInterface(), bMigrated))
		{
			continue;
		}

		// Walk all properties to gather soft references
		for (TFieldIterator<FProperty> propertyIt(resolvedClass); propertyIt; ++propertyIt)
		{
			GatherSoftReferencesFromProperty(*propertyIt, tempObject, uniquePackages);
		}

		filesProcessed++;
	}

	// Convert set to output array
	OutPackageNames = uniquePackages.Array();

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("FSGDynamicTextAssetCookUtils: Gathered %d unique soft reference(s) from %d/%d DTA file(s)"),
		uniquePackages.Num(), filesProcessed, allFiles.Num());

	return uniquePackages.Num();
}
