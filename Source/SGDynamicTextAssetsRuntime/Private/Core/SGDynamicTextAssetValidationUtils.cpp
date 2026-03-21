// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetValidationUtils.h"

#include "SGDynamicTextAssetLogs.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAssetRef.h"

bool FSGDynamicTextAssetValidationUtils::DetectHardReferenceProperties(const UClass* InClass, TArray<FString>& OutViolations)
{
	if (!InClass)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("DetectHardReferenceProperties: Inputted NULL InClass"));
		return false;
	}

	// Find the boundary class so we can skip provider contract properties
	const UClass* boundaryClass = FindProviderBoundaryClass(InClass);

	for (TFieldIterator<FProperty> It(InClass, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		const FProperty* property = *It;
		if (!property)
		{
			continue;
		}
		// Skip over depreacted and transient properties
		if (property->HasAnyPropertyFlags(CPF_Deprecated|CPF_Transient))
		{
			continue;
		}

		// Skip properties declared on the boundary class or its ancestors.
		// These are provider contract properties (DynamicTextAssetId, UserFacingId, Version, etc.).
		// BoundaryClass->IsChildOf(OwnerClass) is true when the owner is the boundary
		// itself or any ancestor above it in the hierarchy.
		if (boundaryClass && property->GetOwnerClass() &&
			boundaryClass->IsChildOf(property->GetOwnerClass()))
		{
			continue;
		}

		IsHardReferenceProperty(property, OutViolations);
	}

	return !OutViolations.IsEmpty();
}

const UClass* FSGDynamicTextAssetValidationUtils::FindProviderBoundaryClass(const UClass* InClass)
{
	if (!InClass)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FindProviderBoundaryClass: Inputted NULL InClass"));
		return nullptr;
	}

	const UClass* highestProvider = nullptr;

	// Walk upward from the given class to UObject, tracking the highest
	// class that implements ISGDynamicTextAssetProvider
	for (const UClass* current = InClass; current != nullptr; current = current->GetSuperClass())
	{
		if (current->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
		{
			highestProvider = current;
		}
	}

	return highestProvider;
}

bool FSGDynamicTextAssetValidationUtils::IsHardReferenceProperty(const FProperty* InProperty, TArray<FString>& OutViolations)
{
	if (!InProperty)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("IsHardReferenceProperty: Inputted NULL InProperty"));
		return false;
	}

	// Soft references are always allowed
	if (InProperty->IsA<FSoftObjectProperty>() || InProperty->IsA<FSoftClassProperty>())
	{
		return false;
	}

	// FSGDynamicTextAssetRef struct is allowed (it resolves by ID, not hard ref)
	if (const FStructProperty* structProp = CastField<FStructProperty>(InProperty))
	{
		if (structProp->Struct == FSGDynamicTextAssetRef::StaticStruct())
		{
			return false;
		}

		// Other structs are fine  - they don't hold hard references by themselves
		return false;
	}

	// FClassProperty must be checked before FObjectProperty since FClassProperty
	// inherits from FObjectProperty. TSubclassOf<> is a hard class reference.
	if (InProperty->IsA<FClassProperty>())
	{
		OutViolations.Add(FString::Printf(
			TEXT("%s (TSubclassOf<>) - Use TSoftClassPtr<> instead"),
			*InProperty->GetName()));
		return true;
	}

	// Instanced objects `UPROPERTY(Instanced)1 are owned sub-objects serialized inline,
	// not external asset references. Allow them if they meet two conditions:
	// 1. The referenced class has EditInlineNew (required for instanced objects)
	// 2. The referenced class itself has no nested hard references
	if (InProperty->HasAnyPropertyFlags(CPF_InstancedReference))
	{
		const FObjectProperty* objectProp = CastField<FObjectProperty>(InProperty);
		if (objectProp && objectProp->PropertyClass)
		{
			// Check 1: EditInlineNew is required for instanced sub-objects
			if (!objectProp->PropertyClass->HasAnyClassFlags(CLASS_EditInlineNew))
			{
				OutViolations.Add(FString::Printf(
					TEXT("%s (Instanced UObject) class %s does not have EditInlineNew, treated as hard reference which is not permitted. Either add EditInlineNew to the class or make this a soft reference."),
					*InProperty->GetName(),
					*objectProp->PropertyClass->GetName()));
				return true;
			}

			// Check 2: Recursively check the instanced class for nested hard references
			TArray<FString> nestedViolations;
			if (DetectHardReferenceProperties(objectProp->PropertyClass, nestedViolations))
			{
				for (const FString& nested : nestedViolations)
				{
					OutViolations.Add(FString::Printf(
						TEXT("%s (Instanced UObject %s) nested property -> %s"),
						*InProperty->GetName(),
						*objectProp->PropertyClass->GetName(),
						*nested));
				}
				return true;
			}

			// Both checks passed, instanced object is allowed
			return false;
		}
	}

	// TObjectPtr<> or raw UObject* - hard object reference
	if (InProperty->IsA<FObjectProperty>())
	{
		OutViolations.Add(FString::Printf(
			TEXT("%s (TObjectPtr<>/UObject*) - Use TSoftObjectPtr<> instead"),
			*InProperty->GetName()));
		return true;
	}

	// TArray<>  - check inner property recursively
	if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(InProperty))
	{
		if (arrayProp->Inner)
		{
			const int32 countBefore = OutViolations.Num();
			IsHardReferenceProperty(arrayProp->Inner, OutViolations);

			// Annotate violations from inner with the container context
			for (int32 i = countBefore; i < OutViolations.Num(); ++i)
			{
				OutViolations[i] = FString::Printf(
					TEXT("%s (TArray element) -> %s"),
					*InProperty->GetName(),
					*OutViolations[i]);
			}

			return OutViolations.Num() > countBefore;
		}

		return false;
	}

	// TMap<>  - check key and value properties recursively
	if (const FMapProperty* mapProp = CastField<FMapProperty>(InProperty))
	{
		const int32 countBefore = OutViolations.Num();

		if (mapProp->KeyProp)
		{
			const int32 keyCountBefore = OutViolations.Num();
			IsHardReferenceProperty(mapProp->KeyProp, OutViolations);

			for (int32 i = keyCountBefore; i < OutViolations.Num(); ++i)
			{
				OutViolations[i] = FString::Printf(
					TEXT("%s (TMap key) -> %s"),
					*InProperty->GetName(),
					*OutViolations[i]);
			}
		}

		if (mapProp->ValueProp)
		{
			const int32 valueCountBefore = OutViolations.Num();
			IsHardReferenceProperty(mapProp->ValueProp, OutViolations);

			for (int32 i = valueCountBefore; i < OutViolations.Num(); ++i)
			{
				OutViolations[i] = FString::Printf(
					TEXT("%s (TMap value) -> %s"),
					*InProperty->GetName(),
					*OutViolations[i]);
			}
		}

		return OutViolations.Num() > countBefore;
	}

	// TSet<>  - check inner property recursively
	if (const FSetProperty* setProp = CastField<FSetProperty>(InProperty))
	{
		if (setProp->ElementProp)
		{
			const int32 countBefore = OutViolations.Num();
			IsHardReferenceProperty(setProp->ElementProp, OutViolations);

			for (int32 i = countBefore; i < OutViolations.Num(); ++i)
			{
				OutViolations[i] = FString::Printf(
					TEXT("%s (TSet element) -> %s"),
					*InProperty->GetName(),
					*OutViolations[i]);
			}

			return OutViolations.Num() > countBefore;
		}

		return false;
	}

	// All other property types (int32, float, FString, FName, enums, etc.) are fine
	return false;
}

bool FSGDynamicTextAssetValidationUtils::IsInstancedObjectProperty(const FProperty* InProperty)
{
	return InProperty && InProperty->HasAnyPropertyFlags(CPF_InstancedReference);
}
