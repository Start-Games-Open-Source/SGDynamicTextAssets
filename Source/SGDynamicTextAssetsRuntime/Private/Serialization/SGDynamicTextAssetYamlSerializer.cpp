// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetYamlSerializer.h"

#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "SGDynamicTextAssetsRuntimeModule.h"
#include "Statics/SGDynamicTextAssetSlateStyles.h"
#include "UObject/TextProperty.h"

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

YAML format uses a metadata wrapper block, for example:
metadata:
  type: UWeaponData
  version: 1.0.0
  id: 550E8400-E29B-41D4-A716-446655440000
  userfacingid: excalibur
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

        // Metadata block — write Asset Type ID GUID to the type field, fall back to class name if unavailable
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
        rootNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA)] = metadataNode;

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

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA);
    const std::string dataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_DATA);
    const std::string typeKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE);
    const std::string versionKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_VERSION);
    const std::string idKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_ID);
    const std::string userFacingIdKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_USER_FACING_ID);

    // Read metadata block
    if (!rootNode.contains(metadataKey))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: YAML missing '%s' block"), *KEY_METADATA);
        return false;
    }
    const fkyaml::node& metadataNode = rootNode[metadataKey];
    if (!metadataNode.is_mapping())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer: '%s' block is not a mapping"), *KEY_METADATA);
        return false;
    }

    // Validate class type — type field may contain a GUID (new format) or class name (legacy)
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

        const std::string propName = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(property->GetName());
        if (!dataNode.contains(propName))
        {
            // Missing optional field - tolerated gracefully
            continue;
        }

        TSharedPtr<FJsonValue> jsonValue = FSGDynamicTextAssetYamlSerializerInternals::YamlNodeToJsonValue(dataNode[propName], property);
        if (jsonValue.IsValid())
        {
            void* valuePtr = property->ContainerPtrToValuePtr<void>(providerObject);
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

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA);
    if (!rootNode.contains(metadataKey))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block '%s'"), *KEY_METADATA);
        return false;
    }

    const fkyaml::node& metadataNode = rootNode[metadataKey];
    if (!metadataNode.is_mapping())
    {
        OutErrorMessage = FString::Printf(TEXT("'%s' block is not a mapping"), *KEY_METADATA);
        return false;
    }

    const std::string typeKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE);
    if (!metadataNode.contains(typeKey))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required field '%s' inside '%s'"), *KEY_TYPE, *KEY_METADATA);
        return false;
    }

    const fkyaml::node& typeNode = metadataNode[typeKey];
    if (!typeNode.is_string() || typeNode.get_value<std::string>().empty())
    {
        OutErrorMessage = FString::Printf(TEXT("Field '%s' inside '%s' is empty or not a string"), *KEY_TYPE, *KEY_METADATA);
        return false;
    }

    const std::string dataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_DATA);
    if (!rootNode.contains(dataKey))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block '%s'"), *KEY_DATA);
        return false;
    }

    return true;
}

bool FSGDynamicTextAssetYamlSerializer::ExtractMetadata(const FString& InString, FSGDynamicTextAssetId& OutId, FString& OutClassName, FString& OutUserFacingId, FString& OutVersion, FSGDynamicTextAssetTypeId& OutAssetTypeId) const
{
    OutAssetTypeId.Invalidate();
    OutClassName.Empty();

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

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA);
    if (!rootNode.contains(metadataKey))
    {
        return false;
    }

    const fkyaml::node& metadataNode = rootNode[metadataKey];
    if (!metadataNode.is_mapping())
    {
        return false;
    }

    const std::string typeKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_TYPE);
    const std::string idKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_ID);
    const std::string versionKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_VERSION);
    const std::string userFacingIdKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_USER_FACING_ID);

    // Type field may contain a GUID (new format) or class name (legacy)
    if (metadataNode.contains(typeKey) && metadataNode[typeKey].is_string())
    {
        const FString typeFieldValue = FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[typeKey].get_value<std::string>());
        FSGDynamicTextAssetTypeId parsedTypeId = FSGDynamicTextAssetTypeId::FromString(typeFieldValue);
        if (parsedTypeId.IsValid())
        {
            // New format: store TypeId and resolve class name from registry
            OutAssetTypeId = parsedTypeId;

            if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                if (const UClass* resolvedClass = registry->ResolveClassForTypeId(parsedTypeId))
                {
                    OutClassName = resolvedClass->GetName();
                }
            }
        }
        else
        {
            // Legacy format: treat value as class name directly
            OutClassName = typeFieldValue;
        }
    }

    if (metadataNode.contains(idKey) && metadataNode[idKey].is_string())
    {
        OutId.ParseString(FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[idKey].get_value<std::string>()));
    }
    if (metadataNode.contains(versionKey) && metadataNode[versionKey].is_string())
    {
        OutVersion = FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[versionKey].get_value<std::string>());
    }
    if (metadataNode.contains(userFacingIdKey) && metadataNode[userFacingIdKey].is_string())
    {
        OutUserFacingId = FSGDynamicTextAssetYamlSerializerInternals::ToFString(metadataNode[userFacingIdKey].get_value<std::string>());
    }

    return OutAssetTypeId.IsValid() || !OutClassName.IsEmpty();
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

    const std::string metadataKey = FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA);
    if (!rootNode.contains(metadataKey) || !rootNode[metadataKey].is_mapping())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetYamlSerializer::UpdateFieldsInPlace: Missing or invalid '%s' block"), *KEY_METADATA);
        return false;
    }

    fkyaml::node& metadataNode = rootNode[metadataKey];
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

        // Metadata block — write Asset Type ID GUID to the type field, fall back to class name if unavailable
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
        rootNode[FSGDynamicTextAssetYamlSerializerInternals::ToStdString(KEY_METADATA)] = metadataNode;

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
