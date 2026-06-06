// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Statics/SGDTAConstants.h"

#include "SGDTASerializerFormat.generated.h"

class ISGDynamicTextAssetSerializer;

/**
 * Type-safe wrapper around a uint32 serializer format identifier.
 *
 * Each registered ISGDynamicTextAssetSerializer is assigned a unique integer
 * ID that identifies its format. This struct wraps that raw uint32 in a
 * dedicated type so that serializer format identity is distinguishable at
 * the type level from arbitrary integers.
 *
 * Built-in format IDs (reserved range 1-99):
 *   0 = Invalid (no format)
 *   1 = JSON  (.dta.json)
 *   2 = XML   (.dta.xml)
 *   3 = YAML  (.dta.yaml)
 *
 * Third-party serializers should use IDs >= 100.
 *
 * Supports binary serialization, network serialization, and text
 * export/import for clipboard and config round-tripping.
 *
 * --------------------------------------------------
 * META TAGS
 *
 * SGDTABitmask: This will turn this type from selecting a single serializer format, for selecting multiple as a bitmask.
 *     Example: UPROPERTY(meta = (SGDTABitmask))
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDTASerializerFormat
{
	GENERATED_BODY()
public:

	FSGDTASerializerFormat() = default;

	explicit FSGDTASerializerFormat(const uint32& InTypeId);

	/** Returns true if the TypeId is non-zero (a valid format). */
	bool IsValid() const;

	/** Returns the underlying uint32 type identifier. */
	uint32 GetTypeId() const;

	/** Sets the underlying type identifier. */
	void SetTypeId(const uint32& InTypeId);

	/**
	 * Returns the TypeId as a decimal string.
	 * Example: "1" for JSON, "0" for invalid.
	 */
	FString ToString() const;

	/** Constructs from an existing uint32 type ID. */
	static FSGDTASerializerFormat FromTypeId(uint32 InTypeId);

	/**
	 * Constructs from a file extension string by looking up the registered serializer.
	 * Returns an invalid format if no serializer is registered for the extension.
	 *
	 * @param Extension The file extension to look up (e.g., ".dta.json")
	 */
	static FSGDTASerializerFormat FromExtension(const FString& Extension);

	/**
	 * Returns the human-readable name of this format by querying the serializer registry.
	 * Returns "Invalid" if the TypeId is not registered.
	 */
	FText GetFormatName() const;

	/**
	 * Returns the file extension for this format by querying the serializer registry.
	 * Returns an empty string if the TypeId is not registered.
	 */
	FString GetFileExtension() const;

	/**
	 * Returns a description of this format by querying the serializer registry.
	 * Returns "Invalid" if the TypeId is not registered.
	 */
	FText GetFormatDescription() const;

	/**
	 * Returns the serializer instance for this format from the registry.
	 * Returns nullptr if the TypeId is invalid or not registered.
	 */
	TSharedPtr<ISGDynamicTextAssetSerializer> GetSerializer() const;

	/** Implicit conversion to uint32 so this struct can be used with switch statements. */
	operator uint32() const;

	void operator=(const uint32& Other);

	bool operator==(const FSGDTASerializerFormat& Other) const;
	bool operator!=(const FSGDTASerializerFormat& Other) const;
	bool operator==(const uint32& Other) const;
	bool operator!=(const uint32& Other) const;

	friend uint32 GetTypeHash(const FSGDTASerializerFormat& Format)
	{
		return GetTypeHash(Format.TypeId);
	}

	/**
	 * Binary archive serialization.
	 * Serializes the underlying uint32 directly without property tags.
	 *
	 * @param Ar The archive to serialize to/from
	 * @return True (serialization always succeeds for this simple struct)
	 */
	bool Serialize(FArchive& Ar);

	/**
	 * Network replication serialization.
	 *
	 * @param Ar The archive to serialize to/from
	 * @param Map Package map for name/object resolution (unused)
	 * @param bOutSuccess Set to true on success
	 * @return True if serialization succeeded
	 */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	/**
	 * Exports the format ID as a decimal text string for clipboard and config usage.
	 *
	 * @param ValueStr Output string to append the exported value to
	 * @param DefaultValue Default value for comparison (unused)
	 * @param Parent Owning UObject (unused)
	 * @param PortFlags Export flags
	 * @param ExportRootScope Root export scope (unused)
	 * @return True if export succeeded
	 */
	bool ExportTextItem(FString& ValueStr, const FSGDTASerializerFormat& DefaultValue,
		UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;

	/**
	 * Imports the format ID from a decimal text string.
	 *
	 * @param Buffer Input text buffer (advanced past consumed characters)
	 * @param PortFlags Import flags
	 * @param Parent Owning UObject (unused)
	 * @param ErrorText Output for error messages
	 * @return True if import succeeded
	 */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	/** Invalid serializer format (TypeId = 0). */
	static const FSGDTASerializerFormat INVALID;

private:

	/** The underlying serializer type identifier. */
	UPROPERTY(Transient)
	uint32 TypeId = SGDynamicTextAssetConstants::INVALID_SERIALIZER_TYPE_ID;
};

template<>
struct TStructOpsTypeTraits<FSGDTASerializerFormat> : public TStructOpsTypeTraitsBase2<FSGDTASerializerFormat>
{
	enum
	{
		WithSerializer = true,
		WithNetSerializer = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithIdenticalViaEquality = true
	};
};
