// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Templates/SubclassOf.h"

#include "SGDynamicTextAssetHardReferenceTestTypes.generated.h"

/**
 * Dynamic text asset with a hard TObjectPtr reference (violation).
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGTestDirtyDynamicTextAsset : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Hard reference - should be detected as violation if SGSkipHardRefValidation is removed. */
	//UPROPERTY() // Comment/uncomment to test hard ref validation in SGDynamicTextAssetHardRefValidator.cs
	UPROPERTY(meta = (SGSkipHardRefValidation))
	TObjectPtr<UObject> HardObjectRef = nullptr;

	/** Safe primitive property */
	UPROPERTY()
	FString SafeString;
};

/**
 * Dynamic text asset with a TSubclassOf reference (violation).
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games"
	, meta = (SGSkipHardRefValidation))
class USGTestSubclassOfDynamicTextAsset : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Hard class reference - should be detected as violation if SGSkipHardRefValidation is removed from the class's meta tags. */
	UPROPERTY()
	TSubclassOf<UObject> HardClassRef;
};

/**
 * Dynamic text asset with a TArray containing hard references (violation).
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGTestContainerDynamicTextAsset : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Array of hard references - should be detected as violation if SGSkipHardRefValidation is removed. */
	UPROPERTY(meta = (SGSkipHardRefValidation))
	TArray<TObjectPtr<UObject>> HardObjectArray;

	/** Safe array of strings */
	UPROPERTY()
	TArray<FString> SafeStringArray;
};

/**
 * Dynamic text asset with only soft references and primitives (clean).
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGTestSoftRefDynamicTextAsset : public USGDynamicTextAsset
{
	GENERATED_BODY()
public:

	/** Soft object reference - allowed */
	UPROPERTY()
	TSoftObjectPtr<UObject> SoftObjectRef;

	/** Soft class reference - allowed */
	UPROPERTY()
	TSoftClassPtr<UObject> SoftClassRef;

	/** FSGDynamicTextAssetRef - allowed (resolves by ID) */
	UPROPERTY()
	FSGDynamicTextAssetRef DynamicTextAssetRef;

	/** Primitive - allowed */
	UPROPERTY()
	int32 SafeInt = 0;
};

/**
 * A UObject-based provider that does NOT inherit from USGDynamicTextAsset.
 * Used to verify the UHT hard reference validator detects providers
 * by interface implementation, not by class hierarchy.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGTestCustomProvider : public UObject, public ISGDynamicTextAssetProvider
{
	GENERATED_BODY()
public:

	// ISGDynamicTextAssetProvider interface
	virtual const FSGDynamicTextAssetId& GetDynamicTextAssetId() const override { return Id; }
	virtual void SetDynamicTextAssetId(const FSGDynamicTextAssetId& InId) override { Id = InId; }
	virtual const FString& GetUserFacingId() const override { return UserFacingId; }
	virtual void SetUserFacingId(const FString& InUserFacingId) override { UserFacingId = InUserFacingId; }
	virtual const FSGDynamicTextAssetVersion& GetVersion() const override { return Version; }
	virtual void SetVersion(const FSGDynamicTextAssetVersion& InVersion) override { Version = InVersion; }
	virtual int32 GetCurrentMajorVersion() const override { return 1; }
	virtual void PostDynamicTextAssetLoaded() override {}
	virtual bool Native_ValidateDynamicTextAsset(FSGDynamicTextAssetValidationResult& OutResult) const override { return true; }
	virtual bool MigrateFromVersion(
		const FSGDynamicTextAssetVersion& OldVersion,
		const FSGDynamicTextAssetVersion& CurrentVersion,
		const TSharedPtr<FJsonObject>& OldData) override { return true; }
	// ~ISGDynamicTextAssetProvider interface

	/** Hard reference - should be detected as violation if SGSkipHardRefValidation is removed. */
	UPROPERTY(meta = (SGSkipHardRefValidation))
	TObjectPtr<UObject> HardObjectRef = nullptr;

	/** Safe primitive property */
	UPROPERTY()
	FString SafeString;

private:

	FSGDynamicTextAssetId Id;
	FString UserFacingId;
	FSGDynamicTextAssetVersion Version;
};

/**
 * Subclass of the custom provider (does NOT inherit from USGDynamicTextAsset).
 * Validates that inherited interface detection works for subclasses of custom providers.
 */
UCLASS(NotBlueprintable, NotBlueprintType, MinimalAPI, Hidden, ClassGroup = "Start Games")
class USGTestCustomProviderChild : public USGTestCustomProvider
{
	GENERATED_BODY()
public:

	/** Hard reference - should be detected as violation if SGSkipHardRefValidation is removed. */
	UPROPERTY(meta = (SGSkipHardRefValidation))
	TObjectPtr<UObject> ChildHardObjectRef = nullptr;

	/** Safe primitive */
	UPROPERTY()
	int32 SafeInt = 0;
};
