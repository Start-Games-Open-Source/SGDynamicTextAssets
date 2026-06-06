// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetBundleData.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Core/SGDTASerializerFormat.h"
#include "Management/SGDynamicTextAssetFileInfo.h"

struct FSlateBrush;
class ISGDynamicTextAssetProvider;

/**
 * Pure C++ interface for dynamic text asset serialization formats.
 *
 * Each serializer handles a specific file format (JSON, XML, etc.)
 * and is registered with FSGDynamicTextAssetFileManager for format dispatch.
 * Serializers are managed as TSharedRef<ISGDynamicTextAssetSerializer>.
 *
 * The JSON serializer (FSGDynamicTextAssetJsonSerializer) is the built-in
 * implementation. External plugins can register custom serializers
 * for additional formats.
 *
 * This is NOT a UInterface  - it uses standard C++ polymorphism
 * with TSharedPtr/TSharedRef semantics.
 */
class SGDYNAMICTEXTASSETSRUNTIME_API ISGDynamicTextAssetSerializer : public TSharedFromThis<ISGDynamicTextAssetSerializer>
{
public:

	virtual ~ISGDynamicTextAssetSerializer() = default;

#if WITH_EDITOR
	/** Intended for editor usage, retrieves the icon brush that this serializer uses. */
	virtual const FSlateBrush* GetIconBrush() const;
#endif


	/** Returns the file extension this serializer handles (e.g., ".dta.json"). */
	virtual FString GetFileExtension() const = 0;

	/** Returns a human-readable name for this format as a FText (e.g., "JSON", "XML"). */
	virtual FText GetFormatName() const = 0;

	/** Utility FString version of ::GetFormatName() that returns a human-readable name for this format (e.g., "JSON", "XML"). */
	FString GetFormatName_String() const;

	/**
	 * Useful for editor tooling and debugging.
	 * Used to give an explanation/description of this serializer.
	 */
	virtual FText GetFormatDescription() const = 0;

	/**
	 * Returns the FSGDTASerializerFormat identifying this serializer.
	 * The format's underlying TypeId is stored in binary (.dta.bin) file headers
	 * to identify which serializer to use when loading.
	 *
	 * IMPORTANT: Format IDs must be globally unique across all registered serializers.
	 * Duplicate ID registration is a fatal error caught at startup via UE_LOG(Fatal).
	 *
	 * Reserved built-in ID range [1, 99]:
	 * 0 = INVALID (FSGDTASerializerFormat::INVALID)
	 * 1 = JSON (.dta.json)
	 * 2 = XML (.dta.xml)
	 * 3 = YAML (.dta.yaml)
	 * 98 = FSGDTATestAltSerializer for tests (.dta.test.alt)
	 * 99 = FSGDTATestSerializer for tests (.dta.test)
	 *
	 * Third-party plugin serializers should use IDs >= 100 to avoid conflicts.
	 * A default-constructed FSGDTASerializerFormat (TypeId=0) is invalid and will
	 * be rejected at registration time.
	 * @see FSGDTASerializerFormat::INVALID
	 */
	virtual FSGDTASerializerFormat GetSerializerFormat() const = 0;

	/**
	 * @deprecated Use GetSerializerFormat() instead.
	 * Returns the underlying uint32 type ID for backward compatibility.
	 */
	UE_DEPRECATED(5.6, "Use GetSerializerFormat() instead. Will be removed in UE 5.7")
	virtual uint32 GetSerializerTypeId() const
	{
		return GetSerializerFormat().GetTypeId();
	}

	/**
	 * Returns the current file format version for this serializer.
	 * Each serializer tracks its own structural format version independently
	 * from the asset data version (KEY_VERSION). When the serializer changes
	 * how it writes files (new fields, renamed keys, changed structure),
	 * this version is incremented.
	 *
	 * Files missing the format version field are assumed to be version 1.0.0
	 * (pre-format-version files).
	 *
	 * @return The current file format version for this serializer
	 */
	virtual FSGDynamicTextAssetVersion GetFileFormatVersion() const = 0;

	/**
	 * Serializes a dynamic text asset provider to a string.
	 *
	 * @param Provider The dynamic text asset provider to serialize
	 * @param OutString The serialized string output
	 * @return True if serialization succeeded
	 */
	virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const = 0;

	/**
	 * Deserializes a string into a dynamic text asset provider.
	 *
	 * @param InString The string to deserialize
	 * @param OutProvider The provider to populate
	 * @param bOutMigrated Set to true if migration was performed
	 * @return True if deserialization succeeded
	 */
	virtual bool DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const = 0;

	/**
	 * Validates the structure of a serialized string without fully deserializing.
	 *
	 * @param InString The string to validate
	 * @param OutErrorMessage Error description if validation fails
	 * @return True if structure is valid
	 */
	virtual bool ValidateStructure(const FString& InString, FString& OutErrorMessage) const = 0;

	/**
	 * Extracts file information from a serialized string into a struct without full deserialization.
	 *
	 * @param InString The serialized string
	 * @param OutFileInfo Populated with extracted file information
	 * @return True if extraction succeeded
	 */
	virtual bool ExtractFileInfo(const FString& InString, FSGDynamicTextAssetFileInfo& OutFileInfo) const = 0;

	/**
	 * Extracts file information from a serialized string without full deserialization.
	 * @deprecated Use the ISGDynamicTextAssetSerializer::ExtractFileInfo instead. Will be removed in UE 5.7
	 */
	UE_DEPRECATED(5.6, "Use the ISGDynamicTextAssetSerializer::ExtractFileInfo instead. Will be removed in UE 5.7")
	virtual bool ExtractMetadata(const FString& InString,
		FSGDynamicTextAssetId& OutId,
		FString& OutClassName,
		FString& OutUserFacingId,
		FString& OutVersion,
		FSGDynamicTextAssetTypeId& OutAssetTypeId) const;

	/**
	 * Updates one or more file information fields within an already-serialized string in-place,
	 * without fully deserializing into a provider object.
	 * Used by Rename and Duplicate operations to patch UserFacingId and/or Id.
	 *
	 * @param InOutContents   The serialized string to update (modified in-place)
	 * @param FieldUpdates    Map of field name to new value (e.g. "UserFacingId" to "new_name")
	 * @return True if at least one field was found and updated
	 */
	virtual bool UpdateFieldsInPlace(FString& InOutContents,
		const TMap<FString, FString>& FieldUpdates) const = 0;

	/**
	 * Generates the default initial file content for a new dynamic text asset of this format.
	 * Called by FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile via the
	 * ON_GENERATE_DEFAULT_CONTENT delegate so that each format produces
	 * syntactically correct initial content rather than hardcoded JSON.
	 *
	 * @param DynamicTextAssetClass The class of the dynamic text asset being created
	 * @param Id The unique ID assigned to the new dynamic text asset
	 * @param UserFacingId The human-readable identifier for the new dynamic text asset
	 * @return The initial file content string in this serializer's format
	 */
	virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass,
		const FSGDynamicTextAssetId& Id,
		const FString& UserFacingId) const = 0;

	/**
	 * Extracts asset bundle metadata from a serialized string without full deserialization.
	 * Resolves the appropriate extender via the Provider's per-DTA override or settings mapping,
	 * then delegates to the extender for format-specific parsing.
	 *
	 * @param InString The serialized string to extract from
	 * @param OutBundleData The bundle data to populate
	 * @param Provider The provider used to resolve the asset bundle extender
	 * @return True if deserialization completed (even with no bundles). False only on system failure.
	 */
	virtual bool ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData,
		const TScriptInterface<ISGDynamicTextAssetProvider>& Provider) const;

	/**
	 * Migrates file content from one file format version to another.
	 * Serializers override this to provide migration logic when the file
	 * format structure changes between versions (e.g., renamed keys,
	 * reorganized blocks).
	 *
	 * The default implementation returns true (no structural changes needed).
	 * Serializers override this when actual structural format changes are introduced.
	 *
	 * @param InOutFileContents The raw file content string, modified in-place on success
	 * @param CurrentFormatVersion The format version found in the file (1.0.0 if absent)
	 * @param TargetFormatVersion The target format version to migrate to
	 * @return True if migration succeeded or was not needed, false on failure
	 */
	virtual bool MigrateFileFormat(FString& InOutFileContents,
		const FSGDynamicTextAssetVersion& CurrentFormatVersion,
		const FSGDynamicTextAssetVersion& TargetFormatVersion) const;

	/**
	 * Updates the fileFormatVersion field in the raw file contents to a new version.
	 * Each serializer overrides this to handle its specific format syntax.
	 * Called by the migration pipeline after MigrateFileFormat() succeeds.
	 *
	 * @param InOutFileContents The raw file content string, modified in-place
	 * @param NewVersion The version to stamp into the file
	 * @return True if the version was successfully updated, false on failure
	 */
	virtual bool UpdateFileFormatVersion(FString& InOutFileContents,
		const FSGDynamicTextAssetVersion& NewVersion) const;

	/** @deprecated Use FSGDTASerializerFormat::INVALID or SGDynamicTextAssetConstants::INVALID_SERIALIZER_TYPE_ID instead. */
	UE_DEPRECATED(5.6, "Use FSGDTASerializerFormat::INVALID or SGDynamicTextAssetConstants::INVALID_SERIALIZER_TYPE_ID instead. Will be removed in UE 5.7")
	static constexpr uint32 INVALID_SERIALIZER_TYPE_ID = SGDynamicTextAssetConstants::INVALID_SERIALIZER_TYPE_ID;

	/**
	 * Wrapper key for the file information block (format-agnostic logical name).
	 * All identity fields (type, version, id, userfacingid) are nested under this block.
	 * Each serializer format determines how this block is represented structurally:
	 *   JSON  -> "sgFileInformation": { ... }
	 *   XML   -> <sgFileInformation>...</sgFileInformation>
	 *   YAML  -> sgFileInformation: ...
	 * Value: "sgFileInformation"
	 */
	static const FString KEY_FILE_INFORMATION;

	/**
	 * Legacy key for the file information block, used for backward compatibility
	 * with files created before format version 2.0.0.
	 * Value: "metadata"
	 */
	static const FString KEY_METADATA_LEGACY;

	/**
	 * Key for the type identifier inside the file information block.
	 * Currently stores class name string (e.g., "UWeaponData").
	 * Will store FSGDynamicTextAssetTypeId GUID string after serializer migration.
	 * Value: "type"
	 */
	static const FString KEY_TYPE;

	/**
	 * Key for the semantic version string inside the file information block.
	 * Value: "version"
	 */
	static const FString KEY_VERSION;

	/**
	 * Key for the dynamic text asset GUID inside the file information block.
	 * Value: "id"
	 */
	static const FString KEY_ID;

	/**
	 * Key for the human-readable identifier inside the file information block.
	 * Value: "userfacingid"
	 */
	static const FString KEY_USER_FACING_ID;

	/**
	 * Key for the file format version string inside the file information block.
	 * Tracks structural changes to the serialization format itself,
	 * independent of the asset data versioning stored in KEY_VERSION.
	 * Files missing this field are assumed to be format version 1.0.0.
	 * Value: "fileFormatVersion"
	 */
	static const FString KEY_FILE_FORMAT_VERSION;

	/**
	 * Key for the per-DTA asset bundle extender override inside the file information block.
	 * When present, the stored ClassId GUID overrides the settings-level extender mapping.
	 * Omitted when no per-DTA override is set (default case).
	 * Value: "assetBundleExtender"
	 */
	static const FString KEY_ASSET_BUNDLE_EXTENDER;

	/**
	 * Key for the property data block at the root level alongside file information.
	 * Value: "data"
	 */
	static const FString KEY_DATA;

	/**
	 * Key for the asset bundle metadata block at the root level.
	 * Contains soft reference groupings extracted from UPROPERTY meta=(AssetBundles="...") tags.
	 * Value: "sgdtAssetBundles"
	 */
	static const FString KEY_SGDT_ASSET_BUNDLES;
};
