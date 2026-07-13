// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ISourceControlProvider.h"
#include "Utilities/SGDTASourceControl.h"

enum class ESGDynamicTextAssetFileChangeKind : uint8;

/**
 * Path-keyed cache of source control status for dynamic text asset files.
 *
 * Entries are populated lazily on first query (delegating to FSGDynamicTextAssetSourceControl)
 * and invalidated by the source-control provider's state-changed delegate and by the file
 * manager's ON_FILE_CHANGED delegate, so consumers (browser tiles, context menus, toolbars)
 * can poll cheaply without hitting the provider per frame.
 *
 * Owned by the editor module (created in StartupModule, destroyed in ShutdownModule) and
 * published through a module-owned singleton slot, mirroring the SOURCE_CONTROL_BRIDGE
 * pattern. Game-thread-only; off-thread ON_FILE_CHANGED broadcasts are marshalled to the
 * game thread before touching the cache.
 */
class SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetSCCStatusCache
{
public:
    /** Registers the provider and file-manager subscriptions and publishes the singleton pointer. */
    FSGDynamicTextAssetSCCStatusCache();

    /** Unregisters all subscriptions and clears the singleton pointer. */
    ~FSGDynamicTextAssetSCCStatusCache();

    // Non-copyable, non-movable (owns delegate handles and the singleton slot).
    FSGDynamicTextAssetSCCStatusCache(const FSGDynamicTextAssetSCCStatusCache&) = delete;
    FSGDynamicTextAssetSCCStatusCache& operator=(const FSGDynamicTextAssetSCCStatusCache&) = delete;
    FSGDynamicTextAssetSCCStatusCache(FSGDynamicTextAssetSCCStatusCache&&) = delete;
    FSGDynamicTextAssetSCCStatusCache& operator=(FSGDynamicTextAssetSCCStatusCache&&) = delete;

    /**
     * Checked accessor to the module-owned instance.
     * Only valid while the editor module is loaded; call IsAvailable() first in
     * shutdown-sensitive code paths.
     */
    static FSGDynamicTextAssetSCCStatusCache& Get();

    /** True while the module-owned instance exists. Defensive callers and tests check this first. */
    static bool IsAvailable();

    /**
     * Returns the cached status for a file, populating the entry lazily on first query.
     * When source control is disabled, returns Unknown immediately without querying or
     * inserting an entry.
     *
     * @param FilePath Absolute or relative path to the file (normalized internally).
     * @return The cached source control status, or Unknown when unavailable.
     */
    ESGDynamicTextAssetSourceControlStatus GetCachedStatus(const FString& FilePath);

    /**
     * Returns the cached "checked out by" username for a file, populating the entry
     * lazily on first query. The username comes exclusively from
     * FSGDynamicTextAssetSourceControl::IsCheckedOutByOther and is cached on the entry.
     *
     * @param FilePath Absolute or relative path to the file (normalized internally).
     * @param OutUsername Receives the other user's name when checked out by another user, else emptied.
     * @return True when the cached status is CheckedOutByOther.
     */
    bool GetCheckedOutByUser(const FString& FilePath, FString& OutUsername);

    /**
     * Removes one path's cache entry so the next query repopulates it.
     * Broadcasts OnStatusChanged only if an entry actually existed.
     *
     * @param FilePath Absolute or relative path to the file (normalized internally).
     */
    void InvalidateCachePath(const FString& FilePath);

    /** Clears the whole cache. Broadcasts OnStatusChanged only if the cache was non-empty. */
    void InvalidateAll();

    /**
     * Fires on the game thread whenever cached state is invalidated.
     * Parameterless by design: the engine's provider state-changed delegate carries no
     * paths, and all current consumers poll GetCachedStatus lazily and only need a
     * repaint trigger. A per-path variant can be added later as a second delegate.
     */
    FSimpleMulticastDelegate OnStatusChanged;

private:
    /** One cached record per normalized absolute file path. */
    struct FCachedEntry
    {
        /** Populated from IsCheckedOutByOther's out-param when Status is CheckedOutByOther, else empty. */
        FString CheckedOutBy;

        /** Cached source control status from GetFileStatus. */
        ESGDynamicTextAssetSourceControlStatus Status = ESGDynamicTextAssetSourceControlStatus::Unknown;
    };

    /**
     * Single population point: GetFileStatus, then IsCheckedOutByOther for the username
     * when the status is CheckedOutByOther. Inserts and returns the new entry.
     */
    FCachedEntry& PopulateEntry(const FString& NormalizedPath);

    /** Provider state-changed handler: InvalidateAll (the engine delegate carries no paths). */
    void HandleProviderStateChanged();

    /**
     * Provider-changed handler: moves the state-changed subscription from the old
     * provider to the new one (when enabled/available) and clears the whole cache.
     */
    void HandleProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);

    /** ON_FILE_CHANGED handler: marshals to the game thread if needed, then processes the change. */
    void HandleDTAFileChanged(ESGDynamicTextAssetFileChangeKind Kind, const FString& FilePath, const FGuid& Guid);

    /**
     * Game-thread body of the file-changed handling: invalidates the cached entry and then
     * kicks off an async provider status refresh for the path. Always runs on the game thread.
     */
    void ProcessDTAFileChangedOnGameThread(const FString& FilePath);

    /**
     * Fires an asynchronous provider FUpdateStatus (with modified-state detection requested)
     * for a single path so providers that only surface a saved local edit after a status
     * refresh (notably Git) converge. No-op when source control is disabled or a refresh for
     * the same path is already in flight. Must be called on the game thread.
     */
    void RequestProviderStatusRefresh(const FString& FilePath);

    /**
     * Completion handler for RequestProviderStatusRefresh: clears the in-flight guard and, on
     * success, invalidates the path so the next query repopulates from fresh provider state.
     */
    void HandleProviderStatusRefreshComplete(const FString& NormalizedPath, ECommandResult::Type Result);

    /** Normalizes a path to the absolute form used as the cache key. */
    static FString NormalizePath(const FString& FilePath);

    /** Cache keyed by normalized absolute file path (FString keys hash/compare case-insensitively). */
    TMap<FString, FCachedEntry> Cache;

    /** Handle for the provider's state-changed subscription; invalid when SCC was disabled at hook time. */
    FDelegateHandle ProviderStateChangedHandle;

    /** Handle for the module-level provider-changed subscription (re-hooks state-changed on switch). */
    FDelegateHandle ProviderChangedHandle;

    /** Handle for the file manager's ON_FILE_CHANGED subscription. */
    FDelegateHandle FileChangedHandle;

    /**
     * Normalized paths with an async status refresh currently in flight. Coalesces a burst of
     * writes to the same file so rapid saves do not stack redundant provider operations.
     */
    TSet<FString> InFlightRefreshPaths;

    /** Module-owned singleton slot, mirrors the SOURCE_CONTROL_BRIDGE pattern. */
    static FSGDynamicTextAssetSCCStatusCache* INSTANCE;
};
