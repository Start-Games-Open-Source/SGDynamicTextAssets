// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/AssetBundleExtenders/SGDTADefaultAssetBundleExtender.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/SGDTASerializer.h"
#include "SGDynamicTextAssetLogs.h"
#include "Statics/SGDTAConstants.h"
#include "XmlFile.h"

THIRD_PARTY_INCLUDES_START
#include <fkYAML/node.hpp>
THIRD_PARTY_INCLUDES_END

namespace SGDTADefaultExtenderInternals
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

	FString XmlUnescape(const FString& Value)
	{
		FString result = Value;
		result.ReplaceInline(TEXT("&apos;"), TEXT("'"), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT("&quot;"), TEXT("\""), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT("&gt;"), TEXT(">"), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT("&lt;"), TEXT("<"), ESearchCase::CaseSensitive);
		result.ReplaceInline(TEXT("&amp;"), TEXT("&"), ESearchCase::CaseSensitive);
		return result;
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

void USGDTADefaultAssetBundleExtender::Native_PostSerialize(
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
				TEXT("USGDTADefaultAssetBundleExtender::Native_PostSerialize: Unrecognized format TypeId=%u"),
				Format.GetTypeId());
			break;
		}
	}
}

bool USGDTADefaultAssetBundleExtender::Native_PreDeserialize(
	FString& InOutContent,
	FSGDynamicTextAssetBundleData& OutBundleData, const FSGDTASerializerFormat& Format) const
{
	// The default extender reads from a root-level block and does not modify property values,
	// so InOutContent is read but not modified.
	switch (Format.GetTypeId())
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
				TEXT("USGDTADefaultAssetBundleExtender::Native_PreDeserialize: Unrecognized format TypeId=%u"),
				Format.GetTypeId());
			return false;
		}
	}
}

void USGDTADefaultAssetBundleExtender::SerializeBundlesJson(
	const FSGDynamicTextAssetBundleData& BundleData,
	FString& InOutContent) const
{
	// Parse the existing JSON content
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InOutContent);
	TSharedPtr<FJsonObject> rootObject;
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTADefaultAssetBundleExtender::Native_PostSerialize: Failed to parse JSON content"));
		return;
	}

	// Build the bundles object
	TSharedPtr<FJsonObject> bundlesObject = MakeShared<FJsonObject>();

	for (const FSGDynamicTextAssetBundle& bundle : BundleData.Bundles)
	{
		TArray<TSharedPtr<FJsonValue>> entryArray;
		entryArray.Reserve(bundle.Entries.Num());

		for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
		{
			TSharedRef<FJsonObject> entryObj = MakeShared<FJsonObject>();
			entryObj->SetStringField(TEXT("property"), entry.PropertyName.ToString());
			entryObj->SetStringField(TEXT("path"), entry.AssetPath.ToString());
			entryArray.Add(MakeShared<FJsonValueObject>(entryObj));
		}

		bundlesObject->SetField(bundle.BundleName.ToString(),
			MakeShared<FJsonValueArray>(entryArray));
	}

	// Add bundles block to root
	rootObject->SetObjectField(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES,
		bundlesObject.ToSharedRef());

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
			TEXT("USGDTADefaultAssetBundleExtender::Native_PostSerialize: Failed to re-serialize JSON content"));
	}
}

bool USGDTADefaultAssetBundleExtender::DeserializeBundlesJson(
	const FString& Content,
	FSGDynamicTextAssetBundleData& OutBundleData) const
{
	// Parse the JSON content
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(Content);
	TSharedPtr<FJsonObject> rootObject;
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTADefaultAssetBundleExtender::DeserializeBundlesJson: Failed to parse JSON content"));
		return false;
	}

	// Look for the bundles block
	const TSharedPtr<FJsonObject>* bundlesObject = nullptr;
	if (!rootObject->TryGetObjectField(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES, bundlesObject)
		|| !bundlesObject || !bundlesObject->IsValid())
	{
		// No bundles block is valid (file has no bundles)
		return false;
	}

	// Iterate each bundle name and its entry array
	for (const TPair<FString, TSharedPtr<FJsonValue>>& bundlePair : (*bundlesObject)->Values)
	{
		const FName bundleName = FName(*bundlePair.Key);

		if (!bundlePair.Value.IsValid() || bundlePair.Value->Type != EJson::Array)
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>& entryArray = bundlePair.Value->AsArray();

		// Find or create this bundle
		FSGDynamicTextAssetBundle* targetBundle = nullptr;
		for (FSGDynamicTextAssetBundle& existing : OutBundleData.Bundles)
		{
			if (existing.BundleName == bundleName)
			{
				targetBundle = &existing;
				break;
			}
		}

		if (!targetBundle)
		{
			FSGDynamicTextAssetBundle& newBundle = OutBundleData.Bundles.AddDefaulted_GetRef();
			newBundle.BundleName = bundleName;
			targetBundle = &newBundle;
		}

		// Process each entry
		for (const TSharedPtr<FJsonValue>& entryValue : entryArray)
		{
			if (!entryValue.IsValid() || entryValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject>& entryObj = entryValue->AsObject();
			FString propertyName;
			FString pathString;

			if (entryObj->TryGetStringField(TEXT("property"), propertyName)
				&& entryObj->TryGetStringField(TEXT("path"), pathString))
			{
				targetBundle->Entries.Emplace(FSoftObjectPath(pathString), FName(*propertyName));
			}
		}
	}

	return OutBundleData.HasBundles();
}

void USGDTADefaultAssetBundleExtender::SerializeBundlesXml(
	const FSGDynamicTextAssetBundleData& BundleData,
	FString& InOutContent) const
{
	using namespace SGDTADefaultExtenderInternals;

	// Build the XML bundle block
	FString bundleXml;
	bundleXml += FString::Printf(TEXT("%s<%s>\n"), *XmlIndent(1),
		*ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);

	for (const FSGDynamicTextAssetBundle& bundle : BundleData.Bundles)
	{
		bundleXml += FString::Printf(TEXT("%s<bundle name=\"%s\">\n"),
			*XmlIndent(2), *XmlEscape(bundle.BundleName.ToString()));

		for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
		{
			bundleXml += FString::Printf(TEXT("%s<entry property=\"%s\" path=\"%s\"/>\n"),
				*XmlIndent(3),
				*XmlEscape(entry.PropertyName.ToString()),
				*XmlEscape(entry.AssetPath.ToString()));
		}

		bundleXml += FString::Printf(TEXT("%s</bundle>\n"), *XmlIndent(2));
	}

	bundleXml += FString::Printf(TEXT("%s</%s>\n"), *XmlIndent(1),
		*ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);

	// Insert before the closing root tag
	const FString closingTag = FString::Printf(TEXT("</%s>"), *SGDynamicTextAssetConstants::XML_ROOT_TAG);
	const int32 insertPos = InOutContent.Find(closingTag, ESearchCase::CaseSensitive);
	if (insertPos != INDEX_NONE)
	{
		InOutContent.InsertAt(insertPos, bundleXml);
	}
	else
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTADefaultAssetBundleExtender::SerializeBundlesXml: Could not find closing root tag"));
	}
}

bool USGDTADefaultAssetBundleExtender::DeserializeBundlesXml(
	const FString& Content,
	FSGDynamicTextAssetBundleData& OutBundleData) const
{
	if (Content.IsEmpty())
	{
		return false;
	}

	FXmlFile xmlFile(Content, EConstructMethod::ConstructFromBuffer);
	if (!xmlFile.IsValid())
	{
		return false;
	}

	const FXmlNode* rootNode = xmlFile.GetRootNode();
	if (!rootNode)
	{
		return false;
	}

	const FXmlNode* bundlesNode = rootNode->FindChildNode(
		ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);
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

		const FString bundleNameStr = SGDTADefaultExtenderInternals::XmlUnescape(bundleNode->GetAttribute(TEXT("name")));
		if (bundleNameStr.IsEmpty())
		{
			continue;
		}

		FSGDynamicTextAssetBundle& bundle = OutBundleData.Bundles.AddDefaulted_GetRef();
		bundle.BundleName = FName(*bundleNameStr);

		for (const FXmlNode* entryNode : bundleNode->GetChildrenNodes())
		{
			if (entryNode->GetTag() != TEXT("entry"))
			{
				continue;
			}

			const FString propertyName = SGDTADefaultExtenderInternals::XmlUnescape(entryNode->GetAttribute(TEXT("property")));
			const FString pathStr = SGDTADefaultExtenderInternals::XmlUnescape(entryNode->GetAttribute(TEXT("path")));

			if (!propertyName.IsEmpty() && !pathStr.IsEmpty())
			{
				bundle.Entries.Emplace(FSoftObjectPath(pathStr), FName(*propertyName));
			}
		}
	}

	return OutBundleData.HasBundles();
}

void USGDTADefaultAssetBundleExtender::SerializeBundlesYaml(
	const FSGDynamicTextAssetBundleData& BundleData,
	FString& InOutContent) const
{
	using namespace SGDTADefaultExtenderInternals;

	try
	{
		fkyaml::node rootNode;
		FString parseError;
		const std::string yamlStr = ToStdString(InOutContent);
		rootNode = fkyaml::node::deserialize(yamlStr);

		if (!rootNode.is_mapping())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("USGDTADefaultAssetBundleExtender::SerializeBundlesYaml: Root node is not a mapping"));
			return;
		}

		fkyaml::node bundlesNode = fkyaml::node::mapping();

		for (const FSGDynamicTextAssetBundle& bundle : BundleData.Bundles)
		{
			fkyaml::node entryArray = fkyaml::node::sequence();

			for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
			{
				fkyaml::node entryNode = fkyaml::node::mapping();
				entryNode["property"] = fkyaml::node(ToStdString(entry.PropertyName.ToString()));
				entryNode["path"] = fkyaml::node(ToStdString(entry.AssetPath.ToString()));
				entryArray.as_seq().emplace_back(entryNode);
			}

			bundlesNode[ToStdString(bundle.BundleName.ToString())] = entryArray;
		}

		rootNode[ToStdString(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES)] = bundlesNode;

		const std::string outputStr = fkyaml::node::serialize(rootNode);
		InOutContent = ToFString(outputStr);
	}
	catch (const fkyaml::exception& e)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTADefaultAssetBundleExtender::SerializeBundlesYaml: YAML error: %s"),
			*ToFString(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("USGDTADefaultAssetBundleExtender::SerializeBundlesYaml: Error: %s"),
			*ToFString(e.what()));
	}
}

bool USGDTADefaultAssetBundleExtender::DeserializeBundlesYaml(
	const FString& Content,
	FSGDynamicTextAssetBundleData& OutBundleData) const
{
	using namespace SGDTADefaultExtenderInternals;

	if (Content.IsEmpty())
	{
		return false;
	}

	try
	{
		const std::string yamlStr = ToStdString(Content);
		fkyaml::node rootNode = fkyaml::node::deserialize(yamlStr);

		if (!rootNode.is_mapping())
		{
			return false;
		}

		const std::string bundlesKey = ToStdString(ISGDynamicTextAssetSerializer::KEY_SGDT_ASSET_BUNDLES);
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
			const FName bundleName = FName(*ToFString(itr.key().get_value<std::string>()));

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
					propertyName = ToFString((*entryItr)["property"].get_value<std::string>());
				}
				if (entryItr->contains("path") && (*entryItr)["path"].is_string())
				{
					pathStr = ToFString((*entryItr)["path"].get_value<std::string>());
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
