// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDTAEditorCommands.h"

#define LOCTEXT_NAMESPACE "FSGDynamicTextAssetEditorCommands"

void FSGDynamicTextAssetEditorCommands::RegisterCommands()
{
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

    UI_COMMAND(SourceControlCheckOut,
        "Check Out",
        "Check this file out of source control so it can be edited",
        EUserInterfaceActionType::Button,
        FInputChord());

    UI_COMMAND(SourceControlMarkForAdd,
        "Mark For Add",
        "Mark this file for add in source control so it is staged / tracked",
        EUserInterfaceActionType::Button,
        FInputChord());

    UI_COMMAND(SourceControlRevert,
        "Revert",
        "Revert this file to the depot version, discarding local changes",
        EUserInterfaceActionType::Button,
        FInputChord());
}

#undef LOCTEXT_NAMESPACE
