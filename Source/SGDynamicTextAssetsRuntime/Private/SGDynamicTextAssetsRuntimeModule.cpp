// Copyright Start Games, Inc. All Rights Reserved.

#include "SGDynamicTextAssetsRuntimeModule.h"

#include "Core/SGDynamicTextAsset.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "Serialization/SGDynamicTextAssetXmlSerializer.h"
#include "Serialization/SGDynamicTextAssetYamlSerializer.h"
#include "Statics/SGDynamicTextAssetSlateStyles.h"

DEFINE_LOG_CATEGORY(LogSGDynamicTextAssetsRuntime);

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

