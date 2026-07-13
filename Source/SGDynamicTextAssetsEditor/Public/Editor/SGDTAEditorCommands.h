// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Styling/AppStyle.h"

/**
 * Toolbar command definitions for the dynamic text asset editor window.
 *
 * Registered once per editor session and shared across all open
 * FSGDynamicTextAssetEditorToolkit instances.
 */
class FSGDynamicTextAssetEditorCommands : public TCommands<FSGDynamicTextAssetEditorCommands>
{
public:

    FSGDynamicTextAssetEditorCommands()
        : TCommands<FSGDynamicTextAssetEditorCommands>(
            TEXT("SGDynamicTextAssetEditor"),
            INVTEXT("Dynamic Text Asset Editor"),
            NAME_None,
            FAppStyle::GetAppStyleSetName())
    {
    }

    virtual void RegisterCommands() override;

    /** Reveal the file in the OS file browser */
    TSharedPtr<FUICommandInfo> ShowInExplorer = nullptr;

    /** Open the Reference Viewer for this dynamic text asset */
    TSharedPtr<FUICommandInfo> OpenReferenceViewer = nullptr;

    /** Check the edited file out of source control */
    TSharedPtr<FUICommandInfo> SourceControlCheckOut = nullptr;

    /** Mark the edited file for add in source control */
    TSharedPtr<FUICommandInfo> SourceControlMarkForAdd = nullptr;

    /** Revert the edited file to the depot version, discarding local changes */
    TSharedPtr<FUICommandInfo> SourceControlRevert = nullptr;
};
