// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

#include "SGDynamicTextAssetVersion.generated.h"

/**
 * Version information for a dynamic text asset using semantic versioning.
 * 
 * Major: Increment for breaking changes (property removed, type changed)
 * Minor: Increment for backwards-compatible additions (new optional property)
 * Patch: Increment for data value changes only
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetVersion
{
    GENERATED_BODY()
public:

    FSGDynamicTextAssetVersion() = default;

    FSGDynamicTextAssetVersion(int32 InMajor, int32 InMinor, int32 InPatch)
        : Major(FMath::Max(InMajor, 1))
        , Minor(FMath::Max(InMinor, 0))
        , Patch(FMath::Max(InPatch, 0))
    { }

    /** Returns true if version fields represent a usable version (Major >= 1) */
    bool IsValid() const;

    /** Returns version as string in format "Major.Minor.Patch" */
    FString ToString() const;

    /** Returns version as FText in format "Major.Minor.Patch" */
    FText ToText() const;

    /**
     * Returns the major version within this struct.
     *
     * Intended for utility with delegates, callbacks, and edgecase approaches to writing code.
     * Most other use cases will be fine with just directly getting the Major member variable of this struct.
     */
    int32 GetMajor() const;

    /**
     * Returns the minor version within this struct.
     *
     * Intended for utility with delegates, callbacks, and edgecase approaches to writing code.
     * Most other use cases will be fine with just directly getting the Minor member variable of this struct.
     */
    int32 GetMinor() const;

    /**
     * Returns the patch version within this struct.
     *
     * Intended for utility with delegates, callbacks, and edgecase approaches to writing code.
     * Most other use cases will be fine with just directly getting the Patch member variable of this struct.
     */
    int32 GetPatch() const;

    /** Parse version from string format "Major.Minor.Patch" */
    static FSGDynamicTextAssetVersion ParseFromString(const FString& VersionString);

    /** Parse version from string into this instance. Returns true if the string was a valid "Major.Minor.Patch" format */
    bool ParseString(const FString& VersionString);

    /** Returns true if major versions match (compatible for loading) */
    bool IsCompatibleWith(const FSGDynamicTextAssetVersion& Other) const;

    /**
     * Returns true if this version falls within the given range (inclusive).
     * Compares Major, Minor, and Patch components using standard ordering.
     *
     * @param Min The minimum version (inclusive).
     * @param Max The maximum version (inclusive).
     * @return True if Min <= this <= Max.
     */
    bool IsInRange(const FSGDynamicTextAssetVersion& Min, const FSGDynamicTextAssetVersion& Max) const;

    bool operator==(const FSGDynamicTextAssetVersion& Other) const;
    bool operator!=(const FSGDynamicTextAssetVersion& Other) const;
    bool operator<(const FSGDynamicTextAssetVersion& Other) const;
    bool operator>(const FSGDynamicTextAssetVersion& Other) const;
    bool operator<=(const FSGDynamicTextAssetVersion& Other) const;
    bool operator>=(const FSGDynamicTextAssetVersion& Other) const;

    /**
     * Custom serialization for optimized binary format.
     * Writes/reads the 3 integers directly without property tags.
     * 
     * @param Ar The archive to serialize to/from
     * @return True (serialization always succeeds for this simple struct)
     */
    bool Serialize(FArchive& Ar);

    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

    /**
     * Exports the Version as a text string for clipboard and config usage.
     *
     * @param ValueStr Output string to append the exported value to
     * @param DefaultValue Default value for comparison (unused)
     * @param Parent Owning UObject (unused)
     * @param PortFlags Export flags
     * @param ExportRootScope Root export scope (unused)
     * @return True if export succeeded
     */
    bool ExportTextItem(FString& ValueStr, const FSGDynamicTextAssetVersion& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;

    /**
     * Imports the Version from a text string.
     *
     * @param Buffer Input text buffer (advanced past consumed characters)
     * @param PortFlags Import flags
     * @param Parent Owning UObject (unused)
     * @param ErrorText Output for error messages
     * @return True if import succeeded
     */
    bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

    /** Major version, breaking changes require migration. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 Major = 1;

    /** Minor version, backwards-compatible additions. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 Minor = 0;

    /** Patch version, data value changes only. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 Patch = 0;
};

template<>
struct TStructOpsTypeTraits<FSGDynamicTextAssetVersion> : public TStructOpsTypeTraitsBase2<FSGDynamicTextAssetVersion>
{
    enum
    {
        WithNetSerializer = true,
        WithSerializer = true,
        WithExportTextItem = true,
        WithImportTextItem = true,
        WithIdenticalViaEquality = true
    };
};
