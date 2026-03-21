// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "UObject/SoftObjectPtr.h"

class FJsonObject;

/**
 * Single entry in a type manifest.
 * Maps a type ID to a class and its parent type in the hierarchy.
 */
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetTypeManifestEntry
{
public:

	FSGDynamicTextAssetTypeManifestEntry() = default;

	FSGDynamicTextAssetTypeManifestEntry(const FSGDynamicTextAssetTypeId& InTypeId,
		const TSoftClassPtr<UObject>& InClass, const FSGDynamicTextAssetTypeId& InParentTypeId);

	/** Unique type identifier for this class. */
	FSGDynamicTextAssetTypeId TypeId;

	/** Soft reference to the UClass  - avoids hard load dependencies. */
	TSoftClassPtr<UObject> Class;

	/**
	 * Cached class name extracted from the soft class path.
	 * Populated during AddType() and LoadFromFile() for fast lookup without resolving the soft pointer.
	 */
	FString ClassName;

	/** Parent type ID. Invalid (zero) GUID indicates a root type. */
	FSGDynamicTextAssetTypeId ParentTypeId;
};

/**
 * Per-root-type manifest that maps type IDs to UClass soft references.
 *
 * Each root class registered via RegisterDynamicTextAssetClass() gets its own
 * manifest, stored as {RootClassName}_TypeManifest.json alongside the root's
 * content folder. The manifest tracks the full class hierarchy beneath that root,
 * assigning stable GUIDs that persist across class renames.
 *
 * Manifest JSON format:
 * {
 *   "$schema": "dta_type_manifest",
 *   "$version": 1,
 *   "rootTypeId": "A1B2C3D4-...",
 *   "types": [
 *     {
 *       "typeId": "A1B2C3D4-...",
 *       "className": "UWeaponData",
 *       "parentTypeId": "00000000-0000-0000-0000-000000000000"
 *     }
 *   ]
 * }
 *
 * Server overlay support: server-provided type data can override or extend
 * the local manifest without modifying the on-disk file. Overlay entries
 * take precedence over local entries when resolving via GetEffectiveEntry().
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetTypeManifest
{
public:

	FSGDynamicTextAssetTypeManifest() = default;

	/**
	 * Loads the manifest from a JSON file.
	 *
	 * @param FilePath Absolute path to the manifest JSON file
	 * @return True if file was loaded and parsed successfully
	 */
	bool LoadFromFile(const FString& FilePath);

	/**
	 * Saves the manifest to a JSON file.
	 *
	 * @param FilePath Absolute path to write the manifest JSON file
	 * @return True if file was written successfully
	 */
	bool SaveToFile(const FString& FilePath) const;

	/**
	 * Finds a local manifest entry by type ID. O(1) lookup.
	 * Does not consider server overlays  - use GetEffectiveEntry() for that.
	 *
	 * @param TypeId The type ID to search for
	 * @return Pointer to the entry if found, nullptr otherwise
	 */
	const FSGDynamicTextAssetTypeManifestEntry* FindByTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;

	/**
	 * Finds a local manifest entry by class name. O(1) lookup.
	 * Does not consider server overlays.
	 *
	 * @param ClassName Class name to search for (e.g., "UWeaponData")
	 * @return Pointer to the entry if found, nullptr otherwise
	 */
	const FSGDynamicTextAssetTypeManifestEntry* FindByClassName(const FString& ClassName) const;

	/**
	 * Adds a type entry to the manifest.
	 * If a type with the same TypeId already exists, it is replaced.
	 *
	 * @param TypeId Unique type identifier
	 * @param InClass Soft class reference
	 * @param ParentTypeId Parent type ID (invalid = root)
	 */
	void AddType(const FSGDynamicTextAssetTypeId& TypeId, const TSoftClassPtr<UObject>& InClass,
		const FSGDynamicTextAssetTypeId& ParentTypeId);

	/**
	 * Removes a type entry from the manifest.
	 *
	 * @param TypeId The type ID to remove
	 * @return True if the entry was found and removed
	 */
	bool RemoveType(const FSGDynamicTextAssetTypeId& TypeId);

	/** Returns all local type entries. Does not include server overlay entries. */
	const TArray<FSGDynamicTextAssetTypeManifestEntry>& GetAllTypes() const;

	/**
	 * Returns all effective type entries, combining local entries with server overlay.
	 * Local entries overridden by server are replaced, server-disabled entries are excluded,
	 * and server-added entries (not present locally) are appended.
	 *
	 * @param OutEntries Array to populate with all effective type entries
	 */
	void GetAllEffectiveTypes(TArray<FSGDynamicTextAssetTypeManifestEntry>& OutEntries) const;

	/**
	 * Returns the soft class pointer for a given type ID.
	 * Checks the server overlay first, then local entries.
	 *
	 * @param TypeId The type ID to look up
	 * @return The soft class pointer, or a null TSoftClassPtr if not found
	 */
	TSoftClassPtr<UObject> GetSoftClassPtr(const FSGDynamicTextAssetTypeId& TypeId) const;

	/**
	 * Returns the soft class pointer for a given class name.
	 * Only checks local entries (not server overlay).
	 *
	 * @param ClassName Class name to look up (e.g., "UWeaponData")
	 * @return The soft class pointer, or a null TSoftClassPtr if not found
	 */
	TSoftClassPtr<UObject> GetSoftClassPtrByClassName(const FString& ClassName) const;

	/** Returns the root type ID for this manifest. */
	const FSGDynamicTextAssetTypeId& GetRootTypeId() const;

	/** Sets the root type ID for this manifest. */
	void SetRootTypeId(const FSGDynamicTextAssetTypeId& InRootTypeId);

	/** Returns the number of local type entries. */
	int32 Num() const;

	/** Returns true if the manifest has been modified since last save/load. */
	bool IsDirty() const;

	/** Clears all local entries and indices. Does not affect server overlay. */
	void Clear();

	/**
	 * Applies server-provided type overrides.
	 * Server overlay is stored separately  - the local manifest file is never modified.
	 *
	 * Expected JSON format (same "types" array as the manifest):
	 * {
	 *   "types": [
	 *     { "typeId": "...", "className": "...", "parentTypeId": "..." }
	 *   ]
	 * }
	 *
	 * Server can: add new type entries, override existing entries (remap type ID
	 * to different class), or remove entries by providing an entry with an empty className.
	 * Invalid server entries (bad GUIDs, missing class paths) are rejected with warnings.
	 *
	 * @param ServerData JSON object containing server type overrides
	 */
	void ApplyServerOverrides(const TSharedPtr<FJsonObject>& ServerData);

	/**
	 * Returns the effective entry for a type ID, considering server overlay.
	 * Server overlay entries take precedence over local entries.
	 * An overlay entry with empty ClassName means the type is disabled.
	 *
	 * @param TypeId The type ID to look up
	 * @return Pointer to the effective entry, or nullptr if not found or disabled
	 */
	const FSGDynamicTextAssetTypeManifestEntry* GetEffectiveEntry(const FSGDynamicTextAssetTypeId& TypeId) const;

	/** Clears all server overlay entries, reverting to local-only state. */
	void ClearServerOverrides();

	/** Returns true if there are any active server overlay entries. */
	bool HasServerOverrides() const;

	/** Current type manifest format version. */
	static constexpr int32 MANIFEST_VERSION = 1;

	/** Key for schema type. */
	static const FString KEY_SCHEMA;

	/** Value for schema type. */
	static const FString VALUE_SCHEMA;

	/** Key for manifest version. */
	static const FString KEY_VERSION;

	/** Key for root type ID. */
	static const FString KEY_ROOT_TYPE_ID;

	/** Key for types array. */
	static const FString KEY_TYPES;

	/** Key for a type entry's type ID. */
	static const FString KEY_TYPE_ID;

	/** Key for a type entry's class name. */
	static const FString KEY_CLASS_NAME;

	/** Key for a type entry's full soft class path (e.g., "/Script/ModuleName.ClassName"). */
	static const FString KEY_CLASS_PATH;

	/** Key for a type entry's parent type ID. */
	static const FString KEY_PARENT_TYPE_ID;

private:

	/** Rebuilds the TypeId and ClassName lookup indices from the entries array. */
	void RebuildIndices();

	/**
	 * Parses a single type entry from a JSON object.
	 *
	 * @param EntryObject JSON object to parse
	 * @param OutEntry Populated on success
	 * @return True if the entry was valid and parsed successfully
	 */
	static bool ParseTypeEntry(const TSharedPtr<FJsonObject>& EntryObject, FSGDynamicTextAssetTypeManifestEntry& OutEntry);

	/** Root type ID for this manifest. */
	FSGDynamicTextAssetTypeId RootTypeId;

	/** All local manifest entries. */
	TArray<FSGDynamicTextAssetTypeManifestEntry> Types;

	/** TypeId -> index into Types array for O(1) lookup. */
	TMap<FSGDynamicTextAssetTypeId, int32> TypeIdIndex;

	/** ClassName -> index into Types array for O(1) lookup. */
	TMap<FString, int32> ClassNameIndex;

	/** Server overlay entries. Stored separately from local entries. */
	TMap<FSGDynamicTextAssetTypeId, FSGDynamicTextAssetTypeManifestEntry> ServerOverlayEntries;

	/** Whether the manifest has been modified since last save/load. */
	uint8 bIsDirty : 1 = 0;
};
