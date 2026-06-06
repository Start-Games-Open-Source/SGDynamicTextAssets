// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAsset.h"
#include "StructUtils/InstancedStruct.h"

#include "SGDTAInstancedTestTypes.generated.h"

/**
 * Base instanced UObject for testing UPROPERTY(Instanced) serialization.
 * EditInlineNew allows it to be created inline as a sub-object of a DTA.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, EditInlineNew, ClassGroup = "Start Games")
class USGDTATestInstancedBase : public UObject
{
	GENERATED_BODY()
public:

	/** Test string property. */
	UPROPERTY()
	FString Name;

	/** Test float property. */
	UPROPERTY()
	float Value = 0.0f;
};

/**
 * Derived instanced UObject for testing polymorphic instanced serialization.
 * Extends USGDTATestInstancedBase with an additional property.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, EditInlineNew, ClassGroup = "Start Games")
class USGDTATestInstancedDerived : public USGDTATestInstancedBase
{
	GENERATED_BODY()
public:

	/** Additional integer property unique to the derived type. */
	UPROPERTY()
	int32 ExtraData = 0;
};

/**
 * Instanced UObject that contains a hard reference.
 * Used to verify recursive validation detects hard refs inside instanced objects.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, EditInlineNew, ClassGroup = "Start Games")
class USGDTATestInstancedWithHardRef : public UObject
{
	GENERATED_BODY()
public:

	/** Hard reference inside an instanced object, should be caught by recursive validation. */
	UPROPERTY()
	TObjectPtr<UObject> HardRef = nullptr;
};

/**
 * Base USTRUCT for testing FInstancedStruct serialization round-trips.
 */
USTRUCT()
struct FTestInstancedStructBase
{
	GENERATED_BODY()

	/** Test integer property. */
	UPROPERTY()
	int32 BaseValue = 0;

	/** Test string property. */
	UPROPERTY()
	FString BaseName;
};

/**
 * Derived USTRUCT for testing polymorphic FInstancedStruct serialization.
 */
USTRUCT()
struct FTestInstancedStructDerived : public FTestInstancedStructBase
{
	GENERATED_BODY()

	/** Additional float property unique to the derived struct. */
	UPROPERTY()
	float DerivedFloat = 0.0f;
};

/**
 * DTA subclass that owns instanced object properties.
 * Tests that properly configured instanced objects (EditInlineNew, no nested hard refs)
 * are allowed by the validation system.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGDTATestInstancedOwnerDTA : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Single instanced sub-object property. */
	UPROPERTY(Instanced)
	TObjectPtr<USGDTATestInstancedBase> SingleInstanced = nullptr;

	/** Array of instanced sub-objects (supports polymorphic entries). */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<USGDTATestInstancedBase>> InstancedArray;

	/** Set of instanced sub-objects. */
	UPROPERTY(Instanced)
	TSet<TObjectPtr<USGDTATestInstancedBase>> InstancedSet;

	/** Map with instanced sub-object values (string keys). */
	UPROPERTY(Instanced)
	TMap<FString, TObjectPtr<USGDTATestInstancedBase>> InstancedMap;

	/** Plain string property to verify mixed property types serialize correctly. */
	UPROPERTY()
	FString PlainString;
};

/**
 * DTA subclass that owns FInstancedStruct properties.
 * Tests round-trip serialization of instanced structs through the pipeline.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGDTATestInstancedStructDTA : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Single instanced struct (polymorphic struct container). */
	UPROPERTY()
	FInstancedStruct SingleStruct;

	/** Array of instanced structs with potentially different types. */
	UPROPERTY()
	TArray<FInstancedStruct> StructArray;
};

/**
 * DTA subclass with an instanced object that contains a hard reference.
 * Used to verify recursive validation catches hard refs inside instanced sub-objects.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGDTATestInstancedHardRefOwnerDTA : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Instanced object containing a hard reference and should fail validation. */
	//UPROPERTY(Instanced) // UNCOMMENT THIS TO TEST NESTED INSTANCED OBJECT VALIDATION
	UPROPERTY(Instanced, meta = (SGSkipHardRefValidation)) // COMMENT THIS TO TEST NESTED INSTANCED OBJECT VALIDATION
	TObjectPtr<USGDTATestInstancedWithHardRef> BadInstanced = nullptr;
};
