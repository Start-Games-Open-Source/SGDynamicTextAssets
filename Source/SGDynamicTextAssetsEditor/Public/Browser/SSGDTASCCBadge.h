// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SImage;

struct FSlateBrush;

/**
 * Self-contained source control status badge for a single dynamic text asset file.
 *
 * Event-based by design: the badge computes its brush and visibility once during
 * Construct and again only when the shared SCC status cache broadcasts OnStatusChanged.
 * No bound Slate attributes anywhere on this path, so nothing re-evaluates per paint.
 */
class SGDYNAMICTEXTASSETSEDITOR_API SSGDynamicTextAssetSCCBadge : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSGDynamicTextAssetSCCBadge)
    { }
        /** Absolute path to the file whose cached source control status drives the badge. */
        SLATE_ARGUMENT(FString, FilePath)
    SLATE_END_ARGS()

    /** Constructs the badge, computes the initial state, and subscribes to the SCC status cache. */
    void Construct(const FArguments& InArgs);

    /** Removes the SCC status cache subscription, guarded against the cache being destroyed first during shutdown. */
    ~SSGDynamicTextAssetSCCBadge();

private:
    /** SCC status cache change handler: recomputes and reapplies the badge state. */
    void HandleStatusChanged();

    /** Computes the badge brush and visibility from the cached status and applies them as static values. */
    void RefreshBadge();

    /**
     * Resolves the provider's own ISourceControlState::GetIcon() brush for the file, matching the
     * native Content Browser overlay. Returns nullptr when no badge should be shown: source control
     * disabled, the cache unavailable (shutdown), an invalid state, or an up-to-date file whose
     * provider icon resolves to the no-brush default.
     *
     * @param FilePath Full path to the file whose provider state drives the badge.
     * @return The provider's state icon brush, or nullptr for the no-badge cases.
     */
    static const FSlateBrush* GetSourceControlBadgeBrush(const FString& FilePath);

    /** The badge image, updated in place by RefreshBadge. */
    TSharedPtr<SImage> BadgeImage;

    /** Absolute path to the file this badge represents. */
    FString FilePath;

    /** Handle for the SCC status cache subscription; removed in the destructor. */
    FDelegateHandle OnStatusChangedHandle;
};
