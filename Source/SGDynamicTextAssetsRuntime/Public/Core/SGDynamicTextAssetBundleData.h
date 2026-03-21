// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

#include "SGDynamicTextAssetBundleData.generated.h"

/**
 * A single entry in an asset bundle, representing one soft reference
 * and the property it was extracted from.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetBundleEntry
{
	GENERATED_BODY()
public:

	FSGDynamicTextAssetBundleEntry() = default;

	FSGDynamicTextAssetBundleEntry(const FSoftObjectPath& InAssetPath, FName InPropertyName);

	/** The soft object path to the referenced asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bundle")
	FSoftObjectPath AssetPath;

	/** The property name this reference was extracted from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bundle")
	FName PropertyName = NAME_None;
};

/**
 * A named bundle grouping one or more soft reference entries.
 * Bundle names correspond to the values in meta=(AssetBundles="...") tags.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetBundle
{
	GENERATED_BODY()
public:

	FSGDynamicTextAssetBundle() = default;

	bool operator==(const FName& Other) const;
	bool operator!=(const FName& Other) const;

	/** The name of this bundle (e.g., "Visual", "Audio"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bundle")
	FName BundleName = NAME_None;

	/** All soft reference entries belonging to this bundle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bundle")
	TArray<FSGDynamicTextAssetBundleEntry> Entries;
};

/**
 * Container for all asset bundle metadata extracted from a UObject.
 *
 * Walks UPROPERTY fields tagged with meta=(AssetBundles="...") on
 * FSoftObjectProperty and FSoftClassProperty members, parses
 * comma-separated bundle names, and collects the referenced
 * FSoftObjectPath values into named bundles.
 *
 * Supports recursive extraction through FStructProperty,
 * FArrayProperty, FMapProperty, FSetProperty, and instanced
 * sub-objects (CPF_InstancedReference).
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetBundleData
{
	GENERATED_BODY()
public:

	FSGDynamicTextAssetBundleData() = default;

	/** Returns true if any bundles have been extracted. */
	bool HasBundles() const;

	/**
	 * Finds a bundle by name.
	 *
	 * @param BundleName The bundle name to search for
	 * @return Pointer to the found bundle, or nullptr if not found
	 */
	const FSGDynamicTextAssetBundle* FindBundle(FName BundleName) const;

	/**
	 * Populates the output array with all bundle names.
	 *
	 * @param OutBundleNames Array to populate with bundle names
	 */
	void GetBundleNames(TArray<FName>& OutBundleNames) const;

	/**
	 * Collects all soft object paths for a specific bundle.
	 * Appends to OutPaths without clearing it first.
	 *
	 * @param BundleName The bundle to get paths for
	 * @param OutPaths Array to append soft object paths to
	 * @return True if the bundle was found and had entries
	 */
	bool GetPathsForBundle(FName BundleName, TArray<FSoftObjectPath>& OutPaths) const;

	/** Clears all bundle data. */
	void Reset();

	/**
	 * Extracts bundle metadata from a UObject by walking its UClass properties.
	 *
	 * Iterates all UPROPERTY fields using TFieldIterator. For each
	 * FSoftObjectProperty or FSoftClassProperty that has
	 * meta=(AssetBundles="..."), parses the comma-separated bundle
	 * names and collects the current FSoftObjectPath value into
	 * the corresponding bundles.
	 *
	 * Recursively handles FStructProperty, FArrayProperty,
	 * FMapProperty, FSetProperty, and instanced sub-objects
	 * (CPF_InstancedReference).
	 *
	 * @param Object The UObject to extract bundle data from
	 */
	void ExtractFromObject(const UObject* Object);

	/** All bundles extracted from the object's properties. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bundle")
	TArray<FSGDynamicTextAssetBundle> Bundles;

private:

#if WITH_EDITORONLY_DATA
	/**
	 * Recursively walks a property to find soft references with AssetBundles metadata.
	 * Adds matching entries to the internal bundle collection.
	 * Only available in editor builds where property metadata is present.
	 *
	 * @param Property The property to inspect
	 * @param ContainerPtr Pointer to the struct/object containing this property
	 * @param InheritedBundleNames Optional bundle names inherited from a parent container
	 *        property (e.g., TArray or TMap tagged with AssetBundles). When non-empty,
	 *        inner soft references use these names instead of requiring their own metadata.
	 * @param OwnerClassName The UClass or UScriptStruct name that owns this property.
	 *        Used to qualify PropertyName entries as "OwnerClass.PropertyName" so that
	 *        same-named properties from different types are distinguishable in serialized output.
	 */
	void ExtractBundlesFromProperty(const FProperty* Property, const void* ContainerPtr,
		const TArray<FString>& InheritedBundleNames = TArray<FString>(),
		FName OwnerClassName = NAME_None);
#endif

	/**
	 * Adds an asset path to the named bundle, creating the bundle if it does not exist.
	 *
	 * @param BundleName The bundle to add to
	 * @param AssetPath The soft object path to add
	 * @param PropertyName The property the path was extracted from
	 */
	void AddEntryToBundle(FName BundleName, const FSoftObjectPath& AssetPath, FName PropertyName);
};
