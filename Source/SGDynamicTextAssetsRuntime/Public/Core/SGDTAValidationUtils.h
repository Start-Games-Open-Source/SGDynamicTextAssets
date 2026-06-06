// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ISGDynamicTextAssetProvider;

/**
 * Shared validation utilities for the SGDynamicTextAssets ecosystem.
 *
 * Provides static analysis functions that enforce design principles
 * such as the soft-reference-only rule across dynamic text asset classes.
 */
class SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetValidationUtils
{
public:

	/**
	 * Scans a UClass for UPROPERTY fields that contain hard references.
	 * Hard references include: TSubclassOf<>, TObjectPtr<>, raw UObject*,
	 * and containers (TArray, TMap, TSet) of these types.
	 *
	 * Properties defined on the highest class in the hierarchy that
	 * implements ISGDynamicTextAssetProvider are exempt, since those are part
	 * of the provider contract (e.g., DynamicTextAssetId, UserFacingId,
	 * Version on USGDynamicTextAsset).
	 *
	 * @param InClass The class to scan
	 * @param OutViolations Array of property paths that violate the soft-reference-only rule
	 * @return True if any hard references were found (violations detected)
	 */
	static bool DetectHardReferenceProperties(const UClass* InClass, TArray<FString>& OutViolations);

	/**
	 * Checks whether a property has the CPF_InstancedReference flag,
	 * indicating it is a UPROPERTY(Instanced) sub-object owned inline.
	 *
	 * @param InProperty The property to check
	 * @return True if the property has CPF_InstancedReference set
	 */
	static bool IsInstancedObjectProperty(const FProperty* InProperty);

private:

	/**
	 * Finds the highest class in the hierarchy that implements
	 * ISGDynamicTextAssetProvider. Properties declared on this class are
	 * exempt from hard reference checks because they are part of
	 * the provider contract.
	 *
	 * @param InClass The class to walk upward from
	 * @return The highest implementing class, or nullptr if none found
	 */
	static const UClass* FindProviderBoundaryClass(const UClass* InClass);

	/**
	 * Checks whether a single property is a hard reference type.
	 * Recursively inspects container inner types (TArray, TMap, TSet).
	 *
	 * @param InProperty The property to check
	 * @param OutViolations Populated with violation descriptions if hard references are found
	 * @return True if the property is or contains a hard reference
	 */
	static bool IsHardReferenceProperty(const FProperty* InProperty, TArray<FString>& OutViolations);
};
