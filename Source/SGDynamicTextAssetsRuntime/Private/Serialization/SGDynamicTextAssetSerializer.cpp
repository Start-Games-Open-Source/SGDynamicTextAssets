// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetSerializer.h"

#if WITH_EDITOR
#include "Styling/AppStyle.h"
#endif

const FString ISGDynamicTextAssetSerializer::KEY_METADATA = TEXT("metadata");
const FString ISGDynamicTextAssetSerializer::KEY_TYPE = TEXT("type");
const FString ISGDynamicTextAssetSerializer::KEY_VERSION = TEXT("version");
const FString ISGDynamicTextAssetSerializer::KEY_ID = TEXT("id");
const FString ISGDynamicTextAssetSerializer::KEY_USER_FACING_ID = TEXT("userfacingid");
const FString ISGDynamicTextAssetSerializer::KEY_FILE_FORMAT_VERSION = TEXT("fileFormatVersion");
const FString ISGDynamicTextAssetSerializer::KEY_DATA = TEXT("data");
const FString ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES = TEXT("sgdtAssetBundles");

#if WITH_EDITOR
const FSlateBrush* ISGDynamicTextAssetSerializer::GetIconBrush() const
{
	// Using generic object as default to display something
	static const FSlateBrush* icon = FAppStyle::GetBrush("ClassIcon.Object");
	return icon;
}
#endif

bool ISGDynamicTextAssetSerializer::ExtractMetadata(const FString& InString, FSGDynamicTextAssetId& OutId,
	FString& OutClassName, FString& OutUserFacingId, FString& OutVersion,
	FSGDynamicTextAssetTypeId& OutAssetTypeId) const
{
	FSGDynamicTextAssetFileMetadata metadata;
	if (!ExtractMetadata(InString, metadata))
	{
		OutAssetTypeId.Invalidate();
		OutClassName.Empty();
		return false;
	}

	OutId = metadata.Id;
	OutClassName = metadata.ClassName;
	OutUserFacingId = metadata.UserFacingId;
	OutVersion = metadata.Version.ToString();
	OutAssetTypeId = metadata.AssetTypeId;
	return true;
}

bool ISGDynamicTextAssetSerializer::ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData) const
{
	// Default implementation does not extract bundles.
	// Format-specific serializers override this to parse their bundle block.
	return false;
}

bool ISGDynamicTextAssetSerializer::MigrateFileFormat(FString& InOutFileContents,
	const FSGDynamicTextAssetVersion& CurrentFormatVersion,
	const FSGDynamicTextAssetVersion& TargetFormatVersion) const
{
	// No migration needed if versions already match
	if (CurrentFormatVersion == TargetFormatVersion)
	{
		return true;
	}

	// Default: no structural changes needed.
	// Serializers override this when format structure changes between versions.
	return true;
}

bool ISGDynamicTextAssetSerializer::UpdateFileFormatVersion(FString& InOutFileContents,
	const FSGDynamicTextAssetVersion& NewVersion) const
{
	// Base class cannot update version in an unknown format.
	// Each serializer must override this for its specific file syntax.
	return false;
}

FString ISGDynamicTextAssetSerializer::GetFormatName_String() const
{
	return GetFormatName().ToString();
}
