// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Management/SGDTASCCBridge.h"

/**
 * The single editor-module implementation of the Runtime-declared source-control
 * command seam (ISGDTASourceControlBridge).
 *
 * A thin wrapper over the static FSGDynamicTextAssetSourceControl calls. The editor
 * module owns one instance and registers it on FSGDynamicTextAssetFileManager in
 * StartupModule, clearing it in ShutdownModule (see SGDTAEditorModule.cpp). This is
 * internal infrastructure and is never implemented by DTA provider types.
 */
class FSGDTADefaultSourceControlBridge final : public ISGDTASourceControlBridge
{
public:
    // ISGDTASourceControlBridge interface
    virtual bool CheckOutFile(const FString& FilePath) override;
    virtual bool MarkForAdd(const FString& FilePath) override;
    virtual bool MarkForDelete(const FString& FilePath) override;
    // ~ISGDTASourceControlBridge interface
};
