// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetYamlSerializer.h"

#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "SGDynamicTextAssetLogs.h"
#include "Statics/SGDynamicTextAssetSlateStyles.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

THIRD_PARTY_INCLUDES_START
#include <fkYAML/node.hpp>
THIRD_PARTY_INCLUDES_END

namespace FSGDynamicTextAssetYamlSerializerInternals
{
    /** Converts an FString to a std::string (UTF-8). */
    std::string ToStdString(const FString& InString)
    {
        const auto utf8 = StringCast<UTF8CHAR>(*InString);
        return std::string(reinterpret_cast<const char*>(utf8.Get()), utf8.Length());
    }

    /** Converts a std::string (UTF-8) to an FString. */
    FString ToFString(const std::string& InString)
    {
        return FString(UTF8_TO_TCHAR(InString.c_str()));
    }

    /**
     * Finds a key in a YAML mapping node using case-insensitive comparison.
     *
     * UE FName is case-insensitive but case-preserving (stores whichever casing
     * was registered first). property->GetName() may therefore return different
     * casings in editor vs packaged builds. YAML keys written in the editor
     * preserve the original casing, so a direct std::string lookup can fail
     * when the FName registration order differs between builds.
     *
     * @param MappingNode The YAML mapping node to search
     * @param Key The key to find (case-insensitive)
     * @param OutActualKey If found, set to the actual key string from the YAML node
     * @return True if a matching key was found
     */
    bool FindKeyCaseInsensitive(const fkyaml::node& MappingNode, const std::string& Key, std::string& OutActualKey)
    {
        if (!MappingNode.is_mapping())
        {
            return false;
        }

        // Try exact match first (fast path)
        if (MappingNode.contains(Key))
        {
            OutActualKey = Key;
            return true;
        }

        // Fall back to case-insensitive search
        for (auto& pair : MappingNode.map_items())
        {
            const std::string& nodeKey = pair.key().get_value<std::string>();
            if (nodeKey.size() == Key.size())
            {
                bool bMatch = true;
                for (size_t i = 0; i < Key.size(); ++i)
                {
                    if (std::tolower(static_cast<unsigned char>(nodeKey[i])) !=
                        std::tolower(static_cast<unsigned char>(Key[i])))
                    {
                        bMatch = false;
                        break;
                    }
                }
                if (bMatch)
                {
                    OutActualKey = nodeKey;
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * Recursively converts a FJsonValue tree to a fkYAML node.
     *
     * @param Value The JSON value to convert
     * @return The corresponding YAML node
     */
    fkyaml::node JsonValueToYamlNode(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid() || Value->Type == EJson::Null)
        {
            return fkyaml::node();
        }

        switch (Value->Type)
        {
            case EJson::String:
            {
                return fkyaml::node(ToStdString(Value->AsString()));
            }
            case EJson::Number:
            {
                const double numValue = Value->AsNumber();
                // If the value is an integer (no fractional part), store as integer for cleaner YAML output
                if (FMath::IsNearlyEqual(numValue, FMath::RoundToDouble(numValue)) && FMath::Abs(numValue) < static_cast<double>(MAX_int64))
                {
                    return fkyaml::node(static_cast<int64>(numValue));
                }
                return fkyaml::node(numValue);
            }
            case EJson::Boolean:
            {
                return fkyaml::node(Value->AsBool());
            }
            case EJson::Object:
            {
                fkyaml::node mapNode = fkyaml::node::mapping();
                const TSharedPtr<FJsonObject>& obj = Value->AsObject();
                if (obj.IsValid())
                {
                    for (const TPair<FString, TSharedPtr<FJsonValue>>& field : obj->Values)
                    {
                        mapNode[ToStdString(field.Key)] = JsonValueToYamlNode(field.Value);
                    }
                }
                return mapNode;
            }
            case EJson::Array:
            {
                fkyaml::node seqNode = fkyaml::node::sequence();
                for (const TSharedPtr<FJsonValue>& item : Value->AsArray())
                {
                    seqNode.as_seq().emplace_back(JsonValueToYamlNode(item));
                }
                return seqNode;
            }
            default:
            {
                return fkyaml::node();
            }
        }
    }

    /**
     * Recursively converts a fkYAML node to a FJsonValue, using the property type to
     * produce the correct EJson variant expected by FJsonObjectConverter.
     *
     * @param YamlNode  Source YAML node
     * @param Property  The UE property descriptor describing the expected type
     * @return The reconstructed FJsonValue, or nullptr on failure
     */
    TSharedPtr<FJsonValue> YamlNodeToJsonValue(const fkyaml::node& YamlNode, FProperty* Property)
    {
        if (!Property)
        {
            return nullptr;
        }

        if (FBoolProperty* boolProp = CastField<FBoolProperty>(Property))
        {
            if (YamlNode.is_boolean())
            {
                return MakeShared<FJsonValueBoolean>(YamlNode.get_value<bool>());
            }
            // Tolerate string representation of booleans
            if (YamlNode.is_string())
            {
                const FString strValue = ToFString(YamlNode.get_value<std::string>());
                const bool bValue = strValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
                return MakeShared<FJsonValueBoolean>(bValue);
            }
            return MakeShared<FJsonValueBoolean>(false);
        }

        if (FNumericProperty* numProp = CastField<FNumericProperty>(Property))
        {
            if (numProp->IsEnum())
            {
                // Enum properties expect a string (the enum value name)
                if (YamlNode.is_string())
                {
                    return MakeShared<FJsonValueString>(ToFString(YamlNode.get_value<std::string>()));
                }
                // Tolerate integer enum values
                if (YamlNode.is_integer())
                {
                    return MakeShared<FJsonValueString>(FString::FromInt(static_cast<int32>(YamlNode.get_value<int64>())));
                }
                return MakeShared<FJsonValueString>(TEXT(""));
            }
            // Numeric properties accept EJson::Number
            if (YamlNode.is_integer())
            {
                return MakeShared<FJsonValueNumber>(static_cast<double>(YamlNode.get_value<int64>()));
            }
            if (YamlNode.is_float_number())
            {
                return MakeShared<FJsonValueNumber>(YamlNode.get_value<double>());
            }
            // Tolerate string representation of numbers
            if (YamlNode.is_string())
            {
                const double numValue = FCString::Atod(*ToFString(YamlNode.get_value<std::string>()));
                return MakeShared<FJsonValueNumber>(numValue);
            }
            return MakeShared<FJsonValueNumber>(0.0f);
        }

        if (CastField<FEnumProperty>(Property))
        {
            if (YamlNode.is_string())
            {
                return MakeShared<FJsonValueString>(ToFString(YamlNode.get_value<std::string>()));
            }
            return MakeShared<FJsonValueString>(TEXT(""));
        }

        if (CastField<FStrProperty>(Property) ||
            CastField<FNameProperty>(Property) ||
            CastField<FTextProperty>(Property))
        {
            if (YamlNode.is_string())
            {
                return MakeShared<FJsonValueString>(ToFString(YamlNode.get_value<std::string>()));
            }
            // Tolerate other scalar types by converting to string
            if (YamlNode.is_integer())
            {
                return MakeShared<FJsonValueString>(FString::Printf(TEXT("%lld"), YamlNode.get_value<int64>()));
            }
            if (YamlNode.is_float_number())
            {
                return MakeShared<FJsonValueString>(FString::SanitizeFloat(YamlNode.get_value<double>()));
            }
            if (YamlNode.is_boolean())
            {
                return MakeShared<FJsonValueString>(YamlNode.get_value<bool>() ? TEXT("true") : TEXT("false"));
            }
            return MakeShared<FJsonValueString>(TEXT(""));
        }

        if (FObjectPropertyBase* objProp = CastField<FObjectPropertyBase>(Property))
        {
            // Soft object/class references serialize as asset path strings
            if (YamlNode.is_string())
            {
                return MakeShared<FJsonValueString>(ToFString(YamlNode.get_value<std::string>()));
            }
            return MakeShared<FJsonValueString>(TEXT(""));
        }

        if (FArrayProperty* arrProp = CastField<FArrayProperty>(Property))
        {
            TArray<TSharedPtr<FJsonValue>> items;
            if (YamlNode.is_sequence())
            {
                for (const fkyaml::node& child : YamlNode)
                {
                    TSharedPtr<FJsonValue> itemValue = YamlNodeToJsonValue(child, arrProp->Inner);
                    if (itemValue.IsValid())
                    {
                        items.Add(itemValue);
                    }
                }
            }
            return MakeShared<FJsonValueArray>(items);
        }

        if (FSetProperty* setProp = CastField<FSetProperty>(Property))
        {
            TArray<TSharedPtr<FJsonValue>> items;
            if (YamlNode.is_sequence())
            {
                for (const fkyaml::node& child : YamlNode)
                {
                    TSharedPtr<FJsonValue> itemValue = YamlNodeToJsonValue(child, setProp->ElementProp);
                    if (itemValue.IsValid())
                    {
                        items.Add(itemValue);
                    }
                }
            }
            return MakeShared<FJsonValueArray>(items);
        }

        if (FMapProperty* mapProp = CastField<FMapProperty>(Property))
        {
            // TMap serializes as a JSON object: YAML mapping key is the map key, value is the map value
            TSharedRef<FJsonObject> obj = MakeShared<FJsonObject>();
            if (YamlNode.is_mapping())
            {
                for (auto& pair : YamlNode.map_items())
                {
                    const FString keyStr = ToFString(pair.key().get_value<std::string>());
                    TSharedPtr<FJsonValue> entryValue = YamlNodeToJsonValue(pair.value(), mapProp->ValueProp);
                    if (entryValue.IsValid())
                    {
                        obj->SetField(keyStr, entryValue);
                    }
                }
            }
            return MakeShared<FJsonValueObject>(obj);
        }

        if (FStructProperty* structProp = CastField<FStructProperty>(Property))
        {
            // YAML node type determines the deserialization path:
            // - Mapping node: always reconstruct as a JSON object from named fields (generic struct)
            // - String node: use custom text import if available (e.g., FSGDynamicTextAssetRef as bare GUID)
            // Checking the node type first avoids HasImportTextItem() returning true for all
            // GENERATED_BODY() structs, which would incorrectly short-circuit plain structs.
            if (YamlNode.is_mapping())
            {
                TSharedRef<FJsonObject> obj = MakeShared<FJsonObject>();
                // Iterate YAML mapping keys and match to struct properties case-insensitively.
                // FJsonObjectConverter::UStructToJsonObject uses StandardizeCase (lowercases first char),
                // so YAML keys are camelCase while FProperty names are PascalCase.
                for (auto& pair : YamlNode.map_items())
                {
                    const FString keyStr = ToFString(pair.key().get_value<std::string>());
                    for (TFieldIterator<FProperty> propIt(structProp->Struct); propIt; ++propIt)
                    {
                        if (keyStr.Equals((*propIt)->GetName(), ESearchCase::IgnoreCase))
                        {
                            TSharedPtr<FJsonValue> fieldValue = YamlNodeToJsonValue(pair.value(), *propIt);
                            if (fieldValue.IsValid())
                            {
                                obj->SetField(keyStr, fieldValue);
                            }
                            break;
                        }
                    }
                }
                return MakeShared<FJsonValueObject>(obj);
            }

            // String node with custom text serialization (e.g., FSGDynamicTextAssetRef as a bare GUID string).
            // Returns FJsonValueString so FJsonObjectConverter::JsonValueToUProperty calls ImportTextItem().
            if (YamlNode.is_string())
            {
                return MakeShared<FJsonValueString>(ToFString(YamlNode.get_value<std::string>()));
            }

            return MakeShared<FJsonValueString>(TEXT(""));
        }

        // Default fallback: treat as string
        if (YamlNode.is_string())
        {
            return MakeShared<FJsonValueString>(ToFString(YamlNode.get_value<std::string>()));
        }
        if (YamlNode.is_integer())
        {
            return MakeShared<FJsonValueString>(FString::Printf(TEXT("%lld"), YamlNode.get_value<int64>()));
        }
        if (YamlNode.is_float_number())
        {
            return MakeShared<FJsonValueString>(FString::SanitizeFloat(YamlNode.get_value<double>()));
        }
        if (YamlNode.is_boolean())
        {
            return MakeShared<FJsonValueString>(YamlNode.get_value<bool>() ? TEXT("true") : TEXT("false"));
        }

        return MakeShared<FJsonValueString>(TEXT(""));
    }

    /**
     * Converts a YAML mapping node representing an instanced sub-object to
     * an FJsonObject suitable for DeserializeValueToInstancedObject.
     *
     * Reads the SG_INST_OBJ_CLASS key to resolve the UClass, then uses the
     * resolved class's property descriptors to convert each child YAML node to
     * the correct FJsonValue variant via YamlNodeToJsonValue. Handles nested
     * instanced objects by recursion.
     *
     * @param YamlNode The YAML mapping node containing the instanced object data
     * @return FJsonValueObject with class key and property values, or FJsonValueNull for null objects
     */
    TSharedPtr<FJsonValue> YamlNodeToInstancedObjectJsonValue(const fkyaml::node& YamlNode)
    {
        // Null node means null sub-object
        if (YamlNode.is_null())
        {
            return MakeShared<FJsonValueNull>();
        }

        // Non-mapping node (e.g., empty scalar) also means null sub-object
        if (!YamlNode.is_mapping())
        {
            return MakeShared<FJsonValueNull>();
        }

        // Empty mapping means null sub-object
        if (YamlNode.empty())
        {
            return MakeShared<FJsonValueNull>();
        }

        // Read class name from the reserved key
        const std::string classKey = ToStdString(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
        if (!YamlNode.contains(classKey))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue: YAML mapping missing '%s' key"),
                *FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
            return nullptr;
        }

        const fkyaml::node& classNode = YamlNode[classKey];
        if (!classNode.is_string())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue: '%s' value is not a string"),
                *FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
            return nullptr;
        }

        const FString className = ToFString(classNode.get_value<std::string>());
        if (className.IsEmpty())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue: '%s' value is empty"),
                *FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
            return nullptr;
        }

        // Resolve class to get property type information for correct JSON variant conversion.
        UClass* resolvedClass = FSGDynamicTextAssetSerializerBase::ResolveInstancedObjectClass(className);
        if (!resolvedClass)
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue: Failed to resolve class '%s'"), *className);
            return nullptr;
        }

        // Build JSON object with class key and typed property values
        TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
        jsonObject->SetStringField(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY, className);

        for (TFieldIterator<FProperty> propIt(resolvedClass); propIt; ++propIt)
        {
            FProperty* subProp = *propIt;
            const std::string subPropName = ToStdString(subProp->GetName());
            if (!YamlNode.contains(subPropName))
            {
                continue;
            }

            // Handle nested single instanced objects recursively
            if (FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(subProp))
            {
                if (CastField<FObjectProperty>(subProp))
                {
                    TSharedPtr<FJsonValue> nestedValue = YamlNodeToInstancedObjectJsonValue(YamlNode[subPropName]);
                    if (nestedValue.IsValid())
                    {
                        jsonObject->SetField(subProp->GetName(), nestedValue);
                    }
                    continue;
                }
            }

            TSharedPtr<FJsonValue> fieldValue = YamlNodeToJsonValue(YamlNode[subPropName], subProp);
            if (fieldValue.IsValid())
            {
                jsonObject->SetField(subProp->GetName(), fieldValue);
            }
        }

        return MakeShared<FJsonValueObject>(jsonObject);
    }

    /**
     * Parses a YAML string into a fkYAML root node.
     *
     * @param InString The YAML string to parse
     * @param OutRootNode The parsed root node
     * @param OutErrorMessage Error description if parsing fails
     * @return True if parsing succeeded
     */
    bool ParseYaml(const FString& InString, fkyaml::node& OutRootNode, FString& OutErrorMessage)
    {
        try
        {
            const std::string yamlStr = ToStdString(InString);
            OutRootNode = fkyaml::node::deserialize(yamlStr);
            return true;
        }
        catch (const fkyaml::exception& e)
        {
            OutErrorMessage = ToFString(e.what());
            return false;
        }
        catch (const std::exception& e)
        {
            OutErrorMessage = ToFString(e.what());
            return false;
        }
    }
}

#if WITH_EDITOR
const FSlateBrush* FSGDynamicTextAssetYamlSerializer::GetIconBrush() const
{
    static const FSlateBrush* icon = FSGDynamicTextAssetSlateStyles::GetBrush(FSGDynamicTextAssetSlateStyles::BRUSH_NAME_YAML);
    return icon;
}
#endif

FString FSGDynamicTextAssetYamlSerializer::GetFileExtension() const
{
    static const FString extension = ".dta.yaml";
    return extension;
}

FText FSGDynamicTextAssetYamlSerializer::GetFormatName() const
{
    static const FText name = INVTEXT("YAML");
    return name;
}

FText FSGDynamicTextAssetYamlSerializer::GetFormatDescription() const
{
#if !UE_BUILD_SHIPPING
    static const FText description = FText::AsCultureInvariant(TEXT(R"(YAML serialization for dynamic text assets.

Uses the fkYAML header-only library (MIT license) for reading and writing.

Property values are converted via the FSGDynamicTextAssetSerializerBase
JSON-intermediate helpers, keeping format-specific complexity
contained to the YAML-to-FJsonValue bridge.

YAML format uses a metadata wrapper block(sgFileInformation), for example:
sgFileInformation:
  type: UWeaponData
  version: 1.0.0
  id: 550E8400-E29B-41D4-A716-446655440000
  userfacingid: excalibur
  fileFormatVersion: 1.5.2
data:
  Damage: 50
  Tags:
    - fast
    - heavy
)"));
    return description;
#else
    return FText::GetEmpty();
#endif
}

uint32 FSGDynamicTextAssetYamlSerializer::GetSerializerTypeId() const
{
    return TYPE_ID;
}

FSGDynamicTextAssetVersion FSGDynamicTextAssetYamlSerializer::GetFileFormatVersion() const
{
    static const FSGDynamicTextAssetVersion version(2, 0, 0);
    return version;
}

bool FSGDynamicTextAssetYamlSerializer::SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const
{
    OutString.Empty();

    if (!Provider)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Inputted NULL Provider"));
        return false;
    }
    const UObject* providerObject = Provider->_getUObject();
    if (!providerObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Provider is not a valid UObject"));
        return false;
    }

    try
    {
        fkyaml::node rootNode = fkyaml::node::mapping();

        // Metadata block
        // Write Asset Type ID GUID to the type field, fall back to class name if unavailable
        FString typeString;
        if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
        {
            const FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(providerObject->GetClass());
            if (typeId.IsValid())
            {
                typeString = typeId.ToString();
            }
        }

        if (typeString.IsEmpty())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetYamlSerializer::SerializeProvider: No valid Asset Type ID found for class '%s', falling back to class name"),
                *providerObject->GetClass()->GetName());
            typeString = providerObject->GetClass()->GetName();
        }

        fkyaml::node metadataNode = fkyaml::node::mapping();
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(typeString));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_VERSION)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(Provider->GetVersion().ToString()));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_ID)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(Provider->GetDynamicTextAssetId().ToString()));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_USER_FACING_ID)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(Provider->GetUserFacingId()));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_FORMAT_VERSION)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(GetFileFormatVersion().ToString()));
        rootNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_INFORMATION)] = metadataNode;

        // Data block
        fkyaml::node dataNode = fkyaml::node::mapping();

        for (TFieldIterator<FProperty> propIt(providerObject->GetClass()); propIt; ++propIt)
        {
            FProperty* property = *propIt;

            if (!ShouldSerializeProperty(property))
            {
                continue;
            }

            const void* valuePtr = property->ContainerPtrToValuePtr<void>(providerObject);

            // Handle instanced object properties.
            // CPF_InstancedReference is on the FObjectProperty itself (single) or on
            // the container's inner/element/value property (TArray, TSet, TMap).

            // Single instanced object
            if (IsInstancedObjectProperty(property))
            {
                if (const FObjectProperty* objectProp = CastField<FObjectProperty>(property))
                {
                    const UObject* subObject = objectProp->GetObjectPropertyValue(valuePtr);
                    TSharedPtr<FJsonValue> jsonValue = SerializeInstancedObjectToValue(objectProp, subObject);
                    if (jsonValue.IsValid())
                    {
                        dataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(property->GetName())] =
                            FSGDynamicTextAssetYamlSerializerInternals::JsonValueToYamlNode(jsonValue);
                    }
                    continue;
                }
            }

            // Array of instanced objects
            if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(property))
            {
                if (arrayProp->Inner && IsInstancedObjectProperty(arrayProp->Inner))
                {
                    if (const FObjectProperty* innerObjProp = CastField<FObjectProperty>(arrayProp->Inner))
                    {
                        FScriptArrayHelper arrayHelper(arrayProp, valuePtr);
                        TArray<TSharedPtr<FJsonValue>> jsonArray;
                        jsonArray.Reserve(arrayHelper.Num());

                        for (int32 i = 0; i < arrayHelper.Num(); ++i)
                        {
                            const UObject* element = innerObjProp->GetObjectPropertyValue(arrayHelper.GetRawPtr(i));
                            jsonArray.Add(SerializeInstancedObjectToValue(innerObjProp, element));
                        }

                        TSharedPtr<FJsonValue> arrayValue = MakeShared<FJsonValueArray>(jsonArray);
                        dataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(property->GetName())] =
                            FSGDynamicTextAssetYamlSerializerInternals::JsonValueToYamlNode(arrayValue);
                        continue;
                    }
                }
            }

            // TSet of instanced objects
            if (const FSetProperty* setProp = CastField<FSetProperty>(property))
            {
                if (setProp->ElementProp && IsInstancedObjectProperty(setProp->ElementProp))
                {
                    if (const FObjectProperty* elemObjProp = CastField<FObjectProperty>(setProp->ElementProp))
                    {
                        FScriptSetHelper setHelper(setProp, valuePtr);
                        TArray<TSharedPtr<FJsonValue>> jsonArray;
                        jsonArray.Reserve(setHelper.Num());

                        for (int32 i = 0; i < setHelper.GetMaxIndex(); ++i)
                        {
                            if (setHelper.IsValidIndex(i))
                            {
                                const UObject* element = elemObjProp->GetObjectPropertyValue(setHelper.GetElementPtr(i));
                                jsonArray.Add(SerializeInstancedObjectToValue(elemObjProp, element));
                            }
                        }

                        TSharedPtr<FJsonValue> arrayValue = MakeShared<FJsonValueArray>(jsonArray);
                        dataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(property->GetName())] =
                            FSGDynamicTextAssetYamlSerializerInternals::JsonValueToYamlNode(arrayValue);
                        continue;
                    }
                }
            }

            // TMap with instanced object values
            if (const FMapProperty* mapProp = CastField<FMapProperty>(property))
            {
                if (mapProp->ValueProp && IsInstancedObjectProperty(mapProp->ValueProp))
                {
                    if (const FObjectProperty* valueObjProp = CastField<FObjectProperty>(mapProp->ValueProp))
                    {
                        FScriptMapHelper mapHelper(mapProp, valuePtr);
                        TSharedRef<FJsonObject> mapObject = MakeShared<FJsonObject>();

                        for (int32 i = 0; i < mapHelper.Num(); ++i)
                        {
                            if (mapHelper.IsValidIndex(i))
                            {
                                FString keyString;
                                mapProp->KeyProp->ExportTextItem_Direct(keyString, mapHelper.GetKeyPtr(i), nullptr, nullptr, PPF_None);

                                const UObject* valueObj = valueObjProp->GetObjectPropertyValue(mapHelper.GetValuePtr(i));
                                mapObject->SetField(keyString, SerializeInstancedObjectToValue(valueObjProp, valueObj));
                            }
                        }

                        TSharedPtr<FJsonValue> mapValue = MakeShared<FJsonValueObject>(mapObject);
                        dataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(property->GetName())] =
                            FSGDynamicTextAssetYamlSerializerInternals::JsonValueToYamlNode(mapValue);
                        continue;
                    }
                }
            }

            TSharedPtr<FJsonValue> jsonValue = SerializePropertyToValue(property, valuePtr);

            if (jsonValue.IsValid())
            {
                dataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(property->GetName())] =
                    FSGDynamicTextAssetYamlSerializerInternals::JsonValueToYamlNode(jsonValue);
            }
            else
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Failed to serialize property(%s) on Provider(%s)"),
                    *property->GetName(), *GetNameSafe(providerObject));
            }
        }

        rootNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_DATA)] = dataNode;

        // Extract and serialize asset bundle metadata from soft reference properties
        FSGDynamicTextAssetBundleData bundleData;
        bundleData.ExtractFromObject(providerObject);

        if (bundleData.HasBundles())
        {
            fkyaml::node bundlesNode = fkyaml::node::mapping();

            for (const FSGDynamicTextAssetBundle& bundle : bundleData.Bundles)
            {
                fkyaml::node entryArray = fkyaml::node::sequence();

                for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
                {
                    fkyaml::node entryNode = fkyaml::node::mapping();
                    entryNode["property"] = fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(entry.PropertyName.ToString()));
                    entryNode["path"] = fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(entry.AssetPath.ToString()));
                    entryArray.as_seq().emplace_back(entryNode);
                }

                bundlesNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(bundle.BundleName.ToString())] = entryArray;
            }

            rootNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_SGDT_ASSET_BUNDLES)] = bundlesNode;
        }

        // Serialize to YAML string
        const std::string yamlStr = fkyaml::node::serialize(rootNode);
        OutString = FSGDynamicTextAssetYamlSerializerInternals::ToFString(yamlStr);
        return true;
    }
    catch (const fkyaml::exception& e)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: fkYAML exception during serialization: %s"),
            *FSGDynamicTextAssetYamlSerializerInternals::ToFString(e.what()));
        return false;
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Exception during serialization: %s"),
            *FSGDynamicTextAssetYamlSerializerInternals::ToFString(e.what()));
        return false;
    }
}

bool FSGDynamicTextAssetYamlSerializer::DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const
{
    bOutMigrated = false;

    if (!OutProvider)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Inputted NULL OutProvider"));
        return false;
    }

    UObject* providerObject = OutProvider->_getUObject();
    if (!providerObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Provider is not a valid UObject"));
        return false;
    }

    if (InString.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Inputted EMPTY InString"));
        return false;
    }

    fkyaml::node rootNode;
    FString parseError;
    if (!FSGDynamicTextAssetYamlSerializerInternals::ParseYaml(InString, rootNode, parseError))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: Failed to parse YAML: %s"), *parseError);
        return false;
    }

    if (!rootNode.is_mapping())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: YAML root is not a mapping"));
        return false;
    }

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_INFORMATION);
    const std::string legacyMetadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA_LEGACY);
    const std::string& activeMetadataKey = rootNode.contains(metadataKey) ? metadataKey : legacyMetadataKey;
    const std::string dataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_DATA);
    const std::string typeKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE);
    const std::string versionKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_VERSION);
    const std::string idKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_ID);
    const std::string userFacingIdKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_USER_FACING_ID);

    // Read file information block (supports legacy "metadata" key)
    if (!rootNode.contains(activeMetadataKey))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: YAML missing '%s' (or legacy '%s') block"), *KEY_FILE_INFORMATION, *KEY_METADATA_LEGACY);
        return false;
    }
    const fkyaml::node& metadataNode = rootNode[activeMetadataKey];
    if (!metadataNode.is_mapping())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: '%s' block is not a mapping"), *KEY_FILE_INFORMATION);
        return false;
    }

    // Validate class type field may contain a GUID (new format) or class name (legacy)
    if (metadataNode.contains(typeKey) && metadataNode[typeKey].is_string())
    {
        const FString typeFieldValue = FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[typeKey].get_value<std::string>());
        FSGDynamicTextAssetTypeId fileTypeId = FSGDynamicTextAssetTypeId::FromString(typeFieldValue);
        if (fileTypeId.IsValid())
        {
            // New format: resolve Asset Type ID GUID to class and validate
            if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                if (const UClass* resolvedClass = registry->ResolveClassForTypeId(fileTypeId))
                {
                    if (resolvedClass != providerObject->GetClass())
                    {
                        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                            TEXT("FSGDynamicTextAssetYamlSerializer: YAML Asset Type ID(%s) resolves to class(%s) but OutProvider is class(%s)"),
                            *typeFieldValue, *resolvedClass->GetName(), *providerObject->GetClass()->GetName());
                        // Continue anyway, might be loading into a parent class
                    }
                }
                else
                {
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                        TEXT("FSGDynamicTextAssetYamlSerializer: Could not resolve Asset Type ID(%s) to a class"),
                        *typeFieldValue);
                }
            }
        }
        else
        {
            // Legacy format: compare class name directly
            if (typeFieldValue != providerObject->GetClass()->GetName())
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                    TEXT("FSGDynamicTextAssetYamlSerializer: YAML typeName(%s) does not match OutProvider(%s)"),
                    *typeFieldValue, *providerObject->GetClass()->GetName());
                // Continue anyway, might be loading into a parent class
            }
        }
    }

    // Extract and apply ID
    if (metadataNode.contains(idKey) && metadataNode[idKey].is_string())
    {
        FSGDynamicTextAssetId id;
        if (id.ParseString(FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[idKey].get_value<std::string>())))
        {
            OutProvider->SetDynamicTextAssetId(id);
        }
    }

    // Extract and apply UserFacingId
    if (metadataNode.contains(userFacingIdKey) && metadataNode[userFacingIdKey].is_string())
    {
        OutProvider->SetUserFacingId(FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[userFacingIdKey].get_value<std::string>()));
    }

    // Extract and apply version
    FSGDynamicTextAssetVersion fileVersion;
    if (metadataNode.contains(versionKey) && metadataNode[versionKey].is_string())
    {
        fileVersion = FSGDynamicTextAssetVersion::ParseFromString(FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[versionKey].get_value<std::string>()));
        OutProvider->SetVersion(fileVersion);
    }

    // Extract file format version (missing = 1.0.0 for pre-format-version files)
    FSGDynamicTextAssetVersion fileFormatVersion(1, 0, 0);
    const std::string formatVersionKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_FORMAT_VERSION);
    if (metadataNode.contains(formatVersionKey) && metadataNode[formatVersionKey].is_string())
    {
        fileFormatVersion = FSGDynamicTextAssetVersion::ParseFromString(
            FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[formatVersionKey].get_value<std::string>()));
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
        TEXT("FSGDynamicTextAssetYamlSerializer: File format version: %s (serializer current: %s)"),
        *fileFormatVersion.ToString(), *GetFileFormatVersion().ToString());

    // Check for version migration
    const FSGDynamicTextAssetVersion currentVersion = OutProvider->GetCurrentVersion();
    if (fileVersion.Major < currentVersion.Major)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
            TEXT("FSGDynamicTextAssetYamlSerializer: Migration required for Provider(%s): file version(%s) -> class version(%s)"),
            *OutProvider->GetDynamicTextAssetId().ToString(), *fileVersion.ToString(), *currentVersion.ToString());

        // Migration passes a null FJsonObject - YAML providers must handle that in MigrateFromVersion
        if (!OutProvider->MigrateFromVersion(fileVersion, currentVersion, nullptr))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
                TEXT("FSGDynamicTextAssetYamlSerializer: Migration failed for Provider(%s) from fileVersion(%s)"),
                *OutProvider->GetDynamicTextAssetId().ToString(), *fileVersion.ToString());
            return false;
        }

        OutProvider->SetVersion(currentVersion);
        bOutMigrated = true;

        UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
            TEXT("FSGDynamicTextAssetYamlSerializer: Migration succeeded for Provider(%s): version(%s)"),
            *OutProvider->GetDynamicTextAssetId().ToString(), *OutProvider->GetVersion().ToString());
    }
    else if (fileVersion.Major > currentVersion.Major)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("FSGDynamicTextAssetYamlSerializer: Provider(%s) has file major version fileVersion Major(%d) which is newer than class currentVersion Major(%d). Loading with best-effort."),
            *OutProvider->GetDynamicTextAssetId().ToString(), fileVersion.Major, currentVersion.Major);
    }

    // Find data block
    if (!rootNode.contains(dataKey))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: YAML missing '%s' block"), *KEY_DATA);
        return false;
    }
    const fkyaml::node& dataNode = rootNode[dataKey];
    if (!dataNode.is_mapping())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: '%s' block is not a mapping"), *KEY_DATA);
        return false;
    }

    // Deserialize properties
    for (TFieldIterator<FProperty> propIt(providerObject->GetClass()); propIt; ++propIt)
    {
        FProperty* property = *propIt;

        if (!ShouldSerializeProperty(property))
        {
            continue;
        }

        // Use case-insensitive lookup to match YAML keys against property names.
        // FName is case-insensitive but case-preserving, so property->GetName()
        // may return different casings in editor vs packaged builds. The YAML file
        // preserves the original casing from when it was written (typically in the editor).
        const std::string propNameFromReflection = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(property->GetName());
        std::string propName;
        if (!FSGDynamicTextAssetYamlSerializerInternals::FindKeyCaseInsensitive(dataNode, propNameFromReflection, propName))
        {
            // Missing optional field - tolerated gracefully
            continue;
        }

        void* valuePtr = property->ContainerPtrToValuePtr<void>(providerObject);

        // Handle instanced object properties.
        // CPF_InstancedReference is on the FObjectProperty itself (single) or on
        // the container's inner/element/value property (TArray, TSet, TMap).

        // Single instanced object
        if (IsInstancedObjectProperty(property))
        {
            if (const FObjectProperty* objectProp = CastField<FObjectProperty>(property))
            {
                TSharedPtr<FJsonValue> jsonValue = FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue(dataNode[propName]);
                if (!DeserializeValueToInstancedObject(jsonValue, objectProp, valuePtr, providerObject))
                {
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                        TEXT("FSGDynamicTextAssetYamlSerializer: Failed to deserialize instanced property(%s) on OutProvider(%s)"),
                        *property->GetName(), *GetNameSafe(providerObject));
                }
                continue;
            }
        }

        // Array of instanced objects
        if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(property))
        {
            if (arrayProp->Inner && IsInstancedObjectProperty(arrayProp->Inner))
            {
                if (const FObjectProperty* innerObjProp = CastField<FObjectProperty>(arrayProp->Inner))
                {
                    const fkyaml::node& arrayNode = dataNode[propName];
                    FScriptArrayHelper arrayHelper(arrayProp, valuePtr);

                    if (arrayNode.is_sequence())
                    {
                        const auto& seq = arrayNode.as_seq();
                        arrayHelper.Resize(static_cast<int32>(seq.size()));

                        for (int32 i = 0; i < static_cast<int32>(seq.size()); ++i)
                        {
                            TSharedPtr<FJsonValue> elemValue = FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue(seq[i]);
                            if (!DeserializeValueToInstancedObject(elemValue, innerObjProp, arrayHelper.GetRawPtr(i), providerObject))
                            {
                                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                    TEXT("FSGDynamicTextAssetYamlSerializer: Failed to deserialize instanced array element [%d] of property(%s) on OutProvider(%s)"),
                                    i, *property->GetName(), *GetNameSafe(providerObject));
                            }
                        }
                    }
                    else
                    {
                        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                            TEXT("FSGDynamicTextAssetYamlSerializer: Expected sequence for instanced array property '%s' on OutProvider(%s), got different YAML node type"),
                            *property->GetName(), *GetNameSafe(providerObject));
                    }
                    continue;
                }
            }
        }

        // TSet of instanced objects
        if (const FSetProperty* setProp = CastField<FSetProperty>(property))
        {
            if (setProp->ElementProp && IsInstancedObjectProperty(setProp->ElementProp))
            {
                if (const FObjectProperty* elemObjProp = CastField<FObjectProperty>(setProp->ElementProp))
                {
                    const fkyaml::node& setNode = dataNode[propName];
                    FScriptSetHelper setHelper(setProp, valuePtr);
                    setHelper.EmptyElements();

                    if (setNode.is_sequence())
                    {
                        const auto& seq = setNode.as_seq();
                        for (int32 i = 0; i < static_cast<int32>(seq.size()); ++i)
                        {
                            const int32 newIndex = setHelper.AddDefaultValue_Invalid_NeedsRehash();
                            TSharedPtr<FJsonValue> elemValue = FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue(seq[i]);
                            if (!DeserializeValueToInstancedObject(elemValue, elemObjProp, setHelper.GetElementPtr(newIndex), providerObject))
                            {
                                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                    TEXT("FSGDynamicTextAssetYamlSerializer: Failed to deserialize instanced set element [%d] of property(%s) on OutProvider(%s)"),
                                    i, *property->GetName(), *GetNameSafe(providerObject));
                            }
                        }
                    }
                    else
                    {
                        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                            TEXT("FSGDynamicTextAssetYamlSerializer: Expected sequence for instanced set property '%s' on OutProvider(%s), got different YAML node type"),
                            *property->GetName(), *GetNameSafe(providerObject));
                    }

                    setHelper.Rehash();
                    continue;
                }
            }
        }

        // TMap with instanced object values
        if (const FMapProperty* mapProp = CastField<FMapProperty>(property))
        {
            if (mapProp->ValueProp && IsInstancedObjectProperty(mapProp->ValueProp))
            {
                if (const FObjectProperty* valueObjProp = CastField<FObjectProperty>(mapProp->ValueProp))
                {
                    const fkyaml::node& mapNode = dataNode[propName];
                    FScriptMapHelper mapHelper(mapProp, valuePtr);
                    mapHelper.EmptyValues();

                    if (mapNode.is_mapping())
                    {
                        for (auto& pair : mapNode.map_items())
                        {
                            const int32 newIndex = mapHelper.AddDefaultValue_Invalid_NeedsRehash();
                            const FString keyStr = FSGDynamicTextAssetYamlSerializerInternals::ToFString(pair.key().get_value<std::string>());

                            uint8* keyPtr = mapHelper.GetKeyPtr(newIndex);
                            mapProp->KeyProp->ImportText_Direct(*keyStr, keyPtr, nullptr, PPF_None);

                            uint8* valPtr = mapHelper.GetValuePtr(newIndex);
                            TSharedPtr<FJsonValue> elemValue = FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToInstancedObjectJsonValue(pair.value());
                            if (!DeserializeValueToInstancedObject(elemValue, valueObjProp, valPtr, providerObject))
                            {
                                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                    TEXT("FSGDynamicTextAssetYamlSerializer: Failed to deserialize instanced map value for key '%s' of property(%s)"),
                                    *keyStr, *property->GetName());
                            }
                        }
                    }
                    else
                    {
                        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                            TEXT("FSGDynamicTextAssetYamlSerializer: Expected mapping for instanced map property '%s' on OutProvider(%s), got different YAML node type"),
                            *property->GetName(), *GetNameSafe(providerObject));
                    }

                    mapHelper.Rehash();
                    continue;
                }
            }
        }

        TSharedPtr<FJsonValue> jsonValue = FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToJsonValue(dataNode[propName], property);
        if (jsonValue.IsValid())
        {
            if (!DeserializeValueToProperty(jsonValue, property, valuePtr))
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("FSGDynamicTextAssetYamlSerializer: Failed to deserialize property(%s) on OutProvider(%s)"),
                    *property->GetName(), *GetNameSafe(providerObject));
            }
        }
    }

    return true;
}

bool FSGDynamicTextAssetYamlSerializer::ValidateStructure(const FString& InString, FString& OutErrorMessage) const
{
    if (InString.IsEmpty())
    {
        OutErrorMessage = TEXT("Inputted EMPTY string");
        return false;
    }

    fkyaml::node rootNode;
    if (!FSGDynamicTextAssetYamlSerializerInternals::ParseYaml(InString, rootNode, OutErrorMessage))
    {
        OutErrorMessage = FString::Printf(TEXT("Failed to parse YAML: %s"), *OutErrorMessage);
        return false;
    }

    if (!rootNode.is_mapping())
    {
        OutErrorMessage = TEXT("YAML root is not a mapping");
        return false;
    }

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_INFORMATION);
    const std::string legacyMetadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA_LEGACY);
    const std::string& activeMetadataKey = rootNode.contains(metadataKey) ? metadataKey : legacyMetadataKey;
    if (!rootNode.contains(activeMetadataKey))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block '%s' (or legacy '%s')"), *KEY_FILE_INFORMATION, *KEY_METADATA_LEGACY);
        return false;
    }

    const fkyaml::node& metadataNode = rootNode[activeMetadataKey];
    if (!metadataNode.is_mapping())
    {
        OutErrorMessage = FString::Printf(TEXT("'%s' block is not a mapping"), *KEY_FILE_INFORMATION);
        return false;
    }

    const std::string typeKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE);
    if (!metadataNode.contains(typeKey))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required field '%s' inside '%s'"), *KEY_TYPE, *KEY_FILE_INFORMATION);
        return false;
    }

    const fkyaml::node& typeNode = metadataNode[typeKey];
    if (!typeNode.is_string() || typeNode.get_value<std::string>().empty())
    {
        OutErrorMessage = FString::Printf(TEXT("Field '%s' inside '%s' is empty or not a string"), *KEY_TYPE, *KEY_FILE_INFORMATION);
        return false;
    }

    // If fileFormatVersion is present, validate it is a parseable version string
    const std::string formatVersionKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_FORMAT_VERSION);
    if (metadataNode.contains(formatVersionKey) && metadataNode[formatVersionKey].is_string())
    {
        const FString formatVersionStr = FSGDynamicTextAssetYamlSerializerInternals::ToFString(
            metadataNode[formatVersionKey].get_value<std::string>());
        FSGDynamicTextAssetVersion parsedVersion = FSGDynamicTextAssetVersion::ParseFromString(formatVersionStr);
        if (!parsedVersion.IsValid())
        {
            OutErrorMessage = FString::Printf(
                TEXT("Field '%s' has invalid version string: %s"),
                *KEY_FILE_FORMAT_VERSION, *formatVersionStr);
            return false;
        }
    }

    const std::string dataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_DATA);
    if (!rootNode.contains(dataKey))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block '%s'"), *KEY_DATA);
        return false;
    }

    return true;
}

bool FSGDynamicTextAssetYamlSerializer::ExtractMetadata(const FString& InString, FSGDynamicTextAssetFileMetadata& OutMetadata) const
{
    OutMetadata = FSGDynamicTextAssetFileMetadata();
    OutMetadata.SerializerTypeId = TYPE_ID;

    fkyaml::node rootNode;
    FString parseError;
    if (!FSGDynamicTextAssetYamlSerializerInternals::ParseYaml(InString, rootNode, parseError))
    {
        return false;
    }

    if (!rootNode.is_mapping())
    {
        return false;
    }

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_INFORMATION);
    const std::string legacyMetadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA_LEGACY);
    const std::string& activeMetadataKey = rootNode.contains(metadataKey) ? metadataKey : legacyMetadataKey;
    if (!rootNode.contains(activeMetadataKey))
    {
        return false;
    }

    const fkyaml::node& metadataNode = rootNode[activeMetadataKey];
    if (!metadataNode.is_mapping())
    {
        return false;
    }

    const std::string typeKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE);
    const std::string idKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_ID);
    const std::string versionKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_VERSION);
    const std::string userFacingIdKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_USER_FACING_ID);
    const std::string formatVersionKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_FORMAT_VERSION);

    // Type field may contain a GUID (new format) or class name (legacy)
    if (metadataNode.contains(typeKey) && metadataNode[typeKey].is_string())
    {
        const FString typeFieldValue = FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[typeKey].get_value<std::string>());
        FSGDynamicTextAssetTypeId parsedTypeId = FSGDynamicTextAssetTypeId::FromString(typeFieldValue);
        if (parsedTypeId.IsValid())
        {
            OutMetadata.AssetTypeId = parsedTypeId;

            if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                if (const UClass* resolvedClass = registry->ResolveClassForTypeId(parsedTypeId))
                {
                    OutMetadata.ClassName = resolvedClass->GetName();
                }
            }
        }
        else
        {
            OutMetadata.ClassName = typeFieldValue;
        }
    }

    if (metadataNode.contains(idKey) && metadataNode[idKey].is_string())
    {
        OutMetadata.Id.ParseString(FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[idKey].get_value<std::string>()));
    }
    if (metadataNode.contains(versionKey) && metadataNode[versionKey].is_string())
    {
        OutMetadata.Version = FSGDynamicTextAssetVersion::ParseFromString(
            FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[versionKey].get_value<std::string>()));
    }
    if (metadataNode.contains(userFacingIdKey) && metadataNode[userFacingIdKey].is_string())
    {
        OutMetadata.UserFacingId = FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[userFacingIdKey].get_value<std::string>());
    }
    if (metadataNode.contains(formatVersionKey) && metadataNode[formatVersionKey].is_string())
    {
        OutMetadata.FileFormatVersion = FSGDynamicTextAssetVersion::ParseFromString(
            FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[formatVersionKey].get_value<std::string>()));
    }

    OutMetadata.bIsValid = OutMetadata.AssetTypeId.IsValid() || !OutMetadata.ClassName.IsEmpty();
    return OutMetadata.bIsValid;
}

bool FSGDynamicTextAssetYamlSerializer::UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const
{
    if (InOutContents.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::UpdateFieldsInPlace: Inputted EMPTY InOutContents"));
        return false;
    }

    if (FieldUpdates.IsEmpty())
    {
        return false;
    }

    // Validate the YAML is well-formed before attempting updates
    FString errorMessage;
    if (!ValidateStructure(InOutContents, errorMessage))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::UpdateFieldsInPlace: Invalid YAML: %s"), *errorMessage);
        return false;
    }

    // Parse, modify, and re-serialize to maintain valid YAML structure.
    // Unlike XML where we can do targeted string replacement on leaf elements,
    // YAML values can span multiple lines and have context-dependent quoting,
    // making regex replacement unreliable. A full round-trip is safer.
    fkyaml::node rootNode;
    FString parseError;
    if (!FSGDynamicTextAssetYamlSerializerInternals::ParseYaml(InOutContents, rootNode, parseError))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::UpdateFieldsInPlace: Failed to parse YAML: %s"), *parseError);
        return false;
    }

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_INFORMATION);
    const std::string legacyMetadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA_LEGACY);
    const std::string& activeMetadataKey = rootNode.contains(metadataKey) ? metadataKey : legacyMetadataKey;
    if (!rootNode.contains(activeMetadataKey) || !rootNode[activeMetadataKey].is_mapping())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::UpdateFieldsInPlace: Missing or invalid '%s' (or legacy '%s') block"), *KEY_FILE_INFORMATION, *KEY_METADATA_LEGACY);
        return false;
    }

    fkyaml::node& metadataNode = rootNode[activeMetadataKey];
    bool bAnyUpdated = false;

    for (const TPair<FString, FString>& update : FieldUpdates)
    {
        const std::string fieldKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(update.Key);
        if (metadataNode.contains(fieldKey))
        {
            metadataNode[fieldKey] = fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(update.Value));
            bAnyUpdated = true;
        }
    }

    if (bAnyUpdated)
    {
        try
        {
            const std::string yamlStr = fkyaml::node::serialize(rootNode);
            InOutContents = FSGDynamicTextAssetYamlSerializerInternals::ToFString(yamlStr);
        }
        catch (const fkyaml::exception& e)
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::UpdateFieldsInPlace: fkYAML serialization failed: %s"),
                *FSGDynamicTextAssetYamlSerializerInternals::ToFString(e.what()));
            return false;
        }
    }

    return bAnyUpdated;
}

FString FSGDynamicTextAssetYamlSerializer::GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const
{
    try
    {
        fkyaml::node rootNode = fkyaml::node::mapping();

        // Metadata block
        // Write Asset Type ID GUID to the type field, fall back to class name if unavailable
        FString typeString;
        if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
        {
            const FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(DynamicTextAssetClass);
            if (typeId.IsValid())
            {
                typeString = typeId.ToString();
            }
        }

        if (typeString.IsEmpty())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetYamlSerializer::GetDefaultFileContent: No valid Asset Type ID found for class '%s', falling back to class name"),
                *GetNameSafe(DynamicTextAssetClass));
            typeString = GetNameSafe(DynamicTextAssetClass);
        }

        fkyaml::node metadataNode = fkyaml::node::mapping();
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(typeString));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_VERSION)] =
            fkyaml::node(std::string("1.0.0"));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_ID)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(Id.ToString()));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_USER_FACING_ID)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(UserFacingId));
        metadataNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_FORMAT_VERSION)] =
            fkyaml::node(FSGDynamicTextAssetYamlSerializerInternals::ToStdString(GetFileFormatVersion().ToString()));
        rootNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_FILE_INFORMATION)] = metadataNode;

        // Empty data block
        rootNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_DATA)] = fkyaml::node::mapping();

        const std::string yamlStr = fkyaml::node::serialize(rootNode);
        return FSGDynamicTextAssetYamlSerializerInternals::ToFString(yamlStr);
    }
    catch (const fkyaml::exception& e)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::GetDefaultFileContent: fkYAML exception: %s"),
            *FSGDynamicTextAssetYamlSerializerInternals::ToFString(e.what()));
        return TEXT("");
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::GetDefaultFileContent: Exception: %s"),
            *FSGDynamicTextAssetYamlSerializerInternals::ToFString(e.what()));
        return TEXT("");
    }
}

bool FSGDynamicTextAssetYamlSerializer::ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData) const
{
    OutBundleData.Reset();

    if (InString.IsEmpty())
    {
        return false;
    }

    try
    {
        fkyaml::node rootNode;
        FString parseError;
        if (!FSGDynamicTextAssetYamlSerializerInternals::ParseYaml(InString, rootNode, parseError))
        {
            return false;
        }

        if (!rootNode.is_mapping())
        {
            return false;
        }

        const std::string bundlesKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_SGDT_ASSET_BUNDLES);
        if (!rootNode.contains(bundlesKey))
        {
            return false;
        }

        fkyaml::node& bundlesNode = rootNode[bundlesKey];
        if (!bundlesNode.is_mapping())
        {
            return false;
        }

        for (auto itr = bundlesNode.begin(); itr != bundlesNode.end(); ++itr)
        {
            const FName bundleName = FName(*FSGDynamicTextAssetYamlSerializerInternals::ToFString(itr.key().get_value<std::string>()));

            if (!itr->is_sequence())
            {
                continue;
            }

            FSGDynamicTextAssetBundle& bundle = OutBundleData.Bundles.AddDefaulted_GetRef();
            bundle.BundleName = bundleName;

            for (auto entryItr = itr->begin(); entryItr != itr->end(); ++entryItr)
            {
                if (!entryItr->is_mapping())
                {
                    continue;
                }

                FString propertyName;
                FString pathStr;

                if (entryItr->contains("property") && (*entryItr)["property"].is_string())
                {
                    propertyName = FSGDynamicTextAssetYamlSerializerInternals::ToFString((*entryItr)["property"].get_value<std::string>());
                }
                if (entryItr->contains("path") && (*entryItr)["path"].is_string())
                {
                    pathStr = FSGDynamicTextAssetYamlSerializerInternals::ToFString((*entryItr)["path"].get_value<std::string>());
                }

                if (!propertyName.IsEmpty() && !pathStr.IsEmpty())
                {
                    bundle.Entries.Emplace(FSoftObjectPath(pathStr), FName(*propertyName));
                }
            }
        }

        return OutBundleData.HasBundles();
    }
    catch (const fkyaml::exception&)
    {
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool FSGDynamicTextAssetYamlSerializer::UpdateFileFormatVersion(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& NewVersion) const
{
    // Match fileFormatVersion: X.Y.Z or fileFormatVersion: "X.Y.Z" (quoted or unquoted)
    const FRegexPattern pattern(TEXT("(fileFormatVersion:\\s*)\"?[0-9]+\\.[0-9]+\\.[0-9]+\"?"));
    FRegexMatcher matcher(pattern, InOutFileContents);

    if (matcher.FindNext())
    {
        // Preserve the key and whitespace prefix (capture group 1), replace only the value
        const FString prefix = matcher.GetCaptureGroup(1);
        const FString replacement = prefix + NewVersion.ToString();
        const int32 matchBegin = matcher.GetMatchBeginning();
        const int32 matchEnd = matcher.GetMatchEnding();

        InOutFileContents = InOutFileContents.Left(matchBegin) + replacement + InOutFileContents.Mid(matchEnd);
        return true;
    }

    // Field not found - insert it as a child of the sgFileInformation (or legacy metadata) block
    const FRegexPattern fileInfoPattern(TEXT("(^|\\n)(sgFileInformation|metadata):(\\s*\\n)"));
    FRegexMatcher fileInfoMatcher(fileInfoPattern, InOutFileContents);

    if (fileInfoMatcher.FindNext())
    {
        // Insert right after the "sgFileInformation:\n" line as a child entry
        const int32 insertAt = fileInfoMatcher.GetMatchEnding();
        const FString insertion = FString::Printf(TEXT("  %s: %s\n"),
            *KEY_FILE_FORMAT_VERSION, *NewVersion.ToString());

        InOutFileContents = InOutFileContents.Left(insertAt) + insertion + InOutFileContents.Mid(insertAt);
        return true;
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
        TEXT("FSGDynamicTextAssetYamlSerializer::UpdateFileFormatVersion: Could not find or insert fileFormatVersion field"));
    return false;
}

bool FSGDynamicTextAssetYamlSerializer::MigrateFileFormat(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& CurrentFormatVersion,
    const FSGDynamicTextAssetVersion& TargetFormatVersion) const
{
    if (CurrentFormatVersion == TargetFormatVersion)
    {
        return true;
    }

    // Migration from 1.x to 2.x: rename top-level "metadata:" key to "sgFileInformation:"
    if (CurrentFormatVersion.Major < 2 && TargetFormatVersion.Major >= 2)
    {
        // Match "metadata:" at the start of a line (top-level YAML key)
        const FRegexPattern pattern(TEXT("(^|\\n)(metadata)(:\\s)"));
        FRegexMatcher matcher(pattern, InOutFileContents);

        if (matcher.FindNext())
        {
            const int32 keyStart = matcher.GetMatchBeginning() + (InOutFileContents[matcher.GetMatchBeginning()] == TEXT('\n') ? 1 : 0);
            const int32 keyEnd = keyStart + 8; // length of "metadata"

            InOutFileContents = InOutFileContents.Left(keyStart) + KEY_FILE_INFORMATION + InOutFileContents.Mid(keyEnd);
        }
    }

    return true;
}
