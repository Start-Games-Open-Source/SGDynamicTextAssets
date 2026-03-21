// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetVersion.h"

/**
 * Aggregated format version data for a single serializer type.
 * Tracks the distribution of file format versions across all DTA files
 * that use this serializer.
 */
struct SGDYNAMICTEXTASSETSEDITOR_API FSGSerializerFormatVersionInfo
{
	/** The serializer's unique type ID (e.g., 1 for JSON, 2 for XML, 3 for YAML). */
	uint32 SerializerTypeId = 0;

	/** Human-readable serializer name (e.g., "JSON"). */
	FString SerializerName;

	/** File extension handled by this serializer (e.g., ".dta.json"). */
	FString FileExtension;

	/** The serializer's current file format version (from GetFileFormatVersion()). */
	FSGDynamicTextAssetVersion CurrentSerializerVersion;

	/** Highest file format version found across all files for this serializer. */
	FSGDynamicTextAssetVersion HighestFound;

	/** Lowest file format version found across all files for this serializer. */
	FSGDynamicTextAssetVersion LowestFound;

	/** Total number of DTA files using this serializer. */
	int32 TotalFileCount = 0;

	/** Count of files at each format version string (e.g., "1.0.0" -> 42). */
	TMap<FString, int32> CountByVersion;
};

/**
 * Project-level cache for DTA metadata that requires fast lookup.
 * Currently tracks per-serializer file format version distribution.
 * Designed to be extensible for future project-level metadata.
 *
 * Persisted as JSON in Saved/SGDynamicTextAssets/ProjectInfoCache.json.
 * Populated by an async scan phase, updated incrementally on file save.
 */
class SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetProjectInfoCache
{
public:

	static constexpr int32 CACHE_VERSION = 1;

	FSGDynamicTextAssetProjectInfoCache() = default;

	/** Save the cache to a JSON file at the given path. */
	bool SaveToFile(const FString& FilePath) const;

	/** Load the cache from a JSON file at the given path. */
	bool LoadFromFile(const FString& FilePath);

	/** Returns true if the cache has been loaded or populated. */
	bool IsValid() const;

	/** Returns true if a full scan is needed (cache missing, dirty, or not loaded). */
	bool NeedsScan() const;

	/** Reset all cached data. Call before a full rescan. */
	void Reset();

	/** Mark the cache as needing a rescan (e.g., after a file delete). */
	void MarkDirty();

	/**
	 * Record a file's format version contribution.
	 * Called during the scan phase for each DTA file, and incrementally on file save.
	 *
	 * @param SerializerTypeId The serializer type that handles this file
	 * @param FileFormatVersion The format version found in the file
	 */
	void RecordFileVersion(uint32 SerializerTypeId, const FSGDynamicTextAssetVersion& FileFormatVersion);

	/**
	 * Populate the CurrentSerializerVersion field for all tracked serializers
	 * by querying the registered serializer instances.
	 * Call after scan phase completes.
	 */
	void PopulateCurrentSerializerVersions();

	/** Get format info for a specific serializer type ID. Returns nullptr if not tracked. */
	const FSGSerializerFormatVersionInfo* GetInfoForSerializer(uint32 SerializerTypeId) const;

	/** Get all serializer format version infos. */
	const TMap<uint32, FSGSerializerFormatVersionInfo>& GetAllFormatVersions() const;

	/** Returns the default cache file path under the project's Saved directory. */
	static FString GetDefaultCachePath();

private:

	/** Cache structure version for future migration. */
	int32 CacheVersion = CACHE_VERSION;

	/** When the cache was last saved. */
	FDateTime SavedAt;

	/** Per-serializer format version tracking. Key = SerializerTypeId. */
	TMap<uint32, FSGSerializerFormatVersionInfo> FormatVersionsBySerializerId;

	/** True if the cache has been loaded or populated by a scan. */
	uint8 bIsLoaded : 1 = false;

	/** True if the cache is stale and needs a rescan. */
	uint8 bIsDirty : 1 = true;
};
