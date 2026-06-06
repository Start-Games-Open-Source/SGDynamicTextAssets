// Copyright Start Games, Inc. All Rights Reserved.

#include "SGDTAScanSubsystem.h"

#include "Editor.h"
#include "Management/SGDTAExtenderManifest.h"
#include "Management/SGDTASerializerExtenderRegistry.h"
#include "Management/SGDTAFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Serialization/AssetBundleExtenders/SGDTAAssetBundleExtender.h"
#include "Serialization/SGDTASerializer.h"
#include "SGDTAEditorLogs.h"
#include "SGDTATickerSubsystem.h"
#include "Statics/SGDTAConstants.h"
#include "Utilities/SGDTAScanPriorities.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

void USGDynamicTextAssetScanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<USGDynamicTextAssetTickerSubsystem>();

	bScanningInProgress = false;
	bForceFullRescan = false;

	// Load project manifest from disk
	const FString manifestPath = FSGDTAProjectManifest::GetDefaultManifestPath();
	ProjectManifest.LoadFromFile(manifestPath);

	// Check organization version and migrate if needed
	if (!ProjectManifest.IsLoaded() || !ProjectManifest.IsCurrentVersion())
	{
		const int32 loadedVersion = ProjectManifest.GetOrganizationVersion();
		UE_LOG(LogSGDynamicTextAssetsEditor, Log,
			TEXT("Project manifest organizationVersion %d != current %d, running migration"),
			loadedVersion, FSGDTAProjectManifest::CURRENT_ORGANIZATION_VERSION);

		if (loadedVersion < 1)
		{
			MigrateOrganizationV0ToV1();
		}

		ProjectManifest.SetOrganizationVersion(FSGDTAProjectManifest::CURRENT_ORGANIZATION_VERSION);
		ProjectManifest.SaveToFile(manifestPath);
	}

	// Register the built-in extender discovery scan phase
	FSGDynamicTextAssetScanPhase extenderDiscoveryPhase;
	extenderDiscoveryPhase.PhaseId = FName(TEXT("ExtenderDiscovery"));
	extenderDiscoveryPhase.DisplayName = INVTEXT("Extender Discovery");
	extenderDiscoveryPhase.Priority = SGDynamicTextAssetScanPriorities::EXTENDER_DISCOVERY;
	extenderDiscoveryPhase.SetupPhase = [this]() { SetupExtenderDiscoveryPhase(); };
	extenderDiscoveryPhase.ProcessOneItem = [this]() { return ProcessOneExtenderDiscoveryItem(); };
	extenderDiscoveryPhase.OnPhaseComplete = [this]() { OnExtenderDiscoveryPhaseComplete(); };
	extenderDiscoveryPhase.GetRemainingCount = [this]() { return PendingExtenderClasses.Num(); };
	RegisterScanPhase(extenderDiscoveryPhase);

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
	PendingExtenderClasses.Empty();
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

FSGDTAProjectManifest& USGDynamicTextAssetScanSubsystem::GetProjectManifest()
{
	return ProjectManifest;
}

const FSGDTAProjectManifest& USGDynamicTextAssetScanSubsystem::GetProjectManifest() const
{
	return ProjectManifest;
}

void USGDynamicTextAssetScanSubsystem::UpdateProjectInfoForFile(const FSGDTASerializerFormat& SerializerFormat, const FSGDynamicTextAssetVersion& FileFormatVersion)
{
	ProjectManifest.RecordFileVersion(SerializerFormat, FileFormatVersion);
	ProjectManifest.SaveToFile(FSGDTAProjectManifest::GetDefaultManifestPath());
}

const TArray<FString>& USGDynamicTextAssetScanSubsystem::GetDiscoveredDTAFiles() const
{
	return DiscoveredDTAFiles;
}

FSGDTATickerWorkItem USGDynamicTextAssetScanSubsystem::ConvertPhaseToWorkItem(const FSGDynamicTextAssetScanPhase& Phase) const
{
	FSGDTATickerWorkItem workItem;
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

	UpdateProjectManifestFileLists();

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("USGDynamicTextAssetScanSubsystem: All scan phases complete"));

	OnScanComplete.Broadcast();
}

void USGDynamicTextAssetScanSubsystem::SetupExtenderDiscoveryPhase()
{
	PendingExtenderClasses.Empty();

	TArray<UClass*> derivedClasses;
	GetDerivedClasses(USGDTAAssetBundleExtender::StaticClass(), derivedClasses);

	for (UClass* derivedClass : derivedClasses)
	{
		if (derivedClass && !derivedClass->HasAnyClassFlags(CLASS_Abstract))
		{
			PendingExtenderClasses.Add(derivedClass);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("ExtenderDiscovery: Found %d non-abstract extender classes to process"),
		PendingExtenderClasses.Num());
}

bool USGDynamicTextAssetScanSubsystem::ProcessOneExtenderDiscoveryItem()
{
	if (PendingExtenderClasses.IsEmpty())
	{
		return false;
	}

	UClass* extenderClass = PendingExtenderClasses.Pop(EAllowShrinking::No);
	if (!extenderClass)
	{
		return PendingExtenderClasses.Num() > 0;
	}

	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	if (!registry)
	{
		return PendingExtenderClasses.Num() > 0;
	}

	TSharedPtr<FSGDTAExtenderManifest> manifest = registry->GetExtenderRegistry().GetOrCreateManifest(
		SGDynamicTextAssetConstants::ASSET_BUNDLE_EXTENDER_FRAMEWORK_KEY);
	if (!manifest.IsValid())
	{
		return PendingExtenderClasses.Num() > 0;
	}

	const FString className = extenderClass->GetName();
	const FSoftObjectPath classPath(extenderClass);
	const FString classPathStr = classPath.ToString();

	if (const FSGDTASerializerExtenderRegistryEntry* existing = manifest->FindByClassName(className))
	{
		// Verify class path is current, update if stale
		if (existing->Class.ToString() != classPathStr)
		{
			manifest->AddExtender(existing->ExtenderId, TSoftClassPtr<UObject>(classPath));
			UE_LOG(LogSGDynamicTextAssetsEditor, Log,
				TEXT("ExtenderDiscovery: Updated classPath for '%s'"), *className);
		}
	}
	else
	{
		// New extender class discovered
		FSGDTAClassId newId = FSGDTAClassId::NewGeneratedId();
		manifest->AddExtender(newId, TSoftClassPtr<UObject>(classPath));
		UE_LOG(LogSGDynamicTextAssetsEditor, Log,
			TEXT("ExtenderDiscovery: Discovered new extender '%s', assigned ClassId(%s)"),
			*className, *newId.ToString());
	}

	return PendingExtenderClasses.Num() > 0;
}

void USGDynamicTextAssetScanSubsystem::OnExtenderDiscoveryPhaseComplete()
{
	USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get();
	if (!registry)
	{
		return;
	}

	// Save manifest if new extenders were discovered
	if (registry->GetExtenderRegistry().HasAnyDirty())
	{
		const FString directory = FSGDynamicTextAssetFileManager::GetSerializerExtendersPath();
		registry->GetExtenderRegistry().SaveAllManifests(directory);
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("ExtenderDiscovery: Phase complete"));
}

void USGDynamicTextAssetScanSubsystem::SetupProjectInfoPhase()
{
	ProjectManifest.ResetFormatVersionData();
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

	if (fileInfo.bIsValid && fileInfo.SerializerFormat.IsValid())
	{
		ProjectManifest.RecordFileVersion(fileInfo.SerializerFormat, fileInfo.FileFormatVersion);
	}

	return !PendingProjectInfoFiles.IsEmpty();
}

void USGDynamicTextAssetScanSubsystem::OnProjectInfoPhaseComplete()
{
	ProjectManifest.PopulateCurrentSerializerVersions();
	ProjectManifest.SaveToFile(FSGDTAProjectManifest::GetDefaultManifestPath());

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("USGDynamicTextAssetScanSubsystem: Project info phase complete, cache saved"));

	OnFormatVersionScanComplete.Broadcast();
}

void USGDynamicTextAssetScanSubsystem::UpdateProjectManifestFileLists()
{
	// Scan TypeManifests directory
	const FString typeManifestsDir = FPaths::Combine(
		FSGDynamicTextAssetFileManager::GetInternalFilesRootPath(), TEXT("TypeManifests"));
	TArray<FString> typeManifestFiles;
	IFileManager::Get().FindFiles(typeManifestFiles,
		*FPaths::Combine(typeManifestsDir, TEXT("*_TypeManifest.json")), true, false);

	TArray<FString> typeManifestRelPaths;
	for (const FString& filename : typeManifestFiles)
	{
		typeManifestRelPaths.Add(FString::Printf(TEXT("TypeManifests/%s"), *filename));
	}
	ProjectManifest.SetTypeManifestPaths(typeManifestRelPaths);

	// Scan SerializerExtenders directory
	const FString extendersDir = FSGDynamicTextAssetFileManager::GetSerializerExtendersPath();
	TArray<FString> extenderFiles;
	IFileManager::Get().FindFiles(extenderFiles,
		*FPaths::Combine(extendersDir, TEXT("*.dta.json")), true, false);

	TArray<FString> extenderRelPaths;
	for (const FString& filename : extenderFiles)
	{
		extenderRelPaths.Add(FString::Printf(TEXT("SerializerExtenders/%s"), *filename));
	}
	ProjectManifest.SetExtenderManifestPaths(extenderRelPaths);

	// Save updated manifest
	ProjectManifest.SaveToFile(FSGDTAProjectManifest::GetDefaultManifestPath());
}

void USGDynamicTextAssetScanSubsystem::MigrateOrganizationV0ToV1()
{
	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("MigrateOrganizationV0ToV1: Starting organization migration from v0 to v1"));

	// Move type manifests from Content/SGDynamicTextAssets/ to Content/_SGDynamicTextAssets/TypeManifests/
	const FString oldRoot = FSGDynamicTextAssetFileManager::GetDynamicTextAssetsRootPath();
	const FString newTypeManifestsDir = FPaths::Combine(
		FSGDynamicTextAssetFileManager::GetInternalFilesRootPath(), TEXT("TypeManifests"));

	// Ensure destination directory exists
	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	platformFile.CreateDirectoryTree(*newTypeManifestsDir);

	// Find all type manifests recursively in the old location
	TArray<FString> foundFiles;
	IFileManager::Get().FindFilesRecursive(foundFiles, *oldRoot, TEXT("*_TypeManifest.json"), true, false);

	int32 movedCount = 0;
	for (const FString& oldFilePath : foundFiles)
	{
		// Skip files already in _SGDynamicTextAssets (already migrated)
		if (oldFilePath.Contains(TEXT("_SGDynamicTextAssets")))
		{
			continue;
		}

		const FString filename = FPaths::GetCleanFilename(oldFilePath);
		const FString newFilePath = FPaths::Combine(newTypeManifestsDir, filename);

		// Skip if destination already exists
		if (platformFile.FileExists(*newFilePath))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Log,
				TEXT("MigrateOrganizationV0ToV1: Skipping '%s' (destination already exists)"), *filename);
			continue;
		}

		if (platformFile.MoveFile(*newFilePath, *oldFilePath))
		{
			movedCount++;
			UE_LOG(LogSGDynamicTextAssetsEditor, Log,
				TEXT("MigrateOrganizationV0ToV1: Moved '%s' to TypeManifests/"), *filename);
		}
		else
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
				TEXT("MigrateOrganizationV0ToV1: Failed to move '%s'"), *oldFilePath);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("MigrateOrganizationV0ToV1: Migration complete (%d files moved)"), movedCount);
}
