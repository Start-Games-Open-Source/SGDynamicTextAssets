// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetSerializerBase.h"

#include "Core/SGDynamicTextAsset.h"
#include "JsonObjectConverter.h"

TSharedPtr<FJsonValue> FSGDynamicTextAssetSerializerBase::SerializePropertyToValue(
    FProperty* Property,
    const void* ValuePtr) const
{
    return FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr, 0, 0);
}

bool FSGDynamicTextAssetSerializerBase::DeserializeValueToProperty(
    const TSharedPtr<FJsonValue>& Value,
    FProperty* Property,
    void* ValuePtr) const
{
    return FJsonObjectConverter::JsonValueToUProperty(Value, Property, ValuePtr, 0, 0);
}

bool FSGDynamicTextAssetSerializerBase::ShouldSerializeProperty(const FProperty* Property) const
{
    // Exclude base metadata fields handled separately as wrapper level fields, not in data block.
    // USGDynamicTextAsset::GetMetadataPropertyNames() uses GET_MEMBER_NAME_CHECKED internally so renaming
    // those properties becomes a compile error rather than a silent runtime mismatch.
    static const TSet<FName> metadataPropertyNames = USGDynamicTextAsset::GetMetadataPropertyNames();

    if (metadataPropertyNames.Contains(Property->GetFName()))
    {
        return false;
    }

    // Exclude deprecated properties from both serialization and deserialization.
    // If an existing file contains a value for a deprecated property, it is silently ignored.
    if (Property->HasAnyPropertyFlags(CPF_Deprecated))
    {
        return false;
    }

    return true;
}
