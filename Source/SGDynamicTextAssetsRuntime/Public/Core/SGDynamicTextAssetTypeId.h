// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/Class.h"
#include "Misc/Guid.h"

#include "SGDynamicTextAssetTypeId.generated.h"

/**
 * Unique identity wrapper for dynamic text asset types (classes).
 *
 * Wraps a raw FGuid in a dedicated type so that asset type identity is
 * distinguishable at the type level from both arbitrary GUIDs and from
 * FSGDynamicTextAssetId (which identifies individual asset instances).
 * This separation prevents accidental misuse between asset IDs and type IDs.
 *
 * Each registered UClass derived from USGDynamicTextAsset is assigned a
 * stable FSGAssetTypeId that persists across class renames, enabling
 * file identity to be decoupled from C++ class names.
 *
 * Supports binary serialization, network serialization, and text
 * export/import for clipboard and config round-tripping.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetTypeId
{
	GENERATED_BODY()
public:

	FSGDynamicTextAssetTypeId() = default;

	FSGDynamicTextAssetTypeId(const FGuid& InGuid);

	explicit FSGDynamicTextAssetTypeId(FGuid&& InGuid);

	/** Returns true if the underlying GUID is valid (non-zero). */
	bool IsValid() const;

	/** Returns the underlying FGuid. */
	const FGuid& GetGuid() const;

	/** Sets the underlying GUID. */
	void SetGuid(const FGuid& InGuid);

	/**
	 * Converts a string to a GUID and stores it (if one GUID is already set then it will be overridden).
	 * @return true if the string was converted successfully, false otherwise.
	 */
	bool ParseString(const FString& GuidString);

	/** Resets to an invalid (all-zero) state. */
	void Invalidate();

	/**
	 * Returns the GUID as a hyphenated string.
	 * Example: "550E8400-E29B-41D4-A716-446655440000"
	 */
	FString ToString() const;

	/** Generates a brand-new unique asset type ID in place. */
	void GenerateNewId();

	/** Generates a brand-new unique asset type ID. */
	static FSGDynamicTextAssetTypeId NewGeneratedId();

	/** Constructs from an existing FGuid. */
	static FSGDynamicTextAssetTypeId FromGuid(const FGuid& InGuid);

	/**
	 * Parses an asset type ID from a string.
	 * Returns an invalid ID if parsing fails.
	 *
	 * @param InString The string to parse (hyphenated GUID format)
	 * @return The parsed FSGAssetTypeId, or an invalid ID on failure
	 */
	static FSGDynamicTextAssetTypeId FromString(const FString& InString);

	void operator=(const FGuid& Other);

	bool operator==(const FGuid& Other) const;
	bool operator!=(const FGuid& Other) const;
	bool operator==(const FSGDynamicTextAssetTypeId& Other) const;
	bool operator!=(const FSGDynamicTextAssetTypeId& Other) const;

	friend uint32 GetTypeHash(const FSGDynamicTextAssetTypeId& Id)
	{
		return GetTypeHash(Id.Guid);
	}

	/**
	 * Binary archive serialization.
	 * Serializes the underlying GUID directly without property tags.
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
	 * Exports the ID as a text string for clipboard and config usage.
	 *
	 * @param ValueStr Output string to append the exported value to
	 * @param DefaultValue Default value for comparison (unused)
	 * @param Parent Owning UObject (unused)
	 * @param PortFlags Export flags
	 * @param ExportRootScope Root export scope (unused)
	 * @return True if export succeeded
	 */
	bool ExportTextItem(FString& ValueStr, const FSGDynamicTextAssetTypeId& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;

	/**
	 * Imports the ID from a text string.
	 *
	 * @param Buffer Input text buffer (advanced past consumed characters)
	 * @param PortFlags Import flags
	 * @param Parent Owning UObject (unused)
	 * @param ErrorText Output for error messages
	 * @return True if import succeeded
	 */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

#if WITH_EDITOR

	DECLARE_MULTICAST_DELEGATE_TwoParams(FSGAssetTypeIdGuidChangeSignature, FGuid/*Previous*/, FGuid/*New*/);
	/**
	 * For editor tooling, broadcasts when this type ID's GUID is changed.
	 * Param 1 = Previous GUID
	 * Param 2 = New/Current GUID
	 */
	FSGAssetTypeIdGuidChangeSignature OnGuidChanged_Editor;

#endif

	/** Invalid Asset Type ID. */
	static const FSGDynamicTextAssetTypeId INVALID_TYPE_ID;

private:

	/** The underlying unique identifier. */
	UPROPERTY(Transient)
	FGuid Guid;
};

template<>
struct TStructOpsTypeTraits<FSGDynamicTextAssetTypeId> : public TStructOpsTypeTraitsBase2<FSGDynamicTextAssetTypeId>
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
