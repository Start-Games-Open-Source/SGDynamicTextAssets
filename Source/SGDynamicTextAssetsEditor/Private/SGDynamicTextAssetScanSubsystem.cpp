// Copyright Start Games, Inc. All Rights Reserved.

#include "SGDynamicTextAssetScanSubsystem.h"

#include "Editor.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "SGDynamicTextAssetTickerSubsystem.h"
#include "Utilities/SGDynamicTextAssetScanPriorities.h"

void USGDynamicTextAssetScanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<USGDynamicTextAssetTickerSubsystem>();

	bScanningInProgress = false;
	bForceFullRescan = false;

	// Load cached project info from disk
	const FString cachePath = FSGDynamicTextAssetProjectInfoCache::GetDefaultCachePath();
	ProjectInfoCache.LoadFromFile(cachePath);

	// Register the built-in project info scan phase
	FSGDynamicTextAssetScanPhase projectInfoPhase;
	projectInfoPhase.PhaseId = FName(TEXT("ProjectInfo"));
	projectInfoPhase.DisplayName = INVTEXT("Project Info");
	projectInfoPhase.Priority = SGDynamicTextAssetScanPriorities::PROJECT_INFO;
	projectInfoPhase.SetupPhase = [this]() { SetupProjectInfoPhase(); };
	projectInfoPhase.ProcessOneItem = [this]() { return ProcessOneProjectInfoItem(); };
	projectInfoPhase.OnPhaseComplete = [this]() { OnProjectInfoPhaseComplete(); };
	projectInfoPhase.GetRemainingCount = [this]() { return PendingProjectInfoFiles.Num(); };
	RegisterScanPhase(projectInfoPhase);

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("USGDynamicTextAssetScanSubsystem initialized"));
}

void USGDynamicTextAssetScanSubsystem::Deinitialize()
{
	RegisteredPhases.Empty();
	DiscoveredDTAFiles.Empty();
	PendingProjectInfoFiles.Empty();

	// Unbind from ticker subsystem
	if (AllWorkCompleteDelegateHandle.IsValid())
	{
		if (USGDynamicTextAssetTickerSubsystem* tickerSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetTickerSubsystem>())
		{
			tickerSubsystem->OnAllWorkComplete.Remove(AllWorkCompleteDelegateHandle);
		}
		AllWorkCompleteDelegateHandle.Reset();
	}

	bScanningInProgress = false;

	Super::Deinitialize();
}

void USGDynamicTextAssetScanSubsystem::RegisterScanPhase(const FSGDynamicTextAssetScanPhase& Phase)
{
	if (!Phase.PhaseId.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
			TEXT("USGDynamicTextAssetScanSubsystem::RegisterScanPhase: Phase has invalid PhaseId, ignoring"));
		return;
	}

	// Replace existing phase with same ID
	RegisteredPhases.RemoveAll([&Phase](const FSGDynamicTextAssetScanPhase& Existing)
	{
		return Existing.PhaseId == Phase.PhaseId;
	});

	RegisteredPhases.Add(Phase);

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
		TEXT("USGDynamicTextAssetScanSubsystem: Registered scan phase '%s' (Priority=%d)"),
		*Phase.PhaseId.ToString(), Phase.Priority);
}

void USGDynamicTextAssetScanSubsystem::UnregisterScanPhase(FName PhaseId)
{
	RegisteredPhases.RemoveAll([PhaseId](const FSGDynamicTextAssetScanPhase& Phase)
	{
		return Phase.PhaseId == PhaseId;
	});
}

void USGDynamicTextAssetScanSubsystem::StartScan(uint8 bInForceFullRescan)
{
	if (bScanningInProgress)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
			TEXT("USGDynamicTextAssetScanSubsystem::StartScan: Scan already in progress, ignoring"));
		return;
	}

	USGDynamicTextAssetTickerSubsystem* tickerSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetTickerSubsystem>();
	if (!tickerSubsystem)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error,
			TEXT("USGDynamicTextAssetScanSubsystem::StartScan: Ticker subsystem not available"));
		return;
	}

	if (RegisteredPhases.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
			TEXT("USGDynamicTextAssetScanSubsystem::StartScan: No phases registered, nothing to do"));
		return;
	}

	bScanningInProgress = true;
	bForceFullRescan = bInForceFullRescan;

	// Discover all DTA files once, shared across phases
	DiscoveredDTAFiles.Empty();
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(DiscoveredDTAFiles);

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("USGDynamicTextAssetScanSubsystem: Starting scan with %d phases, %d DTA files discovered"),
		RegisteredPhases.Num(), DiscoveredDTAFiles.Num());

	// Bind completion callback
	if (!AllWorkCompleteDelegateHandle.IsValid())
	{
		AllWorkCompleteDelegateHandle = tickerSubsystem->OnAllWorkComplete.AddUObject(
			this, &USGDynamicTextAssetScanSubsystem::OnTickerAllWorkComplete);
	}

	// Queue each registered phase as a ticker work item
	for (const FSGDynamicTextAssetScanPhase& phase : RegisteredPhases)
	{
		tickerSubsystem->QueueWork(ConvertPhaseToWorkItem(phase));
	}

	tickerSubsystem->StartProcessing();
}

bool USGDynamicTextAssetScanSubsystem::IsScanningInProgress() const
{
	return bScanningInProgress;
}

FSGDynamicTextAssetProjectInfoCache& USGDynamicTextAssetScanSubsystem::GetProjectInfoCache()
{
	return ProjectInfoCache;
}

const FSGDynamicTextAssetProjectInfoCache& USGDynamicTextAssetScanSubsystem::GetProjectInfoCache() const
{
	return ProjectInfoCache;
}

void USGDynamicTextAssetScanSubsystem::UpdateProjectInfoForFile(uint32 SerializerTypeId, const FSGDynamicTextAssetVersion& FileFormatVersion)
{
	ProjectInfoCache.RecordFileVersion(SerializerTypeId, FileFormatVersion);
	ProjectInfoCache.SaveToFile(FSGDynamicTextAssetProjectInfoCache::GetDefaultCachePath());
}

const TArray<FString>& USGDynamicTextAssetScanSubsystem::GetDiscoveredDTAFiles() const
{
	return DiscoveredDTAFiles;
}

FSGTickerWorkItem USGDynamicTextAssetScanSubsystem::ConvertPhaseToWorkItem(const FSGDynamicTextAssetScanPhase& Phase) const
{
	FSGTickerWorkItem workItem;
	workItem.WorkId = Phase.PhaseId;
	workItem.DisplayName = Phase.DisplayName;
	workItem.Priority = Phase.Priority;

	// Wrap SetupPhase to return remaining count
	workItem.Setup = [setupFunction = Phase.SetupPhase, remainingFunction = Phase.GetRemainingCount]()
	{
		if (setupFunction)
		{
			setupFunction();
		}
		return remainingFunction ? remainingFunction() : 0;
	};

	workItem.ProcessOne = Phase.ProcessOneItem;
	workItem.OnComplete = Phase.OnPhaseComplete;
	workItem.GetRemainingCount = Phase.GetRemainingCount;

	return workItem;
}

void USGDynamicTextAssetScanSubsystem::OnTickerAllWorkComplete()
{
	bScanningInProgress = false;
	DiscoveredDTAFiles.Empty();

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("USGDynamicTextAssetScanSubsystem: All scan phases complete"));

	OnScanComplete.Broadcast();
}

void USGDynamicTextAssetScanSubsystem::SetupProjectInfoPhase()
{
	ProjectInfoCache.Reset();
	PendingProjectInfoFiles = DiscoveredDTAFiles;

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
		TEXT("USGDynamicTextAssetScanSubsystem: Project info phase setup, %d files to scan"),
		PendingProjectInfoFiles.Num());
}

bool USGDynamicTextAssetScanSubsystem::ProcessOneProjectInfoItem()
{
	if (PendingProjectInfoFiles.IsEmpty())
	{
		return false;
	}

	const FString filePath = PendingProjectInfoFiles.Pop(EAllowShrinking::No);
	const FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);

	if (fileInfo.bIsValid && fileInfo.SerializerTypeId != ISGDynamicTextAssetSerializer::INVALID_SERIALIZER_TYPE_ID)
	{
		ProjectInfoCache.RecordFileVersion(fileInfo.SerializerTypeId, fileInfo.FileFormatVersion);
	}

	return !PendingProjectInfoFiles.IsEmpty();
}

void USGDynamicTextAssetScanSubsystem::OnProjectInfoPhaseComplete()
{
	ProjectInfoCache.PopulateCurrentSerializerVersions();
	ProjectInfoCache.SaveToFile(FSGDynamicTextAssetProjectInfoCache::GetDefaultCachePath());

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("USGDynamicTextAssetScanSubsystem: Project info phase complete, cache saved"));

	OnFormatVersionScanComplete.Broadcast();
}
