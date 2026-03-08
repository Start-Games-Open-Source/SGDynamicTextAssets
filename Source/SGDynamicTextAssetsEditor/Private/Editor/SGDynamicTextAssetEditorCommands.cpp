// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDynamicTextAssetEditorCommands.h"

#define LOCTEXT_NAMESPACE "FSGDynamicTextAssetEditorCommands"

void FSGDynamicTextAssetEditorCommands::RegisterCommands()
{
    UI_COMMAND(Revert,
        "Revert",
        "Discard unsaved changes and reload from the source file",
        EUserInterfaceActionType::Button,
        FInputChord());

    UI_COMMAND(ShowInExplorer,
        "Show in Explorer",
        "Open the file location in the system file browser",
        EUserInterfaceActionType::Button,
        FInputChord());

    UI_COMMAND(OpenReferenceViewer,
        "Reference Viewer",
        "View inbound and outbound references for this dynamic text asset",
        EUserInterfaceActionType::Button,
        FInputChord());
}

#undef LOCTEXT_NAMESPACE
