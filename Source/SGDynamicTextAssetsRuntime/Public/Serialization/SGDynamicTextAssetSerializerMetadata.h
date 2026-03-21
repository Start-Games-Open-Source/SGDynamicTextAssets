// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Quick-reference snapshot of a registered serializer's identity.
 * Populated from ISGDynamicTextAssetSerializer accessors for use in
 * UI display, file dispatch, and manifest generation.
 */
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetSerializerMetadata
{
public:
	FSGDynamicTextAssetSerializerMetadata() = default;

	/** Returns true if SerializerTypeId is not INVALID_SERIALIZER_TYPE_ID (0). */
	bool IsValidId() const;

	/**
	 * Globally unique integer ID for the serializer format.
	 * Built-in range [1, 99] (1=JSON, 2=XML, 3=YAML). Third-party serializers use 100+.
	 */
	uint32 SerializerTypeId;

	/** Full file extension handled by this serializer (e.g., ".dta.json", ".dta.xml"). */
	FString FileExtension;

	/** Human-readable display name for the format (e.g., "JSON", "XML", "YAML"). */
	FText FormatName;
};