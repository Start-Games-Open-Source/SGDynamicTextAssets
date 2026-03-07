// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Metadata extracted from a dynamic text asset serializer for simple quick reference.
 */
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetSerializerMetadata
{
public:
	FSGDynamicTextAssetSerializerMetadata() = default;

	/** Returns true if this metadata has a valid serializer type ID. False if otherwise. */
	bool IsValidId() const;

	uint32 SerializerTypeId;
	FString FileExtension;
	FText FormatName;
};