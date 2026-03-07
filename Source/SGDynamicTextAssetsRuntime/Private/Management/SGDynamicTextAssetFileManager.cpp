// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDynamicTextAssetFileManager.h"

#include "SGDynamicTextAssetsRuntimeModule.h"
#include "Core/SGDynamicTextAsset.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Management/SGDynamicTextAssetCookManifest.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

// Direct JSON includes are intentional for ExtractMetadataFromFile fallback paths only.
// These are last-resort parsers for files whose registered serializer is no longer available.
// All other JSON operations are routed through ISGDynamicTextAssetSerializer::UpdateFieldsInPlace
// or ISGDynamicTextAssetSerializer::ExtractMetadata on the registered serializer instance.
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/SGDynamicTextAssetBinarySerializer.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"

const FString FSGDynamicTextAssetFileManager::DEFAULT_TEXT_EXTENSION = TEXT(".dta.json");
const FString FSGDynamicTextAssetFileManager::BINARY_EXTENSION = TEXT(".dta.bin");
const FString FSGDynamicTextAssetFileManager::DEFAULT_RELATIVE_ROOT_PATH = TEXT("SGDynamicTextAssets");

FSGDataGenerateDefaultContentDelegate FSGDynamicTextAssetFileManager::ON_GENERATE_DEFAULT_CONTENT;

TMap<FString, TSharedRef<ISGDynamicTextAssetSerializer>> FSGDynamicTextAssetFileManager::REGISTERED_SERIALIZERS;
TMap<uint32, TSharedRef<ISGDynamicTextAssetSerializer>> FSGDynamicTextAssetFileManager::REGISTERED_SERIALIZERS_BY_ID;

FString FSGDynamicTextAssetFileManager::GetDynamicTextAssetsRootPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(), GetDynamicTextAssetsRelativeRootPath());
}

FString FSGDynamicTextAssetFileManager::GetDynamicTextAssetsRelativeRootPath()
{
    return DEFAULT_RELATIVE_ROOT_PATH;
}

FString FSGDynamicTextAssetFileManager::GetFolderPathForClass(const UClass* DynamicTextAssetClass)
{
    if (!DynamicTextAssetClass)
    {
        return GetDynamicTextAssetsRootPath();
    }

    return FPaths::Combine(GetDynamicTextAssetsRootPath(), BuildClassHierarchyPath(DynamicTextAssetClass));
}

FString FSGDynamicTextAssetFileManager::GetRelativeFolderPathForClass(const UClass* DynamicTextAssetClass)
{
    if (!DynamicTextAssetClass)
    {
        return GetDynamicTextAssetsRelativeRootPath();
    }

    return FPaths::Combine(GetDynamicTextAssetsRelativeRootPath(), BuildClassHierarchyPath(DynamicTextAssetClass));
}

FString FSGDynamicTextAssetFileManager::BuildFilePath(const UClass* DynamicTextAssetClass, const FString& UserFacingId, const FString& Extension)
{
    const FString sanitizedId = SanitizeUserFacingId(UserFacingId);
    const FString folderPath = GetFolderPathForClass(DynamicTextAssetClass);

    return FPaths::Combine(folderPath, sanitizedId + Extension);
}

FString FSGDynamicTextAssetFileManager::BuildRelativeFilePath(const UClass* DynamicTextAssetClass, const FString& UserFacingId, const FString& Extension)
{
    const FString sanitizedId = SanitizeUserFacingId(UserFacingId);
    const FString folderPath = GetRelativeFolderPathForClass(DynamicTextAssetClass);

    return FPaths::Combine(folderPath, sanitizedId + Extension);
}

void FSGDynamicTextAssetFileManager::FindAllFilesForClass(const UClass* DynamicTextAssetClass, TArray<FString>& OutFilePaths, bool bIncludeSubclasses)
{
    OutFilePaths.Reset();

    if (!DynamicTextAssetClass)
    {
        return;
    }

    // Cooked builds: query manifest for matching class names
    if (ShouldUseCookedDirectory())
    {
        const FSGDynamicTextAssetCookManifest* manifest = GetCookManifest();
        if (manifest != nullptr && manifest->IsLoaded())
        {
            // Collect class names to search for
            TArray<FString> classNames;
            classNames.Add(GetNameSafe(DynamicTextAssetClass));

            if (bIncludeSubclasses)
            {
                for (TObjectIterator<UClass> itr; itr; ++itr)
                {
                    if (itr->IsChildOf(DynamicTextAssetClass) && *itr != DynamicTextAssetClass)
                    {
                        classNames.Add(GetNameSafe(*itr));
                    }
                }
            }

            // Query manifest for each class name and build file paths
            for (const FString& className : classNames)
            {
                TArray<const FSGDynamicTextAssetCookManifestEntry*> entries;
                manifest->FindByClassName(className, entries);

                for (const FSGDynamicTextAssetCookManifestEntry* entry : entries)
                {
                    FString binaryPath = BuildBinaryFilePath(entry->Id);
                    if (FileExists(binaryPath))
                    {
                        OutFilePaths.Add(binaryPath);
                    }
                }
            }

            return;
        }

        // Manifest unavailable, fall through to editor path as fallback
    }

    // Editor: folder scan (recursive includes subclass folders, non-recursive excludes them)
    const FString folderPath = GetFolderPathForClass(DynamicTextAssetClass);

    IFileManager& fileManager = IFileManager::Get();

    // Find registered extension files
    TArray<FString> extensions;
    GetAllRegisteredExtensions(extensions);
    for (const FString& ext : extensions)
    {
        TArray<FString> extFiles;
        if (bIncludeSubclasses)
        {
            fileManager.FindFilesRecursive(extFiles, *folderPath, *(TEXT("*") + ext), true, false);
        }
        else
        {
            fileManager.FindFiles(extFiles, *(folderPath / TEXT("*") + ext), true, false);
            // FindFiles returns filenames only, convert to full paths
            for (FString& fileName : extFiles)
            {
                fileName = folderPath / fileName;
            }
        }
        OutFilePaths.Append(extFiles);
    }

    // Find binary files
    TArray<FString> binaryFiles;
    if (bIncludeSubclasses)
    {
        fileManager.FindFilesRecursive(binaryFiles, *folderPath, *(TEXT("*") + BINARY_EXTENSION), true, false);
    }
    else
    {
        fileManager.FindFiles(binaryFiles, *(folderPath / TEXT("*") + BINARY_EXTENSION), true, false);
        // FindFiles returns filenames only, convert to full paths
        for (FString& fileName : binaryFiles)
        {
            fileName = folderPath / fileName;
        }
    }
    OutFilePaths.Append(binaryFiles);
}

void FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(TArray<FString>& OutFilePaths)
{
    OutFilePaths.Reset();

    // Cooked builds: flat scan of cooked directory for .dta.bin files
    if (ShouldUseCookedDirectory())
    {
        const FString cookedRoot = GetCookedDynamicTextAssetsRootPath();
        IFileManager::Get().FindFilesRecursive(OutFilePaths, *cookedRoot, *(TEXT("*") + BINARY_EXTENSION), true, false);
        return;
    }

    // Editor: recursive scan of source directory for JSON + binary
    const FString rootPath = GetDynamicTextAssetsRootPath();
    IFileManager& fileManager = IFileManager::Get();

    // Find registered extension files
    TArray<FString> extensions;
    GetAllRegisteredExtensions(extensions);
    for (const FString& ext : extensions)
    {
        TArray<FString> extFiles;
        fileManager.FindFilesRecursive(extFiles, *rootPath, *(TEXT("*") + ext), true, false);
        OutFilePaths.Append(extFiles);
    }

    TArray<FString> binaryFiles;
    fileManager.FindFilesRecursive(binaryFiles, *rootPath, *(TEXT("*") + BINARY_EXTENSION), true, false);
    OutFilePaths.Append(binaryFiles);
}

FString FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(const FString& FilePath)
{
    FString filename = FPaths::GetCleanFilename(FilePath);

    // Remove .dta.bin explicitly if it's the cooked format
    if (filename.EndsWith(BINARY_EXTENSION))
    {
        filename.LeftChopInline(BINARY_EXTENSION.Len());
        return filename;
    }

    // Remove registered extensions
    FString extension = GetSupportedExtensionForFile(FilePath);
    if (!extension.IsEmpty() && filename.EndsWith(extension))
    {
        filename.LeftChopInline(extension.Len());
    }

    return filename;
}

bool FSGDynamicTextAssetFileManager::IsDynamicTextAssetFile(const FString& FilePath)
{
    if (FilePath.EndsWith(BINARY_EXTENSION))
    {
        return true;
    }
    
    return !GetSupportedExtensionForFile(FilePath).IsEmpty();
}

FString FSGDynamicTextAssetFileManager::SanitizeUserFacingId(const FString& UserFacingId)
{
    FString result = UserFacingId;

    // Replace spaces with underscores
    result.ReplaceInline(TEXT(" "), TEXT("_"));

    // Remove invalid filename characters
    result.ReplaceInline(TEXT("/"), TEXT("_"));
    result.ReplaceInline(TEXT("\\"), TEXT("_"));
    result.ReplaceInline(TEXT(":"), TEXT("_"));
    result.ReplaceInline(TEXT("*"), TEXT("_"));
    result.ReplaceInline(TEXT("?"), TEXT("_"));
    result.ReplaceInline(TEXT("\""), TEXT("_"));
    result.ReplaceInline(TEXT("<"), TEXT("_"));
    result.ReplaceInline(TEXT(">"), TEXT("_"));
    result.ReplaceInline(TEXT("|"), TEXT("_"));

    return result;
}

FSGDynamicTextAssetFileMetadata FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(const FString& FilePath)
{
    FSGDynamicTextAssetFileMetadata metadata;

    if (FilePath.IsEmpty() || !IsDynamicTextAssetFile(FilePath))
    {
        return metadata;
    }

    // Binary files: extract ID from header without decompression
    if (FilePath.EndsWith(BINARY_EXTENSION))
    {
        TArray<uint8> binaryData;
        if (!FSGDynamicTextAssetBinarySerializer::ReadBinaryFile(FilePath, binaryData))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Failed to read binary file at FilePath(%s)"), *FilePath);
            return metadata;
        }

        // Read header (80 bytes, no decompression)
        FSGBinaryDynamicTextAssetHeader header;
        if (!FSGDynamicTextAssetBinarySerializer::ReadHeader(binaryData, header))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Failed to read header from binary file at FilePath(%s)"), *FilePath);
            return metadata;
        }

        metadata.Id = header.Guid;

        // Extract AssetTypeId directly from header (no decompression needed)
        if (header.HasAssetTypeGuid())
        {
            metadata.AssetTypeId = FSGDynamicTextAssetTypeId::FromGuid(header.AssetTypeGuid);
        }

        // In cooked builds, look up ClassName + UserFacingId from manifest (no decompression)
        if (ShouldUseCookedDirectory())
        {
            const FSGDynamicTextAssetCookManifest* manifest = GetCookManifest();
            if (manifest != nullptr)
            {
                const FSGDynamicTextAssetCookManifestEntry* entry = manifest->FindById(metadata.Id);
                if (entry != nullptr)
                {
                    metadata.ClassName = entry->ClassName;
                    metadata.UserFacingId = entry->UserFacingId;

                    // If AssetTypeId not available from header, derive from ClassName via registry
                    if (!metadata.AssetTypeId.IsValid())
                    {
                        if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
                        {
                            if (const UClass* resolvedClass = FindFirstObject<UClass>(*entry->ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous))
                            {
                                metadata.AssetTypeId = registry->GetTypeIdForClass(resolvedClass);
                            }
                        }
                    }

                    metadata.bIsValid = metadata.Id.IsValid();
                    return metadata;
                }
            }
        }

        // Fallback: decompress payload and route to the registered serializer via type ID
        FString payloadString;
        uint32 payloadSerializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
        if (FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, payloadString, payloadSerializerTypeId))
        {
            // Look up the serializer by the integer type ID stored in the binary header
            TSharedPtr<ISGDynamicTextAssetSerializer> payloadSerializer;
            if (payloadSerializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
            {
                payloadSerializer = FindSerializerForTypeId(payloadSerializerTypeId);
            }

            if (payloadSerializer.IsValid())
            {
                FString version;
                payloadSerializer->ExtractMetadata(payloadString, metadata.Id, metadata.ClassName, metadata.UserFacingId, version, metadata.AssetTypeId);
            }
            else
            {
                // This is an intentional fallback, the binary file's SerializerTypeId does not match any
                // currently registered serializer (Ex: the serializer plugin was unloaded or removed).
                // No ISGDynamicTextAssetSerializer instance exists to delegate to, so we
                // attempt a raw JSON parse as a last resort for metadata recovery.
                TSharedPtr<FJsonObject> jsonObject;
                TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(payloadString);
                if (FJsonSerializer::Deserialize(reader, jsonObject) && jsonObject.IsValid())
                {
                    jsonObject->TryGetStringField(TEXT("$type"), metadata.ClassName);
                    jsonObject->TryGetStringField(TEXT("UserFacingId"), metadata.UserFacingId);
                }
            }
        }

        if (metadata.UserFacingId.IsEmpty())
        {
            metadata.UserFacingId = ExtractUserFacingIdFromPath(FilePath);
        }

        metadata.bIsValid = metadata.Id.IsValid();
        return metadata;
    }

    // Non-binary files: dispatch through serializer registry
    FString fileContents;
    if (!ReadRawFileContents(FilePath, fileContents))
    {
        return metadata;
    }

    // Try registry dispatch first
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FindSerializerForFile(FilePath);
    if (serializer.IsValid())
    {
        FString className;
        FString userFacingId;
        FString version;
        if (serializer->ExtractMetadata(fileContents, metadata.Id, className, userFacingId, version, metadata.AssetTypeId))
        {
            metadata.ClassName = className;
            metadata.UserFacingId = userFacingId;

            if (metadata.UserFacingId.IsEmpty())
            {
                metadata.UserFacingId = ExtractUserFacingIdFromPath(FilePath);
            }

            metadata.SerializerTypeId = serializer->GetSerializerTypeId();
            metadata.bIsValid = metadata.Id.IsValid();
            return metadata;
        }
    }

    // Another intentional fallback, the file's extension does not match any registered serializer
    // (Ex: a serializer plugin was unloaded, or the file uses an unrecognized format).
    // No ISGDynamicTextAssetSerializer instance exists to delegate to, so we attempt a raw JSON
    // parse as a last resort for metadata recovery.
    // This path is never taken for correctly configured projects where all serializers are registered at startup.
    TSharedPtr<FJsonObject> jsonObject;
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(fileContents);
    if (!FJsonSerializer::Deserialize(reader, jsonObject) || !jsonObject.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("Failed to parse JSON from FilePath(%s)"), *FilePath);
        return metadata;
    }

    metadata.SerializerTypeId = FSGDynamicTextAssetJsonSerializer::TYPE_ID;

    // Extract ID (serializer writes "$id")
    FString idString;
    if (jsonObject->TryGetStringField(ISGDynamicTextAssetSerializer::KEY_ID, idString))
    {
        metadata.Id.ParseString(idString);
    }

    // Extract UserFacingId from JSON, fall back to filename
    jsonObject->TryGetStringField(TEXT("UserFacingId"), metadata.UserFacingId);

    if (metadata.UserFacingId.IsEmpty())
    {
        metadata.UserFacingId = ExtractUserFacingIdFromPath(FilePath);
    }

    // Extract ClassName (serializer writes "$type")
    jsonObject->TryGetStringField(TEXT("$type"), metadata.ClassName);

    metadata.bIsValid = metadata.Id.IsValid();
    return metadata;
}

bool FSGDynamicTextAssetFileManager::EnsureFolderExistsForClass(const UClass* DynamicTextAssetClass)
{
    const FString folderPath = GetFolderPathForClass(DynamicTextAssetClass);

    IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (platformFile.DirectoryExists(*folderPath))
    {
        return true;
    }

    return platformFile.CreateDirectoryTree(*folderPath);
}

FString FSGDynamicTextAssetFileManager::BuildClassHierarchyPath(const UClass* DynamicTextAssetClass)
{
    if (!DynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
        return FString();
    }
    if (!DynamicTextAssetClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("DynamicTextAssetClass(%s) does not implement ISGDynamicTextAssetProvider interface"), *GetNameSafe(DynamicTextAssetClass));
        return FString();
    }

    // Registry may be nullptr during early startup or shutdown
    const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();

    // Build path from class hierarchy, excluding USGDynamicTextAsset itself
    // Each segment is formatted as {StrippedClassName}_{TypeId} when a valid TypeId is available
    TArray<FString> pathParts;
    const UClass* currentClass = DynamicTextAssetClass;

    while (currentClass && currentClass != USGDynamicTextAsset::StaticClass())
    {
        FString folderName = StripClassPrefix(GetNameSafe(currentClass));

        // Append TypeId suffix when registry is available and the class has a valid TypeId
        if (registry)
        {
            const FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(currentClass);
            if (typeId.IsValid())
            {
                folderName += TEXT("_") + typeId.ToString();
            }
        }

        pathParts.Insert(folderName, 0);
        currentClass = currentClass->GetSuperClass();
    }

    return FString::Join(pathParts, TEXT("/"));
}

FString FSGDynamicTextAssetFileManager::StripClassPrefix(const FString& ClassName)
{
    // Standard Unreal Engine type prefixes to strip
    // U = UObject, A = Actor, F = Struct (shouldn't appear but handle it)
    if (ClassName.Len() > 1)
    {
        const TCHAR firstChar = ClassName[0];
        const TCHAR secondChar = ClassName[1];

        // Check if first char is a prefix and second char is uppercase (indicating it's a prefix, not part of name)
        if ((firstChar == TEXT('U') || firstChar == TEXT('A') || firstChar == TEXT('F')) && FChar::IsUpper(secondChar))
        {
            return ClassName.RightChop(1);
        }
    }

    return ClassName;
}

bool FSGDynamicTextAssetFileManager::ReadRawFileContents(const FString& FilePath, FString& OutContents,
    uint32* OutSerializerTypeId /*= nullptr*/)
{
    OutContents.Empty();
    if (OutSerializerTypeId)
    {
        *OutSerializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
    }

    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ReadRawFileContents: Inputted EMPTY FilePath"));
        return false;
    }

    // Binary files require decompression before returning the payload string.
    // The SerializerTypeId stored in the binary header identifies which deserializer
    // to use and pass it to FindSerializerForTypeId() after this call returns.
    if (FilePath.EndsWith(BINARY_EXTENSION))
    {
        TArray<uint8> binaryData;
        if (!FSGDynamicTextAssetBinarySerializer::ReadBinaryFile(FilePath, binaryData))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ReadRawFileContents: Failed to read binary file at FilePath(%s)"), *FilePath);
            return false;
        }

        uint32 serializerTypeId = ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
        if (!FSGDynamicTextAssetBinarySerializer::BinaryToString(binaryData, OutContents, serializerTypeId))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ReadRawFileContents: Failed to decompress binary file at FilePath(%s)"), *FilePath);
            return false;
        }

        // Forward the TypeId to the caller so they can route to the correct deserializer
        if (OutSerializerTypeId)
        {
            *OutSerializerTypeId = serializerTypeId;
        }

        return true;
    }

    // JSON / text files are read directly — TypeId stays 0 (non-binary)
    if (!FFileHelper::LoadFileToString(OutContents, *FilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ReadRawFileContents: Failed to read file at FilePath(%s)"), *FilePath);
        return false;
    }

    return true;
}

bool FSGDynamicTextAssetFileManager::WriteRawFileContents(const FString& FilePath, const FString& Contents)
{
    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::WriteRawFileContents: Inputted EMPTY FilePath"));
        return false;
    }

    // Ensure parent directory exists
    const FString directory = FPaths::GetPath(FilePath);
    IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!platformFile.DirectoryExists(*directory))
    {
        if (!platformFile.CreateDirectoryTree(*directory))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::WriteRawFileContents: Failed to create directory(%s)"), *directory);
            return false;
        }
    }

    if (!FFileHelper::SaveStringToFile(Contents, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::WriteRawFileContents: Failed to write file at FilePath(%s)"), *FilePath);
        return false;
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Wrote file at FilePath(%s)"), *FilePath);
    return true;
}

bool FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile(const UClass* DynamicTextAssetClass, const FString& UserFacingId, const FString& Extension, FSGDynamicTextAssetId& OutId, FString& OutFilePath)
{
    OutId.Invalidate();
    OutFilePath.Empty();

    if (!DynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile: Inputted NULL DynamicTextAssetClass"));
        return false;
    }

    if (UserFacingId.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile: Inputted EMPTY UserFacingId"));
        return false;
    }

    // Look up the registered serializer for this extension — fail early if none registered
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FindSerializerForExtension(Extension);
    if (!serializer.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile: No serializer registered for extension(%s)"), *Extension);
        return false;
    }

    // Build file path
    OutFilePath = BuildFilePath(DynamicTextAssetClass, UserFacingId, Extension);

    // Check if file already exists
    if (FileExists(OutFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile: File already exists at FilePath(%s)"), *OutFilePath);
        OutFilePath.Empty();
        return false;
    }

    // Ensure folder exists
    if (!EnsureFolderExistsForClass(DynamicTextAssetClass))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile: Failed to create folder for DynamicTextAssetClass(%s)"), *GetNameSafe(DynamicTextAssetClass));
        OutFilePath.Empty();
        return false;
    }
    OutId.GenerateNewId();

    // Use delegate to generate default content (delegate may customize output for editor UX)
    FString fileContent = TEXT("");
    if (ON_GENERATE_DEFAULT_CONTENT.IsBound())
    {
        fileContent = ON_GENERATE_DEFAULT_CONTENT.Execute(DynamicTextAssetClass, OutId, UserFacingId, serializer.ToSharedRef());
    }
    else
    {
        // Delegate not bound (module shutting down?) fall back to serializer directly
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile: ON_GENERATE_DEFAULT_CONTENT is not bound, falling back to serializer default content for Extension(%s)"), *Extension);
        fileContent = serializer->GetDefaultFileContent(DynamicTextAssetClass, OutId, UserFacingId);
    }

    if (!WriteRawFileContents(OutFilePath, fileContent))
    {
        OutId.Invalidate();
        OutFilePath.Empty();
        return false;
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile: Created dynamic text asset file: UserFacingId(%s) ID(%s) FilePath(%s)"),
        *UserFacingId, *OutId.ToString(), *OutFilePath);

    return true;
}

bool FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFile(const FString& FilePath)
{
    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFileInputted EMPTY FilePath"));
        return false;
    }

    IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!platformFile.FileExists(*FilePath))
    {
        // File doesn't exist, consider this success
        return true;
    }

    if (!platformFile.DeleteFile(*FilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFileFailed to delete file at FilePath(%s)"), *FilePath);
        return false;
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFileDeleted file at FilePath(%s)"), *FilePath);
    return true;
}

bool FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId, FString& OutNewFilePath)
{
    OutNewFilePath.Empty();

    if (SourceFilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Inputted EMPTY SourceFilePath"));
        return false;
    }
    if (NewUserFacingId.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Inputted EMPTY NewUserFacingId"));
        return false;
    }

    // Read the source file
    FString sourceContents;
    if (!ReadRawFileContents(SourceFilePath, sourceContents))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Failed to read source file at FilePath(%s)"), *SourceFilePath);
        return false;
    }

    // Extract metadata to get class info
    FSGDynamicTextAssetFileMetadata metadata = ExtractMetadataFromFile(SourceFilePath);
    if (!metadata.bIsValid)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Failed to extract metadata from source file"));
        return false;
    }

    // Find the class from metadata
    UClass* dataObjectClass = FindFirstObject<UClass>(*metadata.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
    if (!dataObjectClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Could not find ClassName(%s)"), *metadata.ClassName);
        return false;
    }

    // Validate resolved class is a USGDynamicTextAssetProvider subclass to prevent class confusion
    if (!dataObjectClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Resolved ClassName(%s) does not implement ISGDynamicTextAssetProvider interface"), *metadata.ClassName);
        return false;
    }

    // Resolve the serializer for this file that was used for both field update and path building
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FindSerializerForFile(SourceFilePath);
    if (!serializer.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: No registered serializer found for SourceFilePath(%s)"), *SourceFilePath);
        return false;
    }

    // Build the new file path using the same extension as the source file
    OutNewFilePath = BuildFilePath(dataObjectClass, NewUserFacingId, serializer->GetFileExtension());

    // Check if already the same path (no-op rename)
    if (FPaths::IsSamePath(SourceFilePath, OutNewFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Source and destination are the same, nothing to do"));
        return true;
    }

    // Check if a file already exists at the destination
    if (FileExists(OutNewFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: File already exists at FilePath(%s)"), *OutNewFilePath);
        OutNewFilePath.Empty();
        return false;
    }

    // Update UserFacingId in the serialized content via the registered serializer
    FString updatedContents = sourceContents;
    {
        TMap<FString, FString> fieldUpdates;
        fieldUpdates.Add(ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID, SanitizeUserFacingId(NewUserFacingId));
        serializer->UpdateFieldsInPlace(updatedContents, fieldUpdates);
    }

    // Write to the new file path
    if (!WriteRawFileContents(OutNewFilePath, updatedContents))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Failed to write new file at FilePath(%s)"), *OutNewFilePath);
        OutNewFilePath.Empty();
        return false;
    }

    // Delete the old file
    if (!DeleteDynamicTextAssetFile(SourceFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("FSGDynamicTextAssetFileManager::RenameDynamicTextAsset: Wrote new file but failed to delete old file at SourceFilePath(%s)"), *SourceFilePath);
        // Still return true since the new file was written successfully
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetFileManager::Renamed dynamic text asset: UserFacingId(%s) -> NewUserFacingId(%s)|(FilePath(%s)"),
        *metadata.UserFacingId, *NewUserFacingId, *OutNewFilePath);

    return true;
}

bool FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId, FSGDynamicTextAssetId& OutNewId, FString& OutNewFilePath)
{
    OutNewId.Invalidate();
    OutNewFilePath.Empty();

    if (SourceFilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: Inputted EMPTY SourceFilePath"));
        return false;
    }

    if (NewUserFacingId.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: Inputted EMPTY NewUserFacingId"));
        return false;
    }

    // Read the source file
    FString sourceContents;
    if (!ReadRawFileContents(SourceFilePath, sourceContents))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: Failed to read source file at FilePath(%s)"), *SourceFilePath);
        return false;
    }

    // Extract metadata to get class info
    FSGDynamicTextAssetFileMetadata metadata = ExtractMetadataFromFile(SourceFilePath);
    if (!metadata.bIsValid)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: Failed to extract metadata from source file"));
        return false;
    }

    // Resolve class via Asset Type ID, with fallback to class name for legacy files
    UClass* dataObjectClass = nullptr;
    if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
    {
        if (metadata.AssetTypeId.IsValid())
        {
            dataObjectClass = registry->ResolveClassForTypeId(metadata.AssetTypeId);
        }

        // Fallback: resolve by class name for legacy files without a valid Asset Type ID
        if (!dataObjectClass && !metadata.ClassName.IsEmpty())
        {
            TArray<UClass*> allClasses;
            registry->GetAllInstantiableClasses(allClasses);
            for (UClass* registeredClass : allClasses)
            {
                if (registeredClass && registeredClass->GetName() == metadata.ClassName)
                {
                    dataObjectClass = registeredClass;
                    break;
                }
            }
        }
    }

    if (!dataObjectClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: Failed to resolve class for AssetTypeId '%s' (ClassName '%s')"),
            *metadata.AssetTypeId.ToString(), *metadata.ClassName);
        return false;
    }

    // Resolve the serializer for this file that was used for both field update and path building
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FindSerializerForFile(SourceFilePath);
    if (!serializer.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: No registered serializer found for SourceFilePath(%s)"), *SourceFilePath);
        return false;
    }

    // Build the new file path using the same extension as the source file
    OutNewFilePath = BuildFilePath(dataObjectClass, NewUserFacingId, serializer->GetFileExtension());

    // Check if file already exists
    if (FileExists(OutNewFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: File already exists at FilePath(%s)"), *OutNewFilePath);
        OutNewFilePath.Empty();
        return false;
    }

    OutNewId.GenerateNewId();

    // Update Id and UserFacingId in the duplicated content via the registered serializer.
    // Both fields are patched in a single pass to avoid a double parse/re-serialize.
    FString newContents = sourceContents;
    {
        TMap<FString, FString> fieldUpdates;
        fieldUpdates.Add(ISGDynamicTextAssetSerializer::KEY_ID, OutNewId.ToString());
        fieldUpdates.Add(ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID, SanitizeUserFacingId(NewUserFacingId));
        serializer->UpdateFieldsInPlace(newContents, fieldUpdates);
    }

    // Write the new file
    if (!WriteRawFileContents(OutNewFilePath, newContents))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset: Failed to write new file at FilePath(%s)"), *OutNewFilePath);
        OutNewId.Invalidate();
        OutNewFilePath.Empty();
        return false;
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetFileManager::Duplicated dynamic text asset: UserFacingId(%s) -> NewUserFacingId(%s) | (NewID(%s))"),
        *metadata.UserFacingId, *NewUserFacingId, *OutNewId.ToString());

    return true;
}

#if WITH_EDITOR
bool FSGDynamicTextAssetFileManager::ConvertFileFormat(const FString& SourceFilePath, const FString& TargetExtension, FString& OutNewFilePath)
{
    OutNewFilePath.Empty();

    if (SourceFilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Inputted EMPTY SourceFilePath"));
        return false;
    }

    if (TargetExtension.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Inputted EMPTY TargetExtension"));
        return false;
    }

    // 1. Find source serializer
    TSharedPtr<ISGDynamicTextAssetSerializer> sourceSerializer = FindSerializerForFile(SourceFilePath);
    if (!sourceSerializer.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: No registered serializer found for SourceFilePath(%s)"), *SourceFilePath);
        return false;
    }

    // 3. Find target serializer
    TSharedPtr<ISGDynamicTextAssetSerializer> targetSerializer = FindSerializerForExtension(TargetExtension);
    if (!targetSerializer.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: No registered serializer found for TargetExtension(%s)"), *TargetExtension);
        return false;
    }

    // Guard against no-op conversion (same format)
    if (sourceSerializer->GetFileExtension().Equals(targetSerializer->GetFileExtension(), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Source and target formats are the same, nothing to do"));
        OutNewFilePath = SourceFilePath;
        return true;
    }

    // 2. Read the source file
    FString sourceContents;
    if (!ReadRawFileContents(SourceFilePath, sourceContents))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Failed to read source file at SourceFilePath(%s)"), *SourceFilePath);
        return false;
    }

    // Extract metadata to resolve the class
    FSGDynamicTextAssetFileMetadata metadata = ExtractMetadataFromFile(SourceFilePath);
    if (!metadata.bIsValid)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Failed to extract metadata from SourceFilePath(%s)"), *SourceFilePath);
        return false;
    }

    // Resolve class via Asset Type ID, with fallback to class name for legacy files
    UClass* dataObjectClass = nullptr;
    if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
    {
        if (metadata.AssetTypeId.IsValid())
        {
            dataObjectClass = registry->ResolveClassForTypeId(metadata.AssetTypeId);
        }

        // Fallback: resolve by class name for legacy files without a valid Asset Type ID
        if (!dataObjectClass && !metadata.ClassName.IsEmpty())
        {
            TArray<UClass*> allClasses;
            registry->GetAllInstantiableClasses(allClasses);
            for (UClass* registeredClass : allClasses)
            {
                if (registeredClass && registeredClass->GetName() == metadata.ClassName)
                {
                    dataObjectClass = registeredClass;
                    break;
                }
            }
        }
    }

    if (!dataObjectClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Failed to resolve class for AssetTypeId '%s' (ClassName '%s')"),
            *metadata.AssetTypeId.ToString(), *metadata.ClassName);
        return false;
    }

    // Guard against abstract classes that cannot be instantiated
    if (dataObjectClass->HasAnyClassFlags(CLASS_Abstract))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Class '%s' is abstract and cannot be instantiated"),
            *dataObjectClass->GetName());
        return false;
    }

    // Create a temporary UObject to round-trip the data through
    UObject* tempObject = NewObject<UObject>(GetTransientPackage(), dataObjectClass);
    if (!tempObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Failed to create temp object of class '%s'"), *dataObjectClass->GetName());
        return false;
    }

    ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(tempObject);
    if (!provider)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Temp object does not implement ISGDynamicTextAssetProvider"));
        return false;
    }

    // Deserialize using the source serializer
    bool bMigrated = false;
    if (!sourceSerializer->DeserializeProvider(sourceContents, provider, bMigrated))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Failed to deserialize from source format '%s'"), *sourceSerializer->GetFormatName_String());
        return false;
    }

    // 4. Re-serialize using the target serializer
    FString targetContents;
    if (!targetSerializer->SerializeProvider(provider, targetContents))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Failed to serialize to target format '%s'"), *targetSerializer->GetFormatName_String());
        return false;
    }

    // 5. Build the new file path: same folder and UserFacingId, new extension
    OutNewFilePath = BuildFilePath(dataObjectClass, metadata.UserFacingId, TargetExtension);

    // Check if the target file already exists
    if (FileExists(OutNewFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Target file already exists at FilePath(%s)"), *OutNewFilePath);
        OutNewFilePath.Empty();
        return false;
    }

    // Write the new file
    if (!WriteRawFileContents(OutNewFilePath, targetContents))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Failed to write target file at FilePath(%s)"), *OutNewFilePath);
        OutNewFilePath.Empty();
        return false;
    }

    // 6. Delete the old file
    if (!DeleteDynamicTextAssetFile(SourceFilePath))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Wrote new file but failed to delete old file at SourceFilePath(%s)"), *SourceFilePath);
        // Still return true since the new file was written successfully
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
        TEXT("FSGDynamicTextAssetFileManager::ConvertFileFormat: Converted '%s' from %s to %s -> FilePath(%s)"),
        *metadata.UserFacingId, *sourceSerializer->GetFormatName_String(), *targetSerializer->GetFormatName_String(), *OutNewFilePath);

    return true;
}
#endif

bool FSGDynamicTextAssetFileManager::FileExists(const FString& FilePath)
{
    if (FilePath.IsEmpty())
    {
        return false;
    }

    return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath);
}

bool FSGDynamicTextAssetFileManager::FindFileForId(const FSGDynamicTextAssetId& Id, FString& OutFilePath, const UClass* SearchClass)
{
    OutFilePath.Empty();

    if (!Id.IsValid())
    {
        return false;
    }

    // Cooked builds: O(1) lookup via ID-named file
    if (ShouldUseCookedDirectory())
    {
        FString binaryPath = BuildBinaryFilePath(Id);
        if (FileExists(binaryPath))
        {
            OutFilePath = binaryPath;
            return true;
        }

        return false;
    }

    // Editor: O(N) scan through JSON source files
    TArray<FString> filePaths;
    if (SearchClass)
    {
        FindAllFilesForClass(SearchClass, filePaths, true);
    }
    else
    {
        FindAllDynamicTextAssetFiles(filePaths);
    }

    for (const FString& filePath : filePaths)
    {
        FSGDynamicTextAssetFileMetadata metadata = ExtractMetadataFromFile(filePath);
        if (metadata.bIsValid && metadata.Id == Id)
        {
            OutFilePath = filePath;
            return true;
        }
    }

    return false;
}

FString FSGDynamicTextAssetFileManager::GetCookedDynamicTextAssetsRootPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SGDynamicTextAssetsCooked"));
}

FString FSGDynamicTextAssetFileManager::GetCookedTypeManifestsPath()
{
    return FPaths::Combine(GetCookedDynamicTextAssetsRootPath(), TEXT("_TypeManifests"));
}

FString FSGDynamicTextAssetFileManager::BuildBinaryFilePath(const FSGDynamicTextAssetId& Id)
{
    return FPaths::Combine(GetCookedDynamicTextAssetsRootPath(), Id.ToString() + BINARY_EXTENSION);
}

bool FSGDynamicTextAssetFileManager::ShouldUseCookedDirectory()
{
#if WITH_EDITOR
    return false;
#else
    return true;
#endif
}

const FSGDynamicTextAssetCookManifest* FSGDynamicTextAssetFileManager::GetCookManifest()
{
#if WITH_EDITOR
    return nullptr;
#else
    static TOptional<FSGDynamicTextAssetCookManifest> cachedManifest;

    if (!cachedManifest.IsSet())
    {
        cachedManifest.Emplace();

        FString cookedRoot = GetCookedDynamicTextAssetsRootPath();
        if (!cachedManifest->LoadFromFileBinary(cookedRoot))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("GetCookManifest: Failed to load cook manifest from cookedRoot(%s)"), *cookedRoot);
        }
    }

    if (cachedManifest.IsSet() && cachedManifest->IsLoaded())
    {
        return &cachedManifest.GetValue();
    }

    return nullptr;
#endif
}

void FSGDynamicTextAssetFileManager::RegisterSerializerInstance(TSharedRef<ISGDynamicTextAssetSerializer> Serializer)
{
    const FString extension = Serializer->GetFileExtension().ToLower();
    if (extension.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("RegisterSerializer: Serializer(%s) returned empty file extension"),
            *Serializer->GetFormatName_String());
        return;
    }

    // Validate the integer type ID, zero is reserved/invalid
    const uint32 typeId = Serializer->GetSerializerTypeId();
    if (typeId == ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Fatal, TEXT("RegisterSerializer: Serializer '%s' returned TypeId 0 IDs must be non-zero. Built-in range is 1-99, third-party must use >= 100."),
            *Serializer->GetFormatName_String());
        return;
    }
    if (REGISTERED_SERIALIZERS_BY_ID.Contains(typeId))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Fatal, TEXT("RegisterSerializer: TypeId(%u) is already registered by serializer(%s). Each serializer must have a unique TypeId."),
            typeId, *REGISTERED_SERIALIZERS_BY_ID[typeId]->GetFormatName_String());
        return;
    }

    if (REGISTERED_SERIALIZERS.Contains(extension))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("RegisterSerializer: Overwriting existing serializer for extension '%s'"), *extension);
    }

    REGISTERED_SERIALIZERS.Add(extension, Serializer);
    REGISTERED_SERIALIZERS_BY_ID.Add(typeId, Serializer);
    UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Registered serializer(%s) | TypeId(%u) | Extension=(%s)"),
        *Serializer->GetFormatName_String(), typeId, *extension);
}

void FSGDynamicTextAssetFileManager::UnregisterSerializerByExtension(const FString& Extension)
{
    const FString lowerExtension = Extension.ToLower();
    if (TSharedRef<ISGDynamicTextAssetSerializer>* serializer = REGISTERED_SERIALIZERS.Find(lowerExtension))
    {
        // Also remove from the type ID map before removing from the extension map
        REGISTERED_SERIALIZERS_BY_ID.Remove((*serializer)->GetSerializerTypeId());
        REGISTERED_SERIALIZERS.Remove(lowerExtension);
        UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("Unregistered serializer for extension '%s'"), *lowerExtension);
    }
}

TSharedPtr<ISGDynamicTextAssetSerializer> FSGDynamicTextAssetFileManager::FindSerializerForExtension(FStringView Extension)
{
    const FString lowerExtension = FString(Extension).ToLower();
    if (TSharedRef<ISGDynamicTextAssetSerializer>* serializer = REGISTERED_SERIALIZERS.Find(lowerExtension))
    {
        return *serializer;
    }

    return nullptr;
}

TSharedPtr<ISGDynamicTextAssetSerializer> FSGDynamicTextAssetFileManager::FindSerializerForFile(const FString& FilePath)
{
    // Extract double extension (e.g., ".dta.json" from "excalibur.dta.json")
    const FString filename = FPaths::GetCleanFilename(FilePath);

    // Find second-to-last dot for double extension
    int32 lastDotIndex = INDEX_NONE;
    if (filename.FindLastChar(TEXT('.'), lastDotIndex) && lastDotIndex > 0)
    {
        int32 prevDotIndex = INDEX_NONE;
        FString beforeLastDot = filename.Left(lastDotIndex);
        if (beforeLastDot.FindLastChar(TEXT('.'), prevDotIndex))
        {
            // Double extension found (e.g., ".dta.json")
            FString doubleExt = filename.Mid(prevDotIndex);
            TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FindSerializerForExtension(doubleExt);
            if (serializer.IsValid())
            {
                return serializer;
            }
        }

        // Fall back to single extension
        FString singleExt = filename.Mid(lastDotIndex + 1);
        return FindSerializerForExtension(singleExt);
    }

    return nullptr;
}

FString FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(const FString& FilePath)
{
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FindSerializerForFile(FilePath);
    if (serializer.IsValid())
    {
        return serializer->GetFileExtension();
    }
    return FString();
}

void FSGDynamicTextAssetFileManager::GetAllRegisteredExtensions(TArray<FString>& OutExtensions)
{
    REGISTERED_SERIALIZERS.GetKeys(OutExtensions);
}

TSharedPtr<ISGDynamicTextAssetSerializer> FSGDynamicTextAssetFileManager::FindSerializerForTypeId(uint32 TypeId)
{
    if (TSharedRef<ISGDynamicTextAssetSerializer>* serializer = REGISTERED_SERIALIZERS_BY_ID.Find(TypeId))
    {
        return *serializer;
    }
    return nullptr;
}

uint32 FSGDynamicTextAssetFileManager::GetTypeIdForExtension(const FString& Extension)
{
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FindSerializerForExtension(Extension);
    if (!serializer.IsValid())
    {
        return ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID;
    }
    return serializer->GetSerializerTypeId();
}

void FSGDynamicTextAssetFileManager::GetAllRegisteredSerializerDescriptions(TArray<FString>& OutDescriptions)
{
    OutDescriptions.Empty();
    OutDescriptions.Reserve(REGISTERED_SERIALIZERS_BY_ID.Num());
    for (const TPair<uint32, TSharedRef<ISGDynamicTextAssetSerializer>>& pair : REGISTERED_SERIALIZERS_BY_ID)
    {
        OutDescriptions.Add(FString::Printf(TEXT("TypeId(%u) | Extension(%s)| Format(%s)"),
            pair.Key, *pair.Value->GetFileExtension(), *pair.Value->GetFormatName_String()));
    }
}

TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> FSGDynamicTextAssetFileManager::GetAllRegisteredSerializers()
{
    TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> result;
    for (const TPair<uint32, TSharedRef<ISGDynamicTextAssetSerializer>>& pair : REGISTERED_SERIALIZERS_BY_ID)
    {
        result.Add(pair.Value.ToSharedPtr());
    }
    return result;
}
