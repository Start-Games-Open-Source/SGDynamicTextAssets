// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Core/SGDynamicTextAssetValidationResult.h"
#include "Core/SGDynamicTextAssetBundleData.h"
#include "Dom/JsonObject.h"
#include "SGDynamicTextAssetDelegates.h"
#include "UObject/Interface.h"

#include "ISGDynamicTextAssetProvider.generated.h"

class FJsonObject;

/**
 * UInterface boilerplate for the dynamic text asset provider interface.
 *
 * BlueprintType allows the interface to be used as a type in
 * Blueprint (e.g., TScriptInterface<ISGDynamicTextAssetProvider>).
 * CannotImplementInterfaceInBlueprint ensures that only C++ classes
 * can provide implementations.
 */
UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class USGDynamicTextAssetProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Core interface that defines what it means to be a dynamic text asset in
 * the SGDynamicTextAssets ecosystem.
 *
 * Any UObject (excluding AActor and UActorComponent subclasses) can
 * implement this interface to participate in the dynamic text asset system,
 * including loading, caching, serialization, and editor tooling.
 *
 * USGDynamicTextAsset is the default concrete implementation. Games may
 * create alternative implementations for specialized use cases
 * (e.g., a UDataAsset-based provider) while sharing the same
 * management infrastructure.
 *
 * Registration guards in USGDynamicTextAssetRegistry enforce:
 * - The class implements ISGDynamicTextAssetProvider
 * - The class is NOT derived from AActor or UActorComponent
 */
class SGDYNAMICTEXTASSETSRUNTIME_API ISGDynamicTextAssetProvider
{
	GENERATED_BODY()
public:

	/** Returns the unique dynamic text asset ID for this provider. */
	virtual const FSGDynamicTextAssetId& GetDynamicTextAssetId() const = 0;

	/**
	 * Sets the unique dynamic text asset ID.
	 * Typically only called during deserialization or creation.
	 *
	 * @param InId The dynamic text asset ID to assign
	 */
	virtual void SetDynamicTextAssetId(const FSGDynamicTextAssetId& InId) = 0;

	/** Returns the human-readable identifier for this dynamic text asset. */
	virtual const FString& GetUserFacingId() const = 0;

	/**
	 * Sets the human-readable identifier.
	 *
	 * @param InUserFacingId The new user-facing ID
	 */
	virtual void SetUserFacingId(const FString& InUserFacingId) = 0;

	/** Returns the version of this dynamic text asset instance (from file). */
	virtual const FSGDynamicTextAssetVersion& GetVersion() const = 0;

	/**
	 * Sets the version. Typically only called during deserialization.
	 *
	 * @param InVersion The version to assign
	 */
	virtual void SetVersion(const FSGDynamicTextAssetVersion& InVersion) = 0;

	/**
	 * Returns the current major version declared by this class.
	 * Used to determine if migration is needed when loading older data.
	 */
	virtual int32 GetCurrentMajorVersion() const = 0;

	/**
	 * Returns the full current version declared by this class.
	 * Subclasses can override to declare minor/patch versions
	 * (e.g., FSGDynamicTextAssetVersion(2, 1, 0)).
	 *
	 * Default implementation returns (GetCurrentMajorVersion(), 0, 0).
	 */
	virtual FSGDynamicTextAssetVersion GetCurrentVersion() const
	{
		return FSGDynamicTextAssetVersion(GetCurrentMajorVersion(), 0, 0);
	}

	/**
	 * Called after this dynamic text asset's properties have been populated
	 * from the serialized data. Override to perform custom
	 * initialization or caching.
	 */
	virtual void PostDynamicTextAssetLoaded() = 0;

	/**
	 * Runs the full validation pipeline for this dynamic text asset.
	 * Includes built-in checks (ID validity, version) and any
	 * custom validation defined by the implementation.
	 *
	 * USGDynamicTextAsset routes this through Native_ValidateDynamicTextAsset,
	 * which runs built-in checks then calls the BlueprintNativeEvent.
	 *
	 * @param OutResult Container to populate with validation entries
	 * @return True if no error-severity entries were produced
	 */
	virtual bool Native_ValidateDynamicTextAsset(FSGDynamicTextAssetValidationResult& OutResult) const;

	/**
	 * Called when loading data from an older version.
	 * Override to transform old data format to the current format.
	 *
	 * UPROPERTYs are NOT populated during migration. The function
	 * operates on raw JSON only, allowing the class to restructure
	 * data freely before deserialization occurs.
	 *
	 * @param OldVersion The full version of the data being loaded
	 * @param CurrentVersion The target version for this class
	 * @param OldData The original JSON data before migration
	 * @return True if migration was successful, false to fail the load
	 */
	virtual bool MigrateFromVersion(
		const FSGDynamicTextAssetVersion& OldVersion,
		const FSGDynamicTextAssetVersion& CurrentVersion,
		const TSharedPtr<FJsonObject>& OldData) = 0;

	/** Returns the asset bundle data extracted from this provider's soft reference properties. */
	virtual const FSGDynamicTextAssetBundleData& GetSGDTAssetBundleData() const = 0;

	/** Returns a mutable reference to the asset bundle data. */
	virtual FSGDynamicTextAssetBundleData& GetMutableSGDTAssetBundleData() = 0;

	/** Returns true if this provider has any asset bundles. */
	virtual bool HasSGDTAssetBundles() const;

#if WITH_EDITOR
	/** Notifies listeners that this dynamic text asset has changed */
	virtual void BroadcastDynamicTextAssetChanged();

	/** Retrieves the delegate for when this dynamic text asset is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi& GetOnDynamicTextAssetChanged() { return OnDynamicTextAssetChanged; }

	/** Retrieves the delegate for when this dynamic text asset's ID is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi& GetOnDynamicTextAssetIdChanged() { return OnDynamicTextAssetIdChanged; }
	/** Retrieves the delegate for when this dynamic text asset User Facing ID is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi& GetOnUserFacingIdChanged() { return OnUserFacingIdChanged; }
	/** Retrieves the delegate for when this dynamic text asset Version is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi& GetOnVersionChanged() { return OnVersionChanged; }
#endif

protected:

#if WITH_EDITOR
	/** Delegate broadcast when this dynamic text asset is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi OnDynamicTextAssetChanged;

	/** Delegate broadcast when this dynamic text asset's ID is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi OnDynamicTextAssetIdChanged;
	/** Delegate broadcast when this dynamic text asset's User Facing ID is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi OnUserFacingIdChanged;
	/** Delegate broadcast when this dynamic text asset's Version is modified in the editor. */
	FOnDynamicTextAssetProvider_Multi OnVersionChanged;
#endif
};
