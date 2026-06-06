// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDTASerializerExtenderRegistry.h"

#include "Management/SGDTAExtenderManifest.h"
#include "Statics/SGDTAConstants.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SGDynamicTextAssetLogs.h"

FSGDTASerializerExtenderRegistryEntry::FSGDTASerializerExtenderRegistryEntry(
	const FSGDTAClassId& InExtenderId,
	const TSoftClassPtr<UObject>& InClass)
	: ExtenderId(InExtenderId)
	, Class(InClass)
{
	// Extract class name from the soft class path for fast lookup
	FString classPath = InClass.ToString();
	if (!classPath.IsEmpty())
	{
		int32 lastDot = INDEX_NONE;
		if (classPath.FindLastChar(TEXT('.'), lastDot))
		{
			ClassName = classPath.Mid(lastDot + 1);
		}
		else
		{
			ClassName = classPath;
		}
	}
}

TSharedPtr<FSGDTAExtenderManifest> FSGDTASerializerExtenderRegistry::GetOrCreateManifest(FName FrameworkKey)
{
	if (TSharedRef<FSGDTAExtenderManifest>* Existing = ManagedManifests.Find(FrameworkKey))
	{
		return *Existing;
	}

	TSharedRef<FSGDTAExtenderManifest> NewManifest = MakeShared<FSGDTAExtenderManifest>(FrameworkKey);
	ManagedManifests.Add(FrameworkKey, NewManifest);
	return NewManifest;
}

TSharedPtr<FSGDTAExtenderManifest> FSGDTASerializerExtenderRegistry::GetManifest(FName FrameworkKey) const
{
	if (const TSharedRef<FSGDTAExtenderManifest>* Found = ManagedManifests.Find(FrameworkKey))
	{
		return *Found;
	}
	return nullptr;
}

TArray<FName> FSGDTASerializerExtenderRegistry::GetAllManifestKeys() const
{
	TArray<FName> Keys;
	ManagedManifests.GetKeys(Keys);
	return Keys;
}

bool FSGDTASerializerExtenderRegistry::LoadAllManifests(const FString& Directory)
{
	ManagedManifests.Empty();

	TArray<FString> foundFiles;
	IFileManager::Get().FindFiles(foundFiles, *(FPaths::Combine(Directory, TEXT("DTA_*_Extenders"))
		+ SGDynamicTextAssetConstants::JSON_FILE_EXTENSION), true, false);

	if (foundFiles.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
			TEXT("FSGDTASerializerExtenderRegistry: No manifest files found in '%s'"), *Directory);
		return false;
	}

	int32 loadedCount = 0;

	for (const FString& FileName : foundFiles)
	{
		// Extract framework key from filename: DTA_{FrameworkKey}_Extenders.dta.json
		// Strip the full compound extension first, then parse prefix/suffix
		static const FString PREFIX = TEXT("DTA_");
		static const FString SUFFIX = TEXT("_Extenders");
		const FString fullExtension = SUFFIX + SGDynamicTextAssetConstants::JSON_FILE_EXTENSION;

		if (!FileName.StartsWith(PREFIX) || !FileName.EndsWith(fullExtension))
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTASerializerExtenderRegistry: Skipping file with unexpected name format: '%s'"), *FileName);
			continue;
		}

		const FString frameworkKeyString = FileName.Mid(PREFIX.Len(), FileName.Len() - PREFIX.Len() - fullExtension.Len());
		if (frameworkKeyString.IsEmpty())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTASerializerExtenderRegistry: Skipping file with empty framework key: '%s'"), *FileName);
			continue;
		}

		const FName frameworkKey(*frameworkKeyString);
		const FString fullPath = FPaths::Combine(Directory, FileName);

		TSharedPtr<FSGDTAExtenderManifest> manifest = GetOrCreateManifest(frameworkKey);
		if (manifest->LoadFromFile(fullPath))
		{
			++loadedCount;
		}
		else
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTASerializerExtenderRegistry: Failed to load manifest from '%s'"), *fullPath);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDTASerializerExtenderRegistry: Loaded %d/%d manifest files from '%s'"),
		loadedCount, foundFiles.Num(), *Directory);

	return loadedCount > 0;
}

bool FSGDTASerializerExtenderRegistry::SaveAllManifests(const FString& Directory) const
{
	bool bAllSucceeded = true;

	for (const TPair<FName, TSharedRef<FSGDTAExtenderManifest>>& Pair : ManagedManifests)
	{
		if (!Pair.Value->IsDirty())
		{
			continue;
		}

		const FString fileName = TEXT("DTA_") + Pair.Key.ToString()
			+ TEXT("_Extenders") + SGDynamicTextAssetConstants::JSON_FILE_EXTENSION;
		const FString fullPath = FPaths::Combine(Directory, fileName);

		if (!Pair.Value->SaveToFile(fullPath))
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDTASerializerExtenderRegistry: Failed to save manifest '%s' to '%s'"),
				*Pair.Key.ToString(), *fullPath);
			bAllSucceeded = false;
		}
	}

	return bAllSucceeded;
}

bool FSGDTASerializerExtenderRegistry::LoadAllManifestsBinary(const FString& Directory)
{
	ManagedManifests.Empty();

	TArray<FString> foundFiles;
	IFileManager::Get().FindFiles(foundFiles, *(FPaths::Combine(Directory, TEXT("DTA_*_Extenders"))
		+ SGDynamicTextAssetConstants::BINARY_FILE_EXTENSION), true, false);

	if (foundFiles.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
			TEXT("FSGDTASerializerExtenderRegistry: No binary manifest files found in '%s'"), *Directory);
		return false;
	}

	int32 loadedCount = 0;

	for (const FString& FileName : foundFiles)
	{
		// Extract framework key from filename: DTA_{FrameworkKey}_Extenders.dta.bin
		// Strip the full compound extension first, then parse prefix/suffix
		static const FString PREFIX = TEXT("DTA_");
		static const FString SUFFIX = TEXT("_Extenders");
		const FString fullExtension = SUFFIX + SGDynamicTextAssetConstants::BINARY_FILE_EXTENSION;

		if (!FileName.StartsWith(PREFIX) || !FileName.EndsWith(fullExtension))
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTASerializerExtenderRegistry: Skipping binary file with unexpected name format: '%s'"), *FileName);
			continue;
		}

		FString frameworkKeyString = FileName.Mid(PREFIX.Len(), FileName.Len() - PREFIX.Len() - fullExtension.Len());
		if (frameworkKeyString.IsEmpty())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTASerializerExtenderRegistry: Skipping binary file with empty framework key: '%s'"), *FileName);
			continue;
		}

		FName frameworkKey(*frameworkKeyString);
		FString fullPath = FPaths::Combine(Directory, FileName);

		TSharedPtr<FSGDTAExtenderManifest> manifest = GetOrCreateManifest(frameworkKey);
		if (manifest->LoadFromBinaryFile(fullPath))
		{
			++loadedCount;
		}
		else
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDTASerializerExtenderRegistry: Failed to load binary manifest from '%s'"), *fullPath);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
		TEXT("FSGDTASerializerExtenderRegistry: Loaded %d/%d binary manifest files from '%s'"),
		loadedCount, foundFiles.Num(), *Directory);

	return loadedCount > 0;
}

bool FSGDTASerializerExtenderRegistry::BakeAllManifests(const FString& CookedDirectory) const
{
	bool bAllSucceeded = true;

	for (const TPair<FName, TSharedRef<FSGDTAExtenderManifest>>& Pair : ManagedManifests)
	{
		const FString fileName = TEXT("DTA_") + Pair.Key.ToString() + TEXT("_Extenders") + SGDynamicTextAssetConstants::BINARY_FILE_EXTENSION;
		const FString fullPath = FPaths::Combine(CookedDirectory, fileName);

		if (!Pair.Value->SaveToBinaryFile(fullPath))
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDTASerializerExtenderRegistry: Failed to bake manifest '%s' to '%s'"),
				*Pair.Key.ToString(), *fullPath);
			bAllSucceeded = false;
		}
		else
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
				TEXT("FSGDTASerializerExtenderRegistry: Baked manifest '%s' to '%s'"),
				*Pair.Key.ToString(), *fullPath);
		}
	}

	return bAllSucceeded;
}

bool FSGDTASerializerExtenderRegistry::HasAnyDirty() const
{
	for (const TPair<FName, TSharedRef<FSGDTAExtenderManifest>>& Pair : ManagedManifests)
	{
		if (Pair.Value->IsDirty())
		{
			return true;
		}
	}
	return false;
}

void FSGDTASerializerExtenderRegistry::ClearAllManifests()
{
	ManagedManifests.Empty();
}

int32 FSGDTASerializerExtenderRegistry::NumManifests() const
{
	return ManagedManifests.Num();
}

bool FSGDTASerializerExtenderRegistry::IsEmptyManfiests() const
{
	return ManagedManifests.IsEmpty();
}

FSGDTAClassId FSGDTASerializerExtenderRegistry::FindExtenderIdByClass(const UClass* InClass) const
{
	if (!InClass)
	{
		return FSGDTAClassId::INVALID_CLASS_ID;
	}

	const FString className = InClass->GetName();

	for (const TPair<FName, TSharedRef<FSGDTAExtenderManifest>>& pair : ManagedManifests)
	{
		if (const FSGDTASerializerExtenderRegistryEntry* entry = pair.Value->FindByClassName(className))
		{
			return entry->ExtenderId;
		}
	}

	return FSGDTAClassId::INVALID_CLASS_ID;
}
