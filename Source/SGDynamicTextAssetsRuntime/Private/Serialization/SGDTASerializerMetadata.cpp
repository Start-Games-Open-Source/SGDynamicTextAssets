// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDTASerializerMetadata.h"

bool FSGDynamicTextAssetSerializerMetadata::IsValidId() const
{
	return SerializerFormat.IsValid();
}
