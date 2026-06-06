// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDTASerializerFormat.h"

/**
 * Quick-reference snapshot of a registered serializer's identity.
 * Populated from ISGDynamicTextAssetSerializer accessors for use in
 * UI display, file dispatch, and manifest generation.
 */
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetSerializerMetadata
{
public:
	FSGDynamicTextAssetSerializerMetadata() = default;

	/** Returns true if the serializer format is valid (non-zero TypeId). */
	bool IsValidId() const;

	/**
	 * The serializer format for this metadata entry.
	 * Built-in range [1, 99] (1=JSON, 2=XML, 3=YAML). Third-party serializers use 100+.
	 */
	FSGDTASerializerFormat SerializerFormat;

	/** Full file extension handled by this serializer (e.g., ".dta.json", ".dta.xml"). */
	FString FileExtension;

	/** Human-readable display name for the format (e.g., "JSON", "XML", "YAML"). */
	FText FormatName;
};