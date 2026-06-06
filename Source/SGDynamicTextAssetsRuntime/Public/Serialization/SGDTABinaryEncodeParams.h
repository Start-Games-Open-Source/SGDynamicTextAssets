// Copyright Start Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDTASerializerFormat.h"
#include "Settings/SGDTASettings.h"

#include "SGDTABinaryEncodeParams.generated.h"


/**
 * Parameters for binary encoding via FSGDynamicTextAssetBinarySerializer::StringToBinary.
 * Groups metadata-related inputs into a single struct for cleaner signatures
 * and easier future extension.
 */
USTRUCT()
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDTABinaryEncodeParams
{
	GENERATED_BODY()
public:

	FSGDTABinaryEncodeParams() = default;

	/** The unique asset instance identity. */
	UPROPERTY()
	FSGDynamicTextAssetId Id;

	/** Format of the serializer that produced the payload. */
	UPROPERTY()
	FSGDTASerializerFormat SerializerFormat;

	/** Asset type (class) identity GUID. */
	UPROPERTY()
	FSGDynamicTextAssetTypeId AssetTypeId;

	/** Compression algorithm to use. */
	UPROPERTY()
	ESGDynamicTextAssetCompressionMethod CompressionMethod = ESGDynamicTextAssetCompressionMethod::Zlib;
};
