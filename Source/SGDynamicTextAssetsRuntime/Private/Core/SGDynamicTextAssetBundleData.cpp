// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetBundleData.h"

#include "SGDynamicTextAssetLogs.h"
#include "UObject/UnrealType.h"

FSGDynamicTextAssetBundleEntry::FSGDynamicTextAssetBundleEntry(const FSoftObjectPath& InAssetPath, FName InPropertyName)
 : AssetPath(InAssetPath)
 , PropertyName(InPropertyName)
{
}

bool FSGDynamicTextAssetBundle::operator==(const FName& Other) const
{
	return BundleName == Other;
}

bool FSGDynamicTextAssetBundle::operator!=(const FName& Other) const
{
	return !((*this) == Other);
}

bool FSGDynamicTextAssetBundleData::HasBundles() const
{
	return !Bundles.IsEmpty();
}

const FSGDynamicTextAssetBundle* FSGDynamicTextAssetBundleData::FindBundle(FName BundleName) const
{
	return Bundles.FindByKey(BundleName);
}

void FSGDynamicTextAssetBundleData::GetBundleNames(TArray<FName>& OutBundleNames) const
{
	OutBundleNames.Reset();
	OutBundleNames.Reserve(Bundles.Num());
	for (const FSGDynamicTextAssetBundle& bundle : Bundles)
	{
		OutBundleNames.Add(bundle.BundleName);
	}
}

bool FSGDynamicTextAssetBundleData::GetPathsForBundle(FName BundleName, TArray<FSoftObjectPath>& OutPaths) const
{
	const FSGDynamicTextAssetBundle* bundle = FindBundle(BundleName);
	if (!bundle || bundle->Entries.IsEmpty())
	{
		return false;
	}

	OutPaths.Reserve(OutPaths.Num() + bundle->Entries.Num());
	for (const FSGDynamicTextAssetBundleEntry& entry : bundle->Entries)
	{
		OutPaths.Add(entry.AssetPath);
	}
	return true;
}

void FSGDynamicTextAssetBundleData::Reset()
{
	Bundles.Reset();
}

void FSGDynamicTextAssetBundleData::ExtractFromObject(const UObject* Object)
{
	if (!Object)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("ExtractFromObject: Object is null"));
		return;
	}

#if WITH_EDITORONLY_DATA
	// In editor builds, re-extract bundle data from live property metadata.
	// Reset first to clear any stale data before re-extraction.
	Reset();
	const FName rootClassName = Object->GetClass()->GetFName();
	for (TFieldIterator<FProperty> itr(Object->GetClass()); itr; ++itr)
	{
		ExtractBundlesFromProperty(*itr, Object, TArray<FString>(), rootClassName);
	}
#else
	// Property metadata (HasMetaData/GetMetaData) is stripped in non-editor builds.
	// Bundle data is populated via deserialization instead. Do NOT reset here,
	// as that would wipe the bundle data that was already deserialized.
	UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
		TEXT("ExtractFromObject: Skipped in non-editor build for '%s'. Bundle data comes from deserialization."),
		*GetNameSafe(Object));
#endif
}

#if WITH_EDITORONLY_DATA

/**
 * Helper to parse AssetBundles metadata from a property into an array of trimmed bundle names.
 * Returns true if the property had AssetBundles metadata and at least one non-empty name was parsed.
 */
static bool ParseBundleNamesFromProperty(const FProperty* Property, TArray<FString>& OutBundleNames)
{
	if (!Property->HasMetaData(TEXT("AssetBundles")))
	{
		return false;
	}

	const FString bundleMeta = Property->GetMetaData(TEXT("AssetBundles"));
	bundleMeta.ParseIntoArray(OutBundleNames, TEXT(","));

	for (FString& name : OutBundleNames)
	{
		name.TrimStartAndEndInline();
	}

	OutBundleNames.RemoveAll([](const FString& Name) { return Name.IsEmpty(); });
	return !OutBundleNames.IsEmpty();
}

void FSGDynamicTextAssetBundleData::ExtractBundlesFromProperty(
	const FProperty* Property,
	const void* ContainerPtr,
	const TArray<FString>& InheritedBundleNames,
	FName OwnerClassName)
{
	if (!Property || !ContainerPtr)
	{
		return;
	}

	// Instanced object properties own sub-objects whose properties may contain soft paths.
	// Walk into the sub-object and recurse on its properties.
	if (const FObjectProperty* objectProp = CastField<FObjectProperty>(Property))
	{
		if (Property->HasAllPropertyFlags(CPF_InstancedReference))
		{
			const void* valuePtr = objectProp->ContainerPtrToValuePtr<void>(ContainerPtr);
			if (const UObject* subObject = objectProp->GetObjectPropertyValue(valuePtr))
			{
				const FName subObjectClassName = subObject->GetClass()->GetFName();
				for (TFieldIterator<FProperty> innerIt(subObject->GetClass()); innerIt; ++innerIt)
				{
					ExtractBundlesFromProperty(*innerIt, subObject, TArray<FString>(), subObjectClassName);
				}
			}
		}
		return;
	}

	// Soft object property: use own AssetBundles metadata, or inherited names from a parent container
	if (const FSoftObjectProperty* softObjProp = CastField<FSoftObjectProperty>(Property))
	{
		TArray<FString> bundleNames;
		ParseBundleNamesFromProperty(Property, bundleNames);

		// If no metadata on this property, fall back to inherited names from parent container
		if (bundleNames.IsEmpty())
		{
			bundleNames = InheritedBundleNames;
		}

		if (bundleNames.IsEmpty())
		{
			return;
		}

		const void* valuePtr = softObjProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr);
		if (!softPtr)
		{
			return;
		}

		const FSoftObjectPath& path = softPtr->ToSoftObjectPath();
		if (!path.IsValid() || path.IsNull())
		{
			return;
		}

		// Skip native class references (/Script/ paths are always available)
		if (path.GetAssetPath().GetPackageName().ToString().StartsWith(TEXT("/Script/")))
		{
			return;
		}

		const FName qualifiedName = FName(*FString::Printf(TEXT("%s.%s"), *OwnerClassName.ToString(), *Property->GetName()));

		for (const FString& name : bundleNames)
		{
			AddEntryToBundle(FName(*name), path, qualifiedName);
		}
		return;
	}

	// Soft class property: use own AssetBundles metadata, or inherited names from a parent container
	if (const FSoftClassProperty* softClassProp = CastField<FSoftClassProperty>(Property))
	{
		TArray<FString> bundleNames;
		ParseBundleNamesFromProperty(Property, bundleNames);

		if (bundleNames.IsEmpty())
		{
			bundleNames = InheritedBundleNames;
		}

		if (bundleNames.IsEmpty())
		{
			return;
		}

		const void* valuePtr = softClassProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr);
		if (!softPtr)
		{
			return;
		}

		const FSoftObjectPath& path = softPtr->ToSoftObjectPath();
		if (!path.IsValid() || path.IsNull())
		{
			return;
		}

		if (path.GetAssetPath().GetPackageName().ToString().StartsWith(TEXT("/Script/")))
		{
			return;
		}

		const FName qualifiedName = FName(*FString::Printf(TEXT("%s.%s"), *OwnerClassName.ToString(), *Property->GetName()));

		for (const FString& name : bundleNames)
		{
			AddEntryToBundle(FName(*name), path, qualifiedName);
		}
		return;
	}

	// Recurse into struct members
	if (const FStructProperty* structProp = CastField<FStructProperty>(Property))
	{
		const void* structPtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		const FName structTypeName = structProp->Struct->GetFName();
		for (TFieldIterator<FProperty> innerIt(structProp->Struct); innerIt; ++innerIt)
		{
			ExtractBundlesFromProperty(*innerIt, structPtr, TArray<FString>(), structTypeName);
		}
		return;
	}

	// Recurse into array elements, propagating the array's AssetBundles meta to inner elements
	if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(Property))
	{
		TArray<FString> containerBundleNames;
		ParseBundleNamesFromProperty(Property, containerBundleNames);

		const void* arrayPtr = arrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptArrayHelper arrayHelper(arrayProp, arrayPtr);

		for (int32 index = 0; index < arrayHelper.Num(); ++index)
		{
			const void* elementPtr = arrayHelper.GetRawPtr(index);
			ExtractBundlesFromProperty(arrayProp->Inner, elementPtr, containerBundleNames, OwnerClassName);
		}
		return;
	}

	// Recurse into map keys and values, propagating the map's AssetBundles meta to inner elements
	if (const FMapProperty* mapProp = CastField<FMapProperty>(Property))
	{
		TArray<FString> containerBundleNames;
		ParseBundleNamesFromProperty(Property, containerBundleNames);

		const void* mapPtr = mapProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptMapHelper mapHelper(mapProp, mapPtr);

		for (FScriptMapHelper::FIterator itr = mapHelper.CreateIterator(); itr; ++itr)
		{
			// Pass the pair pointer, not GetKeyPtr/GetValuePtr. The key/value properties
			// have internal offsets within the pair layout, so ContainerPtrToValuePtr
			// applies that offset once. Using GetKeyPtr/GetValuePtr would double-offset.
			const uint8* pairPtr = mapHelper.GetPairPtr(itr.GetInternalIndex());
			ExtractBundlesFromProperty(mapProp->KeyProp, pairPtr, containerBundleNames, OwnerClassName);
			ExtractBundlesFromProperty(mapProp->ValueProp, pairPtr, containerBundleNames, OwnerClassName);
		}
		return;
	}

	// Recurse into set elements, propagating the set's AssetBundles meta to inner elements
	if (const FSetProperty* setProp = CastField<FSetProperty>(Property))
	{
		TArray<FString> containerBundleNames;
		ParseBundleNamesFromProperty(Property, containerBundleNames);

		const void* setPtr = setProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptSetHelper setHelper(setProp, setPtr);

		for (FScriptSetHelper::FIterator itr = setHelper.CreateIterator(); itr; ++itr)
		{
			const void* elementPtr = setHelper.GetElementPtr(itr.GetInternalIndex());
			ExtractBundlesFromProperty(setProp->ElementProp, elementPtr, containerBundleNames, OwnerClassName);
		}
	}
}
#endif

void FSGDynamicTextAssetBundleData::AddEntryToBundle(FName BundleName, const FSoftObjectPath& AssetPath, FName PropertyName)
{
	FSGDynamicTextAssetBundle* targetBundle = nullptr;
	for (FSGDynamicTextAssetBundle& bundle : Bundles)
	{
		if (bundle.BundleName == BundleName)
		{
			targetBundle = &bundle;
			break;
		}
	}

	if (!targetBundle)
	{
		FSGDynamicTextAssetBundle& newBundle = Bundles.AddDefaulted_GetRef();
		newBundle.BundleName = BundleName;
		targetBundle = &newBundle;
	}

	targetBundle->Entries.Emplace(AssetPath, PropertyName);
}
