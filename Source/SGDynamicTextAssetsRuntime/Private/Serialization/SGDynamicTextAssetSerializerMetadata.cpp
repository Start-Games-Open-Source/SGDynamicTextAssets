// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetSerializerMetadata.h"

#include "Serialization/SGDynamicTextAssetSerializer.h"

bool FSGDynamicTextAssetSerializerMetadata::IsValidId() const
{
	return (SerializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID);
}
