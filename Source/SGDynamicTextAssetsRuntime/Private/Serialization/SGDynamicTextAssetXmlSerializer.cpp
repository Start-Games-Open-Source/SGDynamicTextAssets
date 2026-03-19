// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetXmlSerializer.h"

#include "SGDynamicTextAssetLogs.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "Statics/SGDynamicTextAssetSlateStyles.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
#include "XmlFile.h"
#include "XmlNode.h"

namespace FSGDynamicTextAssetXmlSerializerInternals
{
    /** XML element name for the root wrapper element. */
    static const FString XML_ROOT_TAG = TEXT("DynamicTextAsset");

    /** XML declaration line prepended to every serialized document. */
    static const FString XML_DECLARATION = TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    /** Escapes a string value for safe embedding in XML text content. */
    FString XmlEscape(const FString& Value)
    {
        FString result = Value;
        result.ReplaceInline(TEXT("&"),  TEXT("&amp;"),  ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT("<"),  TEXT("&lt;"),   ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT(">"),  TEXT("&gt;"),   ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT("\""), TEXT("&quot;"), ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT("'"),  TEXT("&apos;"), ESearchCase::CaseSensitive);
        return result;
    }

    /** Unescapes XML text content back to a plain string. */
    FString XmlUnescape(const FString& Value)
    {
        FString result = Value;
        // Order matters: &amp; must be last to avoid double-unescaping
        result.ReplaceInline(TEXT("&lt;"),   TEXT("<"),  ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT("&gt;"),   TEXT(">"),  ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT("&quot;"), TEXT("\""), ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT("&apos;"), TEXT("'"),  ESearchCase::CaseSensitive);
        result.ReplaceInline(TEXT("&amp;"),  TEXT("&"),  ESearchCase::CaseSensitive);
        return result;
    }

    /** Returns a string of spaces for indentation at the given depth (4 spaces per level). */
    FString Indent(int32 Depth)
    {
        return FString::ChrN(Depth * 4, TEXT(' '));
    }

    /**
     * Recursively converts a FJsonValue tree to XML element text and appends it to OutXml.
     *
     * Arrays are represented as repeated <Item> child elements.
     * Objects are represented as named child elements per field.
     * Leaf values (string, number, bool, null) are rendered as text content.
     *
     * @param OutXml  Accumulator string being built
     * @param Tag     XML element name for this value
     * @param Value   The JSON value to render
     * @param Depth   Current indentation depth
     */
    void JsonValueToXmlElement(FString& OutXml, const FString& Tag, const TSharedPtr<FJsonValue>& Value, int32 Depth)
    {
        const FString indent = Indent(Depth);

        if (!Value.IsValid() || Value->Type == EJson::Null)
        {
            OutXml += FString::Printf(TEXT("%s<%s/>\n"), *indent, *Tag);
            return;
        }

        switch (Value->Type)
        {
            case EJson::String:
            {
                OutXml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *indent, *Tag, *XmlEscape(Value->AsString()), *Tag);
                break;
            }
            case EJson::Number:
            {
                // SanitizeFloat trims trailing zeros while preserving enough precision
                OutXml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *indent, *Tag, *FString::SanitizeFloat(Value->AsNumber()), *Tag);
                break;
            }
            case EJson::Boolean:
            {
                OutXml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *indent, *Tag, Value->AsBool() ? TEXT("true") : TEXT("false"), *Tag);
                break;
            }
            case EJson::Object:
            {
                OutXml += FString::Printf(TEXT("%s<%s>\n"), *indent, *Tag);
                for (const TPair<FString, TSharedPtr<FJsonValue>>& field : Value->AsObject()->Values)
                {
                    JsonValueToXmlElement(OutXml, field.Key, field.Value, Depth + 1);
                }
                OutXml += FString::Printf(TEXT("%s</%s>\n"), *indent, *Tag);
                break;
            }
            case EJson::Array:
            {
                OutXml += FString::Printf(TEXT("%s<%s>\n"), *indent, *Tag);
                for (const TSharedPtr<FJsonValue>& item : Value->AsArray())
                {
                    JsonValueToXmlElement(OutXml, TEXT("Item"), item, Depth + 1);
                }
                OutXml += FString::Printf(TEXT("%s</%s>\n"), *indent, *Tag);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    /**
     * Recursively converts an FXmlNode to a FJsonValue, using the property type to
     * produce the correct EJson variant expected by FJsonObjectConverter.
     *
     * @param Node      Source XML node (its text content or children are consumed)
     * @param Property  The UE property descriptor describing the expected type
     * @return The reconstructed FJsonValue, or nullptr on failure
     */
    TSharedPtr<FJsonValue> XmlNodeToJsonValue(const FXmlNode* Node, FProperty* Property)
    {
        if (!Node || !Property)
        {
            return nullptr;
        }

        if (FBoolProperty* boolProp = CastField<FBoolProperty>(Property))
        {
            // FJsonObjectConverter::JsonValueToUProperty requires EJson::Boolean for bool properties
            const bool bValue = Node->GetContent().Equals(TEXT("true"), ESearchCase::IgnoreCase);
            return MakeShared<FJsonValueBoolean>(bValue);
        }
        if (FNumericProperty* numProp = CastField<FNumericProperty>(Property))
        {
            if (numProp->IsEnum())
            {
                // Enum properties expect a string (the enum value name)
                return MakeShared<FJsonValueString>(XmlUnescape(Node->GetContent()));
            }
            // Numeric properties accept EJson::Number
            const double numValue = FCString::Atod(*Node->GetContent());
            return MakeShared<FJsonValueNumber>(numValue);
        }
        if (CastField<FEnumProperty>(Property))
        {
            return MakeShared<FJsonValueString>(XmlUnescape(Node->GetContent()));
        }
        if (CastField<FStrProperty>(Property) ||
            CastField<FNameProperty>(Property) ||
            CastField<FTextProperty>(Property))
        {
            return MakeShared<FJsonValueString>(XmlUnescape(Node->GetContent()));
        }
        if (FObjectPropertyBase* objProp = CastField<FObjectPropertyBase>(Property))
        {
            // Soft object/class references serialize as asset path strings
            return MakeShared<FJsonValueString>(XmlUnescape(Node->GetContent()));
        }
        if (FArrayProperty* arrProp = CastField<FArrayProperty>(Property))
        {
            TArray<TSharedPtr<FJsonValue>> items;
            for (const FXmlNode* child : Node->GetChildrenNodes())
            {
                TSharedPtr<FJsonValue> itemValue = XmlNodeToJsonValue(child, arrProp->Inner);
                if (itemValue.IsValid())
                {
                    items.Add(itemValue);
                }
            }
            return MakeShared<FJsonValueArray>(items);
        }

        if (FSetProperty* setProp = CastField<FSetProperty>(Property))
        {
            TArray<TSharedPtr<FJsonValue>> items;
            for (const FXmlNode* child : Node->GetChildrenNodes())
            {
                TSharedPtr<FJsonValue> itemValue = XmlNodeToJsonValue(child, setProp->ElementProp);
                if (itemValue.IsValid())
                {
                    items.Add(itemValue);
                }
            }
            return MakeShared<FJsonValueArray>(items);
        }
        if (FMapProperty* mapProp = CastField<FMapProperty>(Property))
        {
            // TMap serializes as a JSON object: child element tag is the key, content is the value
            TSharedRef<FJsonObject> obj = MakeShared<FJsonObject>();
            for (const FXmlNode* child : Node->GetChildrenNodes())
            {
                TSharedPtr<FJsonValue> entryValue = XmlNodeToJsonValue(child, mapProp->ValueProp);
                if (entryValue.IsValid())
                {
                    obj->SetField(child->GetTag(), entryValue);
                }
            }
            return MakeShared<FJsonValueObject>(obj);
        }
        if (FStructProperty* structProp = CastField<FStructProperty>(Property))
        {
            // If the struct has custom text serialization, FJsonObjectConverter::UPropertyToJsonValue
            // called ExportTextItem() during serialization and stored the value as plain text content
            // (not child elements). Return FJsonValueString so FJsonObjectConverter::JsonValueToUProperty
            // correctly calls ImportTextItem() on the deserialization side.
            // Example: FSGDynamicTextAssetRef serializes as a bare GUID string, not nested XML children.
            const UScriptStruct::ICppStructOps* CppStructOps = structProp->Struct->GetCppStructOps();
            if (CppStructOps && CppStructOps->HasImportTextItem())
            {
                return MakeShared<FJsonValueString>(XmlUnescape(Node->GetContent()));
            }

            // Generic struct: reconstruct a JSON object from named child elements
            TSharedRef<FJsonObject> obj = MakeShared<FJsonObject>();
            for (TFieldIterator<FProperty> propIt(structProp->Struct); propIt; ++propIt)
            {
                FProperty* fieldProp = *propIt;
                if (const FXmlNode* fieldNode = Node->FindChildNode(fieldProp->GetName()))
                {
                    TSharedPtr<FJsonValue> fieldValue = XmlNodeToJsonValue(fieldNode, fieldProp);
                    if (fieldValue.IsValid())
                    {
                        obj->SetField(fieldProp->GetName(), fieldValue);
                    }
                }
            }
            return MakeShared<FJsonValueObject>(obj);
        }

        // Default fallback: treat as string
        return MakeShared<FJsonValueString>(XmlUnescape(Node->GetContent()));
    }

    /**
     * Converts an XML node representing an instanced sub-object to a FJsonValue.
     *
     * Reads the SG_INST_OBJ_CLASS child to determine the class, then uses the
     * resolved class's property descriptors to convert each child XML node to
     * the correct FJsonValue variant via XmlNodeToJsonValue. Handles nested
     * instanced objects by recursion.
     *
     * @param Node  The XML node containing the instanced object data
     * @return FJsonValueObject with class key and property values, or FJsonValueNull for null objects
     */
    TSharedPtr<FJsonValue> XmlNodeToInstancedObjectJsonValue(const FXmlNode* Node)
    {
        if (!Node)
        {
            return MakeShared<FJsonValueNull>();
        }

        // Self-closing tag or empty node means null sub-object
        const TArray<FXmlNode*>& children = Node->GetChildrenNodes();
        if (children.IsEmpty() && Node->GetContent().IsEmpty())
        {
            return MakeShared<FJsonValueNull>();
        }

        // Read class name from the reserved key child element
        const FXmlNode* classNode = Node->FindChildNode(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
        if (!classNode)
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToInstancedObjectJsonValue: XML node missing '%s' child element"),
                *FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
            return nullptr;
        }

        const FString className = XmlUnescape(classNode->GetContent());
        if (className.IsEmpty())
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToInstancedObjectJsonValue: '%s' element is empty"),
                *FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
            return nullptr;
        }

        // Resolve class to get property type information for correct JSON variant conversion.
        // Uses LoadObject (via ResolveInstancedObjectClass) instead of FindFirstObject to ensure
        // the class can be loaded from disk in packaged builds where it may not yet be in memory.
        UClass* resolvedClass = FSGDynamicTextAssetSerializerBase::ResolveInstancedObjectClass(className);
        if (!resolvedClass)
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                TEXT("FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToInstancedObjectJsonValue: Failed to resolve class '%s'"), *className);
            return nullptr;
        }

        // Build JSON object with class key and typed property values
        TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
        jsonObject->SetStringField(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY, className);

        for (TFieldIterator<FProperty> propIt(resolvedClass); propIt; ++propIt)
        {
            FProperty* subProp = *propIt;
            const FXmlNode* subNode = Node->FindChildNode(subProp->GetName());
            if (!subNode)
            {
                continue;
            }

            // Handle nested single instanced objects recursively
            if (FSGDynamicTextAssetSerializerBase::IsInstancedObjectProperty(subProp))
            {
                if (CastField<FObjectProperty>(subProp))
                {
                    TSharedPtr<FJsonValue> nestedValue = XmlNodeToInstancedObjectJsonValue(subNode);
                    if (nestedValue.IsValid())
                    {
                        jsonObject->SetField(subProp->GetName(), nestedValue);
                    }
                    continue;
                }
            }

            TSharedPtr<FJsonValue> fieldValue = XmlNodeToJsonValue(subNode, subProp);
            if (fieldValue.IsValid())
            {
                jsonObject->SetField(subProp->GetName(), fieldValue);
            }
        }

        return MakeShared<FJsonValueObject>(jsonObject);
    }

}

#if WITH_EDITOR
const FSlateBrush* FSGDynamicTextAssetXmlSerializer::GetIconBrush() const
{
    static const FSlateBrush* icon = FSGDynamicTextAssetSlateStyles::GetBrush(FSGDynamicTextAssetSlateStyles::BRUSH_NAME_XML);
    return icon;
}
#endif

FString FSGDynamicTextAssetXmlSerializer::GetFileExtension() const
{
    static const FString extension = ".dta.xml";
    return extension;
}

FText FSGDynamicTextAssetXmlSerializer::GetFormatName() const
{
    static const FText name = INVTEXT("XML");
    return name;
}

FText FSGDynamicTextAssetXmlSerializer::GetFormatDescription() const
{
#if !UE_BUILD_SHIPPING
    static const FText description = FText::AsCultureInvariant(TEXT(R"(XML serialization for dynamic text assets.

Uses Unreal's built-in XmlParser module (FXmlFile) for reading.
Writes XML strings directly (XmlParser is read-only).

Property values are converted via the FSGDynamicTextAssetSerializerBase
JSON-intermediate helpers, keeping format-specific complexity
contained to the XML-to-FJsonValue bridge.

XML format uses a metadata wrapper block(sgFileInformation), for example:
<?xml version="1.0" encoding="UTF-8"?>
<DynamicTextAsset>
    <sgFileInformation>
        <type>UWeaponData</type>
        <version>1.0.0</version>
        <id>...</id>
        <userfacingid>excalibur</userfacingid>
        <fileFormatVersion>1.5.2</fileFormatVersion>
    </sgFileInformation>
    <data>
        <Damage>50.0</Damage>
    </data>
</DynamicTextAsset>
)"));
    return description;
#else
    return FText::GetEmpty();
#endif
}

uint32 FSGDynamicTextAssetXmlSerializer::GetSerializerTypeId() const
{
    return TYPE_ID;
}

FSGDynamicTextAssetVersion FSGDynamicTextAssetXmlSerializer::GetFileFormatVersion() const
{
    static const FSGDynamicTextAssetVersion version(2, 0, 0);
    return version;
}

bool FSGDynamicTextAssetXmlSerializer::SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const
{
    OutString.Empty();

    if (!Provider)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer: Inputted NULL Provider"));
        return false;
    }
    const UObject* providerObject = Provider->_getUObject();
    if (!providerObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer: Provider is not a valid UObject"));
        return false;
    }

    FString xml;
    xml += FSGDynamicTextAssetXmlSerializerInternals::XML_DECLARATION;
    xml += FString::Printf(TEXT("<%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::XML_ROOT_TAG);

    // Write to metadata
    // Write Asset Type ID GUID to the type element, fall back to class name if unavailable
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
            TEXT("FSGDynamicTextAssetXmlSerializer::SerializeProvider: No valid Asset Type ID found for class '%s', falling back to class name"),
            *providerObject->GetClass()->GetName());
        typeString = providerObject->GetClass()->GetName();
    }

    xml += FString::Printf(TEXT("%s<%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_FILE_INFORMATION);
    xml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_TYPE, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(typeString), *KEY_TYPE);
    xml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_VERSION, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(Provider->GetVersion().ToString()), *KEY_VERSION);
    xml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_ID, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(Provider->GetDynamicTextAssetId().ToString()), *KEY_ID);
    xml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_USER_FACING_ID, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(Provider->GetUserFacingId()), *KEY_USER_FACING_ID);
    xml += FString::Printf(TEXT("%s<%s>%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_FILE_FORMAT_VERSION, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(GetFileFormatVersion().ToString()), *KEY_FILE_FORMAT_VERSION);
    xml += FString::Printf(TEXT("%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_FILE_INFORMATION);

    // Data block
    xml += FString::Printf(TEXT("%s<%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_DATA);

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
                    FSGDynamicTextAssetXmlSerializerInternals::JsonValueToXmlElement(xml, property->GetName(), jsonValue, 2);
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
                    FSGDynamicTextAssetXmlSerializerInternals::JsonValueToXmlElement(xml, property->GetName(), arrayValue, 2);
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
                    FSGDynamicTextAssetXmlSerializerInternals::JsonValueToXmlElement(xml, property->GetName(), arrayValue, 2);
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
                    FSGDynamicTextAssetXmlSerializerInternals::JsonValueToXmlElement(xml, property->GetName(), mapValue, 2);
                    continue;
                }
            }
        }

        TSharedPtr<FJsonValue> jsonValue = SerializePropertyToValue(property, valuePtr);

        if (jsonValue.IsValid())
        {
            FSGDynamicTextAssetXmlSerializerInternals::JsonValueToXmlElement(xml, property->GetName(), jsonValue, 2);
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer: Failed to serialize property(%s) on Provider(%s)"),
                *property->GetName(), *GetNameSafe(providerObject));
        }
    }

    xml += FString::Printf(TEXT("%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_DATA);

    // Extract and serialize asset bundle metadata from soft reference properties
    FSGDynamicTextAssetBundleData bundleData;
    bundleData.ExtractFromObject(providerObject);

    if (bundleData.HasBundles())
    {
        xml += FString::Printf(TEXT("%s<%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_SGDT_ASSET_BUNDLES);

        for (const FSGDynamicTextAssetBundle& bundle : bundleData.Bundles)
        {
            xml += FString::Printf(TEXT("%s<bundle name=\"%s\">\n"),
                *FSGDynamicTextAssetXmlSerializerInternals::Indent(2),
                *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(bundle.BundleName.ToString()));

            for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
            {
                xml += FString::Printf(TEXT("%s<entry property=\"%s\" path=\"%s\"/>\n"),
                    *FSGDynamicTextAssetXmlSerializerInternals::Indent(3),
                    *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(entry.PropertyName.ToString()),
                    *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(entry.AssetPath.ToString()));
            }

            xml += FString::Printf(TEXT("%s</bundle>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(2));
        }

        xml += FString::Printf(TEXT("%s</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_SGDT_ASSET_BUNDLES);
    }

    xml += FString::Printf(TEXT("</%s>\n"), *FSGDynamicTextAssetXmlSerializerInternals::XML_ROOT_TAG);

    OutString = MoveTemp(xml);
    return true;
}

bool FSGDynamicTextAssetXmlSerializer::DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const
{
    bOutMigrated = false;

    if (!OutProvider)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Inputted NULL OutProvider"));
        return false;
    }

    UObject* providerObject = OutProvider->_getUObject();
    if (!providerObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Provider is not a valid UObject"));
        return false;
    }
    if (InString.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Inputted EMPTY InString"));
        return false;
    }
    FXmlFile xmlFile(InString, EConstructMethod::ConstructFromBuffer);
    if (!xmlFile.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Failed to parse XML: %s"), *xmlFile.GetLastError());
        return false;
    }
    const FXmlNode* rootNode = xmlFile.GetRootNode();
    if (!rootNode)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: XML has no root node"));
        return false;
    }
    // Read metadata block
    const FXmlNode* metadataNode = rootNode->FindChildNode(KEY_FILE_INFORMATION);
    if (!metadataNode)
    {
        metadataNode = rootNode->FindChildNode(KEY_METADATA_LEGACY);
    }
    if (!metadataNode)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: XML missing <%s> (or legacy <%s>) block"), *KEY_FILE_INFORMATION, *KEY_METADATA_LEGACY);
        return false;
    }
    // Validate class type element may contain a GUID (new format) or class name (legacy)
    if (const FXmlNode* typeNode = metadataNode->FindChildNode(KEY_TYPE))
    {
        const FString typeFieldValue = FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(typeNode->GetContent());
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
                            TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: XML Asset Type ID(%s) resolves to class(%s) but OutProvider is class(%s)"),
                            *typeFieldValue, *resolvedClass->GetName(), *providerObject->GetClass()->GetName());
                        // Continue anyway, might be loading into a parent class
                    }
                }
                else
                {
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                        TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Could not resolve Asset Type ID(%s) to a class"),
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
                    TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: XML typeName(%s) does not match OutProvider(%s)"),
                    *typeFieldValue, *providerObject->GetClass()->GetName());
                // Continue anyway, might be loading into a parent class
            }
        }
    }
    // Extract and apply ID
    if (const FXmlNode* idNode = metadataNode->FindChildNode(KEY_ID))
    {
        FSGDynamicTextAssetId id;
        if (id.ParseString(FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(idNode->GetContent())))
        {
            OutProvider->SetDynamicTextAssetId(id);
        }
    }
    // Extract and apply UserFacingId
    if (const FXmlNode* userFacingIdNode = metadataNode->FindChildNode(KEY_USER_FACING_ID))
    {
        OutProvider->SetUserFacingId(FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(userFacingIdNode->GetContent()));
    }
    // Extract and apply version
    FSGDynamicTextAssetVersion fileVersion;
    if (const FXmlNode* versionNode = metadataNode->FindChildNode(KEY_VERSION))
    {
        fileVersion = FSGDynamicTextAssetVersion::ParseFromString(FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(versionNode->GetContent()));
        OutProvider->SetVersion(fileVersion);
    }

    // Extract file format version (missing = 1.0.0 for pre-format-version files)
    FSGDynamicTextAssetVersion fileFormatVersion(1, 0, 0);
    if (const FXmlNode* formatVersionNode = metadataNode->FindChildNode(KEY_FILE_FORMAT_VERSION))
    {
        fileFormatVersion = FSGDynamicTextAssetVersion::ParseFromString(
            FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(formatVersionNode->GetContent()));
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
        TEXT("FSGDynamicTextAssetXmlSerializer: File format version: %s (serializer current: %s)"),
        *fileFormatVersion.ToString(), *GetFileFormatVersion().ToString());

    // Check for version migration
    const FSGDynamicTextAssetVersion currentVersion = OutProvider->GetCurrentVersion();
    if (fileVersion.Major < currentVersion.Major)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
            TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Migration required for Provider(%s): file version(%s) -> class version(%s)"),
            *OutProvider->GetDynamicTextAssetId().ToString(), *fileVersion.ToString(), *currentVersion.ToString());

        // Migration passes a null FJsonObject and XML providers must handle that in MigrateFromVersion
        if (!OutProvider->MigrateFromVersion(fileVersion, currentVersion, nullptr))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
                TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Migration failed for Provider(%s) from fileVersion(%s)"),
                *OutProvider->GetDynamicTextAssetId().ToString(), *fileVersion.ToString());
            return false;
        }

        OutProvider->SetVersion(currentVersion);
        bOutMigrated = true;

        UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
            TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Migration succeeded for Provider(%s): version(%s)"),
            *OutProvider->GetDynamicTextAssetId().ToString(), *OutProvider->GetVersion().ToString());
    }
    else if (fileVersion.Major > currentVersion.Major)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Provider(%s) has file major version fileVersion Major(%d) which is newer than class currentVersion Major(%d). Loading with best-effort."),
            *OutProvider->GetDynamicTextAssetId().ToString(), fileVersion.Major, currentVersion.Major);
    }
    // Find data block
    const FXmlNode* dataNode = rootNode->FindChildNode(KEY_DATA);
    if (!dataNode)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: XML missing <%s> block"), *KEY_DATA);
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
        const FXmlNode* propNode = dataNode->FindChildNode(property->GetName());
        if (!propNode)
        {
            // Missing optional field
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
                TSharedPtr<FJsonValue> jsonValue = FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToInstancedObjectJsonValue(propNode);
                if (!DeserializeValueToInstancedObject(jsonValue, objectProp, valuePtr, providerObject))
                {
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                        TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Failed to deserialize instanced property(%s) on OutProvider(%s)"),
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
                    const TArray<FXmlNode*>& childNodes = propNode->GetChildrenNodes();
                    if (childNodes.IsEmpty())
                    {
                        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                            TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Instanced array property '%s' on OutProvider(%s) has no child XML nodes"),
                            *property->GetName(), *GetNameSafe(providerObject));
                    }
                    FScriptArrayHelper arrayHelper(arrayProp, valuePtr);
                    arrayHelper.Resize(childNodes.Num());

                    for (int32 i = 0; i < childNodes.Num(); ++i)
                    {
                        TSharedPtr<FJsonValue> elemValue = FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToInstancedObjectJsonValue(childNodes[i]);
                        if (!DeserializeValueToInstancedObject(elemValue, innerObjProp, arrayHelper.GetRawPtr(i), providerObject))
                        {
                            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Failed to deserialize instanced array element [%d] of property(%s) on OutProvider(%s)"),
                                i, *property->GetName(), *GetNameSafe(providerObject));
                        }
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
                    const TArray<FXmlNode*>& childNodes = propNode->GetChildrenNodes();
                    FScriptSetHelper setHelper(setProp, valuePtr);
                    setHelper.EmptyElements();

                    for (int32 i = 0; i < childNodes.Num(); ++i)
                    {
                        const int32 newIndex = setHelper.AddDefaultValue_Invalid_NeedsRehash();
                        TSharedPtr<FJsonValue> elemValue = FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToInstancedObjectJsonValue(childNodes[i]);
                        if (!DeserializeValueToInstancedObject(elemValue, elemObjProp, setHelper.GetElementPtr(newIndex), providerObject))
                        {
                            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Failed to deserialize instanced set element [%d] of property(%s) on OutProvider(%s)"),
                                i, *property->GetName(), *GetNameSafe(providerObject));
                        }
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
                    FScriptMapHelper mapHelper(mapProp, valuePtr);
                    mapHelper.EmptyValues();

                    for (const FXmlNode* child : propNode->GetChildrenNodes())
                    {
                        const int32 newIndex = mapHelper.AddDefaultValue_Invalid_NeedsRehash();
                        uint8* keyPtr = mapHelper.GetKeyPtr(newIndex);
                        mapProp->KeyProp->ImportText_Direct(*child->GetTag(), keyPtr, nullptr, PPF_None);

                        uint8* valPtr = mapHelper.GetValuePtr(newIndex);
                        TSharedPtr<FJsonValue> elemValue = FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToInstancedObjectJsonValue(child);
                        if (!DeserializeValueToInstancedObject(elemValue, valueObjProp, valPtr, providerObject))
                        {
                            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Failed to deserialize instanced map value for key '%s' of property(%s)"),
                                *child->GetTag(), *property->GetName());
                        }
                    }

                    mapHelper.Rehash();
                    continue;
                }
            }
        }

        TSharedPtr<FJsonValue> jsonValue = FSGDynamicTextAssetXmlSerializerInternals::XmlNodeToJsonValue(propNode, property);
        if (jsonValue.IsValid())
        {
            if (!DeserializeValueToProperty(jsonValue, property, valuePtr))
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("FSGDynamicTextAssetXmlSerializer::DeserializeProvider: Failed to deserialize property(%s) on OutProvider(%s)"),
                    *property->GetName(), *GetNameSafe(providerObject));
            }
        }
    }

    return true;
}

bool FSGDynamicTextAssetXmlSerializer::ValidateStructure(const FString& InString, FString& OutErrorMessage) const
{
    if (InString.IsEmpty())
    {
        OutErrorMessage = TEXT("Inputted EMPTY string");
        return false;
    }
    FXmlFile xmlFile(InString, EConstructMethod::ConstructFromBuffer);
    if (!xmlFile.IsValid())
    {
        OutErrorMessage = FString::Printf(TEXT("Failed to parse XML: %s"), *xmlFile.GetLastError());
        return false;
    }
    const FXmlNode* rootNode = xmlFile.GetRootNode();
    if (!rootNode)
    {
        OutErrorMessage = TEXT("XML has no root node");
        return false;
    }
    const FXmlNode* metadataNode = rootNode->FindChildNode(KEY_FILE_INFORMATION);
    if (!metadataNode)
    {
        metadataNode = rootNode->FindChildNode(KEY_METADATA_LEGACY);
    }
    if (!metadataNode)
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block <%s> (or legacy <%s>)"), *KEY_FILE_INFORMATION, *KEY_METADATA_LEGACY);
        return false;
    }
    const FXmlNode* typeNode = metadataNode->FindChildNode(KEY_TYPE);
    if (!typeNode || typeNode->GetContent().IsEmpty())
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required field <%s> inside <%s>"), *KEY_TYPE, *KEY_FILE_INFORMATION);
        return false;
    }
    // If fileFormatVersion is present, validate it is a parseable version string
    const FXmlNode* formatVersionNode = metadataNode->FindChildNode(KEY_FILE_FORMAT_VERSION);
    if (formatVersionNode && !formatVersionNode->GetContent().IsEmpty())
    {
        FSGDynamicTextAssetVersion parsedVersion = FSGDynamicTextAssetVersion::ParseFromString(
            FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(formatVersionNode->GetContent()));
        if (!parsedVersion.IsValid())
        {
            OutErrorMessage = FString::Printf(
                TEXT("Field <%s> has invalid version string: %s"),
                *KEY_FILE_FORMAT_VERSION, *formatVersionNode->GetContent());
            return false;
        }
    }

    const FXmlNode* dataNode = rootNode->FindChildNode(KEY_DATA);
    if (!dataNode)
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block <%s>"), *KEY_DATA);
        return false;
    }

    return true;
}

bool FSGDynamicTextAssetXmlSerializer::ExtractMetadata(const FString& InString, FSGDynamicTextAssetFileMetadata& OutMetadata) const
{
    OutMetadata = FSGDynamicTextAssetFileMetadata();
    OutMetadata.SerializerTypeId = TYPE_ID;

    FXmlFile xmlFile(InString, EConstructMethod::ConstructFromBuffer);
    if (!xmlFile.IsValid())
    {
        return false;
    }
    const FXmlNode* rootNode = xmlFile.GetRootNode();
    if (!rootNode)
    {
        return false;
    }
    const FXmlNode* metadataNode = rootNode->FindChildNode(KEY_FILE_INFORMATION);
    if (!metadataNode)
    {
        metadataNode = rootNode->FindChildNode(KEY_METADATA_LEGACY);
    }
    if (!metadataNode)
    {
        return false;
    }

    // Type element may contain a GUID (new format) or class name (legacy)
    if (const FXmlNode* typeNode = metadataNode->FindChildNode(KEY_TYPE))
    {
        const FString typeFieldValue = FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(typeNode->GetContent());
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

    if (const FXmlNode* idNode = metadataNode->FindChildNode(KEY_ID))
    {
        OutMetadata.Id.ParseString(FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(idNode->GetContent()));
    }
    if (const FXmlNode* versionNode = metadataNode->FindChildNode(KEY_VERSION))
    {
        OutMetadata.Version = FSGDynamicTextAssetVersion::ParseFromString(
            FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(versionNode->GetContent()));
    }
    if (const FXmlNode* userFacingIdNode = metadataNode->FindChildNode(KEY_USER_FACING_ID))
    {
        OutMetadata.UserFacingId = FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(userFacingIdNode->GetContent());
    }
    if (const FXmlNode* formatVersionNode = metadataNode->FindChildNode(KEY_FILE_FORMAT_VERSION))
    {
        OutMetadata.FileFormatVersion = FSGDynamicTextAssetVersion::ParseFromString(
            FSGDynamicTextAssetXmlSerializerInternals::XmlUnescape(formatVersionNode->GetContent()));
    }

    OutMetadata.bIsValid = OutMetadata.AssetTypeId.IsValid() || !OutMetadata.ClassName.IsEmpty();
    return OutMetadata.bIsValid;
}

bool FSGDynamicTextAssetXmlSerializer::UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const
{
    if (InOutContents.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::UpdateFieldsInPlace: Inputted EMPTY InOutContents"));
        return false;
    }

    if (FieldUpdates.IsEmpty())
    {
        return false;
    }

    // Validate the XML is well-formed before attempting updates
    FString errorMessage;
    if (!ValidateStructure(InOutContents, errorMessage))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetXmlSerializer::UpdateFieldsInPlace: Invalid XML: %s"), *errorMessage);
        return false;
    }

    // Use targeted regex replacement on the metadata block for each field.
    // This avoids round-tripping through a full parse+rebuild which would lose
    // formatting. The metadata elements are simple scalar leaf nodes with no
    // nested content, so regex replacement is safe and exact.
    bool bAnyUpdated = false;
    for (const TPair<FString, FString>& update : FieldUpdates)
    {
        // Build pattern: <tagname>any text</tagname>
        // [^<]* matches any character that is not '<', stopping at the closing tag
        const FString pattern = FString::Printf(TEXT("(<%s>)[^<]*(</\\1>)"), *update.Key);

        // Build replacement with the escaped new value
        const FString replacement = FString::Printf(TEXT("<%s>%s</%s>"), *update.Key, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(update.Value), *update.Key);

        // Only replace if the tag is actually present
        if (InOutContents.Contains(FString::Printf(TEXT("<%s>"), *update.Key)))
        {
            // Use non-regex simple replacement to find the exact pattern:
            // find <key>OLD</key> and replace with <key>NEW</key>.
            // This is safe because metadata values are leaf text nodes with no children.
            const FXmlFile xmlFile(InOutContents, EConstructMethod::ConstructFromBuffer);
            if (xmlFile.IsValid())
            {
                const FXmlNode* rootNode = xmlFile.GetRootNode();
                const FXmlNode* metadataNode = rootNode ? rootNode->FindChildNode(KEY_FILE_INFORMATION) : nullptr;
                if (!metadataNode && rootNode)
                {
                    metadataNode = rootNode->FindChildNode(KEY_METADATA_LEGACY);
                }
                const FXmlNode* fieldNode = metadataNode ? metadataNode->FindChildNode(update.Key) : nullptr;
                if (fieldNode)
                {
                    // Build the exact old and new element strings
                    const FString oldElement = FString::Printf(TEXT("<%s>%s</%s>"), *update.Key, *fieldNode->GetContent(), *update.Key);
                    const FString newElement = FString::Printf(TEXT("<%s>%s</%s>"), *update.Key, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(update.Value), *update.Key);

                    if (InOutContents.Contains(oldElement))
                    {
                        InOutContents.ReplaceInline(*oldElement, *newElement, ESearchCase::CaseSensitive);
                        bAnyUpdated = true;
                    }
                }
            }
        }
    }

    return bAnyUpdated;
}

FString FSGDynamicTextAssetXmlSerializer::GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const
{
    // Write Asset Type ID GUID to the type element, fall back to class name if unavailable
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
            TEXT("FSGDynamicTextAssetXmlSerializer::GetDefaultFileContent: No valid Asset Type ID found for class '%s', falling back to class name"),
            *GetNameSafe(DynamicTextAssetClass));
        typeString = GetNameSafe(DynamicTextAssetClass);
    }

    return FString::Printf(
        TEXT("%s<%s>\n%s<%s>\n%s<%s>%s</%s>\n%s<%s>1.0.0</%s>\n%s<%s>%s</%s>\n%s<%s>%s</%s>\n%s<%s>%s</%s>\n%s</%s>\n%s<%s/>\n</%s>\n"),
        *FSGDynamicTextAssetXmlSerializerInternals::XML_DECLARATION,
        *FSGDynamicTextAssetXmlSerializerInternals::XML_ROOT_TAG,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_FILE_INFORMATION,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_TYPE, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(typeString), *KEY_TYPE,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_VERSION, *KEY_VERSION,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_ID, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(Id.ToString()), *KEY_ID,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_USER_FACING_ID, *FSGDynamicTextAssetXmlSerializerInternals::XmlEscape(UserFacingId), *KEY_USER_FACING_ID,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(2), *KEY_FILE_FORMAT_VERSION, *GetFileFormatVersion().ToString(), *KEY_FILE_FORMAT_VERSION,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_FILE_INFORMATION,
        *FSGDynamicTextAssetXmlSerializerInternals::Indent(1), *KEY_DATA,
        *FSGDynamicTextAssetXmlSerializerInternals::XML_ROOT_TAG
    );
}

bool FSGDynamicTextAssetXmlSerializer::ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData) const
{
    OutBundleData.Reset();

    if (InString.IsEmpty())
    {
        return false;
    }

    FXmlFile xmlFile(InString, EConstructMethod::ConstructFromBuffer);
    if (!xmlFile.IsValid())
    {
        return false;
    }

    const FXmlNode* rootNode = xmlFile.GetRootNode();
    if (!rootNode)
    {
        return false;
    }

    const FXmlNode* bundlesNode = rootNode->FindChildNode(KEY_SGDT_ASSET_BUNDLES);
    if (!bundlesNode)
    {
        return false;
    }

    for (const FXmlNode* bundleNode : bundlesNode->GetChildrenNodes())
    {
        if (bundleNode->GetTag() != TEXT("bundle"))
        {
            continue;
        }

        const FString bundleNameStr = bundleNode->GetAttribute(TEXT("name"));
        if (bundleNameStr.IsEmpty())
        {
            continue;
        }

        const FName bundleName = FName(*bundleNameStr);

        FSGDynamicTextAssetBundle& bundle = OutBundleData.Bundles.AddDefaulted_GetRef();
        bundle.BundleName = bundleName;

        for (const FXmlNode* entryNode : bundleNode->GetChildrenNodes())
        {
            if (entryNode->GetTag() != TEXT("entry"))
            {
                continue;
            }

            const FString propertyName = entryNode->GetAttribute(TEXT("property"));
            const FString pathStr = entryNode->GetAttribute(TEXT("path"));

            if (!propertyName.IsEmpty() && !pathStr.IsEmpty())
            {
                bundle.Entries.Emplace(FSoftObjectPath(pathStr), FName(*propertyName));
            }
        }
    }

    return OutBundleData.HasBundles();
}

bool FSGDynamicTextAssetXmlSerializer::UpdateFileFormatVersion(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& NewVersion) const
{
    // Match <fileFormatVersion>X.Y.Z</fileFormatVersion> with flexible whitespace
    const FRegexPattern pattern(TEXT("<fileFormatVersion>\\s*[0-9]+\\.[0-9]+\\.[0-9]+\\s*</fileFormatVersion>"));
    FRegexMatcher matcher(pattern, InOutFileContents);

    if (matcher.FindNext())
    {
        const FString replacement = FString::Printf(TEXT("<fileFormatVersion>%s</fileFormatVersion>"), *NewVersion.ToString());
        const int32 matchBegin = matcher.GetMatchBeginning();
        const int32 matchEnd = matcher.GetMatchEnding();

        InOutFileContents = InOutFileContents.Left(matchBegin) + replacement + InOutFileContents.Mid(matchEnd);
        return true;
    }

    // Field not found - insert it before the closing tag of the file information block
    const FString closingNew = FString::Printf(TEXT("</%s>"), *KEY_FILE_INFORMATION);
    const FString closingLegacy = FString::Printf(TEXT("</%s>"), *KEY_METADATA_LEGACY);

    int32 insertPos = InOutFileContents.Find(closingNew);
    if (insertPos == INDEX_NONE)
    {
        insertPos = InOutFileContents.Find(closingLegacy);
    }

    if (insertPos != INDEX_NONE)
    {
        // Walk back from the closing tag to the start of its line so the new
        // element is inserted on its own line with correct depth-2 indentation
        // instead of appending to the existing leading whitespace.
        int32 lineStart = insertPos;
        while (lineStart > 0 && InOutFileContents[lineStart - 1] != TEXT('\n'))
        {
            lineStart--;
        }

        const FString insertion = FString::Printf(TEXT("%s<%s>%s</%s>\n"),
            *FSGDynamicTextAssetXmlSerializerInternals::Indent(2),
            *KEY_FILE_FORMAT_VERSION, *NewVersion.ToString(), *KEY_FILE_FORMAT_VERSION);

        InOutFileContents = InOutFileContents.Left(lineStart) + insertion + InOutFileContents.Mid(lineStart);
        return true;
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
        TEXT("FSGDynamicTextAssetXmlSerializer::UpdateFileFormatVersion: Could not find or insert fileFormatVersion element"));
    return false;
}

bool FSGDynamicTextAssetXmlSerializer::MigrateFileFormat(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& CurrentFormatVersion,
    const FSGDynamicTextAssetVersion& TargetFormatVersion) const
{
    if (CurrentFormatVersion == TargetFormatVersion)
    {
        return true;
    }

    // Migration from 1.x to 2.x: rename <metadata> tags to <sgFileInformation>
    if (CurrentFormatVersion.Major < 2 && TargetFormatVersion.Major >= 2)
    {
        InOutFileContents.ReplaceInline(TEXT("<metadata>"), *FString::Printf(TEXT("<%s>"), *KEY_FILE_INFORMATION));
        InOutFileContents.ReplaceInline(TEXT("</metadata>"), *FString::Printf(TEXT("</%s>"), *KEY_FILE_INFORMATION));
    }

    return true;
}
