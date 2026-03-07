// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"

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
 * This is NOT a UInterface — it uses standard C++ polymorphism
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
	 * Returns the unique integer ID for this serializer format.
	 * This ID is stored in binary (.dta.bin) file headers to identify which
	 * serializer to use when loading, without storing the full extension string.
	 *
	 * IMPORTANT: IDs must be globally unique across all registered serializers.
	 * Duplicate ID registration is a fatal error caught at startup via UE_LOG(Fatal).
	 *
	 * Reserved built-in ID range [1, 99]:
	 * 0 = INVALID
	 * 1 = JSON (.dta.json)
	 * 2 = XML (.dta.xml)
	 * 3 = YAML (.dta.yaml)
	 * 98 = FSGTestAltSerializer for tests. (.dta.test.alt)
	 * 99 = FSGTestSerializer for tests. (.dta.test)
	 *
	 * Third-party plugin serializers should use IDs >= 100 to avoid conflicts.
	 * Zero(INVALID_SERIALIZER_TYPE_ID) is invalid and will be rejected at registration time.
	 * @see INVALID_SERIALIZER_TYPE_ID
	 */
	virtual uint32 GetSerializerTypeId() const = 0;

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
	 * Extracts metadata from a serialized string without full deserialization.
	 *
	 * @param InString The serialized string
	 * @param OutId Extracted dynamic text asset ID
	 * @param OutClassName Extracted class name (or TypeId GUID string for new-format files)
	 * @param OutUserFacingId Extracted user-facing ID
	 * @param OutVersion Extracted version string
	 * @param OutAssetTypeId Extracted asset type ID (invalid until serializers populate it)
	 * @return True if extraction succeeded
	 */
	virtual bool ExtractMetadata(const FString& InString,
		FSGDynamicTextAssetId& OutId,
		FString& OutClassName,
		FString& OutUserFacingId,
		FString& OutVersion,
		FSGDynamicTextAssetTypeId& OutAssetTypeId) const = 0;

	/**
	 * Updates one or more metadata fields within an already-serialized string in-place,
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

	/** The invalid serializer type ID to avoid confusion of what the value is. */
	static constexpr uint32 INVALID_SERIALIZER_TYPE_ID = 0;

	/**
	 * Wrapper key for the metadata block (format-agnostic logical name).
	 * All identity fields (type, version, id, userfacingid) are nested under this block.
	 * Each serializer format determines how this block is represented structurally:
	 *   JSON  → "metadata": { ... }
	 *   XML   → <metadata>...</metadata>
	 *   YAML  → metadata: ...
	 * Value: "metadata"
	 */
	static const FString KEY_METADATA;

	/**
	 * Key for the type identifier inside the metadata block.
	 * Currently stores class name string (e.g., "UWeaponData").
	 * Will store FSGDynamicTextAssetTypeId GUID string after serializer migration.
	 * Value: "type"
	 */
	static const FString KEY_TYPE;

	/**
	 * Key for the semantic version string inside the metadata block.
	 * Value: "version"
	 */
	static const FString KEY_VERSION;

	/**
	 * Key for the dynamic text asset GUID inside the metadata block.
	 * Value: "id"
	 */
	static const FString KEY_ID;

	/**
	 * Key for the human-readable identifier inside the metadata block.
	 * Value: "userfacingid"
	 */
	static const FString KEY_USER_FACING_ID;

	/**
	 * Key for the property data block at the root level alongside metadata.
	 * Value: "data"
	 */
	static const FString KEY_DATA;
};
