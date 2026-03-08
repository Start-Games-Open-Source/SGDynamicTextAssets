// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"

/**
 * Abstract base class for serializers that use FJsonValue as an intermediate
 * representation for UE property conversion (the "JSON intermediate" approach).
 *
 * Provides three protected virtual helpers used by SerializeProvider and
 * DeserializeProvider implementations:
 * - SerializePropertyToValue: FProperty → FJsonValue (default: FJsonObjectConverter)
 * - DeserializeValueToProperty: FJsonValue → FProperty (default: FJsonObjectConverter)
 * - ShouldSerializeProperty: Filtering predicate (default: skip metadata + deprecated)
 *
 * All ISGDynamicTextAssetSerializer pure virtuals remain unimplemented here.
 * Concrete serializers (FSGDynamicTextAssetJsonSerializer, FSGDynamicTextAssetXmlSerializer, etc.)
 * inherit from this class and implement the format-specific methods.
 *
 * To bypass the JSON intermediate and convert properties directly to the target format,
 * override SerializePropertyToValue and DeserializeValueToProperty.
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetSerializerBase : public ISGDynamicTextAssetSerializer
{
public:

    virtual ~FSGDynamicTextAssetSerializerBase() = default;

protected:

    /**
     * Converts a single UPROPERTY value to its FJsonValue intermediate form.
     *
     * Default implementation calls FJsonObjectConverter::UPropertyToJsonValue.
     * Override to provide a custom conversion strategy (e.g., a direct XML or YAML node).
     *
     * @param Property The property descriptor (non-const — required by FJsonObjectConverter)
     * @param ValuePtr Const pointer to the property's value inside the containing object
     * @return The FJsonValue intermediate, or nullptr if conversion fails
     */
    virtual TSharedPtr<FJsonValue> SerializePropertyToValue(FProperty* Property, const void* ValuePtr) const;

    /**
     * Converts a FJsonValue intermediate form back into a UPROPERTY value.
     *
     * Default implementation calls FJsonObjectConverter::JsonValueToUProperty.
     * Override to provide a custom conversion strategy.
     *
     * @param Value The FJsonValue to convert (must be valid — callers must check IsValid() first)
     * @param Property The property descriptor
     * @param ValuePtr Non-const pointer to the property's value storage in the containing object
     * @return True if conversion succeeded
     */
    virtual bool DeserializeValueToProperty(const TSharedPtr<FJsonValue>& Value, FProperty* Property, void* ValuePtr) const;

    /**
     * Returns true if the property should be included in serialization and deserialization.
     *
     * Default implementation returns false (excludes) for:
     * - USGDynamicTextAsset base metadata properties: DynamicTextAssetId, Version, UserFacingId.
     * These are stored as wrapper-level fields, not inside the data block.
     * - Properties flagged as CPF_Deprecated.
     * Deprecated properties are excluded from BOTH serialization and deserialization.
     * If an existing file contains a value for a now-deprecated property, that value
     * is silently ignored when loading. This is intentional.
     *
     * Override to add format-specific or class-specific filtering rules.
     *
     * @param Property The property to evaluate
     * @return True if the property should be serialized/deserialized
     */
    virtual bool ShouldSerializeProperty(const FProperty* Property) const;
};
