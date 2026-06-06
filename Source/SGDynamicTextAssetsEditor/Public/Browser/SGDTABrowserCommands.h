// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"

/**
 * Keyboard shortcut commands for the Dynamic Text Asset Browser.
 * Registered separately from FGenericCommands so that custom shortcuts
 * (like Enter-to-open) can appear in the context menu with a key hint.
 */
class FSGDynamicTextAssetBrowserCommands : public TCommands<FSGDynamicTextAssetBrowserCommands>
{
public:
    FSGDynamicTextAssetBrowserCommands()
        : TCommands<FSGDynamicTextAssetBrowserCommands>(
            TEXT("SGDynamicTextAssetBrowser"),
            INVTEXT("Dynamic Text Asset Browser"),
            NAME_None,
            FAppStyle::GetAppStyleSetName()
        )
    { }

    virtual ~FSGDynamicTextAssetBrowserCommands() { }

    // TCommands overrides
    virtual void RegisterCommands() override;
    // ~TCommands overrides

    /** Opens all selected dynamic text assets in the editor (Enter) */
    TSharedPtr<FUICommandInfo> Open = nullptr;
};
