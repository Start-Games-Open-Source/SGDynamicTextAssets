// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetTypeId.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/SoftObjectPtr.h"

#include "SGDynamicTextAssetRegistry.generated.h"

class FJsonObject;
class FSGDynamicTextAssetTypeManifest;

/** Delegate broadcast when the registry changes (class registered/unregistered). */
DECLARE_MULTICAST_DELEGATE(FOnDynamicTextAssetRegistryChanged);

/** Delegate broadcast when type manifests are synced or updated. */
DECLARE_MULTICAST_DELEGATE(FOnTypeManifestUpdated);

/**
 * Singleton registry for managing dynamic text asset class registrations.
 *
 * Modules that define base dynamic text asset types should register them here
 * during their StartupModule. The registry maintains a list of all valid
 * dynamic text asset classes and provides lookup functionality.
 *
 * Registration guards enforce that:
 * - The class implements ISGDynamicTextAssetProvider
 * - The class is NOT derived from AActor or UActorComponent
 *
 * Usage:
 *   USGDynamicTextAssetRegistry::GetChecked().RegisterDynamicTextAssetClass(UMyDynamicTextAsset::StaticClass());
 */
UCLASS(NotBlueprintType, NotBlueprintable, ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetRegistry : public UEngineSubsystem
{
    GENERATED_BODY()
public:

    /** Gets the singleton instance of the registry */
    static USGDynamicTextAssetRegistry* Get();
    static USGDynamicTextAssetRegistry& GetChecked();

    // USubsystem overrides
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // ~USubsystem overrides

    /**
     * Registers a base dynamic text asset class.
     * Child classes are automatically included via reflection.
     *
     * The class must implement ISGDynamicTextAssetProvider and must NOT be
     * derived from AActor or UActorComponent. Registration will fail
     * with an error log if these conditions are not met.
     *
     * @param DynamicTextAssetClass The class to register (must implement ISGDynamicTextAssetProvider)
     * @return True if registration was successful
     */
    bool RegisterDynamicTextAssetClass(UClass* DynamicTextAssetClass);

    /**
     * Unregisters a previously registered dynamic text asset class.
     *
     * @param DynamicTextAssetClass The class to unregister
     * @return True if the class was found and unregistered
     */
    bool UnregisterDynamicTextAssetClass(UClass* DynamicTextAssetClass);

    /**
     * Gets all registered base classes and their children.
     * Uses UClass reflection to find all derived classes.
     * 
     * @param OutClasses Array to populate with all valid dynamic text asset classes
     */
    void GetAllRegisteredClasses(TArray<UClass*>& OutClasses) const;

    /**
     * Gets only the explicitly registered base classes (not their children).
     * 
     * @param OutClasses Array to populate with registered base classes
     */
    void GetAllRegisteredBaseClasses(TArray<UClass*>& OutClasses) const;

    /**
     * Checks if a given class is a registered base class or a derivative of a registered class.
     *
     * @param DynamicTextAssetClass The class to check for registration
     * @return True if the class is registered or derived from a registered class, false otherwise
     */
    bool IsRegisteredClass(UClass* DynamicTextAssetClass) const;

    /**
     * Gets all non-abstract dynamic text asset classes that can be instantiated.
     *
     * @param OutClasses Array to populate with instantiable classes
     */
    void GetAllInstantiableClasses(TArray<UClass*>& OutClasses) const;

    /**
     * Returns true if the inputted class is registered and instanceable.
     * @param DynamicTextAssetClass The dynamic text asset class to evaluate.
     */
    bool IsInstantiableClass(UClass* DynamicTextAssetClass) const;

    /**
     * Gets the folder path for storing dynamic text assets of a given class.
     * Path is based on class inheritance hierarchy.
     * Each segment is formatted as {ClassName}_{TypeId} when a valid TypeId is available.
     * Falls back to class name only when TypeIds have not yet been assigned.
     *
     * @param DynamicTextAssetClass The class to get the folder path for
     * @return Relative folder path (e.g., "USGDynamicTextAsset_{TypeId}/UWeaponData_{TypeId}/UMeleeWeaponData_{TypeId}")
     */
    FString GetFolderPathForClass(UClass* DynamicTextAssetClass) const;

    /**
     * Checks if a class is a valid dynamic text asset class (registered or child of registered).
     * 
     * @param TestClass The class to check
     * @return True if the class is valid for use as a dynamic text asset
     */
    bool IsValidDynamicTextAssetClass(UClass* TestClass) const;

    /**
     * Returns the type ID for a given class.
     *
     * @param DynamicTextAssetClass The class to look up
     * @return The type ID, or an invalid ID if the class is not in any manifest
     */
    FSGDynamicTextAssetTypeId GetTypeIdForClass(const UClass* DynamicTextAssetClass) const;

    /**
     * Returns the soft class pointer for a given type ID.
     *
     * @param TypeId The type ID to look up
     * @return The soft class pointer, or a null TSoftClassPtr if not found
     */
    TSoftClassPtr<UObject> GetSoftClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;

    /**
     * Resolves a type ID to its UClass*.
     * Returns nullptr if the class is not currently loaded.
     *
     * @param TypeId The type ID to resolve
     * @return The UClass*, or nullptr
     */
    UClass* ResolveClassForTypeId(const FSGDynamicTextAssetTypeId& TypeId) const;

    /**
     * Returns the type manifest for a registered root base class.
     *
     * @param RootClass A class previously registered via RegisterDynamicTextAssetClass
     * @return Pointer to the manifest, or nullptr if not a registered root
     */
    const FSGDynamicTextAssetTypeManifest* GetManifestForRootClass(const UClass* RootClass) const;

    /**
     * Synchronizes type manifests with the current class hierarchy.
     * For each registered root class: loads the manifest from disk (or creates a new one),
     * walks all descendants via reflection, assigns type IDs to new classes,
     * saves if changed, and rebuilds the TypeId/Class lookup maps.
     */
    void SyncManifests();

    /**
     * Loads pre-cooked type manifests from the cooked _TypeManifests/ directory.
     * Called during Initialize() in packaged builds where source manifests are unavailable.
     * Resolves soft class references and populates the TypeId/Class lookup maps.
     */
    void LoadCookedManifests();

    /**
     * Applies server-provided type overrides to all matching manifests.
     * Routes overrides by root type ID and rebuilds the TypeId/Class lookup maps.
     * No-op in editor builds (server overlay is runtime-only).
     *
     * Expected JSON format:
     * {
     *   "manifests": {
     *     "<rootTypeId>": { "types": [{ "typeId": "...", "className": "...", "parentTypeId": "..." }] },
     *     ...
     *   }
     * }
     *
     * @param ServerData JSON object containing server type overrides keyed by root type ID
     */
    void ApplyServerTypeOverrides(const TSharedPtr<FJsonObject>& ServerData);

    /**
     * Clears all server type overrides from all manifests, reverting to local-only state.
     * Rebuilds the TypeId/Class lookup maps after clearing.
     */
    void ClearServerTypeOverrides();

    /** Returns true if any manifest currently has active server overlay entries. */
    bool HasServerTypeOverrides() const;

    /**
     * Computes the absolute path to a root class's type manifest file.
     *
     * @param RootClass The registered root base class
     * @return Absolute path to the manifest file
     */
    static FString GetRootClassManifestFilePath(const UClass* RootClass);

    /** Delegate broadcast when classes are registered or unregistered. */
    FOnDynamicTextAssetRegistryChanged OnRegistryChanged;

    /** Delegate broadcast when type manifests are synced or updated. */
    FOnTypeManifestUpdated OnTypeManifestUpdated;

private:

    /** Rebuilds the cached class list from registered bases + reflection */
    void RebuildClassCache() const;

    /**
     * Rebuilds the TypeIdToSoftClassMap and ClassToTypeIdMap from all manifests.
     * Uses GetAllEffectiveTypes() to include server overlay entries.
     * Called after LoadCookedManifests(), ApplyServerTypeOverrides(), and ClearServerTypeOverrides().
     */
    void RebuildTypeIdMaps();

    /** Set of explicitly registered base classes (must all implement ISGDynamicTextAssetProvider) */
    UPROPERTY(Transient)
    TSet<TObjectPtr<UClass>> RegisteredBaseClasses;

    /** Cached array of all classes (base + children), invalidated on change */
    UPROPERTY(Transient)
    mutable TArray<TObjectPtr<UClass>> CachedAllClasses;

    /** Whether the cached class list needs to be rebuilt */
    UPROPERTY()
    mutable uint8 bCacheInvalid : 1 = 1;

    /** One manifest per registered root base class. */
    TMap<TWeakObjectPtr<UClass>, TSharedPtr<FSGDynamicTextAssetTypeManifest>> RootClassManifests;

    /** TypeId -> soft class pointer, built from all manifests during SyncManifests. */
    TMap<FSGDynamicTextAssetTypeId, TSoftClassPtr<UObject>> TypeIdToSoftClassMap;

    /** UClass -> TypeId reverse lookup, built during SyncManifests. */
    TMap<TWeakObjectPtr<const UClass>, FSGDynamicTextAssetTypeId> ClassToTypeIdMap;
};
