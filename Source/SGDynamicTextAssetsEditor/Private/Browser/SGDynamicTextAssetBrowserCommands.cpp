// Copyright Start Games, Inc. All Rights Reserved.

#include "Browser/SGDynamicTextAssetBrowserCommands.h"

#define LOCTEXT_NAMESPACE "SGDynamicTextAssetBrowserCommands"

void FSGDynamicTextAssetBrowserCommands::RegisterCommands()
{
    UI_COMMAND(Open, "Open", "Open all selected dynamic text assets in the editor", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
}

#undef LOCTEXT_NAMESPACE
