// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAssetId.h"

class USGDynamicTextAsset;
class FSGDynamicTextAssetCookManifest;
class ISGDynamicTextAssetSerializer;

struct FSGDynamicTextAssetFileMetadata;

DECLARE_DELEGATE_RetVal_FourParams(FString, FSGDataGenerateDefaultContentDelegate, const UClass*,
    const FSGDynamicTextAssetId&, const FString&, TSharedRef<ISGDynamicTextAssetSerializer>);

/**
 * Resolves a relative file path to an absolute file path.
 *
 * @param RelativePath The relative path (from Content directory)
 * @return Absolute file path
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetFileManager
{
public:

    /**
     * Returns the root directory for all dynamic text assets.
     * @return Absolute path to Content/SGDynamicTextAssets/
     */
    static FString GetDynamicTextAssetsRootPath();

    /**
     * Returns the relative root directory (from Content).
     * @return Relative path "SGDynamicTextAssets/"
     */
    static FString GetDynamicTextAssetsRelativeRootPath();

    /**
     * Returns the folder path for a given dynamic text asset class.
     * Path is based on class name hierarchy from USGDynamicTextAsset.
     * Each folder segment is formatted as {StrippedClassName}_{TypeId} when a valid
     * TypeId is available from the registry. Falls back to class name only when the
     * registry is unavailable or the TypeId has not been assigned yet.
     *
     * @param DynamicTextAssetClass The class to get the folder for
     * @return Absolute path like "Content/SGDynamicTextAssets/WeaponData_{TypeId}/"
     */
    static FString GetFolderPathForClass(const UClass* DynamicTextAssetClass);

    /**
     * Returns the relative folder path for a given dynamic text asset class.
     * Each folder segment is formatted as {StrippedClassName}_{TypeId} when a valid
     * TypeId is available from the registry.
     *
     * @param DynamicTextAssetClass The class to get the folder for
     * @return Relative path like "SGDynamicTextAssets/WeaponData_{TypeId}/"
     */
    static FString GetRelativeFolderPathForClass(const UClass* DynamicTextAssetClass);

    /**
     * Builds the full file path for a dynamic text asset.
     * 
     * @param DynamicTextAssetClass The class of the dynamic text asset
     * @param UserFacingId The human-readable ID (becomes filename)
     * @param Extension File extension to use (defaults to JSON)
     * @return Full absolute path with extension
     */
    static FString BuildFilePath(const UClass* DynamicTextAssetClass, const FString& UserFacingId, const FString& Extension = DEFAULT_TEXT_EXTENSION);

    /**
     * Builds the relative file path (from Content directory) for a dynamic text asset.
     * 
     * @param DynamicTextAssetClass The class of the dynamic text asset
     * @param UserFacingId The human-readable ID (becomes filename)
     * @param Extension File extension to use (defaults to JSON)
     * @return Relative path with extension
     */
    static FString BuildRelativeFilePath(const UClass* DynamicTextAssetClass, const FString& UserFacingId, const FString& Extension = DEFAULT_TEXT_EXTENSION);

    /**
     * Finds all dynamic text asset files of a given class.
     * 
     * @param DynamicTextAssetClass The class to search for (includes subclasses)
     * @param OutFilePaths Array to populate with absolute file paths
     * @param bIncludeSubclasses If true, also searches subclass folders
     */
    static void FindAllFilesForClass(const UClass* DynamicTextAssetClass, TArray<FString>& OutFilePaths, bool bIncludeSubclasses = true);

    /**
     * Finds all dynamic text asset files in the root directory tree.
     * 
     * @param OutFilePaths Array to populate with absolute file paths
     */
    static void FindAllDynamicTextAssetFiles(TArray<FString>& OutFilePaths);

    /**
     * Extracts the UserFacingId from a file path.
     * 
     * @param FilePath The full or relative file path
     * @return The filename without extension (e.g., "excalibur" from "excalibur.dta.json")
     */
    static FString ExtractUserFacingIdFromPath(const FString& FilePath);

    /**
     * Checks if a file path is a valid dynamic text asset file.
     * 
     * @param FilePath The path to check
     * @return True if the path ends with a valid dynamic text asset extension
     */
    static bool IsDynamicTextAssetFile(const FString& FilePath);

    /**
     * Sanitizes a user-facing ID for use as a filename.
     * Removes invalid characters, converts spaces to underscores.
     * 
     * @param UserFacingId The ID to sanitize
     * @return Sanitized string safe for use as filename
     */
    static FString SanitizeUserFacingId(const FString& UserFacingId);

    /**
     * Extracts metadata from a dynamic text asset file without fully loading it.
     * Reads only the header/metadata section of the JSON.
     * 
     * @param FilePath Absolute path to the file
     * @return Metadata struct (check bIsValid)
     */
    static FSGDynamicTextAssetFileMetadata ExtractMetadataFromFile(const FString& FilePath);

    /**
     * Ensures the folder structure exists for a given class.
     * Creates directories if they don't exist.
     * 
     * @param DynamicTextAssetClass The class to create folders for
     * @return True if directory exists or was created successfully
     */
    static bool EnsureFolderExistsForClass(const UClass* DynamicTextAssetClass);

    /**
     * Reads raw file contents from a dynamic text asset file.
     * For binary (.dta.bin) files the payload is decompressed before returning, and
     * the serializer TypeId embedded in the binary header is written to OutSerializerTypeId.
     * For plain text files (e.g. .dta.json) OutSerializerTypeId is set to 0.
     * Pass the TypeId to FindSerializerForTypeId() to obtain the correct deserializer.
     *
     * @param FilePath             Absolute path to the file
     * @param OutContents          Output string — always a text payload after return
     * @param OutSerializerTypeId  For binary files: TypeId from the binary header.
     *                             For text files: 0. Pass nullptr to ignore.
     * @return True if file was read successfully
     */
    static bool ReadRawFileContents(const FString& FilePath, FString& OutContents,
        uint32* OutSerializerTypeId = nullptr);

    /**
     * Writes raw file contents to a dynamic text asset file.
     * Creates parent directories if needed.
     * 
     * @param FilePath Absolute path to the file
     * @param Contents The contents to write
     * @return True if file was written successfully
     */
    static bool WriteRawFileContents(const FString& FilePath, const FString& Contents);

    /**
     * Creates a new dynamic text asset file with a generated ID.
     * 
     * @param DynamicTextAssetClass The class of the dynamic text asset
     * @param UserFacingId The human-readable ID for the file
     * @param Extension File extension to use (defaults to JSON)
     * @param OutId Output for the generated ID
     * @param OutFilePath Output for the created file path
     * @return True if file was created successfully
     */
    static bool CreateDynamicTextAssetFile(const UClass* DynamicTextAssetClass, const FString& UserFacingId,
        const FString& Extension, FSGDynamicTextAssetId& OutId, FString& OutFilePath);

    /**
     * Deletes a dynamic text asset file.
     * 
     * @param FilePath Absolute path to the file to delete
     * @return True if file was deleted or didn't exist
     */
    static bool DeleteDynamicTextAssetFile(const FString& FilePath);

    /**
     * Renames a dynamic text asset by moving the file to a new path based on NewUserFacingId.
     * Updates the UserFacingId field in the JSON if present.
     * The ID remains unchanged.
     *
     * @param SourceFilePath Absolute path to the source file
     * @param NewUserFacingId The new human-readable ID for the dynamic text asset
     * @param OutNewFilePath Output for the new file path after rename
     * @return True if the rename succeeded
     */
    static bool RenameDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId, FString& OutNewFilePath);

    /**
     * Duplicates an existing dynamic text asset file with a new name and ID.
     * 
     * @param SourceFilePath Absolute path to the source file
     * @param NewUserFacingId The human-readable ID for the duplicate
     * @param OutNewId Output for the generated ID of the duplicate
     * @param OutNewFilePath Output for the created file path
     * @return True if file was duplicated successfully
     */
    static bool DuplicateDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId,
        FSGDynamicTextAssetId& OutNewId, FString& OutNewFilePath);

#if WITH_EDITOR
    /**
     * Converts a dynamic text asset file from one format to another.
     * Reads the source file, deserializes into an in-memory provider instance,
     * re-serializes using the target format's serializer, writes the new file,
     * and deletes the old file. All metadata (GUID, UserFacingId, Version,
     * AssetTypeId) is preserved through the round-trip.
     *
     * Source control operations (mark-for-add, mark-for-delete) are NOT handled
     * by this method — the caller is responsible for managing source control state.
     *
     * @param SourceFilePath   Absolute path to the source file
     * @param TargetExtension  Target format extension (e.g., ".dta.xml", ".dta.yaml")
     * @param OutNewFilePath   Output for the new file's absolute path
     * @return True if the conversion succeeded
     */
    static bool ConvertFileFormat(const FString& SourceFilePath, const FString& TargetExtension, FString& OutNewFilePath);
#endif

    /**
     * Checks if a dynamic text asset file exists.
     * 
     * @param FilePath Absolute path to check
     * @return True if file exists
     */
    static bool FileExists(const FString& FilePath);

    /**
     * Finds the file path for a given Dynamic Text Asset Id by scanning files.
     *
     * @param Id The Id to search for
     * @param OutFilePath Output for the found file path
     * @param SearchClass Optional class to scope the search (faster)
     * @return True if found
     */
    static bool FindFileForId(const FSGDynamicTextAssetId& Id, FString& OutFilePath, const UClass* SearchClass = nullptr);

    /**
     * Returns the root directory for cooked dynamic text assets.
     * @return Absolute path to Content/SGDynamicTextAssetsCooked/
     */
    static FString GetCookedDynamicTextAssetsRootPath();

    /**
     * Returns the path to the cooked type manifests subdirectory.
     * @return Absolute path to Content/SGDynamicTextAssetsCooked/_TypeManifests/
     */
    static FString GetCookedTypeManifestsPath();

    /**
     * Builds the full file path for a cooked binary file using its Id.
     *
     * @param Id The Id of the dynamic text asset
     * @return Absolute path: {CookedRoot}/{Id}.dta.bin
     */
    static FString BuildBinaryFilePath(const FSGDynamicTextAssetId& Id);

    /**
     * Returns whether the cooked directory should be used for file lookups.
     * True in packaged builds, false in editor.
     */
    static bool ShouldUseCookedDirectory();

    /**
     * Lazily loads and returns the cook manifest.
     * Returns nullptr in editor builds or if the manifest file is not found.
     */
    static const FSGDynamicTextAssetCookManifest* GetCookManifest();

    // Serializer Registry

    /**
     * Registers a serializer by type.
     * Creates a TSharedRef<T> internally and registers by its file extension.
     *
     * @tparam T The serializer class to register
     */
    template<typename T>
    static void RegisterSerializer()
    {
        static_assert(std::is_base_of_v<ISGDynamicTextAssetSerializer, T>, "T must derive from ISGDynamicTextAssetSerializer");
        RegisterSerializerInstance(MakeShared<T>());
    }

    /**
     * Unregisters a serializer by type.
     * Instantiates a temporary T to retrieve the file extension.
     *
     * @tparam T The serializer class to unregister
     */
    template<typename T>
    static void UnregisterSerializer()
    {
        static_assert(std::is_base_of_v<ISGDynamicTextAssetSerializer, T>, "T must derive from ISGDynamicTextAssetSerializer");
        const T temp;
        UnregisterSerializerByExtension(temp.GetFileExtension());
    }

    /**
     * Finds a registered serializer by file extension.
     *
     * @param Extension The file extension to look up (case-insensitive)
     * @return The serializer, or nullptr if not found
     */
    static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForExtension(FStringView Extension);

    /**
     * Finds a registered serializer by its integer type ID.
     * Used when loading binary (.dta.bin) files to route the payload to the correct deserializer.
     *
     * @param TypeId The serializer type ID stored in the binary file header
     * @return The serializer, or nullptr if not found
     */
    static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForTypeId(uint32 TypeId);

    /**
     * Returns the integer TypeId for the serializer registered under the given file extension.
     * Returns 0 if no serializer is registered for that extension.
     * Use this to convert a known extension string to a binary-format TypeId.
     *
     * @param Extension File extension without leading dot (e.g., "dta.json")
     */
    static uint32 GetTypeIdForExtension(const FString& Extension);

    /**
     * Finds a registered serializer for a given file path.
     * Extracts the double extension (e.g., "dta.json" from "file.dta.json").
     *
     * @param FilePath The file path to extract the extension from
     * @return The serializer, or nullptr if no match
     */
    static TSharedPtr<ISGDynamicTextAssetSerializer> FindSerializerForFile(const FString& FilePath);

    /**
     * Returns the extension for the given file path if it's a registered extension.
     * Returns empty string if not a recognized dynamic text asset file format.
     * 
     * @param FilePath The file path to check
     * @return The canonical extension, e.g. ".dta.json", or empty string
     */
    static FString GetSupportedExtensionForFile(const FString& FilePath);

    /**
     * Returns all registered file extensions.
     *
     * @param OutExtensions Array to populate with extension strings
     */
    static void GetAllRegisteredExtensions(TArray<FString>& OutExtensions);

    /**
     * Builds a diagnostic description string for each registered serializer.
     * Each entry is formatted as: "TypeId=N | Extension='ext' | Format='name'"
     * Used by USGDynamicTextAssetStatics::LogRegisteredSerializers() and GetRegisteredSerializerDescriptions().
     *
     * @param OutDescriptions Array populated with one entry per registered serializer
     */
    static void GetAllRegisteredSerializerDescriptions(TArray<FString>& OutDescriptions);

    /**
     * Returns a list of all currently registered serializer instances.
     *
     * @return Array of shared pointers to registered serializers
     */
    static TArray<TSharedPtr<ISGDynamicTextAssetSerializer>> GetAllRegisteredSerializers();

    /** Strips standard Unreal type prefixes (U, A, F) from class name */
    static FString StripClassPrefix(const FString& ClassName);

    /**
     * Delegate for generating default file contents for a new dynamic text asset.
     * Fired by CreateDynamicTextAssetFile after a new ID is generated.
     *
     * Params: DynamicTextAssetClass, Id, UserFacingId, Serializer
     * The Serializer param is the registered ISGDynamicTextAssetSerializer for the file's
     * extension. Call Serializer->GetDefaultFileContent(DynamicTextAssetClass, Id, UserFacingId)
     * to produce valid default content, or return a custom string for editor UX needs.
     */
    static FSGDataGenerateDefaultContentDelegate ON_GENERATE_DEFAULT_CONTENT;

    /** File extension for dynamic text assets (includes dot) as the text file. Currently using JSON. */
    static const FString DEFAULT_TEXT_EXTENSION;

    /** File extension for binary dynamic text assets (includes dot) */
    static const FString BINARY_EXTENSION;

    /** Relative root directory (from Content). */
    static const FString DEFAULT_RELATIVE_ROOT_PATH;

private:

    /**
     * Builds a relative path from class hierarchy (excludes USGDynamicTextAsset).
     * Each segment is formatted as {StrippedClassName}_{TypeId} when a valid TypeId
     * is available from the registry. Falls back to class name only during early
     * startup or when the registry has not yet assigned TypeIds.
     */
    static FString BuildClassHierarchyPath(const UClass* DynamicTextAssetClass);

    /** Registers a serializer instance. Called by the template overload. */
    static void RegisterSerializerInstance(TSharedRef<ISGDynamicTextAssetSerializer> Serializer);

    /** Removes a serializer by extension string. Called by the template overload. */
    static void UnregisterSerializerByExtension(const FString& Extension);

    /** Registered serializers keyed by file extension (case-insensitive) */
    static TMap<FString, TSharedRef<ISGDynamicTextAssetSerializer>> REGISTERED_SERIALIZERS;

    /** Registered serializers keyed by integer type ID — used for binary file routing */
    static TMap<uint32, TSharedRef<ISGDynamicTextAssetSerializer>> REGISTERED_SERIALIZERS_BY_ID;
};

