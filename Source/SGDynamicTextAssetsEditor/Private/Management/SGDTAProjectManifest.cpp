// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDTAProjectManifest.h"

#include "SGDTAEditorLogs.h"
#include "Management/SGDTAFileManager.h"
#include "Serialization/SGDTASerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

static const FString MANIFEST_SCHEMA_ID = TEXT("dta_project_manifest");

bool FSGDTAProjectManifest::SaveToFile(const FString& FilePath) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema"), MANIFEST_SCHEMA_ID);
	rootObject->SetNumberField(TEXT("organizationVersion"), OrganizationVersion);

	// Type manifests array
	TArray<TSharedPtr<FJsonValue>> typeManifestValues;
	for (const FString& path : TypeManifestRelativePaths)
	{
		typeManifestValues.Add(MakeShared<FJsonValueString>(path));
	}
	rootObject->SetArrayField(TEXT("typeManifests"), typeManifestValues);

	// Extender manifests array
	TArray<TSharedPtr<FJsonValue>> extenderManifestValues;
	for (const FString& path : ExtenderManifestRelativePaths)
	{
		extenderManifestValues.Add(MakeShared<FJsonValueString>(path));
	}
	rootObject->SetArrayField(TEXT("extenderManifests"), extenderManifestValues);

	// Format versions
	TSharedRef<FJsonObject> formatVersionsObject = MakeShared<FJsonObject>();
	for (const TPair<FSGDTASerializerFormat, FSGDTASerializerFormatVersionInfo>& pair : FormatVersionsBySerializerId)
	{
		const FSGDTASerializerFormatVersionInfo& info = pair.Value;
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

		formatVersionsObject->SetObjectField(pair.Key.ToString(), serializerObject);
	}
	rootObject->SetObjectField(TEXT("formatVersions"), formatVersionsObject);

	FString outputString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outputString);
	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error,
			TEXT("FSGDTAProjectManifest::SaveToFile: Failed to serialize JSON"));
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
			TEXT("FSGDTAProjectManifest::SaveToFile: Failed to write file: %s"), *FilePath);
		return false;
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("FSGDTAProjectManifest: Saved to %s (orgVersion=%d, %d type manifests, %d extender manifests, %d serializers tracked)"),
		*FilePath, OrganizationVersion, TypeManifestRelativePaths.Num(),
		ExtenderManifestRelativePaths.Num(), FormatVersionsBySerializerId.Num());
	return true;
}

bool FSGDTAProjectManifest::LoadFromFile(const FString& FilePath)
{
	FString fileContents;
	if (!FFileHelper::LoadFileToString(fileContents, *FilePath))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
			TEXT("FSGDTAProjectManifest::LoadFromFile: File not found: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(fileContents);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
			TEXT("FSGDTAProjectManifest::LoadFromFile: Failed to parse JSON: %s"), *FilePath);
		return false;
	}

	// Validate schema
	FString schema;
	if (!rootObject->TryGetStringField(TEXT("schema"), schema) || schema != MANIFEST_SCHEMA_ID)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
			TEXT("FSGDTAProjectManifest::LoadFromFile: Invalid or missing schema field in %s (got '%s')"),
			*FilePath, *schema);
		return false;
	}

	OrganizationVersion = static_cast<int32>(rootObject->GetNumberField(TEXT("organizationVersion")));

	// Type manifests
	TypeManifestRelativePaths.Empty();
	const TArray<TSharedPtr<FJsonValue>>* typeManifestsArray;
	if (rootObject->TryGetArrayField(TEXT("typeManifests"), typeManifestsArray))
	{
		for (const TSharedPtr<FJsonValue>& value : *typeManifestsArray)
		{
			FString path;
			if (value->TryGetString(path))
			{
				TypeManifestRelativePaths.Add(MoveTemp(path));
			}
		}
	}

	// Extender manifests
	ExtenderManifestRelativePaths.Empty();
	const TArray<TSharedPtr<FJsonValue>>* extenderManifestsArray;
	if (rootObject->TryGetArrayField(TEXT("extenderManifests"), extenderManifestsArray))
	{
		for (const TSharedPtr<FJsonValue>& value : *extenderManifestsArray)
		{
			FString path;
			if (value->TryGetString(path))
			{
				ExtenderManifestRelativePaths.Add(MoveTemp(path));
			}
		}
	}

	// Format versions
	FormatVersionsBySerializerId.Empty();
	const TSharedPtr<FJsonObject>* formatVersionsPtr;
	if (rootObject->TryGetObjectField(TEXT("formatVersions"), formatVersionsPtr) && formatVersionsPtr->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : (*formatVersionsPtr)->Values)
		{
			FSGDTASerializerFormat serializerFormat(static_cast<uint32>(FCString::Atoi(*pair.Key)));
			const TSharedPtr<FJsonObject>& serializerObject = pair.Value->AsObject();
			if (!serializerObject.IsValid())
			{
				continue;
			}

			FSGDTASerializerFormatVersionInfo info;
			info.SerializerFormat = serializerFormat;
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

			FormatVersionsBySerializerId.Add(serializerFormat, MoveTemp(info));
		}
	}

	bIsLoaded = true;
	bFormatVersionsDirty = false;

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("FSGDTAProjectManifest: Loaded from %s (orgVersion=%d, %d type manifests, %d extender manifests, %d serializers tracked)"),
		*FilePath, OrganizationVersion, TypeManifestRelativePaths.Num(),
		ExtenderManifestRelativePaths.Num(), FormatVersionsBySerializerId.Num());
	return true;
}

FString FSGDTAProjectManifest::GetDefaultManifestPath()
{
	return FPaths::Combine(FSGDynamicTextAssetFileManager::GetInternalFilesRootPath(), TEXT("ProjectManifest.json"));
}

int32 FSGDTAProjectManifest::GetOrganizationVersion() const
{
	return OrganizationVersion;
}

void FSGDTAProjectManifest::SetOrganizationVersion(int32 InVersion)
{
	OrganizationVersion = InVersion;
}

bool FSGDTAProjectManifest::IsCurrentVersion() const
{
	return OrganizationVersion == CURRENT_ORGANIZATION_VERSION;
}

bool FSGDTAProjectManifest::IsLoaded() const
{
	return bIsLoaded;
}

const TArray<FString>& FSGDTAProjectManifest::GetTypeManifestPaths() const
{
	return TypeManifestRelativePaths;
}

void FSGDTAProjectManifest::SetTypeManifestPaths(const TArray<FString>& InPaths)
{
	TypeManifestRelativePaths = InPaths;
}

const TArray<FString>& FSGDTAProjectManifest::GetExtenderManifestPaths() const
{
	return ExtenderManifestRelativePaths;
}

void FSGDTAProjectManifest::SetExtenderManifestPaths(const TArray<FString>& InPaths)
{
	ExtenderManifestRelativePaths = InPaths;
}

FString FSGDTAProjectManifest::ResolveRelativePath(const FString& RelativePath)
{
	return FPaths::Combine(FSGDynamicTextAssetFileManager::GetInternalFilesRootPath(), RelativePath);
}

FString FSGDTAProjectManifest::MakeRelativePath(const FString& AbsolutePath)
{
	FString relativePath = AbsolutePath;
	FString basePath = FSGDynamicTextAssetFileManager::GetInternalFilesRootPath() / TEXT("");
	FPaths::MakePathRelativeTo(relativePath, *basePath);
	return relativePath;
}

void FSGDTAProjectManifest::RecordFileVersion(const FSGDTASerializerFormat& SerializerFormat, const FSGDynamicTextAssetVersion& FileFormatVersion)
{
	if (!SerializerFormat.IsValid())
	{
		return;
	}

	FSGDTASerializerFormatVersionInfo& info = FormatVersionsBySerializerId.FindOrAdd(SerializerFormat);

	// Initialize if new entry
	if (!info.SerializerFormat.IsValid())
	{
		info.SerializerFormat = SerializerFormat;

		// Populate serializer metadata from registered serializer
		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFormat(SerializerFormat);
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

void FSGDTAProjectManifest::PopulateCurrentSerializerVersions()
{
	for (TPair<FSGDTASerializerFormat, FSGDTASerializerFormatVersionInfo>& pair : FormatVersionsBySerializerId)
	{
		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFormat(pair.Key);
		if (serializer.IsValid())
		{
			pair.Value.CurrentSerializerVersion = serializer->GetFileFormatVersion();
			pair.Value.SerializerName = serializer->GetFormatName_String();
			pair.Value.FileExtension = serializer->GetFileExtension();
		}
	}
}

const FSGDTASerializerFormatVersionInfo* FSGDTAProjectManifest::GetInfoForSerializer(const FSGDTASerializerFormat& SerializerFormat) const
{
	return FormatVersionsBySerializerId.Find(SerializerFormat);
}

const TMap<FSGDTASerializerFormat, FSGDTASerializerFormatVersionInfo>& FSGDTAProjectManifest::GetAllFormatVersions() const
{
	return FormatVersionsBySerializerId;
}

void FSGDTAProjectManifest::ResetFormatVersionData()
{
	FormatVersionsBySerializerId.Empty();
	bFormatVersionsDirty = true;
}

void FSGDTAProjectManifest::MarkFormatVersionsDirty()
{
	bFormatVersionsDirty = true;
}

bool FSGDTAProjectManifest::FormatVersionsNeedScan() const
{
	return !bIsLoaded || bFormatVersionsDirty;
}
