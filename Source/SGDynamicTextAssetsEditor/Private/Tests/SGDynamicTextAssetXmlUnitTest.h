// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetRef.h"

#include "SGDynamicTextAssetXmlUnitTest.generated.h"

/**
 * Minimal struct for XML serializer nested USTRUCT round-trip tests.
 */
USTRUCT()
struct FTestXmlItemStats
{
	GENERATED_BODY()

	/** Base armor rating. */
	UPROPERTY()
	int32 BaseArmor = 0;

	/** Maximum durability. */
	UPROPERTY()
	int32 MaxDurability = 0;
};

/**
 * Extended USGDynamicTextAsset subclass for XML serializer unit testing.
 * Adds complex-type properties (TArray, TMap, TSet, nested struct, soft ref)
 * to cover all required round-trip test cases for FSGDynamicTextAssetXmlSerializer.
 * Editor-only, not exposed to Blueprint.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGDynamicTextAssetXmlUnitTest : public USGDynamicTextAsset
{
	GENERATED_BODY()

public:

	/** Test float property for basic round-trip. */
	UPROPERTY()
	float TestDamage = 0.0f;

	/** Test string property for basic round-trip. */
	UPROPERTY()
	FString TestName;

	/** Test integer property for basic round-trip. */
	UPROPERTY()
	int32 TestCount = 0;

	/** Test array property for TArray<FString> round-trip. */
	UPROPERTY()
	TArray<FString> TestTags;

	/** Test map property for TMap<FString, int32> round-trip. */
	UPROPERTY()
	TMap<FString, int32> TestAttributes;

	/** Test set property for TSet<FString> round-trip. */
	UPROPERTY()
	TSet<FString> TestKeywords;

	/** Test nested struct property for USTRUCT round-trip. */
	UPROPERTY()
	FTestXmlItemStats TestStats;

	/** Test soft object reference for TSoftObjectPtr round-trip. */
	UPROPERTY()
	TSoftObjectPtr<UObject> TestMeshRef;

	/** Test dynamic text asset reference for FSGDynamicTextAssetRef round-trip (ID only). */
	UPROPERTY()
	FSGDynamicTextAssetRef TestRef;
};
