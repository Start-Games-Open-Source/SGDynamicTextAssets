// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"

/**
 * Single entry in the cook manifest.
 * Maps a dynamic text asset ID to its class name and user-facing ID.
 */
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetCookManifestEntry
{
public:

	FSGDynamicTextAssetCookManifestEntry() = default;

	FSGDynamicTextAssetCookManifestEntry(const FSGDynamicTextAssetId& InID, const FString& InClassName,
		const FString& InUserFacingId, const FSGDynamicTextAssetTypeId& InAssetTypeId);

	/** Unique identifier for the dynamic text asset */
	FSGDynamicTextAssetId Id;

	/** Class name including prefix (e.g., "UWeaponData") */
	FString ClassName;

	/** Human-readable identifier (e.g., "excalibur") */
	FString UserFacingId;

	/** Asset Type ID for this entry's class. */
	FSGDynamicTextAssetTypeId AssetTypeId;
};

/**
 * Manifest for cooked dynamic text asset files.
 *
 * Produced during cooking alongside flat ID-named binary files.
 * Maps ID -> ClassName + UserFacingId, enabling class-based lookups
 * and file info queries in packaged builds without decompressing binaries.
 *
 * Binary format (dta_manifest.bin):
 *   [Header: 32 bytes - FSGDynamicTextAssetCookManifestBinaryHeader]
 *   [Entries: 40 bytes each - FGuid Id, FGuid AssetTypeGuid, uint32 classNameIdx, uint32 userFacingIdIdx]
 *   [String Table: FSGDynamicTextAssetCookManifestStringTable]
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetCookManifest
{
public:

	FSGDynamicTextAssetCookManifest() = default;

	/**
	 * Adds an entry to the manifest.
	 *
	 * @param Id Unique identifier for the dynamic text asset
	 * @param ClassName Class name including prefix (e.g., "UWeaponData")
	 * @param UserFacingId Human-readable identifier
	 * @param AssetTypeId Asset Type ID for this entry's class
	 */
	void AddEntry(const FSGDynamicTextAssetId& Id, const FString& ClassName, const FString& UserFacingId,
		const FSGDynamicTextAssetTypeId& AssetTypeId);

	/**
	 * Saves the manifest to a binary file (dta_manifest.bin) in the given directory.
	 * Writes a compact binary format with a deduplicated string table and
	 * truncated SHA-1 integrity hash.
	 *
	 * @param DirectoryPath Absolute path to the output directory
	 * @return True if file was written successfully
	 */
	bool SaveToFileBinary(const FString& DirectoryPath) const;

	/**
	 * Deprecated: Forwards to SaveToFileBinary(). Will be removed once all callers are updated.
	 * @see SaveToFileBinary
	 */
	bool SaveToFile(const FString& DirectoryPath) const { return SaveToFileBinary(DirectoryPath); }

	/**
	 * Loads the manifest from a binary file (dta_manifest.bin) in the given directory.
	 * Reads the compact binary format, validates the header and content hash,
	 * deserializes entries and string table, and builds internal lookup indices.
	 *
	 * @param DirectoryPath Absolute path to the directory containing the manifest
	 * @return True if file was loaded and parsed successfully
	 */
	bool LoadFromFileBinary(const FString& DirectoryPath);

	/**
	 * Deprecated: Forwards to LoadFromFileBinary(). Will be removed once all callers are updated.
	 * @see LoadFromFileBinary
	 */
	bool LoadFromFile(const FString& DirectoryPath) { return LoadFromFileBinary(DirectoryPath); }

	/** Returns true if the manifest has been successfully loaded from disk. */
	bool IsLoaded() const;

	/**
	 * Finds a manifest entry by ID. O(1) lookup.
	 *
	 * @param Id The ID to search for
	 * @return Pointer to the entry if found, nullptr otherwise
	 */
	const FSGDynamicTextAssetCookManifestEntry* FindById(const FSGDynamicTextAssetId& Id) const;

	/**
	 * Finds all manifest entries matching a class name.
	 *
	 * @param ClassName Class name to search for (e.g., "UWeaponData")
	 * @param OutEntries Array to populate with matching entries
	 */
	void FindByClassName(const FString& ClassName, TArray<const FSGDynamicTextAssetCookManifestEntry*>& OutEntries) const;

	/**
	 * Finds all manifest entries matching an Asset Type ID.
	 *
	 * @param AssetTypeId The Asset Type ID to search for
	 * @param OutEntries Array to populate with matching entries
	 */
	void FindByAssetTypeId(const FSGDynamicTextAssetTypeId& AssetTypeId, TArray<const FSGDynamicTextAssetCookManifestEntry*>& OutEntries) const;

	/** Returns the full list of manifest entries. */
	const TArray<FSGDynamicTextAssetCookManifestEntry>& GetEntries() const;

	/** Returns the number of entries in the manifest. */
	int32 Num() const;

	/** Clears all entries and indices. */
	void Clear();

	/** Filename for the manifest file. */
	static const TCHAR* MANIFEST_FILENAME;

private:

	/** Rebuilds the ID and class name lookup indices from the entries array. */
	void RebuildIndices();

	/** All manifest entries */
	TArray<FSGDynamicTextAssetCookManifestEntry> Entries;

	/** ID -> index into Entries array for O(1) lookup */
	TMap<FSGDynamicTextAssetId, int32> IdIndex;

	/** ClassName -> indices into Entries array for class-based queries */
	TMap<FString, TArray<int32>> ClassNameIndex;

	/** AssetTypeId -> indices into Entries array for type-based queries */
	TMap<FSGDynamicTextAssetTypeId, TArray<int32>> AssetTypeIdIndex;

	/** Whether the manifest was successfully loaded from disk */
	uint8 bIsLoaded : 1 = 0;
};
