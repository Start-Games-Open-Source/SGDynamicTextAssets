// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SSGDTADirtyBadge.h"

#include "Editor/SGDTAEditorToolkit.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleDefaults.h"
#include "Utilities/SGDTAEditorConstants.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

void SSGDynamicTextAssetDirtyBadge::Construct(const FArguments& InArgs)
{
    FilePath = InArgs._FilePath;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(FSGDynamicTextAssetEditorConstants::DIRTY_BADGE_SIZE)
        .HeightOverride(FSGDynamicTextAssetEditorConstants::DIRTY_BADGE_SIZE)
        [
            SAssignNew(BadgeImage, SImage)
        ]
    ];

    // Compute the initial state once so a browser opened while an editor is already dirty
    // shows the star on first paint; static values only, no bound attributes
    RefreshBadge();

    // Per-instance subscription. AddSP binds weakly, so recycled row widgets that die
    // without unsubscribing are compacted automatically by the multicast delegate.
    OnDirtyStateChangedHandle = FSGDynamicTextAssetEditorToolkit::OnDirtyStateChanged.AddSP(
        this, &SSGDynamicTextAssetDirtyBadge::HandleDirtyChanged);
}

SSGDynamicTextAssetDirtyBadge::~SSGDynamicTextAssetDirtyBadge()
{
    // The delegate is a program-lifetime static on the toolkit class, so unlike the SCC badge
    // there is no destructible cache to gate on; a valid-handle check is sufficient.
    if (OnDirtyStateChangedHandle.IsValid())
    {
        FSGDynamicTextAssetEditorToolkit::OnDirtyStateChanged.Remove(OnDirtyStateChangedHandle);
        OnDirtyStateChangedHandle.Reset();
    }
}

void SSGDynamicTextAssetDirtyBadge::HandleDirtyChanged()
{
    RefreshBadge();
}

void SSGDynamicTextAssetDirtyBadge::RefreshBadge()
{
    if (!BadgeImage.IsValid())
    {
        return;
    }

    const bool bDirty = FSGDynamicTextAssetEditorToolkit::HasOpenEditorWithUnsavedChanges(FilePath);
    BadgeImage->SetImage(bDirty ? FAppStyle::Get().GetBrush("ContentBrowser.ContentDirty") : FStyleDefaults::GetNoBrush());
    BadgeImage->SetVisibility(bDirty ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}
