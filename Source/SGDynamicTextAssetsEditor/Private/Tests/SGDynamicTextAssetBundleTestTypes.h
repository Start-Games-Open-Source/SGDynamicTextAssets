// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAsset.h"

#include "SGDynamicTextAssetBundleTestTypes.generated.h"

/**
 * Test struct containing a bundled soft reference inside a struct.
 */
USTRUCT()
struct FSGBundleTestStruct
{
	GENERATED_BODY()
public:

	/** Soft reference inside a struct, tagged with a bundle. */
	UPROPERTY(meta = (AssetBundles = "StructBundle"))
	TSoftObjectPtr<UObject> StructSoftRef;

	/** Non-bundled primitive for comparison. */
	UPROPERTY()
	int32 StructInt = 0;
};

/**
 * Instanced sub-object for bundle testing.
 * Contains a soft reference tagged with a bundle.
 */
UCLASS(EditInlineNew, NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden)
class USGBundleTestInstancedObject : public UObject
{
	GENERATED_BODY()
public:

	/** Bundled soft reference on an instanced sub-object. */
	UPROPERTY(meta = (AssetBundles = "InstancedBundle"))
	TSoftObjectPtr<UObject> InstancedSoftRef;
};

/**
 * Dynamic text asset with various bundled soft reference patterns for testing.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGBundleTestDynamicTextAsset : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Single soft reference in one bundle. */
	UPROPERTY(meta = (AssetBundles = "Visual"))
	TSoftObjectPtr<UObject> VisualRef;

	/** Single soft reference in another bundle. */
	UPROPERTY(meta = (AssetBundles = "Audio"))
	TSoftObjectPtr<UObject> AudioRef;

	/** Soft reference in multiple bundles (comma-separated). */
	UPROPERTY(meta = (AssetBundles = "Visual,Audio"))
	TSoftObjectPtr<UObject> SharedRef;

	/** Soft class reference in a bundle. */
	UPROPERTY(meta = (AssetBundles = "Classes"))
	TSoftClassPtr<UObject> ClassRef;

	/** Soft reference with NO bundle tag (should be excluded). */
	UPROPERTY()
	TSoftObjectPtr<UObject> UnbundledRef;

	/** Array of bundled soft references. */
	UPROPERTY(meta = (AssetBundles = "ArrayBundle"))
	TArray<TSoftObjectPtr<UObject>> ArrayRefs;

	/** Map with bundled soft reference values. */
	UPROPERTY(meta = (AssetBundles = "MapBundle"))
	TMap<FName, TSoftObjectPtr<UObject>> MapRefs;

	/** Set of bundled soft references. */
	UPROPERTY(meta = (AssetBundles = "SetBundle"))
	TSet<TSoftObjectPtr<UObject>> SetRefs;

	/** Struct containing a bundled soft reference. */
	UPROPERTY()
	FSGBundleTestStruct StructWithBundle;

	/** Instanced sub-object containing a bundled soft reference. */
	UPROPERTY(Instanced)
	TObjectPtr<USGBundleTestInstancedObject> InstancedObj = nullptr;

	/** Plain primitive (no bundle relevance). */
	UPROPERTY()
	float TestFloat = 0.0f;
};
