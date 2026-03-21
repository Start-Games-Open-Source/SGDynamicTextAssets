// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetVersion.h"

/**
 * File information extracted from a dynamic text asset file header.
 * Populated by ISGDynamicTextAssetSerializer::ExtractFileInfo() or
 * FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile() for lightweight
 * file inspection without full deserialization.
 */
struct FSGDynamicTextAssetFileInfo
{
public:
    FSGDynamicTextAssetFileInfo() = default;

    /** True when extraction succeeded and at least one type identifier (AssetTypeId or ClassName) is present. */
    bool bIsValid = false;

    /** Unique GUID for this asset instance. Immutable once assigned at creation. */
    FSGDynamicTextAssetId Id;

    /**
     * GUID identifying the asset's class type (e.g., UWeaponData).
     * Persists across class renames unlike ClassName.
     */
    FSGDynamicTextAssetTypeId AssetTypeId;

    /** Human-readable identifier for the asset. Can be renamed in the editor, unlike Id. */
    FString UserFacingId;

    /**
     * UClass name string (e.g., "UWeaponData").
     * Used as a fallback when AssetTypeId cannot be resolved through the registry.
     */
    FString ClassName;

    /** Integer ID of the serializer format that produced this file (e.g., 1=JSON, 2=XML, 3=YAML). */
    uint32 SerializerTypeId;

    /** Semantic version of the asset data, controlled by the asset author. */
    FSGDynamicTextAssetVersion Version;

    /**
     * Structural format version of the serializer that wrote this file.
     * Tracks changes to the serialization layout (renamed keys, new blocks).
     * Files missing this field default to version 1.0.0.
     */
    FSGDynamicTextAssetVersion FileFormatVersion;
};
