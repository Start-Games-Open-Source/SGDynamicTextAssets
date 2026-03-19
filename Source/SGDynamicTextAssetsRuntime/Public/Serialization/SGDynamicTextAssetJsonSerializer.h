// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Serialization/SGDynamicTextAssetSerializerBase.h"

class USGDynamicTextAsset;

/**
 * JSON serialization for dynamic text assets.
 *
 * Implements ISGDynamicTextAssetSerializer for polymorphic format dispatch
 * while also providing static utility methods for direct usage with
 * USGDynamicTextAsset pointers.
 *
 * Uses Unreal's property reflection system to serialize/deserialize
 * all UPROPERTY marked fields on USGDynamicTextAsset subclasses.
 *
 * JSON format uses a metadata wrapper block, for example:
 * {
 *   "metadata": {
 *     "type": "UWeaponData",
 *     "version": "1.0.0",
 *     "id": "...",
 *     "userfacingid": "excalibur"
 *   },
 *   "data": { ... properties ... }
 * }
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetJsonSerializer : public FSGDynamicTextAssetSerializerBase
{
public:

    FSGDynamicTextAssetJsonSerializer() = default;

#if WITH_EDITOR
    virtual const FSlateBrush* GetIconBrush() const override;
#endif

    /**
     * Unique integer ID for the JSON serializer format.
     * Stored in binary (.dta.bin) file headers so the loader can route the
     * decompressed payload back to this serializer without a string lookup.
     *
     * Range 1–99 is reserved for built-in serializers.
     * Third-party serializers should use IDs >= 100.
     */
    static constexpr uint32 TYPE_ID = 1;

    // ISGDynamicTextAssetSerializer overrides
    virtual FString GetFileExtension() const override;
    virtual FText GetFormatName() const override;
    virtual FText GetFormatDescription() const override;
    virtual uint32 GetSerializerTypeId() const override;
    virtual FSGDynamicTextAssetVersion GetFileFormatVersion() const override;
    virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const override;
    virtual bool DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const override;
    virtual bool ValidateStructure(const FString& InString, FString& OutErrorMessage) const override;
    virtual bool ExtractMetadata(const FString& InString, FSGDynamicTextAssetFileMetadata& OutMetadata) const override;
    virtual bool UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const override;
    virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const override;
    virtual bool ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData) const override;
    virtual bool UpdateFileFormatVersion(FString& InOutFileContents, const FSGDynamicTextAssetVersion& NewVersion) const override;
    // ~ISGDynamicTextAssetSerializer overrides
};
