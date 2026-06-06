// Copyright Start Games, Inc. All Rights Reserved.

#include "SGDTARuntimeModule.h"

#include "Management/SGDTAFileManager.h"
#include "Serialization/SGDTAJsonSerializer.h"
#include "Serialization/SGDTAXmlSerializer.h"
#include "Serialization/SGDTAYamlSerializer.h"
#include "SGDynamicTextAssetLogs.h"
#include "Statics/SGDTASlateStyles.h"

#if WITH_EDITOR
#include "Management/SGDTAEditorCache.h"
#endif

#define LOCTEXT_NAMESPACE "FSGDynamicTextAssetsRuntimeModule"

/**
 * Implementation of the SGDynamicTextAssetsRuntime module.
 * Handles initialization and shutdown of the dynamic text asset runtime system.
 */
class FSGDynamicTextAssetsRuntimeModule : public ISGDynamicTextAssetsRuntimeModule
{
public:
    /** Called when the module is loaded into memory. */
    virtual void StartupModule() override
    {
        // Register slate styles
        FSGDynamicTextAssetSlateStyles::Initialize();

        FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetJsonSerializer>();
        FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetXmlSerializer>();
        FSGDynamicTextAssetFileManager::RegisterSerializer<FSGDynamicTextAssetYamlSerializer>();

        FSGDynamicTextAssetFileManager::ON_GENERATE_DEFAULT_CONTENT.BindStatic([]
            (const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId, TSharedRef<ISGDynamicTextAssetSerializer> Serializer) -> FString
        {
            return Serializer->GetDefaultFileContent(DynamicTextAssetClass, Id, UserFacingId);
        });

        UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("SGDynamicTextAssetsRuntime module initialized"));
    }

    /** Called when the module is unloaded from memory. */
    virtual void ShutdownModule() override
    {
        FSGDynamicTextAssetFileManager::ON_GENERATE_DEFAULT_CONTENT.Unbind();
        FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGDynamicTextAssetYamlSerializer>();
        FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGDynamicTextAssetXmlSerializer>();
        FSGDynamicTextAssetFileManager::UnregisterSerializer<FSGDynamicTextAssetJsonSerializer>();

#if WITH_EDITOR
        FSGDynamicTextAssetEditorCache::TearDown();
#endif

        // Deregister slate styles
        FSGDynamicTextAssetSlateStyles::Shutdown();

        UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("SGDynamicTextAssetsRuntime module shutdown"));
    }

    /**
     * Returns whether this is a gameplay module (true) or editor-only module (false).
     * Gameplay modules are loaded in packaged builds.
     */
    virtual bool IsGameModule() const override
    {
        return true;
    }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSGDynamicTextAssetsRuntimeModule, SGDynamicTextAssetsRuntime)

