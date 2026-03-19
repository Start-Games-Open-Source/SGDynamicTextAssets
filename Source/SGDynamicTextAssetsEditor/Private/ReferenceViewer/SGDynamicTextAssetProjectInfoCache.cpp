// Copyright Start Games, Inc. All Rights Reserved.

#include "ReferenceViewer/SGDynamicTextAssetProjectInfoCache.h"

#include "SGDynamicTextAssetEditorLogs.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#include "Settings/SGDynamicTextAssetEditorSettings.h"

bool FSGDynamicTextAssetProjectInfoCache::SaveToFile(const FString& FilePath) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetNumberField(TEXT("version"), CacheVersion);
	rootObject->SetStringField(TEXT("savedAt"), FDateTime::UtcNow().ToIso8601());

	TSharedRef<FJsonObject> formatVersionsObject = MakeShared<FJsonObject>();

	for (const TPair<uint32, FSGSerializerFormatVersionInfo>& pair : FormatVersionsBySerializerId)
	{
		const FSGSerializerFormatVersionInfo& info = pair.Value;
		TSharedRef<FJsonObject> serializerObject = MakeShared<FJsonObject>();

		serializerObject->SetStringField(TEXT("serializerName"), info.SerializerName);
		serializerObject->SetStringField(TEXT("fileExtension"), info.FileExtension);
		serializerObject->SetStringField(TEXT("currentSerializerVersion"), info.CurrentSerializerVersion.ToString());
		serializerObject->SetStringField(TEXT("highestFound"), info.HighestFound.ToString());
		serializerObject->SetStringField(TEXT("lowestFound"), info.LowestFound.ToString());
		serializerObject->SetNumberField(TEXT("totalFileCount"), info.TotalFileCount);

		TSharedRef<FJsonObject> countByVersionObject = MakeShared<FJsonObject>();
		for (const TPair<FString, int32>& versionPair : info.CountByVersion)
		{
			countByVersionObject->SetNumberField(versionPair.Key, versionPair.Value);
		}
		serializerObject->SetObjectField(TEXT("countByVersion"), countByVersionObject);

		formatVersionsObject->SetObjectField(FString::FromInt(pair.Key), serializerObject);
	}

	rootObject->SetObjectField(TEXT("formatVersions"), formatVersionsObject);

	FString outputString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outputString);
	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error,
			TEXT("FSGDynamicTextAssetProjectInfoCache::SaveToFile: Failed to serialize JSON"));
		return false;
	}

	// Ensure directory exists
	const FString directory = FPaths::GetPath(FilePath);
	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!platformFile.DirectoryExists(*directory))
	{
		platformFile.CreateDirectoryTree(*directory);
	}

	if (!FFileHelper::SaveStringToFile(outputString, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error,
			TEXT("FSGDynamicTextAssetProjectInfoCache::SaveToFile: Failed to write file: %s"), *FilePath);
		return false;
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("FSGDynamicTextAssetProjectInfoCache: Saved to %s (%d serializers tracked)"),
		*FilePath, FormatVersionsBySerializerId.Num());
	return true;
}

bool FSGDynamicTextAssetProjectInfoCache::LoadFromFile(const FString& FilePath)
{
	FString fileContents;
	if (!FFileHelper::LoadFileToString(fileContents, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
			TEXT("FSGDynamicTextAssetProjectInfoCache::LoadFromFile: File not found: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(fileContents);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
			TEXT("FSGDynamicTextAssetProjectInfoCache::LoadFromFile: Failed to parse JSON: %s"), *FilePath);
		return false;
	}

	CacheVersion = static_cast<int32>(rootObject->GetNumberField(TEXT("version")));

	FString savedAtStr;
	if (rootObject->TryGetStringField(TEXT("savedAt"), savedAtStr))
	{
		FDateTime::ParseIso8601(*savedAtStr, SavedAt);
	}

	FormatVersionsBySerializerId.Empty();

	const TSharedPtr<FJsonObject>* formatVersionsPtr;
	if (rootObject->TryGetObjectField(TEXT("formatVersions"), formatVersionsPtr) && formatVersionsPtr->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : (*formatVersionsPtr)->Values)
		{
			uint32 serializerTypeId = static_cast<uint32>(FCString::Atoi(*pair.Key));
			const TSharedPtr<FJsonObject>& serializerObject = pair.Value->AsObject();
			if (!serializerObject.IsValid())
			{
				continue;
			}

			FSGSerializerFormatVersionInfo info;
			info.SerializerTypeId = serializerTypeId;
			serializerObject->TryGetStringField(TEXT("serializerName"), info.SerializerName);
			serializerObject->TryGetStringField(TEXT("fileExtension"), info.FileExtension);

			FString versionStr;
			if (serializerObject->TryGetStringField(TEXT("currentSerializerVersion"), versionStr))
			{
				info.CurrentSerializerVersion = FSGDynamicTextAssetVersion::ParseFromString(versionStr);
			}
			if (serializerObject->TryGetStringField(TEXT("highestFound"), versionStr))
			{
				info.HighestFound = FSGDynamicTextAssetVersion::ParseFromString(versionStr);
			}
			if (serializerObject->TryGetStringField(TEXT("lowestFound"), versionStr))
			{
				info.LowestFound = FSGDynamicTextAssetVersion::ParseFromString(versionStr);
			}

			info.TotalFileCount = static_cast<int32>(serializerObject->GetNumberField(TEXT("totalFileCount")));

			const TSharedPtr<FJsonObject>* countByVersionPtr;
			if (serializerObject->TryGetObjectField(TEXT("countByVersion"), countByVersionPtr) && countByVersionPtr->IsValid())
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& countPair : (*countByVersionPtr)->Values)
				{
					info.CountByVersion.Add(countPair.Key, static_cast<int32>(countPair.Value->AsNumber()));
				}
			}

			FormatVersionsBySerializerId.Add(serializerTypeId, MoveTemp(info));
		}
	}

	bIsLoaded = true;
	bIsDirty = false;

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("FSGDynamicTextAssetProjectInfoCache: Loaded from %s (%d serializers tracked)"),
		*FilePath, FormatVersionsBySerializerId.Num());
	return true;
}

bool FSGDynamicTextAssetProjectInfoCache::IsValid() const
{
	return bIsLoaded && !bIsDirty;
}

bool FSGDynamicTextAssetProjectInfoCache::NeedsScan() const
{
	return !bIsLoaded || bIsDirty;
}

void FSGDynamicTextAssetProjectInfoCache::Reset()
{
	FormatVersionsBySerializerId.Empty();
	bIsDirty = true;
}

void FSGDynamicTextAssetProjectInfoCache::MarkDirty()
{
	bIsDirty = true;
}

void FSGDynamicTextAssetProjectInfoCache::RecordFileVersion(uint32 SerializerTypeId, const FSGDynamicTextAssetVersion& FileFormatVersion)
{
	if (SerializerTypeId == ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
	{
		return;
	}

	FSGSerializerFormatVersionInfo& info = FormatVersionsBySerializerId.FindOrAdd(SerializerTypeId);

	// Initialize if new entry
	if (info.SerializerTypeId == 0)
	{
		info.SerializerTypeId = SerializerTypeId;

		// Populate serializer metadata from registered serializer
		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForTypeId(SerializerTypeId);
		if (serializer.IsValid())
		{
			info.SerializerName = serializer->GetFormatName_String();
			info.FileExtension = serializer->GetFileExtension();
			info.CurrentSerializerVersion = serializer->GetFileFormatVersion();
		}

		info.HighestFound = FileFormatVersion;
		info.LowestFound = FileFormatVersion;
	}
	else
	{
		// Update highest/lowest
		if (FileFormatVersion > info.HighestFound)
		{
			info.HighestFound = FileFormatVersion;
		}
		if (FileFormatVersion < info.LowestFound)
		{
			info.LowestFound = FileFormatVersion;
		}
	}

	info.TotalFileCount++;

	const FString versionKey = FileFormatVersion.ToString();
	int32& count = info.CountByVersion.FindOrAdd(versionKey);
	count++;
}

void FSGDynamicTextAssetProjectInfoCache::PopulateCurrentSerializerVersions()
{
	for (TPair<uint32, FSGSerializerFormatVersionInfo>& pair : FormatVersionsBySerializerId)
	{
		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForTypeId(pair.Key);
		if (serializer.IsValid())
		{
			pair.Value.CurrentSerializerVersion = serializer->GetFileFormatVersion();
			pair.Value.SerializerName = serializer->GetFormatName_String();
			pair.Value.FileExtension = serializer->GetFileExtension();
		}
	}
}

const FSGSerializerFormatVersionInfo* FSGDynamicTextAssetProjectInfoCache::GetInfoForSerializer(uint32 SerializerTypeId) const
{
	return FormatVersionsBySerializerId.Find(SerializerTypeId);
}

const TMap<uint32, FSGSerializerFormatVersionInfo>& FSGDynamicTextAssetProjectInfoCache::GetAllFormatVersions() const
{
	return FormatVersionsBySerializerId;
}

FString FSGDynamicTextAssetProjectInfoCache::GetDefaultCachePath()
{
	return USGDynamicTextAssetEditorSettings::Get()->GetCacheRootFolder() / TEXT("ProjectInfoCache.json");
}
