// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetTypeId.h"

/**
 * Metadata extracted from a dynamic text asset file header.
 */
struct FSGDynamicTextAssetFileMetadata
{
public:
    FSGDynamicTextAssetFileMetadata() = default;

    bool bIsValid = false;
    FSGDynamicTextAssetId Id;
    FSGDynamicTextAssetTypeId AssetTypeId;
    FString UserFacingId;
    FString ClassName;
    uint32 SerializerTypeId;
};
