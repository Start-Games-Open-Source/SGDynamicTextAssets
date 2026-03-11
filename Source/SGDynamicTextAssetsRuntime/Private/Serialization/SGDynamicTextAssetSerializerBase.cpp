// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetSerializerBase.h"

#include "SGDynamicTextAssetLogs.h"
#include "Core/SGDynamicTextAsset.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"

const FString FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY = TEXT("SG_INST_OBJ_CLASS");

bool FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(const FProperty* InProperty)
{
    return InProperty && InProperty->HasAnyPropertyFlags(CPF_InstancedReference);
}

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

    // Exclude deprecated and transient properties from both serialization and deserialization.
    // If an existing file contains a value for a deprecated property, it is silently ignored.
    // Transient will be cleared because thats intentional so we dont need to serialize it out
    if (Property->HasAnyPropertyFlags(CPF_Deprecated|CPF_Transient))
    {
        return false;
    }

    return true;
}

TSharedPtr<FJsonValue> FSGDynamicTextAssetSerializerBase::SerializeInstancedObjectToValue(
    const FObjectProperty* Property,
    const UObject* SubObject) const
{
    if (!SubObject)
    {
        return MakeShared<FJsonValueNull>();
    }

    TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();

    // Store the full class path so deserialization can reconstruct the correct type.
    // GetPathName returns "/Script/Module.ClassName" for C++ classes and
    // "/Game/Path/BP_Name.BP_Name_C" for Blueprint classes.
    jsonObject->SetStringField(INSTANCED_OBJECT_CLASS_KEY, SubObject->GetClass()->GetPathName());

    // Serialize each UPROPERTY on the sub-object
    for (TFieldIterator<FProperty> propIt(SubObject->GetClass()); propIt; ++propIt)
    {
        FProperty* subProperty = *propIt;

        if (!ShouldSerializeProperty(subProperty))
        {
            continue;
        }

        const void* valuePtr = subProperty->ContainerPtrToValuePtr<void>(SubObject);

        // Recurse for nested instanced objects
        if (IsInstancedObjectProperty(subProperty))
        {
            if (const FObjectProperty* nestedObjProp = CastField<FObjectProperty>(subProperty))
            {
                const UObject* nestedObject = nestedObjProp->GetObjectPropertyValue(valuePtr);
                TSharedPtr<FJsonValue> nestedValue = SerializeInstancedObjectToValue(nestedObjProp, nestedObject);
                if (nestedValue.IsValid())
                {
                    jsonObject->SetField(subProperty->GetName(), nestedValue);
                }
                continue;
            }
        }

        TSharedPtr<FJsonValue> jsonValue = SerializePropertyToValue(subProperty, valuePtr);
        if (jsonValue.IsValid())
        {
            jsonObject->SetField(subProperty->GetName(), jsonValue);
        }
    }

    return MakeShared<FJsonValueObject>(jsonObject);
}

bool FSGDynamicTextAssetSerializerBase::DeserializeValueToInstancedObject(
    const TSharedPtr<FJsonValue>& Value,
    const FObjectProperty* Property,
    void* PropertyValuePtr,
    UObject* Outer) const
{
    if (!Property || !PropertyValuePtr || !Outer)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DeserializeValueToInstancedObject: Invalid parameters"));
        return false;
    }

    // Null JSON value means nullptr sub-object
    if (!Value.IsValid() || Value->IsNull())
    {
        Property->SetObjectPropertyValue(PropertyValuePtr, nullptr);
        return true;
    }

    // Must be a JSON object
    const TSharedPtr<FJsonObject>* jsonObjectPtr = nullptr;
    if (!Value->TryGetObject(jsonObjectPtr) || !jsonObjectPtr || !jsonObjectPtr->IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DeserializeValueToInstancedObject: Expected JSON object for instanced property '%s'"),
            *Property->GetName());
        return false;
    }

    const TSharedPtr<FJsonObject>& jsonObject = *jsonObjectPtr;

    // Read the class name from the reserved key
    FString className;
    if (!jsonObject->TryGetStringField(INSTANCED_OBJECT_CLASS_KEY, className) || className.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DeserializeValueToInstancedObject: Missing '%s' field in instanced object JSON for property '%s'"),
            *INSTANCED_OBJECT_CLASS_KEY, *Property->GetName());
        return false;
    }

    // Resolve class from stored path
    UClass* resolvedClass = ResolveInstancedObjectClass(className);
    if (!resolvedClass)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DeserializeValueToInstancedObject: Could not find class '%s' for instanced property '%s'"),
            *className, *Property->GetName());
        return false;
    }

    // Verify it is a subclass of the property's expected type
    if (!resolvedClass->IsChildOf(Property->PropertyClass))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DeserializeValueToInstancedObject: Class '%s' is not a subclass of '%s' for instanced property '%s'"),
            *className, *Property->PropertyClass->GetName(), *Property->GetName());
        return false;
    }

    // Create the sub-object
    UObject* newSubObject = NewObject<UObject>(Outer, resolvedClass);
    if (!newSubObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
            TEXT("DeserializeValueToInstancedObject: Failed to create sub-object of class '%s' for property '%s'"),
            *className, *Property->GetName());
        return false;
    }

    // Deserialize each property on the sub-object
    for (TFieldIterator<FProperty> propIt(resolvedClass); propIt; ++propIt)
    {
        FProperty* subProperty = *propIt;

        if (!ShouldSerializeProperty(subProperty))
        {
            continue;
        }

        if (!jsonObject->HasField(subProperty->GetName()))
        {
            continue;
        }

        void* valuePtr = subProperty->ContainerPtrToValuePtr<void>(newSubObject);
        TSharedPtr<FJsonValue> fieldValue = jsonObject->TryGetField(subProperty->GetName());

        if (!fieldValue.IsValid())
        {
            continue;
        }

        // Recurse for nested instanced objects
        if (IsInstancedObjectProperty(subProperty))
        {
            const FObjectProperty* nestedObjProp = CastField<FObjectProperty>(subProperty);
            if (nestedObjProp)
            {
                if (!DeserializeValueToInstancedObject(fieldValue, nestedObjProp, valuePtr, newSubObject))
                {
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                        TEXT("DeserializeValueToInstancedObject: Failed to deserialize nested instanced property '%s' on class '%s'"),
                        *subProperty->GetName(), *className);
                }
                continue;
            }
        }

        if (!DeserializeValueToProperty(fieldValue, subProperty, valuePtr))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("DeserializeValueToInstancedObject: Failed to deserialize property '%s' on instanced class '%s'"),
                *subProperty->GetName(), *className);
        }
    }

    // Set the property to point to the new sub-object
    Property->SetObjectPropertyValue(PropertyValuePtr, newSubObject);

    return true;
}

UClass* FSGDynamicTextAssetSerializerBase::ResolveInstancedObjectClass(const FString& ClassIdentifier)
{
    if (ClassIdentifier.IsEmpty())
    {
        return nullptr;
    }

    if (UClass* resolvedClass = LoadObject<UClass>(nullptr, *ClassIdentifier))
    {
        return resolvedClass;
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
        TEXT("ResolveInstancedObjectClass: LoadObject failed for '%s'"),
        *ClassIdentifier);
    return nullptr;
}

namespace SGDynamicTextAssetSerializerBasePrivate
{

static void CollectFromObject(const UObject* Object, TSet<TObjectPtr<UClass>>& OutClasses)
{
    if (!Object)
    {
        return;
    }

    for (TFieldIterator<FProperty> propIt(Object->GetClass()); propIt; ++propIt)
    {
        FProperty* property = *propIt;
        const void* valuePtr = property->ContainerPtrToValuePtr<void>(Object);

        // Single instanced object
        if (FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(property))
        {
            if (const FObjectProperty* objectProp = CastField<FObjectProperty>(property))
            {
                if (const UObject* subObject = objectProp->GetObjectPropertyValue(valuePtr))
                {
                    OutClasses.Add(subObject->GetClass());
                    CollectFromObject(subObject, OutClasses);
                }
                continue;
            }
        }

        // TArray of instanced objects
        if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(property))
        {
            if (arrayProp->Inner && FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(arrayProp->Inner))
            {
                if (const FObjectProperty* innerObjProp = CastField<FObjectProperty>(arrayProp->Inner))
                {
                    FScriptArrayHelper arrayHelper(arrayProp, valuePtr);
                    for (int32 i = 0; i < arrayHelper.Num(); ++i)
                    {
                        if (const UObject* element = innerObjProp->GetObjectPropertyValue(arrayHelper.GetRawPtr(i)))
                        {
                            OutClasses.Add(element->GetClass());
                            CollectFromObject(element, OutClasses);
                        }
                    }
                    continue;
                }
            }
        }

        // TSet of instanced objects
        if (const FSetProperty* setProp = CastField<FSetProperty>(property))
        {
            if (setProp->ElementProp && FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(setProp->ElementProp))
            {
                if (const FObjectProperty* elemObjProp = CastField<FObjectProperty>(setProp->ElementProp))
                {
                    FScriptSetHelper setHelper(setProp, valuePtr);
                    for (int32 i = 0; i < setHelper.GetMaxIndex(); ++i)
                    {
                        if (setHelper.IsValidIndex(i))
                        {
                            if (const UObject* element = elemObjProp->GetObjectPropertyValue(setHelper.GetElementPtr(i)))
                            {
                                OutClasses.Add(element->GetClass());
                                CollectFromObject(element, OutClasses);
                            }
                        }
                    }
                    continue;
                }
            }
        }

        // TMap with instanced object values
        if (const FMapProperty* mapProp = CastField<FMapProperty>(property))
        {
            if (mapProp->ValueProp && FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(mapProp->ValueProp))
            {
                if (const FObjectProperty* valueObjProp = CastField<FObjectProperty>(mapProp->ValueProp))
                {
                    FScriptMapHelper mapHelper(mapProp, valuePtr);
                    for (int32 i = 0; i < mapHelper.GetMaxIndex(); ++i)
                    {
                        if (mapHelper.IsValidIndex(i))
                        {
                            if (const UObject* element = valueObjProp->GetObjectPropertyValue(mapHelper.GetValuePtr(i)))
                            {
                                OutClasses.Add(element->GetClass());
                                CollectFromObject(element, OutClasses);
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace SGDynamicTextAssetSerializerBasePrivate

void FSGDynamicTextAssetSerializerBase::CollectInstancedObjectClasses(
    const UObject* Object,
    TSet<TObjectPtr<UClass>>& OutClasses)
{
    SGDynamicTextAssetSerializerBasePrivate::CollectFromObject(Object, OutClasses);
}
