// Copyright Start Games, Inc. All Rights Reserved.

#include "Settings/SGDynamicTextAssetEditorSettings.h"

#include "Misc/Paths.h"

USGDynamicTextAssetEditorSettings* USGDynamicTextAssetEditorSettings::Get()
{
	return GetMutableDefault<USGDynamicTextAssetEditorSettings>();
}

FString USGDynamicTextAssetEditorSettings::GetCacheRootFolder() const
{
	return FPaths::ProjectSavedDir() / CacheRootFolderPath;
}
