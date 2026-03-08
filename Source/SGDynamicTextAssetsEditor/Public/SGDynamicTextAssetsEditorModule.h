// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSGDynamicTextAssetsEditor, Log, All);

/**
 * Module interface for SGDynamicTextAssetsEditor.
 * Provides editor tooling for browsing, creating, editing, and managing dynamic text assets.
 */
class ISGDynamicTextAssetsEditorModule : public IModuleInterface
{
public:
    /**
     * Gets the module instance, loading it if necessary.
     * @warning Do not call during shutdown - module may be unloaded.
     */
    static inline ISGDynamicTextAssetsEditorModule& Get()
    {
        return FModuleManager::LoadModuleChecked<ISGDynamicTextAssetsEditorModule>("SGDynamicTextAssetsEditor");
    }

    /** Returns true if the module is loaded and ready to use. */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("SGDynamicTextAssetsEditor");
    }
};
