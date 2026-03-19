// Copyright Start Games, Inc. All Rights Reserved.

#include "SGDynamicTextAssetTickerSubsystem.h"

#include "SGDynamicTextAssetEditorLogs.h"
#include "Framework/Notifications/NotificationManager.h"

void USGDynamicTextAssetTickerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bIsProcessing = false;
	CurrentWorkIndex = 0;
	TotalItemsAcrossAll = 0;
	ItemsProcessedAcrossAll = 0;
}

void USGDynamicTextAssetTickerSubsystem::Deinitialize()
{
	// Remove ticker if still active
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	CloseNotification(false);
	QueuedWork.Empty();
	ActiveWork.Empty();
	bIsProcessing = false;

	Super::Deinitialize();
}

void USGDynamicTextAssetTickerSubsystem::QueueWork(const FSGTickerWorkItem& WorkItem)
{
	if (!WorkItem.WorkId.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
			TEXT("USGDynamicTextAssetTickerSubsystem::QueueWork: WorkItem has invalid WorkId, ignoring"));
		return;
	}

	// Replace existing item with same ID
	QueuedWork.RemoveAll([&WorkItem](const FSGTickerWorkItem& Existing)
	{
		return Existing.WorkId == WorkItem.WorkId;
	});

	QueuedWork.Add(WorkItem);

	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
		TEXT("USGDynamicTextAssetTickerSubsystem: Queued work '%s' (Priority=%d). Total queued: %d"),
		*WorkItem.WorkId.ToString(), WorkItem.Priority, QueuedWork.Num());
}

void USGDynamicTextAssetTickerSubsystem::DequeueWork(FName WorkId)
{
	QueuedWork.RemoveAll([WorkId](const FSGTickerWorkItem& Item)
	{
		return Item.WorkId == WorkId;
	});
}

void USGDynamicTextAssetTickerSubsystem::StartProcessing()
{
	if (bIsProcessing)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
			TEXT("USGDynamicTextAssetTickerSubsystem::StartProcessing: Already processing, ignoring"));
		return;
	}

	if (QueuedWork.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
			TEXT("USGDynamicTextAssetTickerSubsystem::StartProcessing: No work queued, ignoring"));
		return;
	}

	// Move queued work to active, sorted by priority (ascending)
	ActiveWork = MoveTemp(QueuedWork);
	QueuedWork.Empty();
	ActiveWork.Sort([](const FSGTickerWorkItem& A, const FSGTickerWorkItem& B)
	{
		return A.Priority < B.Priority;
	});

	// Set up each work item and compute total items
	TotalItemsAcrossAll = 0;
	ItemsProcessedAcrossAll = 0;
	CurrentWorkIndex = 0;

	for (FSGTickerWorkItem& workItem : ActiveWork)
	{
		if (workItem.Setup)
		{
			workItem.Setup();
		}

		if (workItem.GetRemainingCount)
		{
			TotalItemsAcrossAll += workItem.GetRemainingCount();
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log,
		TEXT("USGDynamicTextAssetTickerSubsystem: Starting processing %d work items (%d total items)"),
		ActiveWork.Num(), TotalItemsAcrossAll);

	// Fast path: no actual work to do
	if (TotalItemsAcrossAll == 0)
	{
		// Still call OnComplete for each work item
		for (FSGTickerWorkItem& workItem : ActiveWork)
		{
			if (workItem.OnComplete)
			{
				workItem.OnComplete();
			}
			OnWorkItemComplete.Broadcast(workItem.WorkId);
		}
		ActiveWork.Empty();
		OnAllWorkComplete.Broadcast();
		return;
	}

	bIsProcessing = true;

	// Register ticker
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USGDynamicTextAssetTickerSubsystem::ProcessBatch),
		0.0f
	);
}

bool USGDynamicTextAssetTickerSubsystem::IsProcessing() const
{
	return bIsProcessing;
}

int32 USGDynamicTextAssetTickerSubsystem::GetQueuedWorkCount() const
{
	return QueuedWork.Num();
}

bool USGDynamicTextAssetTickerSubsystem::ProcessBatch(float DeltaTime)
{
	static constexpr double TIME_BUDGET_SECONDS = 0.010;
	const double startTime = FPlatformTime::Seconds();

	while ((FPlatformTime::Seconds() - startTime) < TIME_BUDGET_SECONDS)
	{
		if (CurrentWorkIndex >= ActiveWork.Num())
		{
			// All work items complete
			FinishAllWork(true);
			return false;
		}

		FSGTickerWorkItem& currentItem = ActiveWork[CurrentWorkIndex];

		if (currentItem.ProcessOne && currentItem.ProcessOne())
		{
			// More work remains in this item
			ItemsProcessedAcrossAll++;
		}
		else
		{
			// This work item is done
			ItemsProcessedAcrossAll++;

			if (currentItem.OnComplete)
			{
				currentItem.OnComplete();
			}
			OnWorkItemComplete.Broadcast(currentItem.WorkId);

			UE_LOG(LogSGDynamicTextAssetsEditor, Log,
				TEXT("USGDynamicTextAssetTickerSubsystem: Work item '%s' complete"),
				*currentItem.WorkId.ToString());

			CurrentWorkIndex++;
		}
	}

	// Update progress
	if (CurrentWorkIndex < ActiveWork.Num())
	{
		const FSGTickerWorkItem& currentItem = ActiveWork[CurrentWorkIndex];
		int32 remaining = currentItem.GetRemainingCount ? currentItem.GetRemainingCount() : 0;

		FText statusText = FText::Format(
			INVTEXT("SG Dynamic Text Assets\n{0}... ({1} remaining)"),
			currentItem.DisplayName,
			FText::AsNumber(remaining)
		);

		float progress = (TotalItemsAcrossAll > 0)
			? static_cast<float>(ItemsProcessedAcrossAll) / static_cast<float>(TotalItemsAcrossAll)
			: 0.0f;

		UpdateNotification(statusText, progress);
		OnWorkProgress.Broadcast(ItemsProcessedAcrossAll, TotalItemsAcrossAll, statusText);
	}

	return true;
}

void USGDynamicTextAssetTickerSubsystem::FinishAllWork(uint8 bSuccess)
{
	bIsProcessing = false;

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	CloseNotification(bSuccess);
	ActiveWork.Empty();
	CurrentWorkIndex = 0;
	TotalItemsAcrossAll = 0;
	ItemsProcessedAcrossAll = 0;

	OnAllWorkComplete.Broadcast();
}

void USGDynamicTextAssetTickerSubsystem::UpdateNotification(const FText& StatusText, float Progress)
{
	if (!NotificationItem.IsValid())
	{
		FNotificationInfo info(INVTEXT("SG Dynamic Text Assets"));
		info.bFireAndForget = false;
		info.bUseThrobber = true;
		info.bUseSuccessFailIcons = false;
		info.ExpireDuration = 0.0f;
		info.FadeOutDuration = 0.5f;

		NotificationItem = FSlateNotificationManager::Get().AddNotification(info);
		if (NotificationItem.IsValid())
		{
			NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}

	if (NotificationItem.IsValid())
	{
		int32 percentage = FMath::RoundToInt(Progress * 100.0f);
		FText progressText = FText::Format(
			INVTEXT("{0} ({1}%)"),
			StatusText,
			FText::AsNumber(percentage)
		);
		NotificationItem->SetText(progressText);
	}
}

void USGDynamicTextAssetTickerSubsystem::CloseNotification(uint8 bSuccess)
{
	if (NotificationItem.IsValid())
	{
		if (bSuccess)
		{
			NotificationItem->SetText(INVTEXT("SG Dynamic Text Assets\nScan complete"));
			NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		}
		else
		{
			NotificationItem->SetText(INVTEXT("SG Dynamic Text Assets\nScan failed"));
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		}

		NotificationItem->ExpireAndFadeout();
		NotificationItem.Reset();
	}
}
