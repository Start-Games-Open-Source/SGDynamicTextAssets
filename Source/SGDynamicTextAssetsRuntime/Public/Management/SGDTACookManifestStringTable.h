// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Deduplicated string table for the binary cook manifest.
 *
 * Stores unique strings (class names, user-facing IDs) and provides
 * index-based access. Duplicate strings map to the same index,
 * significantly reducing file size when many entries share the same
 * class name (e.g., all weapons reference "UWeaponData" once).
 *
 * Wire format:
 *   uint32 Count (number of unique strings)
 *   For each string: uint16 ByteLength + UTF-8 bytes (no null terminator)
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetCookManifestStringTable
{
public:

	/** Sentinel value representing an absent or empty string field. */
	static constexpr uint32 INVALID_STRING_INDEX = 0xFFFFFFFF;

	FSGDynamicTextAssetCookManifestStringTable() = default;

	/**
	 * Adds a non-empty string to the table, deduplicating via internal map.
	 * If the string already exists, returns the existing index.
	 *
	 * @param InString The string to add (must not be empty, use INVALID_STRING_INDEX for absent fields)
	 * @return Index into the string table, or INVALID_STRING_INDEX if InString was empty
	 */
	uint32 AddString(const FString& InString);

	/**
	 * Retrieves a string by index.
	 *
	 * @param Index The string table index (or INVALID_STRING_INDEX for absent fields)
	 * @return The string at the given index, or an empty string if the index is invalid or out of range
	 */
	const FString& GetString(uint32 Index) const;

	/**
	 * Serializes the string table to an archive.
	 * Writes uint32 Count, then for each string: uint16 ByteLength + UTF-8 bytes.
	 *
	 * @param Ar The archive to write to (must be saving)
	 */
	void SerializeTo(FArchive& Ar) const;

	/**
	 * Deserializes the string table from an archive.
	 * Reads the format written by SerializeTo and rebuilds internal data structures.
	 *
	 * @param Ar The archive to read from (must be loading)
	 * @return True if deserialization succeeded, false on malformed data
	 */
	bool DeserializeFrom(FArchive& Ar);

	/** Returns true if the string table contains no strings. */
	bool IsEmpty() const;

	/** Returns the number of unique strings in the table. */
	int32 Num() const;

	/** Clears all strings and resets internal data structures. */
	void Reset();

private:

	/** Indexed storage of unique strings. */
	TArray<FString> Strings;

	/** Maps string content to its index in the Strings array for O(1) deduplication. */
	TMap<FString, uint32> DeduplicationMap;
};
