// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDTASCCBadge.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Styling/StyleDefaults.h"
#include "Textures/SlateIcon.h"
#include "Utilities/SGDTAEditorConstants.h"
#include "Utilities/SGDTASCCStatusCache.h"
#include "Utilities/SGDTASourceControl.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

void SSGDynamicTextAssetSCCBadge::Construct(const FArguments& InArgs)
{
    FilePath = InArgs._FilePath;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(FSGDynamicTextAssetEditorConstants::SCC_BADGE_SIZE)
        .HeightOverride(FSGDynamicTextAssetEditorConstants::SCC_BADGE_SIZE)
        [
            SAssignNew(BadgeImage, SImage)
        ]
    ];

    // Compute the initial state once; static values only, no bound attributes
    RefreshBadge();

    // Per-instance subscription. AddSP binds weakly, so recycled row widgets that die
    // without unsubscribing are compacted automatically by the multicast delegate.
    if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
    {
        OnStatusChangedHandle = FSGDynamicTextAssetSCCStatusCache::Get().OnStatusChanged.AddSP(
            this, &SSGDynamicTextAssetSCCBadge::HandleStatusChanged);
    }
}

SSGDynamicTextAssetSCCBadge::~SSGDynamicTextAssetSCCBadge()
{
    // Guarded: the module can tear the cache down before the browser tab closes during shutdown
    if (OnStatusChangedHandle.IsValid() && FSGDynamicTextAssetSCCStatusCache::IsAvailable())
    {
        FSGDynamicTextAssetSCCStatusCache::Get().OnStatusChanged.Remove(OnStatusChangedHandle);
        OnStatusChangedHandle.Reset();
    }
}

void SSGDynamicTextAssetSCCBadge::HandleStatusChanged()
{
    RefreshBadge();
}

void SSGDynamicTextAssetSCCBadge::RefreshBadge()
{
    if (!BadgeImage.IsValid())
    {
        return;
    }

    const FSlateBrush* badgeBrush = GetSourceControlBadgeBrush(FilePath);
    BadgeImage->SetImage(badgeBrush ? badgeBrush : FStyleDefaults::GetNoBrush());
    BadgeImage->SetVisibility(badgeBrush ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

const FSlateBrush* SSGDynamicTextAssetSCCBadge::GetSourceControlBadgeBrush(const FString& FilePath)
{
    // Shutdown-safe: rows can still paint after the module has destroyed the cache.
    // Also skip when source control is disabled, since there is no provider to query.
    if (!FSGDynamicTextAssetSCCStatusCache::IsAvailable()
        || !FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        return nullptr;
    }

    // Read the provider's own state icon directly, matching the native Content Browser overlay.
    // Each provider bakes in its own correct policy (including an empty icon for an up-to-date
    // file), so the badge stays faithful per provider and consistent with the tile tooltip,
    // which reads GetDisplayTooltip() from the same state.
    const FSourceControlStatePtr state =
        ISourceControlModule::Get().GetProvider().GetState(FilePath, EStateCacheUsage::Use);
    if (!state.IsValid())
    {
        return nullptr;
    }

    // GetIcon() resolves to FStyleDefaults::GetNoBrush() when the provider has no overlay for this
    // state (an up-to-date file). Treat that (and null) as "no badge", mirroring SAssetViewItem's
    // bHasCCStateBrush check, so an up-to-date file shows nothing.
    const FSlateBrush* brush = state->GetIcon().GetIcon();
    if (brush == nullptr || brush == FStyleDefaults::GetNoBrush())
    {
        return nullptr;
    }

    return brush;
}
