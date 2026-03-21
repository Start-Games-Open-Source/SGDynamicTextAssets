// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Management/SGDynamicTextAssetTypeManifest.h"

namespace SGTypeRegistryTestUtils
{
	/**
	 * Finds a registered (non-hidden, non-abstract) child class of USGDynamicTextAsset
	 * that has a valid TypeId in the registry. Returns nullptr if none is found.
	 * Test classes with Hidden/Deprecated flags are skipped by SyncManifests,
	 * so this function discovers a class that actually has a TypeId assigned.
	 */
	UClass* FindRegisteredChildClass(USGDynamicTextAssetRegistry* Registry)
	{
		if (!Registry)
		{
			return nullptr;
		}

		TArray<UClass*> allClasses;
		Registry->GetAllRegisteredClasses(allClasses);

		for (UClass* cls : allClasses)
		{
			if (cls == USGDynamicTextAsset::StaticClass())
			{
				continue;
			}

			// Must be a child of the base class with a valid TypeId
			if (cls->IsChildOf(USGDynamicTextAsset::StaticClass()))
			{
				FSGDynamicTextAssetTypeId typeId = Registry->GetTypeIdForClass(cls);
				if (typeId.IsValid())
				{
					return cls;
				}
			}
		}

		return nullptr;
	}
}

// ---------------------------------------------------------------------------
// 1. SyncManifests  - Base Class Has TypeId
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_SyncManifests_BaseClassHasTypeId,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.SyncManifests.BaseClassHasTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_SyncManifests_BaseClassHasTypeId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(USGDynamicTextAsset::StaticClass());
	TestTrue(TEXT("Base USGDynamicTextAsset class should have a valid TypeId"), typeId.IsValid());

	return true;
}

// ---------------------------------------------------------------------------
// 2. SyncManifests  - Child Class Has TypeId
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_SyncManifests_ChildClassHasTypeId,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.SyncManifests.ChildClassHasTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_SyncManifests_ChildClassHasTypeId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* childClass = SGTypeRegistryTestUtils::FindRegisteredChildClass(registry);
	TestNotNull(TEXT("Should find at least one registered child class"), childClass);
	if (!childClass)
	{
		return false;
	}

	FSGDynamicTextAssetTypeId baseTypeId = registry->GetTypeIdForClass(USGDynamicTextAsset::StaticClass());
	FSGDynamicTextAssetTypeId childTypeId = registry->GetTypeIdForClass(childClass);

	TestTrue(TEXT("Child class should have a valid TypeId"), childTypeId.IsValid());
	TestTrue(TEXT("Child TypeId should differ from base TypeId"), childTypeId != baseTypeId);

	return true;
}

// ---------------------------------------------------------------------------
// 3. SyncManifests  - Manifest Exists For Root
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_SyncManifests_ManifestExistsForRoot,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.SyncManifests.ManifestExistsForRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_SyncManifests_ManifestExistsForRoot::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	const FSGDynamicTextAssetTypeManifest* manifest =
		registry->GetManifestForRootClass(USGDynamicTextAsset::StaticClass());

	TestNotNull(TEXT("Manifest should exist for the root base class"), manifest);

	return true;
}

// ---------------------------------------------------------------------------
// 4. SyncManifests  - Manifest Contains Child Class
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_SyncManifests_ManifestContainsChildClass,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.SyncManifests.ManifestContainsChildClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_SyncManifests_ManifestContainsChildClass::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* childClass = SGTypeRegistryTestUtils::FindRegisteredChildClass(registry);
	TestNotNull(TEXT("Should find at least one registered child class"), childClass);
	if (!childClass)
	{
		return false;
	}

	const FSGDynamicTextAssetTypeManifest* manifest =
		registry->GetManifestForRootClass(USGDynamicTextAsset::StaticClass());
	TestNotNull(TEXT("Manifest should exist"), manifest);
	if (!manifest)
	{
		return false;
	}

	// ClassName in manifest is the UClass name (without U prefix in the reflection system)
	FString className = childClass->GetName();
	const FSGDynamicTextAssetTypeManifestEntry* entry = manifest->FindByClassName(className);
	TestNotNull(TEXT("Manifest should contain the child class"), entry);

	if (entry)
	{
		// Verify the TypeId matches what the registry reports for this class
		FSGDynamicTextAssetTypeId expectedTypeId = registry->GetTypeIdForClass(childClass);
		TestTrue(TEXT("Manifest entry TypeId should match registry TypeId"),
			entry->TypeId == expectedTypeId);
	}

	return true;
}

// ---------------------------------------------------------------------------
// 5. ResolveClass  - Valid TypeId
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_ResolveClass_ValidTypeId,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.ResolveClass.ValidTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_ResolveClass_ValidTypeId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* childClass = SGTypeRegistryTestUtils::FindRegisteredChildClass(registry);
	TestNotNull(TEXT("Should find at least one registered child class"), childClass);
	if (!childClass)
	{
		return false;
	}

	FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(childClass);
	TestTrue(TEXT("TypeId should be valid"), typeId.IsValid());

	UClass* resolvedClass = registry->ResolveClassForTypeId(typeId);
	TestNotNull(TEXT("ResolveClassForTypeId should return a valid class"), resolvedClass);
	TestEqual(TEXT("Resolved class should match the original class"),
		resolvedClass, childClass);

	return true;
}

// ---------------------------------------------------------------------------
// 6. ResolveClass  - Invalid TypeId
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_ResolveClass_InvalidTypeId,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.ResolveClass.InvalidTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_ResolveClass_InvalidTypeId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* resolvedClass = registry->ResolveClassForTypeId(FSGDynamicTextAssetTypeId::INVALID_TYPE_ID);
	TestNull(TEXT("ResolveClassForTypeId with INVALID_TYPE_ID should return nullptr"), resolvedClass);

	return true;
}

// ---------------------------------------------------------------------------
// 7. GetSoftClass  - Valid TypeId
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_GetSoftClass_ValidTypeId,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.GetSoftClass.ValidTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_GetSoftClass_ValidTypeId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* childClass = SGTypeRegistryTestUtils::FindRegisteredChildClass(registry);
	TestNotNull(TEXT("Should find at least one registered child class"), childClass);
	if (!childClass)
	{
		return false;
	}

	FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(childClass);

	TSoftClassPtr<UObject> softClass = registry->GetSoftClassForTypeId(typeId);
	TestFalse(TEXT("GetSoftClassForTypeId should return a non-null soft class ptr"),
		softClass.IsNull());

	return true;
}

// ---------------------------------------------------------------------------
// 8. FolderPath  - Contains TypeId
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_FolderPath_ContainsTypeId,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.FolderPath.ContainsTypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_FolderPath_ContainsTypeId::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* childClass = SGTypeRegistryTestUtils::FindRegisteredChildClass(registry);
	TestNotNull(TEXT("Should find at least one registered child class"), childClass);
	if (!childClass)
	{
		return false;
	}

	FSGDynamicTextAssetTypeId childTypeId = registry->GetTypeIdForClass(childClass);
	TestTrue(TEXT("Child TypeId should be valid"), childTypeId.IsValid());

	FString folderPath = FSGDynamicTextAssetFileManager::GetRelativeFolderPathForClass(childClass);

	TestTrue(TEXT("Folder path should contain the child class TypeId string"),
		folderPath.Contains(childTypeId.ToString()));

	return true;
}

// ---------------------------------------------------------------------------
// 9. FolderPath  - Hierarchy Segments
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_FolderPath_HierarchySegments,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.FolderPath.HierarchySegments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_FolderPath_HierarchySegments::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* childClass = SGTypeRegistryTestUtils::FindRegisteredChildClass(registry);
	TestNotNull(TEXT("Should find at least one registered child class"), childClass);
	if (!childClass)
	{
		return false;
	}

	// Base class folder path  - the root class itself is the boundary
	FString baseFolderPath = FSGDynamicTextAssetFileManager::GetRelativeFolderPathForClass(
		USGDynamicTextAsset::StaticClass());

	// Child class folder path  - should have at least one segment beyond the root
	FString childFolderPath = FSGDynamicTextAssetFileManager::GetRelativeFolderPathForClass(childClass);

	TestFalse(TEXT("Base folder path should not be empty"), baseFolderPath.IsEmpty());
	TestFalse(TEXT("Child folder path should not be empty"), childFolderPath.IsEmpty());

	// Child path should be longer than (or at minimum different from) base path
	// because it has an additional hierarchy segment
	TestTrue(TEXT("Child folder path should be longer than base folder path"),
		childFolderPath.Len() > baseFolderPath.Len());

	// Child path should start with the same root directory as the base path
	TestTrue(TEXT("Child folder path should start with the DTA root directory"),
		childFolderPath.Contains(TEXT("SGDynamicTextAssets")));

	return true;
}

// ---------------------------------------------------------------------------
// 10. TypeId Roundtrip  - Class → TypeId → Class
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRegistry_TypeIdRoundtrip_ClassToTypeIdToClass,
	"SGDynamicTextAssets.Runtime.Management.Registry.TypeId.TypeIdRoundtrip.ClassToTypeIdToClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRegistry_TypeIdRoundtrip_ClassToTypeIdToClass::RunTest(const FString& Parameters)
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	TestNotNull(TEXT("Registry singleton should exist"), registry);
	if (!registry)
	{
		return false;
	}

	UClass* childClass = SGTypeRegistryTestUtils::FindRegisteredChildClass(registry);
	TestNotNull(TEXT("Should find at least one registered child class"), childClass);
	if (!childClass)
	{
		return false;
	}

	// Forward: Class → TypeId
	FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(childClass);
	TestTrue(TEXT("TypeId should be valid"), typeId.IsValid());

	// Reverse: TypeId → Class
	UClass* resolvedClass = registry->ResolveClassForTypeId(typeId);
	TestEqual(TEXT("Roundtrip should resolve back to the original class"),
		resolvedClass, childClass);

	return true;
}
