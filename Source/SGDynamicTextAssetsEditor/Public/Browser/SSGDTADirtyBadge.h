// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SImage;

/**
 * Self-contained unsaved-change (dirty) star badge for a single dynamic text asset file.
 *
 * Event-based by design, mirroring SSGDynamicTextAssetSCCBadge: the badge computes its
 * brush and visibility once during Construct and again only when the toolkit broadcasts
 * FSGDynamicTextAssetEditorToolkit::OnDirtyStateChanged. No bound Slate attributes anywhere
 * on this path, so nothing re-evaluates per paint. The authoritative dirty state comes from
 * the path-keyed FSGDynamicTextAssetEditorToolkit::HasOpenEditorWithUnsavedChanges() query.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetDirtyBadge : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetDirtyBadge)
    { }
        /** Absolute path to the file whose unsaved-change state drives the badge. */
        SLATE_ARGUMENT(FString, FilePath)
    SLATE_END_ARGS()

    /** Constructs the badge, computes the initial state, and subscribes to the toolkit dirty-state signal. */
    void Construct(const FArguments& InArgs);

    /** Removes the dirty-state subscription, guarded against being called after the handle was already reset. */
    ~SSGDynamicTextAssetDirtyBadge();

private:
    /** Toolkit dirty-state change handler: recomputes and reapplies the badge state. */
    void HandleDirtyChanged();

    /** Computes the badge brush and visibility from the dirty query and applies them as static values. */
    void RefreshBadge();

    /** The badge image, updated in place by RefreshBadge. */
    TSharedPtr<SImage> BadgeImage;

    /** Absolute path to the file this badge represents. */
    FString FilePath;

    /** Handle for the toolkit dirty-state subscription; removed in the destructor. */
    FDelegateHandle OnDirtyStateChangedHandle;
};
