// Copyright Start Games, Inc. All Rights Reserved.

#include "Utilities/SGDTASCCStatusCache.h"

#include "Async/Async.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Management/SGDTAFileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SGDTAEditorLogs.h"
#include "SourceControlOperations.h"

FSGDynamicTextAssetSCCStatusCache* FSGDynamicTextAssetSCCStatusCache::INSTANCE = nullptr;

FSGDynamicTextAssetSCCStatusCache::FSGDynamicTextAssetSCCStatusCache()
{
    checkf(INSTANCE == nullptr, TEXT("FSGDynamicTextAssetSCCStatusCache is module-owned and must only be constructed once"));
    INSTANCE = this;

    // Provider state-changed subscription. Skipped when source control is disabled at
    // construction; HandleProviderChanged re-hooks it if a provider comes online later.
    if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        ProviderStateChangedHandle = ISourceControlModule::Get().GetProvider().RegisterSourceControlStateChanged_Handle(
            FSourceControlStateChanged::FDelegate::CreateRaw(this, &FSGDynamicTextAssetSCCStatusCache::HandleProviderStateChanged));
    }

    // Module-level provider-changed subscription so enabling or switching the SCC
    // provider mid-session moves the state-changed hook onto the new provider.
    ProviderChangedHandle = ISourceControlModule::Get().RegisterProviderChanged(
        FSourceControlProviderChanged::FDelegate::CreateRaw(this, &FSGDynamicTextAssetSCCStatusCache::HandleProviderChanged));

    // File-manager subscription is provider-independent: local mutations (create/delete/
    // rename/modify) must invalidate cached state even when SCC is disabled.
    FileChangedHandle = FSGDynamicTextAssetFileManager::ON_FILE_CHANGED.AddRaw(
        this, &FSGDynamicTextAssetSCCStatusCache::HandleDTAFileChanged);

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("SCC status cache created (provider hook %s)"),
        ProviderStateChangedHandle.IsValid() ? TEXT("registered") : TEXT("skipped, source control disabled"));
}

FSGDynamicTextAssetSCCStatusCache::~FSGDynamicTextAssetSCCStatusCache()
{
    FSGDynamicTextAssetFileManager::ON_FILE_CHANGED.Remove(FileChangedHandle);
    FileChangedHandle.Reset();

    // GetModulePtr instead of Get(): during editor shutdown the SourceControl module may
    // already be gone, and unregistering against an unloaded module must be a no-op.
    if (ISourceControlModule* sourceControlModule = FModuleManager::GetModulePtr<ISourceControlModule>("SourceControl"))
    {
        if (ProviderStateChangedHandle.IsValid())
        {
            sourceControlModule->GetProvider().UnregisterSourceControlStateChanged_Handle(ProviderStateChangedHandle);
        }

        if (ProviderChangedHandle.IsValid())
        {
            sourceControlModule->UnregisterProviderChanged(ProviderChangedHandle);
        }
    }

    ProviderStateChangedHandle.Reset();
    ProviderChangedHandle.Reset();

    Cache.Empty();
    INSTANCE = nullptr;

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("SCC status cache destroyed"));
}

FSGDynamicTextAssetSCCStatusCache& FSGDynamicTextAssetSCCStatusCache::Get()
{
    checkf(INSTANCE != nullptr, TEXT("FSGDynamicTextAssetSCCStatusCache::Get() called outside the editor module's lifetime"));
    return *INSTANCE;
}

bool FSGDynamicTextAssetSCCStatusCache::IsAvailable()
{
    return INSTANCE != nullptr;
}

ESGDynamicTextAssetSourceControlStatus FSGDynamicTextAssetSCCStatusCache::GetCachedStatus(const FString& FilePath)
{
    // Disabled SCC: no query, no insertion, so a later enable starts from a clean cache.
    if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        return ESGDynamicTextAssetSourceControlStatus::Unknown;
    }

    const FString normalizedPath = NormalizePath(FilePath);

    if (const FCachedEntry* existingEntry = Cache.Find(normalizedPath))
    {
        return existingEntry->Status;
    }

    return PopulateEntry(normalizedPath).Status;
}

bool FSGDynamicTextAssetSCCStatusCache::GetCheckedOutByUser(const FString& FilePath, FString& OutUsername)
{
    OutUsername.Empty();

    if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        return false;
    }

    const FString normalizedPath = NormalizePath(FilePath);

    const FCachedEntry* entry = Cache.Find(normalizedPath);
    if (entry == nullptr)
    {
        entry = &PopulateEntry(normalizedPath);
    }

    if (entry->Status == ESGDynamicTextAssetSourceControlStatus::CheckedOutByOther)
    {
        OutUsername = entry->CheckedOutBy;
        return true;
    }

    return false;
}

void FSGDynamicTextAssetSCCStatusCache::InvalidateCachePath(const FString& FilePath)
{
    // Broadcast only when an entry was actually removed to avoid spurious repaints.
    if (Cache.Remove(NormalizePath(FilePath)) > 0)
    {
        OnStatusChanged.Broadcast();
    }
}

void FSGDynamicTextAssetSCCStatusCache::InvalidateAll()
{
    // Broadcast only when the cache held anything to avoid spurious repaints.
    if (!Cache.IsEmpty())
    {
        Cache.Empty();
        OnStatusChanged.Broadcast();
    }
}

FSGDynamicTextAssetSCCStatusCache::FCachedEntry& FSGDynamicTextAssetSCCStatusCache::PopulateEntry(const FString& NormalizedPath)
{
    FCachedEntry newEntry;
    newEntry.Status = FSGDynamicTextAssetSourceControl::GetFileStatus(NormalizedPath);

    // Username comes exclusively from the IsCheckedOutByOther out-param, cached on the entry.
    if (newEntry.Status == ESGDynamicTextAssetSourceControlStatus::CheckedOutByOther)
    {
        FSGDynamicTextAssetSourceControl::IsCheckedOutByOther(NormalizedPath, newEntry.CheckedOutBy);
    }

    return Cache.Add(NormalizedPath, MoveTemp(newEntry));
}

void FSGDynamicTextAssetSCCStatusCache::HandleProviderStateChanged()
{
    // The engine delegate carries no paths, so drop everything; entries repopulate
    // lazily on the next query with no eager provider round-trips.
    InvalidateAll();
}

void FSGDynamicTextAssetSCCStatusCache::HandleProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
    // Move the state-changed hook off the outgoing provider.
    if (ProviderStateChangedHandle.IsValid())
    {
        OldProvider.UnregisterSourceControlStateChanged_Handle(ProviderStateChangedHandle);
        ProviderStateChangedHandle.Reset();
    }

    // Hook the incoming provider when it is enabled and available. IsEnabled() reflects
    // the new provider at broadcast time because the module switches before broadcasting.
    if (ISourceControlModule::Get().IsEnabled() && NewProvider.IsAvailable())
    {
        ProviderStateChangedHandle = NewProvider.RegisterSourceControlStateChanged_Handle(
            FSourceControlStateChanged::FDelegate::CreateRaw(this, &FSGDynamicTextAssetSCCStatusCache::HandleProviderStateChanged));
    }

    // All cached state belonged to the old provider.
    InvalidateAll();
}

void FSGDynamicTextAssetSCCStatusCache::HandleDTAFileChanged(ESGDynamicTextAssetFileChangeKind Kind, const FString& FilePath, const FGuid& Guid)
{
    // Invalidation is uniform across all change kinds, so Kind is intentionally not switched on.

    // ON_FILE_CHANGED can legally fire off the game thread (e.g. WriteRawFileContents
    // with bAutoCheckOut = false). The cache is game-thread-only, so marshal across.
    // The path is captured by value and the instance re-resolved on the game thread
    // because the cache may be destroyed before the task runs.
    if (!IsInGameThread())
    {
        const FString filePathCopy = FilePath;
        AsyncTask(ENamedThreads::GameThread, [filePathCopy]()
        {
            if (IsAvailable())
            {
                Get().ProcessDTAFileChangedOnGameThread(filePathCopy);
            }
        });

        return;
    }

    ProcessDTAFileChangedOnGameThread(FilePath);
}

void FSGDynamicTextAssetSCCStatusCache::ProcessDTAFileChangedOnGameThread(const FString& FilePath)
{
    // Drop the stale entry immediately so local-only change signals repaint even when source
    // control is disabled (this path is provider-independent).
    InvalidateCachePath(FilePath);

    // Ask the provider to re-evaluate this single path. Some providers (notably Git) only
    // surface a saved local edit as "modified" after a status refresh; nothing else triggers
    // one after a write, so without this the badge/tooltip/Revert stay stale. No-op when source
    // control is disabled.
    RequestProviderStatusRefresh(FilePath);
}

void FSGDynamicTextAssetSCCStatusCache::RequestProviderStatusRefresh(const FString& FilePath)
{
    // Nothing to refresh without a live provider.
    if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        return;
    }

    const FString normalizedPath = NormalizePath(FilePath);

    // Coalesce a burst of writes to the same file: while a refresh for this path is still in
    // flight, skip launching another. The path is removed in the completion handler, which
    // always fires. Different paths each still get their own refresh.
    if (InFlightRefreshPaths.Contains(normalizedPath))
    {
        return;
    }

    // Request modified-state detection. Perforce only populates IsModified() when this is set;
    // the Git provider reports working-tree modification from a plain status regardless, so the
    // flag is harmless there. This mirrors the option applied on the browser refresh path.
    TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> updateOperation = ISourceControlOperation::Create<FUpdateStatus>();
    updateOperation->SetUpdateModifiedState(true);

    InFlightRefreshPaths.Add(normalizedPath);

    // The completion delegate captures the path by value and re-resolves the singleton rather
    // than capturing this: the cache may be destroyed before the async op completes. Operation
    // complete delegates fire on the game thread, so touching the cache there is safe.
    ISourceControlProvider& sourceControlProvider = ISourceControlModule::Get().GetProvider();
    sourceControlProvider.Execute(
        updateOperation,
        {normalizedPath},
        EConcurrency::Asynchronous,
        FSourceControlOperationComplete::CreateLambda(
            [normalizedPath](const FSourceControlOperationRef& Operation, ECommandResult::Type Result)
            {
                if (IsAvailable())
                {
                    Get().HandleProviderStatusRefreshComplete(normalizedPath, Result);
                }
            })
    );
}

void FSGDynamicTextAssetSCCStatusCache::HandleProviderStatusRefreshComplete(const FString& NormalizedPath, ECommandResult::Type Result)
{
    // Clear the in-flight guard regardless of result so a later write can refresh again.
    InFlightRefreshPaths.Remove(NormalizedPath);

    if (Result != ECommandResult::Succeeded)
    {
        // Failed or cancelled: leave the cache as-is (the earlier invalidate already forced a
        // repopulate from whatever provider state exists).
        UE_LOG(LogSGDynamicTextAssetsEditor, Error,
            TEXT("SCC status cache: async status refresh did not succeed for '%s' (result %d)"),
            *NormalizedPath, static_cast<int32>(Result));
        return;
    }

    // The provider's state cache now reflects the on-disk file, so drop our entry and let the
    // next query repopulate with fresh state. InvalidateCachePath broadcasts OnStatusChanged,
    // which repaints badges/tooltips and re-evaluates Revert enablement.
    InvalidateCachePath(NormalizedPath);
}

FString FSGDynamicTextAssetSCCStatusCache::NormalizePath(const FString& FilePath)
{
    // Normalize so browser paths, file-manager broadcast paths, and provider paths all
    // collide on the same key. TMap<FString, ...> compares case-insensitively, which is
    // correct for Windows paths.
    FString normalized = FPaths::ConvertRelativePathToFull(FilePath);
    FPaths::NormalizeFilename(normalized);
    return normalized;
}
