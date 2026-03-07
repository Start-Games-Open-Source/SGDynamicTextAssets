// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/Object.h"
#include "SGDynamicTextAssetId.h"
#include "SGDynamicTextAssetVersion.h"
#include "SGDynamicTextAssetValidationResult.h"
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
 * - Soft references only (no hard references or asset bundles)
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
    friend class FSGDynamicTextAssetIdentityCustomization;
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
    // ~ISGDynamicTextAssetProvider interface

    /** Returns the version as a formatted string */
    UFUNCTION(BlueprintPure, Category = "Dynamic Text Asset")
    FString GetVersionString() const;

    /**
     * Returns the FNames of the base metadata UPROPERTY fields: DynamicTextAssetId, UserFacingId, Version.
     * Used by serializers to exclude these fields from the data block during property iteration.
     *
     * Implemented via GET_MEMBER_NAME_CHECKED in SGDynamicTextAsset.cpp so that renaming any
     * of these properties becomes a compile error rather than a silent runtime mismatch.
     */
    static TSet<FName> GetMetadataPropertyNames();

protected:

    /**
     * Handles validating soft paths (soft objects and soft classes) properties on this dynamic text asset
     * with the goal of confirming if they are pointing to a real asset and the path isn't invalid.
     */
    void ValidateSoftPathsInProperty(const FProperty* Property,
        const void* ContainerPtr,
        const FString& PropertyPath,
        FSGDynamicTextAssetValidationResult& OutResult) const;

    /**
     * Called after this dynamic text asset's properties have been populated from JSON.
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

private:

    /** Unique identifier that never changes after creation */
    UPROPERTY(VisibleAnywhere, Category = "Dynamic Text Asset|Identity", meta = (AllowPrivateAccess = "true"))
    FSGDynamicTextAssetId DynamicTextAssetId;

    /** Human-readable identifier, can be renamed */
    UPROPERTY(VisibleAnywhere, Category = "Dynamic Text Asset|Identity", meta = (AllowPrivateAccess = "true"))
    FString UserFacingId;

    /** Semantic version of this dynamic text asset instance */
    UPROPERTY(VisibleAnywhere, Category = "Dynamic Text Asset|Identity", meta = (AllowPrivateAccess = "true"))
    FSGDynamicTextAssetVersion Version;
};
