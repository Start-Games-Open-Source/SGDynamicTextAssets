// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Serialization/SGDynamicTextAssetSerializerBase.h"

/**
 * YAML serialization for dynamic text assets.
 *
 * Implements ISGDynamicTextAssetSerializer for polymorphic format dispatch.
 * Uses the fkYAML header-only library (MIT license) for reading and writing
 * YAML documents.
 *
 * Property values are converted via the FSGDynamicTextAssetSerializerBase
 * JSON-intermediate helpers (SerializePropertyToValue /
 * DeserializeValueToProperty), keeping format-specific complexity
 * contained to the YAML-to-FJsonValue bridge.
 *
 * YAML format uses a metadata wrapper block, for example:
 * metadata:
 *   type: UWeaponData
 *   version: 1.0.0
 *   id: 550E8400-E29B-41D4-A716-446655440000
 *   userfacingid: excalibur
 * data:
 *   Damage: 50.0
 *   Tags:
 *     - fast
 *     - heavy
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetYamlSerializer : public FSGDynamicTextAssetSerializerBase
{
public:

    FSGDynamicTextAssetYamlSerializer() = default;

#if WITH_EDITOR
    virtual const FSlateBrush* GetIconBrush() const override;
#endif

    /**
     * Unique integer ID for the YAML serializer format.
     * Stored in binary (.dta.bin) file headers so the loader can route the
     * decompressed payload back to this serializer without a string lookup.
     *
     * Range 1-99 is reserved for built-in serializers.
     * Third-party serializers should use IDs >= 100.
     */
    static constexpr uint32 TYPE_ID = 3;

    // ISGDynamicTextAssetSerializer overrides
    virtual FString GetFileExtension() const override;
    virtual FText GetFormatName() const override;
    virtual FText GetFormatDescription() const override;
    virtual uint32 GetSerializerTypeId() const override;
    virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const override;
    virtual bool DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const override;
    virtual bool ValidateStructure(const FString& InString, FString& OutErrorMessage) const override;
    virtual bool ExtractMetadata(const FString& InString, FSGDynamicTextAssetId& OutId, FString& OutClassName, FString& OutUserFacingId, FString& OutVersion, FSGDynamicTextAssetTypeId& OutAssetTypeId) const override;
    virtual bool UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const override;
    virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const override;
    // ~ISGDynamicTextAssetSerializer overrides
};
