// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSGDynamicTextAssetsRuntime, Log, All);

/**
 * Module interface for SGDynamicTextAssetsRuntime.
 * Provides runtime loading, caching, and access to dynamic text assets.
 */
class ISGDynamicTextAssetsRuntimeModule : public IModuleInterface
{
public:
    /**
     * Gets the module instance, loading it if necessary.
     * @warning Do not call during shutdown - module may be unloaded.
     */
    static inline ISGDynamicTextAssetsRuntimeModule& Get()
    {
        return FModuleManager::LoadModuleChecked<ISGDynamicTextAssetsRuntimeModule>("SGDynamicTextAssetsRuntime");
    }

    /** Returns true if the module is loaded and ready to use. */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("SGDynamicTextAssetsRuntime");
    }
};
