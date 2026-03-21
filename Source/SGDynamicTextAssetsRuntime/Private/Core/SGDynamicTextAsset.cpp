// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAsset.h"

USGDynamicTextAsset::USGDynamicTextAsset()
{
    // DynamicTextAssetId will be set by the subsystem when loading
    // UserFacingId will be populated from JSON
    // Version will be populated from JSON
}

const FSGDynamicTextAssetId& USGDynamicTextAsset::GetDynamicTextAssetId() const
{
    return DynamicTextAssetId;
}

void USGDynamicTextAsset::BP_PostDynamicTextAssetLoaded_Implementation()
{
    // Default implementation does nothing
    // Subclasses can override for custom initialization
}

void USGDynamicTextAsset::SetDynamicTextAssetId(const FSGDynamicTextAssetId& InId)
{
    DynamicTextAssetId = InId;
#if WITH_EDITOR
    OnDynamicTextAssetIdChanged.Broadcast(this);
#endif
}

const FString& USGDynamicTextAsset::GetUserFacingId() const
{
    return UserFacingId;
}

void USGDynamicTextAsset::SetUserFacingId(const FString& InUserFacingId)
{
    UserFacingId = InUserFacingId;
#if WITH_EDITOR
    OnUserFacingIdChanged.Broadcast(this);
#endif
}

const FSGDynamicTextAssetVersion& USGDynamicTextAsset::GetVersion() const
{
    return Version;
}

int32 USGDynamicTextAsset::GetCurrentMajorVersion() const
{
    // Using default value
    return 1;
}

void USGDynamicTextAsset::SetVersion(const FSGDynamicTextAssetVersion& InVersion)
{
    Version = InVersion;
#if WITH_EDITOR
    OnVersionChanged.Broadcast(this);
#endif
}

void USGDynamicTextAsset::PostDynamicTextAssetLoaded()
{
    BP_PostDynamicTextAssetLoaded();
    SGDTAssetBundleData.ExtractFromObject(this);
}

const FSGDynamicTextAssetBundleData& USGDynamicTextAsset::GetSGDTAssetBundleData() const
{
    return SGDTAssetBundleData;
}

FSGDynamicTextAssetBundleData& USGDynamicTextAsset::GetMutableSGDTAssetBundleData()
{
    return SGDTAssetBundleData;
}

bool USGDynamicTextAsset::HasSGDTAssetBundles() const
{
    return SGDTAssetBundleData.HasBundles();
}

bool USGDynamicTextAsset::Native_ValidateDynamicTextAsset(FSGDynamicTextAssetValidationResult& OutResult) const
{
    if (!ISGDynamicTextAssetProvider::Native_ValidateDynamicTextAsset(OutResult))
    {
        return false;
    }

    ValidateDynamicTextAsset(OutResult);

    return OutResult.IsValid();
}

bool USGDynamicTextAsset::MigrateFromVersion(
    const FSGDynamicTextAssetVersion& OldVersion,
    const FSGDynamicTextAssetVersion& CurrentVersion,
    const TSharedPtr<FJsonObject>& OldData)
{
    // Default implementation: no migration needed
    // Subclasses should override to handle breaking changes between versions
    return true;
}

FString USGDynamicTextAsset::GetVersionString() const
{
    return Version.ToString();
}

TSet<FName> USGDynamicTextAsset::GetMetadataPropertyNames()
{
    // GET_MEMBER_NAME_CHECKED is called here (inside a member function body) so that it has
    // access to the private members DynamicTextAssetId, UserFacingId, and Version.
    // If any of these properties are renamed, this function will produce a compile error.
    return {
        GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, DynamicTextAssetId),
        GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, UserFacingId),
        GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, Version)
    };
}