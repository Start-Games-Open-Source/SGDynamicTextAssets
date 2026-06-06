// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EditorSubsystem.h"
#include "Management/SGDTAProjectManifest.h"

#include "SGDTAScanSubsystem.generated.h"

struct FSGDTATickerWorkItem;
class USGDynamicTextAssetTickerSubsystem;

/**
 * A scan phase that can be registered with the scan subsystem.
 * Each phase represents a category of DTA file scanning work
 * (e.g., project info extraction, reference scanning).
 */
struct SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetScanPhase
{
	/** Unique identifier for this scan phase. */
	FName PhaseId;

	/** Human-readable name for toast display. */
	FText DisplayName;

	/** Execution priority. Lower values run first. @see SGDynamicTextAssetScanPriorities */
	int32 Priority = 100;

	/**
	 * Called at the start of this phase. Populate pending work, reset state, etc.
	 * The shared DTA file list (if needed) is available via the scan subsystem.
	 */
	TFunction<void()> SetupPhase;

	/**
	 * Process a single unit of work within this phase.
	 * @return True if more work remains, false when this phase is complete.
	 */
	TFunction<bool()> ProcessOneItem;

	/** Called when all items in this phase have been processed. */
	TFunction<void()> OnPhaseComplete;

	/** Returns the number of remaining items (for progress). */
	TFunction<int32()> GetRemainingCount;
};

/**
 * Editor subsystem that orchestrates DTA file scanning.
 *
 * Uses USGDynamicTextAssetTickerSubsystem for time-sliced execution.
 * External systems register FSGDynamicTextAssetScanPhase objects to participate
 * in the scan. When StartScan() is called, phases are converted to ticker work
 * items and processed in priority order.
 *
 * Also owns the FSGDTAProjectManifest and registers the built-in
 * "Project Info" scan phase for format version tracking.
 */
UCLASS()
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetScanSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	// UEditorSubsystem overrides
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~UEditorSubsystem overrides

	/**
	 * Register a scan phase.
	 * Phases are processed in priority order (lower values first) when StartScan() is called.
	 * If a phase with the same PhaseId already exists, it is replaced.
	 */
	void RegisterScanPhase(const FSGDynamicTextAssetScanPhase& Phase);

	/** Unregister a scan phase by ID. */
	void UnregisterScanPhase(FName PhaseId);

	/**
	 * Start scanning all registered phases.
	 * Discovers DTA files, queues all phases as work items with the ticker subsystem,
	 * and starts processing. No-op if a scan is already in progress.
	 *
	 * @param bForceFullRescan If true, ignores cached timestamps and rescans everything.
	 */
	void StartScan(uint8 bForceFullRescan = false);

	/** Returns true if a scan is currently in progress. */
	bool IsScanningInProgress() const;

	/** Access the project manifest (organization versioning and format version tracking). */
	FSGDTAProjectManifest& GetProjectManifest();
	const FSGDTAProjectManifest& GetProjectManifest() const;

	/**
	 * Incrementally update the project info cache for a single file.
	 * Called after a DTA file is saved to keep the cache current without a full rescan.
	 */
	void UpdateProjectInfoForFile(const FSGDTASerializerFormat& SerializerFormat, const FSGDynamicTextAssetVersion& FileFormatVersion);

	/**
	 * Get the list of DTA files discovered during the current scan.
	 * Only valid during an active scan (after StartScan, before completion).
	 * Phases that need the file list should copy it during their SetupPhase callback.
	 */
	const TArray<FString>& GetDiscoveredDTAFiles() const;

	/** Broadcast when all scan phases have completed. */
	DECLARE_MULTICAST_DELEGATE(FOnScanComplete);
	FOnScanComplete OnScanComplete;

	/** Broadcast when the project info (format version) scan phase completes. */
	DECLARE_MULTICAST_DELEGATE(FOnFormatVersionScanComplete);
	FOnFormatVersionScanComplete OnFormatVersionScanComplete;

private:

	/** Convert a scan phase to a ticker work item. */
	FSGDTATickerWorkItem ConvertPhaseToWorkItem(const FSGDynamicTextAssetScanPhase& Phase) const;

	/** Called when the ticker subsystem finishes all queued work. */
	void OnTickerAllWorkComplete();

	/** Setup callback for the built-in extender discovery scan phase. */
	void SetupExtenderDiscoveryPhase();

	/** Process one class in the extender discovery scan phase. */
	bool ProcessOneExtenderDiscoveryItem();

	/** Completion callback for the extender discovery scan phase. */
	void OnExtenderDiscoveryPhaseComplete();

	/** Setup callback for the built-in project info scan phase. */
	void SetupProjectInfoPhase();

	/** Process one file in the project info scan phase. */
	bool ProcessOneProjectInfoItem();

	/** Completion callback for the project info scan phase. */
	void OnProjectInfoPhaseComplete();

	/** Scan manifest directories and update the ProjectManifest's file list fields. */
	void UpdateProjectManifestFileLists();

	/** Run organization v0-to-v1 migration (move type manifests to new location). */
	void MigrateOrganizationV0ToV1();

	/** Registered scan phases from external systems. */
	TArray<FSGDynamicTextAssetScanPhase> RegisteredPhases;

	/** The project manifest instance. */
	FSGDTAProjectManifest ProjectManifest;

	/** DTA files discovered at the start of a scan. Shared across phases. */
	TArray<FString> DiscoveredDTAFiles;

	/** Extender discovery phase's pending classes to process. */
	TArray<UClass*> PendingExtenderClasses;

	/** Project info phase's own copy of files to process. */
	TArray<FString> PendingProjectInfoFiles;

	/** Whether a scan is in progress. */
	uint8 bScanningInProgress : 1;

	/** Whether the current scan was forced (ignore timestamps). */
	uint8 bForceFullRescan : 1;

	/** Handle for the OnAllWorkComplete delegate binding. */
	FDelegateHandle AllWorkCompleteDelegateHandle;
};
