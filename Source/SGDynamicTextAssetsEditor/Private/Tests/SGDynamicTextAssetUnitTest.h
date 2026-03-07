// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetRef.h"

#include "SGDynamicTextAssetUnitTest.generated.h"

/**
 * Minimal concrete subclass of USGDynamicTextAsset for unit testing.
 * Editor-only, not exposed to Blueprint.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGDynamicTextAssetUnitTest : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Test float property for serialization roundtrip verification */
	UPROPERTY()
	float TestDamage = 0.0f;

	/** Test string property for serialization roundtrip verification */
	UPROPERTY()
	FString TestName;

	/** Test integer property for serialization roundtrip verification */
	UPROPERTY()
	int32 TestCount = 0;

	/** Test dynamic text asset reference for FSGDynamicTextAssetRef round-trip (ID only). */
	UPROPERTY()
	FSGDynamicTextAssetRef TestRef;
};
