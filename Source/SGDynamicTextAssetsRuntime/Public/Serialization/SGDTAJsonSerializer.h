// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Serialization/SGDTASerializerBase.h"

class USGDynamicTextAsset;

/**
 * JSON serialization for dynamic text assets.
 *
 * Uses Unreal's property reflection system to serialize/deserialize
 * all UPROPERTY marked fields on USGDynamicTextAsset subclasses.
 *
 * JSON format uses a metadata wrapper block(sgFileInformation), for example:
 * {
 *   "sgFileInformation": {
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

    /** FSGDTASerializerFormat for the JSON serializer. */
    static const FSGDTASerializerFormat FORMAT;

    /** @deprecated Use FORMAT instead. */
    UE_DEPRECATED(5.6, "Use FORMAT instead. Will be removed in UE 5.7")
    static constexpr uint32 TYPE_ID = SGDynamicTextAssetConstants::JSON_SERIALIZER_TYPE_ID;

    // ISGDynamicTextAssetSerializer overrides
    virtual FString GetFileExtension() const override;
    virtual FText GetFormatName() const override;
    virtual FText GetFormatDescription() const override;
    virtual FSGDTASerializerFormat GetSerializerFormat() const override;
    virtual FSGDynamicTextAssetVersion GetFileFormatVersion() const override;
    virtual bool SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const override;
    virtual bool DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const override;
    virtual bool ValidateStructure(const FString& InString, FString& OutErrorMessage) const override;
    virtual bool ExtractFileInfo(const FString& InString, FSGDynamicTextAssetFileInfo& OutFileInfo) const override;
    virtual bool UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const override;
    virtual FString GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const override;
    virtual bool ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData,
        const TScriptInterface<ISGDynamicTextAssetProvider>& Provider) const override;
    virtual bool UpdateFileFormatVersion(FString& InOutFileContents, const FSGDynamicTextAssetVersion& NewVersion) const override;
    virtual bool MigrateFileFormat(FString& InOutFileContents, const FSGDynamicTextAssetVersion& CurrentFormatVersion, const FSGDynamicTextAssetVersion& TargetFormatVersion) const override;
    // ~ISGDynamicTextAssetSerializer overrides
};
