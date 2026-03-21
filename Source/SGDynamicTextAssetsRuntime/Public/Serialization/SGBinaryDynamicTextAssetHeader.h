// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Ensuring binary layout across platforms
// as a safety measure by making alignment boundary 1
#pragma pack(push, 1)
/**
 * Header structure for binary dynamic text asset files.
 * Fixed 80-byte header for quick metadata access without decompression.
 */
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGBinaryDynamicTextAssetHeader
{
public:

	FSGBinaryDynamicTextAssetHeader();

	/** Returns true if the magic number is valid */
	bool IsValid() const;

	/** Returns true if the ContentHash field is non-zero (file with integrity check) */
	bool HasContentHash() const;

	/** Returns true if the AssetTypeGuid field is non-zero (valid asset type identity) */
	bool HasAssetTypeGuid() const;

	/** Returns the magic number as a hex string (from instance data) */
	FString GetMagicNumberString() const;

	/** Returns the expected magic number as a hex string (static) */
	static FString GetExpectedMagicNumberString();

	/** Serializes the header to/from an archive */
	void Serialize(FArchive& Ar);

	/**
	 * 5 bytes for "SGDTA".
	 * (0x53 = 'S', 0x47 = 'G', 0x44 = 'D', 0x54 = 'T', 0x41 = 'A')
	 *
	 * Increment CURRENT_SCHEMA_VERSION if this changes.
	 * Expected magic number value as a char array
	 */
	static constexpr uint8 MAGIC_NUMBER[] = { 0x53, 0x47, 0x44, 0x54, 0x41 };

	/** Current plugin schema version */
	static constexpr uint32 CURRENT_SCHEMA_VERSION = 1;

	/** Size of the SHA-1 content hash in bytes (160 bits) */
	static constexpr uint32 CONTENT_HASH_SIZE = 20;

	/** Total header size in bytes */
	static constexpr uint32 HEADER_SIZE = 80;

	/**
	 * 5 bytes
	 *
	 * Magic hexadecimal number identifying this as a DTA binary file: "SGDTA"
	 */
	uint8 MagicNumber[sizeof(MAGIC_NUMBER)];

	/**
	 * 3 bytes + 5 bytes (MagicNumber) = 8 bytes
	 *
	 * Padding for magic number alignment.
	 */
	uint8 MagicNumberPadding[8 - sizeof(MAGIC_NUMBER)] = { 0 };

	/**
	 * 4 bytes
	 * Plugin schema version for format compatibility checks
	 */
	uint32 PluginSchemaVersion = CURRENT_SCHEMA_VERSION;

	/**
	 * 4 bytes
	 * Compression method used (maps to ESGDynamicTextAssetCompressionMethod)
	 */
	uint32 CompressionType = 0;

	/**
	 * 4 bytes
	 * Size of the data before compression
	 */
	uint32 UncompressedSize = 0;

	/**
	 * 4 bytes
	 * Size of the compressed data
	 */
	uint32 CompressedSize = 0;

	/**
	 * 4 bytes
	 *
	 * Integer ID identifying which serializer produced the compressed payload.
	 * Resolved at load time via FSGDynamicTextAssetFileManager::FindSerializerForTypeId().
	 * Zero is invalid  - all registered serializers must return a non-zero ID.
	 *
	 * Built-in IDs (reserved range 1–99):
	 *   1 = JSON (.dta.json)   - see FSGDynamicTextAssetJsonSerializer::TYPE_ID
	 *
	 * Third-party plugin serializers must use IDs >= 100 to avoid conflicts.
	 * Duplicate ID registration is a fatal error caught at startup.
	 */
	uint32 SerializerTypeId = 0;

	/**
	 * 16 bytes
	 * GUID of the dynamic text asset instance.
	 */
	FGuid Guid;

	/**
	 * 16 bytes
	 *
	 * GUID of the asset type (class identity).
	 * Enables class resolution without decompressing the payload.
	 * Resolved at load time via USGDynamicTextAssetRegistry::ResolveClassForTypeId().
	 * All zeros when the type is unknown or was not set at write time.
	 */
	FGuid AssetTypeGuid;

	/**
	 * 20 bytes
	 *
	 * SHA-1 hash of the compressed payload for integrity verification.
	 * Computed over the compressed data bytes after the header.
	 * If all zeros, hash validation is skipped (future-proofing for schema changes).
	 */
	uint8 ContentHash[CONTENT_HASH_SIZE] = { 0 };
};
// Return the pack to normal behavior
#pragma pack(pop)
