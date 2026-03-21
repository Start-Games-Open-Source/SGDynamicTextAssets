// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Serialization/SGDynamicTextAssetSerializerBase.h"

/**
 * XML serialization for dynamic text assets.
 *
 * Implements ISGDynamicTextAssetSerializer for polymorphic format dispatch.
 * Uses Unreal's built-in XmlParser module (FXmlFile) for reading and
 * constructs XML strings manually for writing (the XmlParser module
 * is read-only).
 *
 * Property values are converted via the FSGDynamicTextAssetSerializerBase
 * JSON-intermediate helpers (SerializePropertyToValue /
 * DeserializeValueToProperty), keeping format-specific complexity
 * contained to the XML-to-FJsonValue bridge.
 *
 * XML format uses a metadata wrapper block(sgFileInformation), for example:
 * <?xml version="1.0" encoding="UTF-8"?>
 * <DynamicTextAsset>
 *     <sgFileInformation>
 *         <type>UWeaponData</type>
 *         <version>1.0.0</version>
 *         <id>550E8400-E29B-41D4-A716-446655440000</id>
 *         <userfacingid>excalibur</userfacingid>
 *     </sgFileInformation>
 *     <data>
 *         <Damage>50.0</Damage>
 *         <Tags>
 *             <Item>fast</Item>
 *             <Item>heavy</Item>
 *         </Tags>
 *     </data>
 * </DynamicTextAsset>
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetXmlSerializer : public FSGDynamicTextAssetSerializerBase
{
public:

    FSGDynamicTextAssetXmlSerializer() = default;

#if WITH_EDITOR
    virtual const FSlateBrush* GetIconBrush() const override;
#endif

    /**
     * Unique integer ID for the XML serializer format.
     * Stored in binary (.dta.bin) file headers so the loader can route the
     * decompressed payload back to this serializer without a string lookup.
     *
     * Range 1–99 is reserved for built-in serializers.
     * Third-party serializers should use IDs >= 100.
     */
    static constexpr uint32 TYPE_ID = 2;

    // ISGDynamicTextAssetSerializer overrides
    virtual FString GetFileExtension() const override;
    virtual FText GetFormatName() const override;
    virtual FText GetFormatDescription() const override;
    virtual uint32 GetSerializerTypeId() const override;
    virtual FSGDynamicTextAssetVersion GetFileFormatVersion() const override;
    virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const override;
    virtual bool DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const override;
    virtual bool ValidateStructure(const FString& InString, FString& OutErrorMessage) const override;
    virtual bool ExtractFileInfo(const FString& InString, FSGDynamicTextAssetFileInfo& OutFileInfo) const override;
    virtual bool UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const override;
    virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const override;
    virtual bool ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData) const override;
    virtual bool UpdateFileFormatVersion(FString& InOutFileContents, const FSGDynamicTextAssetVersion& NewVersion) const override;
    virtual bool MigrateFileFormat(FString& InOutFileContents, const FSGDynamicTextAssetVersion& CurrentFormatVersion, const FSGDynamicTextAssetVersion& TargetFormatVersion) const override;
    // ~ISGDynamicTextAssetSerializer overrides
};
