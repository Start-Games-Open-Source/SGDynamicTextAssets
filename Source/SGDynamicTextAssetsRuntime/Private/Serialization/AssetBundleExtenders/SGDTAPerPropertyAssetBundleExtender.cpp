// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/AssetBundleExtenders/SGDTAPerPropertyAssetBundleExtender.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/SGDTASerializer.h"
#include "Serialization/SGDTASerializerBase.h"
#include "SGDynamicTextAssetLogs.h"
#include "Statics/SGDTAConstants.h"
#include "XmlFile.h"

THIRD_PARTY_INCLUDES_START
#include <fkYAML/node.hpp>
THIRD_PARTY_INCLUDES_END

namespace SGDTAPerPropertyExtenderKeys
{
	static const FString KEY_VALUE = TEXT("value");
	static const FString KEY_ASSET_BUNDLES = TEXT("assetBundles");
	static const FString KEY_ITEM = TEXT("Item");
}

namespace SGDTAPerPropertyExtenderInternals
{
	FString XmlIndent(int32 Depth)
	{
		return FString::ChrN(Depth * 4, TEXT(' '));
	}

	FString XmlEscape(const FString& Value)
	{
		FString result = Value;
		result.ReplaceInline(TEXT("&"), TEXT("&amp;"), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT("<"), TEXT("&lt;"), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT(">"), TEXT("&gt;"), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT("\""), TEXT("&quot;"), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT("'"), TEXT("&apos;"), ESearchCase::CaseSensitive);
		return result;
	}

	/** Recursively writes an FXmlNode subtree to an XML string. */
	void XmlNodeToString(const FXmlNode* Node, FString& OutXml, int32 Depth)
	{
		if (!Node)
		{
			return;
		}

		const FString& tag = Node->GetTag();
		const TArray<FXmlNode*>& children = Node->GetChildrenNodes();
		const FString& content = Node->GetContent();

		OutXml += FString::Printf(TEXT("%s<%s"), *XmlIndent(Depth), *tag);

		// Write attributes
		for (const FXmlAttribute& attr : Node->GetAttributes())
		{
			OutXml += FString::Printf(TEXT(" %s=\"%s\""), *attr.GetTag(), *XmlEscape(attr.GetValue()));
		}

		if (children.Num() > 0)
		{
			OutXml += TEXT(">\n");
			for (const FXmlNode* child : children)
			{
				XmlNodeToString(child, OutXml, Depth + 1);
			}
			OutXml += FString::Printf(TEXT("%s</%s>\n"), *XmlIndent(Depth), *tag);
		}
		else if (!content.IsEmpty())
		{
			OutXml += FString::Printf(TEXT(">%s</%s>\n"), *XmlEscape(content), *tag);
		}
		else
		{
			OutXml += TEXT("/>\n");
		}
	}

	std::string ToStdString(const FString& InString)
	{
		const auto utf8 = StringCast<UTF8CHAR>(*InString);
		return std::string(reinterpret_cast<const char*>(utf8.Get()), utf8.Length());
	}

	FString ToFString(const std::string& InString)
	{
		return FString(UTF8_TO_TCHAR(InString.c_str()));
	}
}

USGDTAPerPropertyAssetBundleExtender::FClassPropertyBundlesMap
USGDTAPerPropertyAssetBundleExtender::BuildPropertyToBundlesMap(
	const FSGDynamicTextAssetBundleData& BundleData)
{
	FClassPropertyBundlesMap classPropertyBundles;

	for (const FSGDynamicTextAssetBundle& bundle : BundleData.Bundles)
	{
		for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
		{
			const FString qualifiedName = entry.PropertyName.ToString();

			FString className;
			FString propertyName;

			int32 dotIndex = INDEX_NONE;
			if (qualifiedName.FindLastChar(TEXT('.'), dotIndex))
			{
				className = qualifiedName.Left(dotIndex);
				propertyName = qualifiedName.Mid(dotIndex + 1);
			}
			else
			{
				propertyName = qualifiedName;
			}

			classPropertyBundles.FindOrAdd(className).FindOrAdd(propertyName).AddUnique(
				bundle.BundleName.ToString());
		}
	}

	return classPropertyBundles;
}

FString USGDTAPerPropertyAssetBundleExtender::DetermineRootClassName(
	const TSharedPtr<FJsonObject>& DataObject,
	const FClassPropertyBundlesMap& ClassPropertyBundles)
{
	FString bestClassName;
	int32 bestMatchCount = -1;

	for (const TPair<FString, TMap<FString, TArray<FString>>>& classPair : ClassPropertyBundles)
	{
		int32 matchCount = 0;
		for (const TPair<FString, TArray<FString>>& propPair : classPair.Value)
		{
			if (DataObject->HasField(propPair.Key))
			{
				matchCount++;
			}
		}

		if (matchCount > bestMatchCount)
		{
			bestMatchCount = matchCount;
			bestClassName = classPair.Key;
		}
	}

	return bestClassName;
}

FString USGDTAPerPropertyAssetBundleExtender::ExtractClassNameFromMarker(
	const FString& MarkerValue, bool bIsClassPath)
{
	if (!bIsClassPath)
	{
		return MarkerValue;
	}

	// SG_INST_OBJ_CLASS stores full paths like "/Game/BP_Test.BP_Test_C"
	// Extract the last segment after the final dot
	int32 dotIndex = INDEX_NONE;
	if (MarkerValue.FindLastChar(TEXT('.'), dotIndex))
	{
		return MarkerValue.Mid(dotIndex + 1);
	}
	return MarkerValue;
}

void USGDTAPerPropertyAssetBundleExtender::WrapBundledPropertiesRecursive(
	const TSharedPtr<FJsonObject>& JsonObject,
	const FString& ClassName,
	const FClassPropertyBundlesMap& ClassPropertyBundles)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	// Wrap properties that belong to this class context
	const TMap<FString, TArray<FString>>* propertyMap = ClassPropertyBundles.Find(ClassName);
	if (propertyMap)
	{
		for (const TPair<FString, TArray<FString>>& propPair : *propertyMap)
		{
			const FString& propertyKey = propPair.Key;
			const TArray<FString>& bundleNames = propPair.Value;

			if (!JsonObject->HasField(propertyKey))
			{
				continue;
			}

			TSharedPtr<FJsonValue> originalValue = JsonObject->TryGetField(propertyKey);

			TSharedRef<FJsonObject> wrapperObject = MakeShared<FJsonObject>();
			wrapperObject->SetField(SGDTAPerPropertyExtenderKeys::KEY_VALUE, originalValue);

			TArray<TSharedPtr<FJsonValue>> bundleArray;
			bundleArray.Reserve(bundleNames.Num());
			for (const FString& bundleName : bundleNames)
			{
				bundleArray.Add(MakeShared<FJsonValueString>(bundleName));
			}
			wrapperObject->SetArrayField(SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES, bundleArray);

			JsonObject->SetObjectField(propertyKey, wrapperObject);
		}
	}

	// Collect fields to recurse into (snapshot keys since wrapping may have modified the object)
	TArray<FString> fieldKeys;
	JsonObject->Values.GetKeys(fieldKeys);

	for (const FString& fieldKey : fieldKeys)
	{
		TSharedPtr<FJsonValue> fieldValue = JsonObject->TryGetField(fieldKey);
		if (!fieldValue.IsValid())
		{
			continue;
		}

		// Check JSON objects for type markers
		if (fieldValue->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject>& fieldObject = fieldValue->AsObject();
			if (!fieldObject.IsValid())
			{
				continue;
			}

			// Skip wrapped properties (they have "value" + "assetBundles")
			if (fieldObject->HasField(SGDTAPerPropertyExtenderKeys::KEY_VALUE) &&
				fieldObject->HasField(SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES))
			{
				continue;
			}

			FString nestedClassName;
			FString classMarkerValue;

			if (fieldObject->TryGetStringField(
					FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY, classMarkerValue))
			{
				nestedClassName = ExtractClassNameFromMarker(classMarkerValue, true);
			}
			else if (fieldObject->TryGetStringField(
					FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY, classMarkerValue))
			{
				nestedClassName = ExtractClassNameFromMarker(classMarkerValue, false);
			}
			else if (fieldObject->TryGetStringField(
					FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY, classMarkerValue))
			{
				nestedClassName = ExtractClassNameFromMarker(classMarkerValue, false);
			}

			if (!nestedClassName.IsEmpty())
			{
				WrapBundledPropertiesRecursive(fieldObject, nestedClassName, ClassPropertyBundles);
			}
		}
		else if (fieldValue->Type == EJson::Array)
		{
			// Recurse into array elements that have type markers
			const TArray<TSharedPtr<FJsonValue>>& jsonArray = fieldValue->AsArray();
			for (const TSharedPtr<FJsonValue>& element : jsonArray)
			{
				if (!element.IsValid() || element->Type != EJson::Object)
				{
					continue;
				}

				const TSharedPtr<FJsonObject>& elemObject = element->AsObject();
				if (!elemObject.IsValid())
				{
					continue;
				}

				FString elemClassName;
				FString elemMarkerValue;

				if (elemObject->TryGetStringField(
						FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY, elemMarkerValue))
				{
					elemClassName = ExtractClassNameFromMarker(elemMarkerValue, true);
				}
				else if (elemObject->TryGetStringField(
						FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY, elemMarkerValue))
				{
					elemClassName = ExtractClassNameFromMarker(elemMarkerValue, false);
				}
				else if (elemObject->TryGetStringField(
						FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY, elemMarkerValue))
				{
					elemClassName = ExtractClassNameFromMarker(elemMarkerValue, false);
				}

				if (!elemClassName.IsEmpty())
				{
					WrapBundledPropertiesRecursive(elemObject, elemClassName, ClassPropertyBundles);
				}
			}
		}
	}
}

void USGDTAPerPropertyAssetBundleExtender::ExtractBundlesFromJsonObjectRecursive(
	const TSharedPtr<FJsonObject>& JsonObject,
	const FString& ClassName,
	FSGDynamicTextAssetBundleData& OutBundleData)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	// Collect unwrap operations to apply after iteration (cannot modify Values during iteration)
	TArray<TPair<FString, TSharedPtr<FJsonValue>>> unwrapOps;

	for (const TPair<FString, TSharedPtr<FJsonValue>>& field : JsonObject->Values)
	{
		const FString& fieldKey = field.Key;
		const TSharedPtr<FJsonValue>& fieldValue = field.Value;

		if (!fieldValue.IsValid())
		{
			continue;
		}

		if (fieldValue->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject>& fieldObject = fieldValue->AsObject();
			if (!fieldObject.IsValid())
			{
				continue;
			}

			// Check if this is a wrapped property (has "value" + "assetBundles")
			if (fieldObject->HasField(SGDTAPerPropertyExtenderKeys::KEY_VALUE) &&
				fieldObject->HasField(SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES))
			{
				const TArray<TSharedPtr<FJsonValue>>* bundleArray = nullptr;
				if (!fieldObject->TryGetArrayField(
						SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES, bundleArray) || !bundleArray)
				{
					continue;
				}

				TSharedPtr<FJsonValue> valueField = fieldObject->TryGetField(
					SGDTAPerPropertyExtenderKeys::KEY_VALUE);

				FString assetPath;
				if (valueField.IsValid() && valueField->Type == EJson::String)
				{
					assetPath = valueField->AsString();
				}

				FName qualifiedName;
				if (!ClassName.IsEmpty())
				{
					qualifiedName = FName(*FString::Printf(TEXT("%s.%s"), *ClassName, *fieldKey));
				}
				else
				{
					qualifiedName = FName(*fieldKey);
				}

				for (const TSharedPtr<FJsonValue>& bundleValue : *bundleArray)
				{
					if (!bundleValue.IsValid() || bundleValue->Type != EJson::String)
					{
						continue;
					}

					const FName bundleName = FName(*bundleValue->AsString());
					FSGDynamicTextAssetBundle* targetBundle = FindOrCreateBundle(OutBundleData, bundleName);
					targetBundle->Entries.Emplace(FSoftObjectPath(assetPath), qualifiedName);
				}

				// Queue unwrap: replace the wrapper object with just the "value" field
				if (valueField.IsValid())
				{
					unwrapOps.Emplace(fieldKey, valueField);
				}

				continue;
			}

			// Check for type markers and recurse
			FString nestedClassName;
			FString markerValue;

			if (fieldObject->TryGetStringField(
					FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY, markerValue))
			{
				nestedClassName = ExtractClassNameFromMarker(markerValue, true);
			}
			else if (fieldObject->TryGetStringField(
					FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY, markerValue))
			{
				nestedClassName = ExtractClassNameFromMarker(markerValue, false);
			}
			else if (fieldObject->TryGetStringField(
					FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY, markerValue))
			{
				nestedClassName = ExtractClassNameFromMarker(markerValue, false);
			}

			if (!nestedClassName.IsEmpty())
			{
				ExtractBundlesFromJsonObjectRecursive(fieldObject, nestedClassName, OutBundleData);
			}
		}
		else if (fieldValue->Type == EJson::Array)
		{
			// Recurse into array elements that have type markers
			const TArray<TSharedPtr<FJsonValue>>& jsonArray = fieldValue->AsArray();
			for (const TSharedPtr<FJsonValue>& element : jsonArray)
			{
				if (!element.IsValid() || element->Type != EJson::Object)
				{
					continue;
				}

				const TSharedPtr<FJsonObject>& elemObject = element->AsObject();
				if (!elemObject.IsValid())
				{
					continue;
				}

				FString elemClassName;
				FString elemMarkerValue;

				if (elemObject->TryGetStringField(
						FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY, elemMarkerValue))
				{
					elemClassName = ExtractClassNameFromMarker(elemMarkerValue, true);
				}
				else if (elemObject->TryGetStringField(
						FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY, elemMarkerValue))
				{
					elemClassName = ExtractClassNameFromMarker(elemMarkerValue, false);
				}
				else if (elemObject->TryGetStringField(
						FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY, elemMarkerValue))
				{
					elemClassName = ExtractClassNameFromMarker(elemMarkerValue, false);
				}

				if (!elemClassName.IsEmpty())
				{
					ExtractBundlesFromJsonObjectRecursive(elemObject, elemClassName, OutBundleData);
				}
			}
		}
	}

	// Apply unwrap operations: replace wrapped objects with their unwrapped values
	for (const TPair<FString, TSharedPtr<FJsonValue>>& op : unwrapOps)
	{
		JsonObject->SetField(op.Key, op.Value);
	}
}

FSGDynamicTextAssetBundle* USGDTAPerPropertyAssetBundleExtender::FindOrCreateBundle(
	FSGDynamicTextAssetBundleData& OutBundleData, FName BundleName)
{
	for (FSGDynamicTextAssetBundle& existing : OutBundleData.Bundles)
	{
		if (existing.BundleName == BundleName)
		{
			return &existing;
		}
	}

	FSGDynamicTextAssetBundle& newBundle = OutBundleData.Bundles.AddDefaulted_GetRef();
	newBundle.BundleName = BundleName;
	return &newBundle;
}

void USGDTAPerPropertyAssetBundleExtender::Native_PostSerialize(
	const FSGDynamicTextAssetBundleData& BundleData,
	FString& InOutContent, const FSGDTASerializerFormat& Format) const
{
	if (!BundleData.HasBundles())
	{
		return;
	}

	switch (Format)
	{
		case SGDynamicTextAssetConstants::JSON_SERIALIZER_TYPE_ID:
		{
			SerializeBundlesJson(BundleData, InOutContent);
			break;
		}
		case SGDynamicTextAssetConstants::XML_SERIALIZER_TYPE_ID:
		{
			SerializeBundlesXml(BundleData, InOutContent);
			break;
		}
		case SGDynamicTextAssetConstants::YAML_SERIALIZER_TYPE_ID:
		{
			SerializeBundlesYaml(BundleData, InOutContent);
			break;
		}
		// Unrecognized format
		default:
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("USGDTAPerPropertyAssetBundleExtender::Native_PostSerialize: Unrecognized format TypeId=%u"),
				Format.GetTypeId());
			break;
		}
	}
}

bool USGDTAPerPropertyAssetBundleExtender::Native_PreDeserialize(
	FString& InOutContent,
	FSGDynamicTextAssetBundleData& OutBundleData, const FSGDTASerializerFormat& Format) const
{
	switch (Format)
	{
		case SGDynamicTextAssetConstants::JSON_SERIALIZER_TYPE_ID:
		{
			return DeserializeBundlesJson(InOutContent, OutBundleData);
		}
		case SGDynamicTextAssetConstants::XML_SERIALIZER_TYPE_ID:
		{
			return DeserializeBundlesXml(InOutContent, OutBundleData);
		}
		case SGDynamicTextAssetConstants::YAML_SERIALIZER_TYPE_ID:
		{
			return DeserializeBundlesYaml(InOutContent, OutBundleData);
		}
		// Unrecognized format
		default:
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("USGDTAPerPropertyAssetBundleExtender::Native_PreDeserialize: Unrecognized format TypeId=%u"),
				Format.GetTypeId());
			return false;
		}
	}
}

void USGDTAPerPropertyAssetBundleExtender::SerializeBundlesJson(
	const FSGDynamicTextAssetBundleData& BundleData,
	FString& InOutContent) const
{
	// Parse the existing JSON content
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InOutContent);
	TSharedPtr<FJsonObject> rootObject;
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesJson: Failed to parse JSON content"));
		return;
	}

	// Get the data section
	const TSharedPtr<FJsonObject>* dataObjectPtr = nullptr;
	if (!rootObject->TryGetObjectField(ISGDynamicTextAssetSerializer::KEY_DATA, dataObjectPtr)
		|| !dataObjectPtr || !dataObjectPtr->IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesJson: No data section found"));
		return;
	}

	TSharedPtr<FJsonObject> dataObject = *dataObjectPtr;

	// Build two-level lookup: ClassName -> PropertyName -> BundleNames
	FClassPropertyBundlesMap classPropertyBundles = BuildPropertyToBundlesMap(BundleData);

	// Determine root class name by matching bundle entries to top-level data fields
	const FString rootClassName = DetermineRootClassName(dataObject, classPropertyBundles);

	// Recursively wrap bundled properties at all nesting levels
	WrapBundledPropertiesRecursive(dataObject, rootClassName, classPropertyBundles);

	// Remove any existing root-level bundles block
	rootObject->RemoveField(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);

	// Re-serialize back to string with pretty print
	FString outputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&outputString);

	if (FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		InOutContent = MoveTemp(outputString);
	}
	else
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesJson: Failed to re-serialize JSON content"));
	}
}

bool USGDTAPerPropertyAssetBundleExtender::DeserializeBundlesJson(
	FString& InOutContent,
	FSGDynamicTextAssetBundleData& OutBundleData) const
{
	// Parse the JSON content
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InOutContent);
	TSharedPtr<FJsonObject> rootObject;
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::DeserializeBundlesJson: Failed to parse JSON content"));
		return false;
	}

	// Get the data section
	const TSharedPtr<FJsonObject>* dataObjectPtr = nullptr;
	if (!rootObject->TryGetObjectField(ISGDynamicTextAssetSerializer::KEY_DATA, dataObjectPtr)
		|| !dataObjectPtr || !dataObjectPtr->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>& dataObject = *dataObjectPtr;

	// Get the type name from file information for root class context
	FString rootClassName;
	const TSharedPtr<FJsonObject>* fileInfoPtr = nullptr;
	if (rootObject->TryGetObjectField(ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION, fileInfoPtr)
		&& fileInfoPtr && fileInfoPtr->IsValid())
	{
		(*fileInfoPtr)->TryGetStringField(ISGDynamicTextAssetSerializer::KEY_TYPE, rootClassName);
	}

	// Recursively extract bundle data AND unwrap properties in a single pass
	ExtractBundlesFromJsonObjectRecursive(dataObject, rootClassName, OutBundleData);

	// Re-serialize the modified JSON (with unwrapped properties) back into the content string
	if (OutBundleData.HasBundles())
	{
		FString outputString;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&outputString);

		if (FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
		{
			InOutContent = MoveTemp(outputString);
		}
	}

	return OutBundleData.HasBundles();
}

namespace SGDTAPerPropertyExtenderXml
{
	using namespace SGDTAPerPropertyExtenderInternals;

	/** Checks if an XML node has a type marker child element and returns the class name. */
	static FString GetClassNameFromXmlNode(const FXmlNode* Node)
	{
		if (!Node)
		{
			return FString();
		}

		// Check SG_INST_OBJ_CLASS
		const FXmlNode* instClassNode = Node->FindChildNode(
			FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
		if (instClassNode && !instClassNode->GetContent().IsEmpty())
		{
			return USGDTAPerPropertyAssetBundleExtender::ExtractClassNameFromMarker(
				instClassNode->GetContent(), true);
		}

		// Check SG_STRUCT_TYPE
		const FXmlNode* structTypeNode = Node->FindChildNode(
			FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY);
		if (structTypeNode && !structTypeNode->GetContent().IsEmpty())
		{
			return USGDTAPerPropertyAssetBundleExtender::ExtractClassNameFromMarker(
				structTypeNode->GetContent(), false);
		}

		// Check SG_INST_STRUCT_TYPE
		const FXmlNode* instStructNode = Node->FindChildNode(
			FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY);
		if (instStructNode && !instStructNode->GetContent().IsEmpty())
		{
			return USGDTAPerPropertyAssetBundleExtender::ExtractClassNameFromMarker(
				instStructNode->GetContent(), false);
		}

		return FString();
	}

	/** Recursively wraps bundled properties in XML nodes. Outputs the modified XML fragment. */
	static void WrapXmlNodeRecursive(
		const FXmlNode* Node,
		const FString& ClassName,
		const USGDTAPerPropertyAssetBundleExtender::FClassPropertyBundlesMap& ClassPropertyBundles,
		FString& OutXml,
		int32 Depth)
	{
		if (!Node)
		{
			return;
		}

		const TMap<FString, TArray<FString>>* propertyMap = ClassPropertyBundles.Find(ClassName);

		for (const FXmlNode* childNode : Node->GetChildrenNodes())
		{
			const FString& tag = childNode->GetTag();
			const TArray<FString>* bundleNames = propertyMap ? propertyMap->Find(tag) : nullptr;

			if (bundleNames)
			{
				// Wrap this property
				OutXml += FString::Printf(TEXT("%s<%s>\n"), *XmlIndent(Depth), *tag);

				const TArray<FXmlNode*>& children = childNode->GetChildrenNodes();
				const FString& content = childNode->GetContent();

				if (children.Num() > 0)
				{
					OutXml += FString::Printf(TEXT("%s<%s>\n"),
						*XmlIndent(Depth + 1), *SGDTAPerPropertyExtenderKeys::KEY_VALUE);
					for (const FXmlNode* originalChild : children)
					{
						XmlNodeToString(originalChild, OutXml, Depth + 2);
					}
					OutXml += FString::Printf(TEXT("%s</%s>\n"),
						*XmlIndent(Depth + 1), *SGDTAPerPropertyExtenderKeys::KEY_VALUE);
				}
				else
				{
					OutXml += FString::Printf(TEXT("%s<%s>%s</%s>\n"),
						*XmlIndent(Depth + 1), *SGDTAPerPropertyExtenderKeys::KEY_VALUE,
						*XmlEscape(content), *SGDTAPerPropertyExtenderKeys::KEY_VALUE);
				}

				OutXml += FString::Printf(TEXT("%s<%s>\n"),
					*XmlIndent(Depth + 1), *SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES);
				for (const FString& bundleName : *bundleNames)
				{
					OutXml += FString::Printf(TEXT("%s<%s>%s</%s>\n"),
						*XmlIndent(Depth + 2), *SGDTAPerPropertyExtenderKeys::KEY_ITEM,
						*XmlEscape(bundleName), *SGDTAPerPropertyExtenderKeys::KEY_ITEM);
				}
				OutXml += FString::Printf(TEXT("%s</%s>\n"),
					*XmlIndent(Depth + 1), *SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES);

				OutXml += FString::Printf(TEXT("%s</%s>\n"), *XmlIndent(Depth), *tag);
			}
			else
			{
				// Check for type markers and recurse
				const FString nestedClassName = GetClassNameFromXmlNode(childNode);
				if (!nestedClassName.IsEmpty())
				{
					// Write opening tag
					OutXml += FString::Printf(TEXT("%s<%s>\n"), *XmlIndent(Depth), *tag);

					// Write type marker children first, then recurse for the rest
					WrapXmlNodeRecursive(childNode, nestedClassName, ClassPropertyBundles,
						OutXml, Depth + 1);

					OutXml += FString::Printf(TEXT("%s</%s>\n"), *XmlIndent(Depth), *tag);
				}
				else
				{
					// Non-bundled, no type marker: write as-is
					XmlNodeToString(childNode, OutXml, Depth);
				}
			}
		}
	}

	/** Recursively extracts bundle data from XML nodes. */
	static void ExtractBundlesFromXmlNodeRecursive(
		const FXmlNode* Node,
		const FString& ClassName,
		FSGDynamicTextAssetBundleData& OutBundleData)
	{
		if (!Node)
		{
			return;
		}

		for (const FXmlNode* childNode : Node->GetChildrenNodes())
		{
			const FString& tag = childNode->GetTag();

			// Check if wrapped (has <value> + <assetBundles>)
			const FXmlNode* valueNode = childNode->FindChildNode(
				SGDTAPerPropertyExtenderKeys::KEY_VALUE);
			const FXmlNode* bundlesNode = childNode->FindChildNode(
				SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES);

			if (valueNode && bundlesNode)
			{
				FString assetPath = valueNode->GetContent();

				FName qualifiedName;
				if (!ClassName.IsEmpty())
				{
					qualifiedName = FName(*FString::Printf(TEXT("%s.%s"), *ClassName, *tag));
				}
				else
				{
					qualifiedName = FName(*tag);
				}

				for (const FXmlNode* itemNode : bundlesNode->GetChildrenNodes())
				{
					if (itemNode->GetTag() != SGDTAPerPropertyExtenderKeys::KEY_ITEM)
					{
						continue;
					}

					const FString bundleNameStr = itemNode->GetContent();
					if (bundleNameStr.IsEmpty())
					{
						continue;
					}

					FSGDynamicTextAssetBundle* targetBundle =
						USGDTAPerPropertyAssetBundleExtender::FindOrCreateBundle(
							OutBundleData, FName(*bundleNameStr));
					targetBundle->Entries.Emplace(FSoftObjectPath(assetPath), qualifiedName);
				}
				continue;
			}

			// Check for type markers and recurse
			const FString nestedClassName = GetClassNameFromXmlNode(childNode);
			if (!nestedClassName.IsEmpty())
			{
				ExtractBundlesFromXmlNodeRecursive(childNode, nestedClassName, OutBundleData);
			}
		}
	}
}

void USGDTAPerPropertyAssetBundleExtender::SerializeBundlesXml(
	const FSGDynamicTextAssetBundleData& BundleData,
	FString& InOutContent) const
{
	using namespace SGDTAPerPropertyExtenderInternals;

	// Parse the XML to find the data section
	FXmlFile xmlFile(InOutContent, EConstructMethod::ConstructFromBuffer);
	if (!xmlFile.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesXml: Failed to parse XML content"));
		return;
	}

	const FXmlNode* rootNode = xmlFile.GetRootNode();
	if (!rootNode)
	{
		return;
	}

	const FXmlNode* dataNode = rootNode->FindChildNode(ISGDynamicTextAssetSerializer::KEY_DATA);
	if (!dataNode)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesXml: No data section found"));
		return;
	}

	// Build two-level lookup and determine root class name
	FClassPropertyBundlesMap classPropertyBundles = BuildPropertyToBundlesMap(BundleData);

	// Determine root class name from file information type
	FString rootClassName;
	const FXmlNode* fileInfoNode = rootNode->FindChildNode(ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION);
	if (fileInfoNode)
	{
		const FXmlNode* typeNode = fileInfoNode->FindChildNode(ISGDynamicTextAssetSerializer::KEY_TYPE);
		if (typeNode)
		{
			rootClassName = typeNode->GetContent();
		}
	}

	// If no type info, try heuristic (match bundle classes to data children)
	if (rootClassName.IsEmpty())
	{
		// Build a temporary JSON-like match by checking data child tags
		TSet<FString> dataChildTags;
		for (const FXmlNode* child : dataNode->GetChildrenNodes())
		{
			dataChildTags.Add(child->GetTag());
		}

		int32 bestCount = -1;
		for (const TPair<FString, TMap<FString, TArray<FString>>>& classPair : classPropertyBundles)
		{
			int32 count = 0;
			for (const TPair<FString, TArray<FString>>& propPair : classPair.Value)
			{
				if (dataChildTags.Contains(propPair.Key))
				{
					count++;
				}
			}
			if (count > bestCount)
			{
				bestCount = count;
				rootClassName = classPair.Key;
			}
		}
	}

	// Build the modified data section
	FString newDataXml;
	newDataXml += FString::Printf(TEXT("%s<%s>\n"), *XmlIndent(1),
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	SGDTAPerPropertyExtenderXml::WrapXmlNodeRecursive(
		dataNode, rootClassName, classPropertyBundles, newDataXml, 2);

	newDataXml += FString::Printf(TEXT("%s</%s>\n"), *XmlIndent(1),
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	// Replace the old data section in the content string
	const FString dataOpenTag = FString::Printf(TEXT("<%s>"),
		*ISGDynamicTextAssetSerializer::KEY_DATA);
	const FString dataCloseTag = FString::Printf(TEXT("</%s>"),
		*ISGDynamicTextAssetSerializer::KEY_DATA);

	const int32 dataStart = InOutContent.Find(dataOpenTag, ESearchCase::CaseSensitive);
	const int32 dataEnd = InOutContent.Find(dataCloseTag, ESearchCase::CaseSensitive);

	if (dataStart != INDEX_NONE && dataEnd != INDEX_NONE)
	{
		const int32 replaceEnd = dataEnd + dataCloseTag.Len();
		int32 adjustedEnd = replaceEnd;
		if (adjustedEnd < InOutContent.Len() && InOutContent[adjustedEnd] == TEXT('\n'))
		{
			adjustedEnd++;
		}

		FString before = InOutContent.Left(dataStart);
		FString after = InOutContent.Mid(adjustedEnd);

		// Remove any existing root-level sgdtAssetBundles block from after
		const FString bundlesOpenTag = FString::Printf(TEXT("<%s>"),
			*ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);
		const FString bundlesCloseTag = FString::Printf(TEXT("</%s>"),
			*ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);

		const int32 bundlesStart = after.Find(bundlesOpenTag, ESearchCase::CaseSensitive);
		const int32 bundlesEnd = after.Find(bundlesCloseTag, ESearchCase::CaseSensitive);

		if (bundlesStart != INDEX_NONE && bundlesEnd != INDEX_NONE)
		{
			int32 bundlesReplaceEnd = bundlesEnd + bundlesCloseTag.Len();
			if (bundlesReplaceEnd < after.Len() && after[bundlesReplaceEnd] == TEXT('\n'))
			{
				bundlesReplaceEnd++;
			}
			after = after.Left(bundlesStart) + after.Mid(bundlesReplaceEnd);
		}

		InOutContent = before + newDataXml + after;
	}
}

bool USGDTAPerPropertyAssetBundleExtender::DeserializeBundlesXml(
	FString& InOutContent,
	FSGDynamicTextAssetBundleData& OutBundleData) const
{
	if (InOutContent.IsEmpty())
	{
		return false;
	}

	FXmlFile xmlFile(InOutContent, EConstructMethod::ConstructFromBuffer);
	if (!xmlFile.IsValid())
	{
		return false;
	}

	const FXmlNode* rootNode = xmlFile.GetRootNode();
	if (!rootNode)
	{
		return false;
	}

	const FXmlNode* dataNode = rootNode->FindChildNode(ISGDynamicTextAssetSerializer::KEY_DATA);
	if (!dataNode)
	{
		return false;
	}

	// Get type name from file information for root class context
	FString rootClassName;
	const FXmlNode* fileInfoNode = rootNode->FindChildNode(ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION);
	if (fileInfoNode)
	{
		const FXmlNode* typeNode = fileInfoNode->FindChildNode(ISGDynamicTextAssetSerializer::KEY_TYPE);
		if (typeNode)
		{
			rootClassName = typeNode->GetContent();
		}
	}

	// Recursively extract bundle data from all nesting levels
	SGDTAPerPropertyExtenderXml::ExtractBundlesFromXmlNodeRecursive(
		dataNode, rootClassName, OutBundleData);

	return OutBundleData.HasBundles();
}

namespace SGDTAPerPropertyExtenderYaml
{
	using namespace SGDTAPerPropertyExtenderInternals;

	/** Gets class name from a YAML mapping node's type marker keys. */
	static FString GetClassNameFromYamlNode(const fkyaml::node& Node)
	{
		if (!Node.is_mapping())
		{
			return FString();
		}

		const std::string instClassKey = ToStdString(FSGDynamicTextAssetSerializerBase::INSTANCED_OBJECT_CLASS_KEY);
		if (Node.contains(instClassKey) && Node[instClassKey].is_string())
		{
			return USGDTAPerPropertyAssetBundleExtender::ExtractClassNameFromMarker(
				ToFString(Node[instClassKey].get_value<std::string>()), true);
		}

		const std::string structTypeKey = ToStdString(FSGDynamicTextAssetSerializerBase::STRUCT_TYPE_KEY);
		if (Node.contains(structTypeKey) && Node[structTypeKey].is_string())
		{
			return USGDTAPerPropertyAssetBundleExtender::ExtractClassNameFromMarker(
				ToFString(Node[structTypeKey].get_value<std::string>()), false);
		}

		const std::string instStructKey = ToStdString(FSGDynamicTextAssetSerializerBase::INSTANCED_STRUCT_TYPE_KEY);
		if (Node.contains(instStructKey) && Node[instStructKey].is_string())
		{
			return USGDTAPerPropertyAssetBundleExtender::ExtractClassNameFromMarker(
				ToFString(Node[instStructKey].get_value<std::string>()), false);
		}

		return FString();
	}

	/** Recursively wraps bundled properties in YAML nodes. */
	static void WrapYamlNodeRecursive(
		fkyaml::node& MappingNode,
		const FString& ClassName,
		const USGDTAPerPropertyAssetBundleExtender::FClassPropertyBundlesMap& ClassPropertyBundles)
	{
		if (!MappingNode.is_mapping())
		{
			return;
		}

		const TMap<FString, TArray<FString>>* propertyMap = ClassPropertyBundles.Find(ClassName);

		// First pass: wrap bundled properties
		if (propertyMap)
		{
			for (auto itr = MappingNode.begin(); itr != MappingNode.end(); ++itr)
			{
				const FString propertyKey = ToFString(itr.key().get_value<std::string>());
				const TArray<FString>* bundleNames = propertyMap->Find(propertyKey);

				if (!bundleNames)
				{
					continue;
				}

				fkyaml::node wrapperNode = fkyaml::node::mapping();
				wrapperNode[ToStdString(SGDTAPerPropertyExtenderKeys::KEY_VALUE)] = *itr;

				fkyaml::node bundleArray = fkyaml::node::sequence();
				for (const FString& bundleName : *bundleNames)
				{
					bundleArray.as_seq().emplace_back(fkyaml::node(ToStdString(bundleName)));
				}
				wrapperNode[ToStdString(SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES)] = bundleArray;

				*itr = wrapperNode;
			}
		}

		// Second pass: recurse into nested typed mappings
		for (auto itr = MappingNode.begin(); itr != MappingNode.end(); ++itr)
		{
			if (itr->is_mapping())
			{
				// Skip wrapped properties
				const std::string valueKey = ToStdString(SGDTAPerPropertyExtenderKeys::KEY_VALUE);
				const std::string bundlesKey = ToStdString(SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES);
				if (itr->contains(valueKey) && itr->contains(bundlesKey))
				{
					continue;
				}

				const FString nestedClassName = GetClassNameFromYamlNode(*itr);
				if (!nestedClassName.IsEmpty())
				{
					WrapYamlNodeRecursive(*itr, nestedClassName, ClassPropertyBundles);
				}
			}
			else if (itr->is_sequence())
			{
				for (auto elemItr = itr->begin(); elemItr != itr->end(); ++elemItr)
				{
					if (elemItr->is_mapping())
					{
						const FString elemClassName = GetClassNameFromYamlNode(*elemItr);
						if (!elemClassName.IsEmpty())
						{
							WrapYamlNodeRecursive(*elemItr, elemClassName, ClassPropertyBundles);
						}
					}
				}
			}
		}
	}

	/** Recursively extracts bundle data from YAML nodes. */
	static void ExtractBundlesFromYamlNodeRecursive(
		const fkyaml::node& MappingNode,
		const FString& ClassName,
		FSGDynamicTextAssetBundleData& OutBundleData)
	{
		if (!MappingNode.is_mapping())
		{
			return;
		}

		const std::string valueKey = ToStdString(SGDTAPerPropertyExtenderKeys::KEY_VALUE);
		const std::string bundlesKey = ToStdString(SGDTAPerPropertyExtenderKeys::KEY_ASSET_BUNDLES);

		for (auto itr = MappingNode.begin(); itr != MappingNode.end(); ++itr)
		{
			const FString propertyKey = ToFString(itr.key().get_value<std::string>());

			if (itr->is_mapping())
			{
				// Check if wrapped (has "value" + "assetBundles")
				if (itr->contains(valueKey) && itr->contains(bundlesKey))
				{
					FString assetPath;
					const fkyaml::node& valNode = (*itr)[valueKey];
					if (valNode.is_string())
					{
						assetPath = ToFString(valNode.get_value<std::string>());
					}

					FName qualifiedName;
					if (!ClassName.IsEmpty())
					{
						qualifiedName = FName(*FString::Printf(TEXT("%s.%s"), *ClassName, *propertyKey));
					}
					else
					{
						qualifiedName = FName(*propertyKey);
					}

					const fkyaml::node& bundlesNode = (*itr)[bundlesKey];
					if (bundlesNode.is_sequence())
					{
						for (auto bundleItr = bundlesNode.begin(); bundleItr != bundlesNode.end(); ++bundleItr)
						{
							if (!bundleItr->is_string())
							{
								continue;
							}
							const FName bundleName = FName(*ToFString(bundleItr->get_value<std::string>()));
							FSGDynamicTextAssetBundle* targetBundle =
								USGDTAPerPropertyAssetBundleExtender::FindOrCreateBundle(OutBundleData, bundleName);
							targetBundle->Entries.Emplace(FSoftObjectPath(assetPath), qualifiedName);
						}
					}
					continue;
				}

				// Check for type markers and recurse
				const FString nestedClassName = GetClassNameFromYamlNode(*itr);
				if (!nestedClassName.IsEmpty())
				{
					ExtractBundlesFromYamlNodeRecursive(*itr, nestedClassName, OutBundleData);
				}
			}
			else if (itr->is_sequence())
			{
				for (auto elemItr = itr->begin(); elemItr != itr->end(); ++elemItr)
				{
					if (elemItr->is_mapping())
					{
						const FString elemClassName = GetClassNameFromYamlNode(*elemItr);
						if (!elemClassName.IsEmpty())
						{
							ExtractBundlesFromYamlNodeRecursive(*elemItr, elemClassName, OutBundleData);
						}
					}
				}
			}
		}
	}
}

void USGDTAPerPropertyAssetBundleExtender::SerializeBundlesYaml(
	const FSGDynamicTextAssetBundleData& BundleData,
	FString& InOutContent) const
{
	using namespace SGDTAPerPropertyExtenderInternals;

	try
	{
		const std::string yamlStr = ToStdString(InOutContent);
		fkyaml::node rootNode = fkyaml::node::deserialize(yamlStr);

		if (!rootNode.is_mapping())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesYaml: Root node is not a mapping"));
			return;
		}

		const std::string dataKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_DATA);
		if (!rootNode.contains(dataKey))
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesYaml: No data section found"));
			return;
		}

		fkyaml::node& dataNode = rootNode[dataKey];
		if (!dataNode.is_mapping())
		{
			return;
		}

		// Build two-level lookup
		FClassPropertyBundlesMap classPropertyBundles = BuildPropertyToBundlesMap(BundleData);

		// Determine root class name from file information
		FString rootClassName;
		const std::string fileInfoKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION);
		const std::string typeKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_TYPE);

		if (rootNode.contains(fileInfoKey) && rootNode[fileInfoKey].is_mapping()
			&& rootNode[fileInfoKey].contains(typeKey) && rootNode[fileInfoKey][typeKey].is_string())
		{
			rootClassName = ToFString(rootNode[fileInfoKey][typeKey].get_value<std::string>());
		}

		// If no type info, use heuristic
		if (rootClassName.IsEmpty())
		{
			TSet<FString> dataChildKeys;
			for (auto itr = dataNode.begin(); itr != dataNode.end(); ++itr)
			{
				dataChildKeys.Add(ToFString(itr.key().get_value<std::string>()));
			}

			int32 bestCount = -1;
			for (const  TPair<FString, TMap<FString, TArray<FString>>>& classPair : classPropertyBundles)
			{
				int32 count = 0;
				for (const TPair<FString, TArray<FString>>& propPair : classPair.Value)
				{
					if (dataChildKeys.Contains(propPair.Key))
					{
						count++;
					}
				}
				if (count > bestCount)
				{
					bestCount = count;
					rootClassName = classPair.Key;
				}
			}
		}

		// Recursively wrap bundled properties
		SGDTAPerPropertyExtenderYaml::WrapYamlNodeRecursive(
			dataNode, rootClassName, classPropertyBundles);

		// Remove any existing root-level bundles key
		const std::string bundlesKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);
		if (rootNode.contains(bundlesKey))
		{
			rootNode.as_map().erase(bundlesKey);
		}

		const std::string outputStr = fkyaml::node::serialize(rootNode);
		InOutContent = ToFString(outputStr);
	}
	catch (const fkyaml::exception& e)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesYaml: YAML error: %s"),
			*SGDTAPerPropertyExtenderInternals::ToFString(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTAPerPropertyAssetBundleExtender::SerializeBundlesYaml: Error: %s"),
			*SGDTAPerPropertyExtenderInternals::ToFString(e.what()));
	}
}

bool USGDTAPerPropertyAssetBundleExtender::DeserializeBundlesYaml(
	FString& InOutContent,
	FSGDynamicTextAssetBundleData& OutBundleData) const
{
	using namespace SGDTAPerPropertyExtenderInternals;

	if (InOutContent.IsEmpty())
	{
		return false;
	}

	try
	{
		const std::string yamlStr = ToStdString(InOutContent);
		fkyaml::node rootNode = fkyaml::node::deserialize(yamlStr);

		if (!rootNode.is_mapping())
		{
			return false;
		}

		const std::string dataKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_DATA);
		if (!rootNode.contains(dataKey))
		{
			return false;
		}

		fkyaml::node& dataNode = rootNode[dataKey];
		if (!dataNode.is_mapping())
		{
			return false;
		}

		// Get type name for root class context
		FString rootClassName;
		const std::string fileInfoKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_FILE_INFORMATION);
		const std::string typeKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_TYPE);

		if (rootNode.contains(fileInfoKey) && rootNode[fileInfoKey].is_mapping()
			&& rootNode[fileInfoKey].contains(typeKey) && rootNode[fileInfoKey][typeKey].is_string())
		{
			rootClassName = ToFString(rootNode[fileInfoKey][typeKey].get_value<std::string>());
		}

		// Recursively extract bundle data from all nesting levels
		SGDTAPerPropertyExtenderYaml::ExtractBundlesFromYamlNodeRecursive(
			dataNode, rootClassName, OutBundleData);

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
