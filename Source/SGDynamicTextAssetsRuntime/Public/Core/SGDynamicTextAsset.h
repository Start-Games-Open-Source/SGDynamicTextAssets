// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/Object.h"
#include "SGDynamicTextAssetId.h"
#include "SGDynamicTextAssetVersion.h"
#include "SGDTAValidationResult.h"
#include "ISGDynamicTextAssetProvider.h"

#include "SGDynamicTextAsset.generated.h"

/**
 * Base utility class for dynamic text assets that is provided by default by the SG Dynamic Text Assets plugin.
 * 
 * Dynamic text assets are runtime UObject wrappers for text-based configuration data.
 * They are identified by a unique ID and provide a human-readable UserFacingId.
 *
 * Key characteristics:
 * - Read-only at runtime (constant data)
 * - Soft references only (no hard references), with asset bundle support via meta=(AssetBundles="...")
 * - Runtime classes defined in C++ only (usable in Blueprints)
 *
 * Subclasses should:
 * - Define UPROPERTY fields for their configuration data
 * - Override PostDynamicTextAssetLoaded() for custom initialization
 * - Override ValidateDynamicTextAsset() for custom validation rules
 * - Override MigrateFromVersion() for handling breaking changes
 *
 * Registered as a Dynamic Text Asset base type within FSGDynamicTextAssetsRuntimeModule::StartupModule
 *
 * Abstract ISGDynamicTextAssetProvider's are registered but won't show up in the dynamic text asset browser since they can't be instanced.
 * The dynamic text asset browser will only show registered Instantiable Dynamic Text Assets and root dynamic text asset type's.
 * We don't register Dynamic Text Assets that have their UCLASS marked with:
 * - Hidden
 * - Deprecated
 * - HideDropdown
 */
UCLASS(Abstract, BlueprintType, ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAsset : public UObject, public ISGDynamicTextAssetProvider
{
    GENERATED_BODY()

    friend class USGDynamicTextAssetSubsystem;
    friend class FSGDynamicTextAssetJsonSerializer;

#if WITH_EDITOR
    friend class FSGDTADetailCustomization;
#endif

public:

    USGDynamicTextAsset();

    // ISGDynamicTextAssetProvider interface
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    virtual const FSGDynamicTextAssetId& GetDynamicTextAssetId() const override;
    virtual void SetDynamicTextAssetId(const FSGDynamicTextAssetId& InId) override;

    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    virtual const FString& GetUserFacingId() const override;
    virtual void SetUserFacingId(const FString& InUserFacingId) override;

    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    virtual const FSGDynamicTextAssetVersion& GetVersion() const override;

    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    virtual int32 GetCurrentMajorVersion() const override;
    virtual void SetVersion(const FSGDynamicTextAssetVersion& InVersion) override;

    virtual void PostDynamicTextAssetLoaded() override;
    virtual bool Native_ValidateDynamicTextAsset(FSGDynamicTextAssetValidationResult& OutResult) const override;
    virtual bool MigrateFromVersion(
        const FSGDynamicTextAssetVersion& OldVersion,
        const FSGDynamicTextAssetVersion& CurrentVersion,
        const TSharedPtr<FJsonObject>& OldData) override;
    virtual const FSGDynamicTextAssetBundleData& GetSGDTAssetBundleData() const override;
    virtual FSGDynamicTextAssetBundleData& GetSGDTAssetBundleData_Mutable() override;
    virtual bool HasSGDTAssetBundles() const override;
    virtual FSGDTAClassId GetAssetBundleExtenderOverride() const override;
    virtual void SetAssetBundleExtenderOverride(const FSGDTAClassId& InOverride) override;
    // ~ISGDynamicTextAssetProvider interface

    /** Returns the version as a formatted string */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    FString GetVersionString() const;

    /**
     * Returns the FNames of the base metadata UPROPERTY that will have custom editor tooling.
     *
     * Implemented via GET_MEMBER_NAME_CHECKED in SGDynamicTextAsset.cpp so that renaming any
     * of these properties becomes a compile error rather than a silent runtime mismatch.
     */
    static TSet<FName> GetFileInformationPropertyNames();

    /**
     * Returns the FNames of the base metadata UPROPERTY that will have custom editor tooling.
     *
     * Implemented via GET_MEMBER_NAME_CHECKED in SGDynamicTextAsset.cpp so that renaming any
     * of these properties becomes a compile error rather than a silent runtime mismatch.
     */
    UE_DEPRECATED(5.6, "Use GetFileInformationPropertyNames instead. Will be removed in UE 5.7")
    static TSet<FName> GetMetadataPropertyNames();

protected:

    /**
     * Called after this dynamic text asset's properties have been populated from the serializer.
     * Override to perform custom initialization or caching.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Dynamic Text Asset", meta = (DisplayName = "Post Dynamic Text Asset Loaded"))
    void BP_PostDynamicTextAssetLoaded();
    virtual void BP_PostDynamicTextAssetLoaded_Implementation();

    /**
     * Validates this dynamic text asset's data.
     * Override in C++ or Blueprint to add custom validation rules.
     *
     * @param OutResult Container to populate with validation entries
     * @return True if no error-severity entries were produced
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Dynamic Text Asset")
    bool ValidateDynamicTextAsset(FSGDynamicTextAssetValidationResult& OutResult) const;
    virtual bool ValidateDynamicTextAsset_Implementation(FSGDynamicTextAssetValidationResult& OutResult) const { return true; }

    /**
     * - Optional -
     * Per-DTA override for the asset bundle extender.
     *
     * - Valid: This extender is used instead of the settings-level mapping.
     * - Invalid: The system falls back to settings configuration.
     */
    UPROPERTY(EditAnywhere, Category = "SGDTA|File Information", meta = (SGDTAClassType = "SGDTAAssetBundleExtender"))
    FSGDTAClassId AssetBundleExtenderOverride;

private:

    /** Unique identifier that never changes after creation */
    UPROPERTY(VisibleAnywhere, Category = "SGDTA|Identity", meta = (AllowPrivateAccess = "true"))
    FSGDynamicTextAssetId DynamicTextAssetId;

    /** Human-readable identifier, can be renamed */
    UPROPERTY(VisibleAnywhere, Category = "SGDTA|Identity", meta = (AllowPrivateAccess = "true"))
    FString UserFacingId;

    /** Semantic version of this dynamic text asset instance */
    UPROPERTY(VisibleAnywhere, Category = "SGDTA|Identity", meta = (AllowPrivateAccess = "true"))
    FSGDynamicTextAssetVersion Version;

    /** Asset bundle data extracted from soft reference properties with AssetBundles metadata. */
    UPROPERTY(Transient)
    FSGDynamicTextAssetBundleData SGDTAssetBundleData;
};
