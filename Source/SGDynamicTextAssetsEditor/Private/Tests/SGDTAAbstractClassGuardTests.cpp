// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDynamicTextAsset.h"
#include "Management/SGDynamicTextAssetRegistry.h"

// These tests lock in the shared invariant that the create-dialog validation guard and the
// editor toolkit load guard both rely on: the abstract dynamic text asset base class must be
// recognised as non-instantiable so that neither the create flow writes a file with it nor the
// load flow tries to instantiate it (which would crash NewObject).
//
// The guards themselves are not exercised directly here. The create dialog's ValidateInputs and
// SelectedClass are private with no injection seam, and the toolkit's LoadFromFile and FilePath
// are private with the only public entry point (OpenEditorForFile) spawning a real editor window.
// Neither is callable headless without invasive friending, so the behaviour is covered by this
// invariant test plus manual verification.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAbstractGuard_BaseClass_IsAbstract,
	"SGDynamicTextAssets.Editor.AbstractClassGuard.BaseClass.IsAbstract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAbstractGuard_BaseClass_IsAbstract::RunTest(const FString& Parameters)
{
	UClass* baseClass = USGDynamicTextAsset::StaticClass();
	TestNotNull(TEXT("Base class should exist"), baseClass);
	if (!baseClass)
	{
		return false;
	}

	TestTrue(TEXT("Base USGDynamicTextAsset class should carry CLASS_Abstract"),
		baseClass->HasAnyClassFlags(CLASS_Abstract));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAbstractGuard_Registry_BaseClassNotInstantiable,
	"SGDynamicTextAssets.Editor.AbstractClassGuard.Registry.BaseClassNotInstantiable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAbstractGuard_Registry_BaseClassNotInstantiable::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	TestFalse(TEXT("Registry should report the abstract base class as non-instantiable"),
		registry->IsInstantiableClass(USGDynamicTextAsset::StaticClass()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAbstractGuard_Registry_InstantiableListExcludesBase,
	"SGDynamicTextAssets.Editor.AbstractClassGuard.Registry.InstantiableListExcludesBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAbstractGuard_Registry_InstantiableListExcludesBase::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	TArray<UClass*> instantiableClasses;
	registry->GetAllInstantiableClasses(instantiableClasses);

	// The picker seeds the create dialog from this list, so it must never contain the abstract
	// base class. Every entry it returns must also be instantiable and non-abstract.
	TestFalse(TEXT("Instantiable list should not contain the abstract base class"),
		instantiableClasses.Contains(USGDynamicTextAsset::StaticClass()));

	for (UClass* cls : instantiableClasses)
	{
		if (!cls)
		{
			continue;
		}

		TestFalse(
			FString::Printf(TEXT("Instantiable list entry '%s' should not be abstract"), *cls->GetName()),
			cls->HasAnyClassFlags(CLASS_Abstract));
		TestTrue(
			FString::Printf(TEXT("Instantiable list entry '%s' should be reported instantiable"), *cls->GetName()),
			registry->IsInstantiableClass(cls));
	}

	return true;
}
