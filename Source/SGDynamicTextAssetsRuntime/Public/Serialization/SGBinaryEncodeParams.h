// Copyright Start Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Settings/SGDynamicTextAssetSettings.h"

#include "SGBinaryEncodeParams.generated.h"


/**
 * Parameters for binary encoding via FSGDynamicTextAssetBinarySerializer::StringToBinary.
 * Groups metadata-related inputs into a single struct for cleaner signatures
 * and easier future extension.
 */
USTRUCT()
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGBinaryEncodeParams
{
	GENERATED_BODY()
public:

	FSGBinaryEncodeParams() = default;

	/** The unique asset instance identity. */
	UPROPERTY()
	FSGDynamicTextAssetId Id;

	/** Integer ID of the serializer that produced the payload (1–99 built-in, 100+ third-party). */
	UPROPERTY()
	uint32 SerializerTypeId = 0;

	/** Asset type (class) identity GUID. */
	UPROPERTY()
	FSGDynamicTextAssetTypeId AssetTypeId;

	/** Compression algorithm to use. */
	UPROPERTY()
	ESGDynamicTextAssetCompressionMethod CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;
};
