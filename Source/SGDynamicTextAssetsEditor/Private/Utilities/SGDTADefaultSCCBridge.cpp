// Copyright Start Games, Inc. All Rights Reserved.

#include "Utilities/SGDTADefaultSCCBridge.h"

#include "Utilities/SGDTASourceControl.h"

bool FSGDTADefaultSourceControlBridge::CheckOutFile(const FString& FilePath)
{
    return FSGDynamicTextAssetSourceControl::CheckOutFile(FilePath);
}

bool FSGDTADefaultSourceControlBridge::MarkForAdd(const FString& FilePath)
{
    return FSGDynamicTextAssetSourceControl::MarkForAdd(FilePath);
}

bool FSGDTADefaultSourceControlBridge::MarkForDelete(const FString& FilePath)
{
    return FSGDynamicTextAssetSourceControl::MarkForDelete(FilePath);
}
