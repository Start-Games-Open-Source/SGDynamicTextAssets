// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EditorSubsystem.h"
#include "Containers/Ticker.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "SGDynamicTextAssetTickerSubsystem.generated.h"

/**
 * A unit of time-sliced work to be processed by the ticker subsystem.
 * Each work item represents a batch of similar operations (e.g., scanning files,
 * processing assets) that should be spread across multiple frames.
 */
struct SGDYNAMICTEXTASSETSEDITOR_API FSGTickerWorkItem
{
	/** Unique identifier for this work item. Used for dequeue and completion callbacks. */
	FName WorkId;

	/** Human-readable name shown in the toast notification during processing. */
	FText DisplayName;

	/** Execution priority. Lower values run first. @see SGDynamicTextAssetScanPriorities */
	int32 Priority = 100;

	/**
	 * Called once when this work item begins processing.
	 * Use to populate pending work queues, reset state, etc.
	 * @return The total number of items to process (for progress calculation).
	 */
	TFunction<int32()> Setup;

	/**
	 * Process a single unit of work.
	 * Called repeatedly within the per-frame time budget.
	 * @return True if more work remains, false when this item is complete.
	 */
	TFunction<bool()> ProcessOne;

	/** Called once when all items for this work unit have been processed. */
	TFunction<void()> OnComplete;

	/** Returns the number of remaining items (for progress display). */
	TFunction<int32()> GetRemainingCount;
};

/**
 * Editor subsystem that handles time-sliced work processing.
 *
 * Generic infrastructure with no knowledge of DTA scanning, files, or caches.
 * External systems queue FSGTickerWorkItem objects, then call StartProcessing()
 * to begin frame-budgeted execution.
 *
 * The ticker is only active while work is being processed. When all work
 * completes (or no work is queued), the ticker is removed.
 */
UCLASS()
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetTickerSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	// UEditorSubsystem overrides
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~UEditorSubsystem overrides

	/**
	 * Queue a work item for processing.
	 * Does not start processing until StartProcessing() is called.
	 * If a work item with the same WorkId is already queued, it is replaced.
	 */
	void QueueWork(const FSGTickerWorkItem& WorkItem);

	/**
	 * Remove a queued work item by ID.
	 * Only effective before StartProcessing() is called or if the item
	 * has not yet begun processing.
	 */
	void DequeueWork(FName WorkId);

	/**
	 * Start processing all queued work items in priority order.
	 * No-op if already processing or if no work is queued.
	 */
	void StartProcessing();

	/** Returns true if the ticker is active and processing work. */
	bool IsProcessing() const;

	/** Returns the number of currently queued (but not yet started) work items. */
	int32 GetQueuedWorkCount() const;

	/** Broadcast when all queued work items have completed. */
	DECLARE_MULTICAST_DELEGATE(FOnAllWorkComplete);
	FOnAllWorkComplete OnAllWorkComplete;

	/** Broadcast when a single work item completes. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorkItemComplete, FName /*WorkId*/);
	FOnWorkItemComplete OnWorkItemComplete;

	/** Broadcast with progress updates during processing. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWorkProgress, int32 /*Current*/, int32 /*Total*/, const FText& /*StatusText*/);
	FOnWorkProgress OnWorkProgress;

private:

	/** Per-frame ticker callback. Returns true to keep ticking, false to remove. */
	bool ProcessBatch(float DeltaTime);

	/** Create or update the toast notification with current progress. */
	void UpdateNotification(const FText& StatusText, float Progress);

	/** Close the toast notification with success/failure state. */
	void CloseNotification(uint8 bSuccess);

	/** Complete all processing and clean up state. */
	void FinishAllWork(uint8 bSuccess);

	/** Work items waiting to be started (not yet sorted or processing). */
	TArray<FSGTickerWorkItem> QueuedWork;

	/** Work items sorted by priority, actively being processed. */
	TArray<FSGTickerWorkItem> ActiveWork;

	/** Index into ActiveWork for the currently processing item. */
	int32 CurrentWorkIndex = 0;

	/** Total items across all active work items (for overall progress). */
	int32 TotalItemsAcrossAll = 0;

	/** Items processed across all active work items so far. */
	int32 ItemsProcessedAcrossAll = 0;

	/** Handle to the registered frame ticker. Invalid when not processing. */
	FTSTicker::FDelegateHandle TickerHandle;

	/** Active toast notification widget. Null when not showing. */
	TSharedPtr<SNotificationItem> NotificationItem;

	/** True while the ticker is active and processing work. */
	uint8 bIsProcessing : 1;
};
