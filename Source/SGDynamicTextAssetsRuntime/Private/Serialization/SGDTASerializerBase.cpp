// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDTASerializerBase.h"

#include "Core/ISGDynamicTextAssetProvider.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Serialization/AssetBundleExtenders/SGDTAAssetBundleExtender.h"
#include "Settings/SGDTASettings.h"
#include "SGDynamicTextAssetLogs.h"
#include "Statics/SGDynamicTextAssetStatics.h"
#include "Core/SGDynamicTextAsset.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"

const FString FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY = TEXT("SG_INST_OBJ_CLASS");
const FString FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY = TEXT("SG_STRUCT_TYPE");
const FString FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY = TEXT("SG_INST_STRUCT_TYPE");

bool FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(const FProperty* InProperty)
{
    return InProperty && InProperty->HasAnyPropertyFlags(CPF_InstancedReference);
}

namespace SGDTASerializerPrivate
{

// Inject SG_STRUCT_TYPE or SG_INST_STRUCT_TYPE into a JSON object based on the struct property type.
static void InjectStructMarkerIntoObject(
    const TSharedPtr<FJsonObject>& JsonObject,
    const FStructProperty* StructProp,
    const void* StructValuePtr)
{
    if (!JsonObject.IsValid() || !StructProp)
    {
        return;
    }

    if (StructProp->Struct == FInstancedStruct::StaticStruct())
    {
        const FInstancedStruct* instStruct = static_cast<const FInstancedStruct*>(StructValuePtr);
        if (instStruct && instStruct->IsValid() && instStruct->GetScriptStruct())
        {
            JsonObject->SetStringField(
                FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY,
                instStruct->GetScriptStruct()->GetFName().ToString());
        }
    }
    else
    {
        JsonObject->SetStringField(
            FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY,
            StructProp->Struct->GetFName().ToString());
    }
}

// Post-process a serialized JSON value to inject struct type markers based on the property descriptor.
static void InjectStructTypeMarkers(
    const TSharedPtr<FJsonValue>& JsonValue,
    FProperty* Property,
    const void* ValuePtr)
{
    if (!JsonValue.IsValid() || !Property || !ValuePtr)
    {
        return;
    }

    // Single struct property
    if (const FStructProperty* structProp = CastField<FStructProperty>(Property))
    {
        const TSharedPtr<FJsonObject>* objPtr = nullptr;
        if (JsonValue->TryGetObject(objPtr) && objPtr && objPtr->IsValid())
        {
            InjectStructMarkerIntoObject(*objPtr, structProp, ValuePtr);
        }
        return;
    }

    // TArray with struct inner type
    if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(Property))
    {
        const FStructProperty* innerStructProp = CastField<FStructProperty>(arrayProp->Inner);
        if (!innerStructProp)
        {
            return;
        }

        const TArray<TSharedPtr<FJsonValue>>* jsonArray = nullptr;
        if (!JsonValue->TryGetArray(jsonArray) || !jsonArray)
        {
            return;
        }

        FScriptArrayHelper arrayHelper(arrayProp, ValuePtr);
        const int32 count = FMath::Min(jsonArray->Num(), arrayHelper.Num());
        for (int32 i = 0; i < count; ++i)
        {
            const TSharedPtr<FJsonValue>& elementValue = (*jsonArray)[i];
            const TSharedPtr<FJsonObject>* elemObjPtr = nullptr;
            if (elementValue.IsValid() && elementValue->TryGetObject(elemObjPtr) && elemObjPtr && elemObjPtr->IsValid())
            {
                InjectStructMarkerIntoObject(*elemObjPtr, innerStructProp, arrayHelper.GetRawPtr(i));
            }
        }
        return;
    }

    // TMap with struct value type
    if (const FMapProperty* mapProp = CastField<FMapProperty>(Property))
    {
        const FStructProperty* valueStructProp = CastField<FStructProperty>(mapProp->ValueProp);
        if (!valueStructProp)
        {
            return;
        }

        // FJsonObjectConverter serializes TMap as a JSON object with string keys.
        // Each value may be a struct that needs a type marker.
        const TSharedPtr<FJsonObject>* mapObjPtr = nullptr;
        if (!JsonValue->TryGetObject(mapObjPtr) || !mapObjPtr || !mapObjPtr->IsValid())
        {
            return;
        }

        FScriptMapHelper mapHelper(mapProp, ValuePtr);
        for (FScriptMapHelper::FIterator itr = mapHelper.CreateIterator(); itr; ++itr)
        {
            // Build the key string the same way FJsonObjectConverter does
            FString keyString;
            mapProp->KeyProp->ExportTextItem_Direct(keyString, mapHelper.GetKeyPtr(itr.GetInternalIndex()),
                nullptr, nullptr, PPF_None);

            TSharedPtr<FJsonValue> mapEntryValue = (*mapObjPtr)->TryGetField(keyString);
            const TSharedPtr<FJsonObject>* entryObjPtr = nullptr;
            if (mapEntryValue.IsValid() && mapEntryValue->TryGetObject(entryObjPtr) && entryObjPtr && entryObjPtr->IsValid())
            {
                InjectStructMarkerIntoObject(*entryObjPtr, valueStructProp, mapHelper.GetValuePtr(itr.GetInternalIndex()));
            }
        }
        return;
    }

    // TSet with struct element type
    if (const FSetProperty* setProp = CastField<FSetProperty>(Property))
    {
        const FStructProperty* elemStructProp = CastField<FStructProperty>(setProp->ElementProp);
        if (!elemStructProp)
        {
            return;
        }

        const TArray<TSharedPtr<FJsonValue>>* jsonArray = nullptr;
        if (!JsonValue->TryGetArray(jsonArray) || !jsonArray)
        {
            return;
        }

        FScriptSetHelper setHelper(setProp, ValuePtr);
        int32 jsonIndex = 0;
        for (FScriptSetHelper::FIterator itr = setHelper.CreateIterator(); itr && jsonIndex < jsonArray->Num(); ++itr, ++jsonIndex)
        {
            const TSharedPtr<FJsonValue>& elementValue = (*jsonArray)[jsonIndex];
            const TSharedPtr<FJsonObject>* elemObjPtr = nullptr;
            if (elementValue.IsValid() && elementValue->TryGetObject(elemObjPtr) && elemObjPtr && elemObjPtr->IsValid())
            {
                InjectStructMarkerIntoObject(*elemObjPtr, elemStructProp, setHelper.GetElementPtr(itr.GetInternalIndex()));
            }
        }
    }
}

// Strip SG_STRUCT_TYPE / SG_INST_STRUCT_TYPE from a single JSON object.
// Returns a new cleaned FJsonValueObject if stripping was needed, or the original value if not.
static TSharedPtr<FJsonValue> StripStructMarkerFromObject(const TSharedPtr<FJsonValue>& JsonValue)
{
    const TSharedPtr<FJsonObject>* objPtr = nullptr;
    if (!JsonValue.IsValid() || !JsonValue->TryGetObject(objPtr) || !objPtr || !objPtr->IsValid())
    {
        return JsonValue;
    }

    const TSharedPtr<FJsonObject>& obj = *objPtr;
    if (!obj->HasField(FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY) &&
        !obj->HasField(FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY))
    {
        return JsonValue;
    }

    TSharedPtr<FJsonObject> cleaned = MakeShared<FJsonObject>();
    for (const auto& pair : obj->Values)
    {
        if (pair.Key != FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY &&
            pair.Key != FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY)
        {
            cleaned->SetField(pair.Key, pair.Value);
        }
    }
    return MakeShared<FJsonValueObject>(cleaned);
}

// Strip struct type markers from a JSON value before passing to FJsonObjectConverter.
static TSharedPtr<FJsonValue> StripStructTypeMarkers(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property)
{
    if (!JsonValue.IsValid() || !Property)
    {
        return JsonValue;
    }

    // Single struct property
    if (CastField<FStructProperty>(Property))
    {
        return StripStructMarkerFromObject(JsonValue);
    }

    // TArray with struct inner type
    if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(Property))
    {
        if (!CastField<FStructProperty>(arrayProp->Inner))
        {
            return JsonValue;
        }

        const TArray<TSharedPtr<FJsonValue>>* jsonArray = nullptr;
        if (!JsonValue->TryGetArray(jsonArray) || !jsonArray)
        {
            return JsonValue;
        }

        // Check if any element needs stripping before allocating
        bool bNeedsStripping = false;
        for (const auto& elem : *jsonArray)
        {
            const TSharedPtr<FJsonObject>* elemObjPtr = nullptr;
            if (elem.IsValid() && elem->TryGetObject(elemObjPtr) && elemObjPtr && elemObjPtr->IsValid())
            {
                if ((*elemObjPtr)->HasField(FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY) ||
                    (*elemObjPtr)->HasField(FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY))
                {
                    bNeedsStripping = true;
                    break;
                }
            }
        }

        if (!bNeedsStripping)
        {
            return JsonValue;
        }

        TArray<TSharedPtr<FJsonValue>> cleanedArray;
        cleanedArray.Reserve(jsonArray->Num());
        for (const auto& elem : *jsonArray)
        {
            cleanedArray.Add(StripStructMarkerFromObject(elem));
        }
        return MakeShared<FJsonValueArray>(cleanedArray);
    }

    // TMap with struct value type
    if (const FMapProperty* mapProp = CastField<FMapProperty>(Property))
    {
        if (!CastField<FStructProperty>(mapProp->ValueProp))
        {
            return JsonValue;
        }

        const TSharedPtr<FJsonObject>* mapObjPtr = nullptr;
        if (!JsonValue->TryGetObject(mapObjPtr) || !mapObjPtr || !mapObjPtr->IsValid())
        {
            return JsonValue;
        }

        bool bNeedsStripping = false;
        for (const auto& pair : (*mapObjPtr)->Values)
        {
            const TSharedPtr<FJsonObject>* entryObjPtr = nullptr;
            if (pair.Value.IsValid() && pair.Value->TryGetObject(entryObjPtr) && entryObjPtr && entryObjPtr->IsValid())
            {
                if ((*entryObjPtr)->HasField(FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY) ||
                    (*entryObjPtr)->HasField(FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY))
                {
                    bNeedsStripping = true;
                    break;
                }
            }
        }

        if (!bNeedsStripping)
        {
            return JsonValue;
        }

        TSharedPtr<FJsonObject> cleanedMap = MakeShared<FJsonObject>();
        for (const auto& pair : (*mapObjPtr)->Values)
        {
            cleanedMap->SetField(pair.Key, StripStructMarkerFromObject(pair.Value));
        }
        return MakeShared<FJsonValueObject>(cleanedMap);
    }

    // TSet with struct element type
    if (const FSetProperty* setProp = CastField<FSetProperty>(Property))
    {
        if (!CastField<FStructProperty>(setProp->ElementProp))
        {
            return JsonValue;
        }

        const TArray<TSharedPtr<FJsonValue>>* jsonArray = nullptr;
        if (!JsonValue->TryGetArray(jsonArray) || !jsonArray)
        {
            return JsonValue;
        }

        bool bNeedsStripping = false;
        for (const auto& elem : *jsonArray)
        {
            const TSharedPtr<FJsonObject>* elemObjPtr = nullptr;
            if (elem.IsValid() && elem->TryGetObject(elemObjPtr) && elemObjPtr && elemObjPtr->IsValid())
            {
                if ((*elemObjPtr)->HasField(FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY) ||
                    (*elemObjPtr)->HasField(FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY))
                {
                    bNeedsStripping = true;
                    break;
                }
            }
        }

        if (!bNeedsStripping)
        {
            return JsonValue;
        }

        TArray<TSharedPtr<FJsonValue>> cleanedArray;
        cleanedArray.Reserve(jsonArray->Num());
        for (const auto& elem : *jsonArray)
        {
            cleanedArray.Add(StripStructMarkerFromObject(elem));
        }
        return MakeShared<FJsonValueArray>(cleanedArray);
    }

    return JsonValue;
}

} // namespace SGDTASerializerPrivate

TSharedPtr<FJsonValue> FSGDynamicTextAssetSerializerBase::SerializePropertyToValue(
    FProperty* Property,
    const void* ValuePtr) const
{
    TSharedPtr<FJsonValue> result = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr, 0, 0);

    SGDTASerializerPrivate::InjectStructTypeMarkers(result, Property, ValuePtr);

    return result;
}

bool FSGDynamicTextAssetSerializerBase::DeserializeValueToProperty(
    const TSharedPtr<FJsonValue>& Value,
    FProperty* Property,
    void* ValuePtr) const
{
    TSharedPtr<FJsonValue> cleanedValue = SGDTASerializerPrivate::StripStructTypeMarkers(Value, Property);
    return FJsonObjectConverter::JsonValueToUProperty(cleanedValue, Property, ValuePtr, 0, 0);
}

bool FSGDynamicTextAssetSerializerBase::ShouldSerializeProperty(const FProperty* Property) const
{
    // Exclude base metadata fields handled separately as wrapper level fields, not in data block.
    // USGDynamicTextAsset::GetFileInformationPropertyNames() uses GET_MEMBER_NAME_CHECKED internally so renaming
    // those properties becomes a compile error rather than a silent runtime mismatch.
    static const TSet<FName> metadataPropertyNames = USGDynamicTextAsset::GetFileInformationPropertyNames();

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

namespace SGDTASerializerBasePrivate
{

/** Resolves the asset bundle extender for a provider. Returns nullptr if not available. */
static USGDTAAssetBundleExtender* ResolveExtender(
    const TScriptInterface<ISGDynamicTextAssetProvider>& Provider,
    const FSGDTASerializerFormat& Format)
{
    const FSGDTAClassId extenderClassId =
        USGDynamicTextAssetStatics::ResolveAssetBundleExtender(Provider, Format);
    if (!extenderClassId.IsValid())
    {
        return nullptr;
    }

    USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
    if (!registry)
    {
        return nullptr;
    }

    return registry->GetOrCreateAssetBundleExtender(extenderClassId);
}

/** Builds a TScriptInterface from a raw ISGDynamicTextAssetProvider pointer. */
static TScriptInterface<ISGDynamicTextAssetProvider> MakeProviderInterface(
    const ISGDynamicTextAssetProvider* Provider)
{
    const UObject* providerObject = Cast<UObject>(const_cast<ISGDynamicTextAssetProvider*>(Provider));
    TScriptInterface<ISGDynamicTextAssetProvider> providerInterface;
    if (providerObject)
    {
        providerInterface.SetObject(const_cast<UObject*>(providerObject));
        providerInterface.SetInterface(const_cast<ISGDynamicTextAssetProvider*>(Provider));
    }
    return providerInterface;
}

} // namespace SGDTASerializerBasePrivate

void FSGDynamicTextAssetSerializerBase::PreSerializeAssetBundles(
    const ISGDynamicTextAssetProvider* Provider,
    FString& InOutContent) const
{
    if (!Provider)
    {
        return;
    }

    TScriptInterface<ISGDynamicTextAssetProvider> providerInterface =
        SGDTASerializerBasePrivate::MakeProviderInterface(Provider);

    USGDTAAssetBundleExtender* extender =
        SGDTASerializerBasePrivate::ResolveExtender(providerInterface, GetSerializerFormat());
    if (!extender)
    {
        return;
    }

    extender->NotifyPreSerialize(InOutContent, GetSerializerFormat());
}

void FSGDynamicTextAssetSerializerBase::PostSerializeAssetBundles(
    const ISGDynamicTextAssetProvider* Provider,
    FString& InOutContent) const
{
    if (!Provider)
    {
        return;
    }

    const UObject* providerObject = Cast<UObject>(const_cast<ISGDynamicTextAssetProvider*>(Provider));
    if (!providerObject)
    {
        return;
    }

    TScriptInterface<ISGDynamicTextAssetProvider> providerInterface =
        SGDTASerializerBasePrivate::MakeProviderInterface(Provider);

    USGDTAAssetBundleExtender* extender =
        SGDTASerializerBasePrivate::ResolveExtender(providerInterface, GetSerializerFormat());
    if (!extender)
    {
        return;
    }

    FSGDynamicTextAssetBundleData bundleData;
    extender->NotifyExtractBundles(providerObject, bundleData);
    extender->NotifyPostSerialize(bundleData, InOutContent, GetSerializerFormat());
}

bool FSGDynamicTextAssetSerializerBase::PreDeserializeAssetBundles(
    FString& InOutContent,
    FSGDynamicTextAssetBundleData& OutBundleData,
    const TScriptInterface<ISGDynamicTextAssetProvider>& Provider) const
{
    USGDTAAssetBundleExtender* extender =
        SGDTASerializerBasePrivate::ResolveExtender(Provider, GetSerializerFormat());
    if (!extender)
    {
        // No extender resolved is not a fatal error (file may have no bundles configured)
        return true;
    }

    extender->NotifyPreDeserialize(InOutContent, OutBundleData, GetSerializerFormat());
    return true;
}

bool FSGDynamicTextAssetSerializerBase::PostDeserializeAssetBundles(
    const FString& Content,
    FSGDynamicTextAssetBundleData& InOutBundleData,
    const TScriptInterface<ISGDynamicTextAssetProvider>& Provider) const
{
    USGDTAAssetBundleExtender* extender =
        SGDTASerializerBasePrivate::ResolveExtender(Provider, GetSerializerFormat());
    if (!extender)
    {
        return true;
    }

    extender->NotifyPostDeserialize(Content, InOutBundleData, GetSerializerFormat());
    return true;
}
