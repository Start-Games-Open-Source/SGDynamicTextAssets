// Copyright Start Games, Inc. All Rights Reserved.

#include "Settings/SGDTAEditorSettings.h"

#include "Misc/Paths.h"

USGDynamicTextAssetEditorSettings* USGDynamicTextAssetEditorSettings::Get()
{
	return GetMutableDefault<USGDynamicTextAssetEditorSettings>();
}

FString USGDynamicTextAssetEditorSettings::GetCacheRootFolder() const
{
	return FPaths::ProjectSavedDir() / CacheRootFolderPath;
}
