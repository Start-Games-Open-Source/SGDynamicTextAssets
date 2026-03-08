// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/ISGDynamicTextAssetProvider.h"

#include "SGDynamicTextAssetRef.generated.h"

class UGameInstance;
class USGDynamicTextAssetSubsystem;

/// Convenience macro for asynchronous loading of FSGDynamicTextAssetRef with automatic weak-pointer safety.
///
/// This macro simplifies the boilerplate of calling LoadAsync by:
/// - Creating a weak pointer (`self`) to avoid dangling references if the object is destroyed during async load
/// - Declaring the lambda with standard parameters: Provider (TScriptInterface<ISGDynamicTextAssetProvider>) and bSuccess (bool)
/// - Supporting additional lambda captures via variadic arguments
///
/// @param DTA_Ref          The FSGDynamicTextAssetRef to load
/// @param ContextObject    The UObject context (typically `this`), used for subsystem lookup and weak pointer creation
/// @param FunctionDefinition The lambda body enclosed in braces { ... }
/// @param ...              (Optional) Additional variables to capture in the lambda, comma-separated. Recommend using copies to avoid lifetime failures.
///
/// Usage Examples:
///
/// Basic usage - load a weapon info asset:
///   SG_LOAD_REF_SIMPLE(WeaponTypeRef, this,
///   {
///       if (!self.IsValid() || !bSuccess)
///       {
///           return;
///       }
///
///       if (UWeaponInfoDTA* Info = Cast<UWeaponInfoDTA>(Provider.GetObject()))
///       {
///           // Use the loaded asset
///           WeaponClass = Info->WeaponToSpawn;
///       }
///   });
///
/// With additional captures - capture a local variable:
///   int32 SlotIndex = 2;
///   SG_LOAD_REF_SIMPLE(ItemRef, this,
///   {
///       if (!self.IsValid() || !bSuccess)
///       {
///           return;
///       }
///
///       self->ProcessItem(Provider, SlotIndex);
///   }, SlotIndex);
///
/// Multiple additional captures:
///   SG_LOAD_REF_SIMPLE(QuestRef, this,
///   {
///       if (!self.IsValid() || !bSuccess)
///       {
///           return;
///       }
///
///       self->OnQuestLoaded(QuestId, bIsMainQuest);
///   }, QuestId, bIsMainQuest);
#define SG_LOAD_REF_SIMPLE(DTA_Ref, ContextObject, FunctionDefinition, ...) \
{ \
    TWeakObjectPtr<ThisClass> self = ContextObject; \
    DTA_Ref.LoadAsync(ContextObject, [self __VA_OPT__(,__VA_ARGS__)](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess) FunctionDefinition); \
}

/**
 * Lightweight reference to a dynamic text asset by ID.
 *
 * Use this instead of raw pointers to dynamic text assets in your game code.
 * The reference stores only the ID and can resolve to the actual object
 * through the subsystem when needed.
 *
 * Features:
 * - ID-based: Stable across sessions, serialization-friendly
 * - Type-filtered: Editor can restrict to specific dynamic text asset subclasses
 * - Lazy resolution: Object is fetched from cache only when Get() is called
 * 
 * Usage:
 *   UPROPERTY(EditAnywhere, meta = (DynamicTextAssetType = "UWeaponData"))
 *   FSGDynamicTextAssetRef WeaponRef;
 *   
 *   // In code:
 *   TScriptInterface<ISGDynamicTextAssetProvider> Provider = WeaponRef.Get(GetGameInstance());
 *   UWeaponData* Weapon = WeaponRef.Get<UWeaponData>(GetGameInstance());
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetRef
{
    GENERATED_BODY()
public:

    FSGDynamicTextAssetRef() = default;

    FSGDynamicTextAssetRef(const FSGDynamicTextAssetId& InId)
        : Id(InId)
    { }
    explicit FSGDynamicTextAssetRef(const FSGDynamicTextAssetId&& InId)
        : Id(InId)
    { }

    /** Returns true if this reference has no valid ID */
    bool IsNull() const;

    /** Returns true if this reference has a valid ID */
    bool IsValid() const;

    /** Returns the referenced ID */
    const FSGDynamicTextAssetId& GetId() const;

    /**
     * Resolves and returns the user-facing ID by looking up the dynamic text asset's file metadata.
     * Works in any context — editor, runtime, without a world or game instance.
     * Returns an empty string if the ID is invalid or no matching file is found.
     */
    FString GetUserFacingId() const;

    /** Sets the ID for this reference */
    void SetId(const FSGDynamicTextAssetId& InId);

    /** Clears this reference */
    void Reset();

    /**
     * Resolves this reference to the dynamic text asset from the subsystem cache.
     * Returns nullptr if not cached or ID is invalid.
     * 
     * @param GameInstance The game instance to get the subsystem from
     * @return The resolved provider, or empty TScriptInterface if not cached or ID is invalid
     */
    TScriptInterface<ISGDynamicTextAssetProvider> Get(const UGameInstance* GameInstance) const;

    /**
     * Type-safe version of Get().
     * 
     * @param GameInstance The game instance to get the subsystem from
     * @return The resolved dynamic text asset cast to T, or nullptr
     */
    template<typename T>
    T* Get(const UGameInstance* GameInstance) const
    {
        return Cast<T>(Get(GameInstance).GetObject());
    }

    /**
     * Resolves this reference using a world context object.
     * 
     * @param WorldContextObject Any UObject with a valid world
     * @return The resolved provider, or empty TScriptInterface if not cached or ID is invalid
     */
    TScriptInterface<ISGDynamicTextAssetProvider> Get(const UObject* WorldContextObject) const;

    /**
     * Type-safe version of Get() with world context that returns the object that has the ISGDynamicTextAssetProvider interface.
     */
    template<typename T>
    T* Get(const UObject* WorldContextObject) const
    {
        static_assert(TIsDerivedFrom<T, UObject>::Value, "Type must be derived from UObject.");
        return Cast<T>(Get(WorldContextObject).GetObject());
    }

    /**
     * Loads this reference asynchronously if not already cached.
     * The file path is resolved automatically from the ID.
     * In editor non-PIE contexts (no GameInstance), falls back to synchronous
     * loading via FSGDynamicTextAssetEditorCache and invokes the callback immediately.
     *
     * @param WorldContextObject Any UObject with a valid world (or editor context)
     * @param OnComplete Callback when loading completes
     */
    void LoadAsync(const UObject* WorldContextObject, TFunction<void(TScriptInterface<ISGDynamicTextAssetProvider> /*Provider*/, bool/*bSuccess*/)> OnComplete) const;
    /**
     * Loads this reference asynchronously if not already cached.
     * The file path is resolved automatically from the ID.
     * In editor non-PIE contexts (no GameInstance), falls back to synchronous
     * loading via FSGDynamicTextAssetEditorCache and invokes the callback immediately.
     *
     * @param WorldContextObject Any UObject with a valid world (or editor context)
     * @param OnComplete Callback when loading completes
     */
    void LoadAsync(const UObject* WorldContextObject, FOnDynamicTextAssetRefLoaded OnComplete) const;
    /**
     * Loads this reference asynchronously if not already cached.
     * The file path is resolved automatically from the ID.
     * In editor non-PIE contexts (no GameInstance), falls back to synchronous
     * loading via FSGDynamicTextAssetEditorCache and invokes the callback immediately.
     *
     * @param WorldContextObject Any UObject with a valid world (or editor context)
     * @param OnComplete Callback when loading completes
     */
    void LoadAsync(const UObject* WorldContextObject, FOnDynamicTextAssetLoaded OnComplete) const;

    bool operator==(const FSGDynamicTextAssetRef& Other) const;
    bool operator!=(const FSGDynamicTextAssetRef& Other) const;
    bool operator==(const FSGDynamicTextAssetId& Other) const;
    bool operator!=(const FSGDynamicTextAssetId& Other) const;

    /**
     * Exports the ID as a text string for clipboard and config usage.
     *
     * @param ValueStr Output string to append the exported value to
     * @param DefaultValue Default value for comparison (unused)
     * @param Parent Owning UObject (unused)
     * @param PortFlags Export flags
     * @param ExportRootScope Root export scope (unused)
     * @return True if export succeeded
     */
    bool ExportTextItem(FString& ValueStr, const FSGDynamicTextAssetRef& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;

    /**
     * Imports the ID from a text string.
     *
     * @param Buffer Input text buffer (advanced past consumed characters)
     * @param PortFlags Import flags
     * @param Parent Owning UObject (unused)
     * @param ErrorText Output for error messages
     * @return True if import succeeded
     */
    bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

    /**
     * Binary archive serialization.
     * Serializes only the Id (GUID) without property tags.
     *
     * @param Ar The archive to serialize to/from
     * @return True (serialization always succeeds)
     */
    bool Serialize(FArchive& Ar);

    /**
     * Network replication serialization.
     *
     * @param Ar The archive to serialize to/from
     * @param Map Package map for name/object resolution (unused)
     * @param bOutSuccess Set to true on success
     * @return True if serialization succeeded
     */
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

    friend uint32 GetTypeHash(const FSGDynamicTextAssetRef& Ref)
    {
        return GetTypeHash(Ref.Id);
    }

    /** The unique identifier referencing the dynamic text asset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSGDynamicTextAssetId Id;
};

template<>
struct TStructOpsTypeTraits<FSGDynamicTextAssetRef> : public TStructOpsTypeTraitsBase2<FSGDynamicTextAssetRef>
{
    enum
    {
        WithSerializer = true,
        WithNetSerializer = true,
        WithExportTextItem = true,
        WithImportTextItem = true,
        WithIdenticalViaEquality = true
    };
};
