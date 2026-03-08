// Copyright Start Games, Inc. All Rights Reserved.

#include "SGDynamicTextAssetsEditorModule.h"

#include "Browser/SSGDynamicTextAssetBrowser.h"
#include "Core/SGDynamicTextAsset.h"
#include "Editor/FSGDynamicTextAssetIdCustomization.h"
#include "Editor/SGDynamicTextAssetIdentityCustomization.h"
#include "Customization/SGAssetTypeIdCustomization.h"
#include "Browser/SGDynamicTextAssetBrowserCommands.h"
#include "Editor/SGDynamicTextAssetEditorCommands.h"
#include "Editor/SGDynamicTextAssetRefCustomization.h"
#include "Editor/FSGDynamicTextAssetEditorToolkit.h"
#include "ReferenceViewer/SGDynamicTextAssetReferenceSubsystem.h"
#include "ReferenceViewer/SSGDynamicTextAssetReferenceViewer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LevelEditor.h"
#include "PropertyEditorModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Utilities/SGDynamicTextAssetCookUtils.h"
#include "Utilities/SGDynamicTextAssetSourceControl.h"
#include "ToolMenus.h"
#include "Editor.h"
#include "UObject/ICookInfo.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY(LogSGDynamicTextAssetsEditor);

#define LOCTEXT_NAMESPACE "FSGDynamicTextAssetsEditorModule"

/**
 * Implementation of the SGDynamicTextAssetsEditor module.
 * Handles initialization and shutdown of the dynamic text asset editor tools.
 */
class FSGDynamicTextAssetsEditorModule : public ISGDynamicTextAssetsEditorModule
{
public:
    /** Called when the module is loaded into memory. */
    virtual void StartupModule() override
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("SGDynamicTextAssetsEditor module initialized"));

        // Register TCommands singletons (must outlive all widget/toolkit instances)
        FSGDynamicTextAssetEditorCommands::Register();
        FSGDynamicTextAssetBrowserCommands::Register();

        // Register tab spawner
        RegisterTabSpawner();

        // Register menu extensions
        RegisterMenuExtensions();

        // Register detail customizations
        RegisterDetailCustomizations();

        // Start async reference scan after Asset Registry finishes loading
        // This runs in background without blocking the editor
        FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& assetRegistry = assetRegistryModule.Get();
        
        if (assetRegistry.IsLoadingAssets())
        {
            // Asset registry still loading - wait for completion
            assetRegistry.OnFilesLoaded().AddLambda([]()
            {
                if (GEditor)
                {
                    if (USGDynamicTextAssetReferenceSubsystem* refSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetReferenceSubsystem>())
                    {
                        refSubsystem->RebuildReferenceCacheAsync();
                        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Started async reference scan after Asset Registry ready"));
                    }
                }
            });
        }
        else
        {
            // Asset registry already loaded - start scan after a short delay to let editor finish initializing
            FCoreDelegates::OnPostEngineInit.AddLambda([]()
            {
                if (GEditor)
                {
                    GEditor->GetTimerManager()->SetTimerForNextTick([]()
                    {
                        if (GEditor)
                        {
                            if (USGDynamicTextAssetReferenceSubsystem* refSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetReferenceSubsystem>())
                            {
                                refSubsystem->RebuildReferenceCacheAsync();
                                UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Started async reference scan on editor startup"));
                            }
                        }
                    });
                }
            });
        }

        // Subscribe to cook delegate for automatic dynamic text asset cooking during packaging
        CookStartedHandle = UE::Cook::FDelegates::CookStarted.AddRaw(this, &FSGDynamicTextAssetsEditorModule::OnCookStarted);

        // Subscribe to ModifyCook delegate to ensure DTA soft references are included in cooked builds
        ModifyCookHandle = UE::Cook::FDelegates::ModifyCook.AddRaw(this, &FSGDynamicTextAssetsEditorModule::OnModifyCook);

        // Register the cooked dynamic text assets directory for staging into packaged builds
        RegisterCookedDirectoryForStaging();

        // Warn if cooked files are tracked by source control (runs after editor finishes initializing)
        FCoreDelegates::OnPostEngineInit.AddLambda([]()
        {
            if (GEditor)
            {
                GEditor->GetTimerManager()->SetTimerForNextTick([]()
                {
                    WarnIfCookedFilesInSourceControl();
                });
            }
        });
    }

    /** Called when the module is unloaded from memory. */
    virtual void ShutdownModule() override
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("SGDynamicTextAssetsEditor module shutdown"));

        // Unsubscribe from cook delegates
        UE::Cook::FDelegates::CookStarted.Remove(CookStartedHandle);
        UE::Cook::FDelegates::ModifyCook.Remove(ModifyCookHandle);

        // Unregister TCommands singletons
        FSGDynamicTextAssetEditorCommands::Unregister();
        FSGDynamicTextAssetBrowserCommands::Unregister();

        // Unregister detail customizations
        UnregisterDetailCustomizations();

        // Unregister tab spawner
        UnregisterTabSpawner();

        // Cleanup menu extensions
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }

    /**
     * Returns whether this is a gameplay module (true) or editor-only module (false).
     * Gameplay modules are loaded in packaged builds.
     */
    virtual bool IsGameModule() const override
    {
        return false;  // Editor-only module
    }

private:

    /** Registers the browser and editor tab spawners */
    void RegisterTabSpawner()
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            SSGDynamicTextAssetBrowser::GetTabId(),
            FOnSpawnTab::CreateRaw(this, &FSGDynamicTextAssetsEditorModule::SpawnBrowserTab))
            .SetDisplayName(SSGDynamicTextAssetBrowser::GetTabDisplayName())
            .SetTooltipText(SSGDynamicTextAssetBrowser::GetTabTooltip())
            .SetMenuType(ETabSpawnerMenuType::Hidden)
            .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"));

        // Register the reference viewer tab spawner
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            SSGDynamicTextAssetReferenceViewer::GetTabId(),
            FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) -> TSharedRef<SDockTab>
            {
                // Default spawner - actual tabs are created by SSGDynamicTextAssetReferenceViewer::OpenViewer
                return SNew(SDockTab).TabRole(ETabRole::NomadTab);
            }))
            .SetDisplayName(SSGDynamicTextAssetReferenceViewer::GetTabDisplayName())
            .SetTooltipText(SSGDynamicTextAssetReferenceViewer::GetTabTooltip())
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Registered Dynamic Text Asset Browser and Reference Viewer tab spawners"));
    }

    /** Unregisters the browser, editor, and reference viewer tab spawners */
    void UnregisterTabSpawner()
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SSGDynamicTextAssetBrowser::GetTabId());
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SSGDynamicTextAssetReferenceViewer::GetTabId());
    }

    /** Spawns the browser tab */
    TSharedRef<SDockTab> SpawnBrowserTab(const FSpawnTabArgs& Args)
    {
        return SNew(SDockTab)
            .Clipping(EWidgetClipping::ClipToBounds)
            .TabRole(ETabRole::NomadTab)
            .ToolTipText(SSGDynamicTextAssetBrowser::GetTabTooltip())
            [
                SNew(SSGDynamicTextAssetBrowser)
            ];
    }

    /** Registers menu extensions for the Window menu */
    void RegisterMenuExtensions()
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSGDynamicTextAssetsEditorModule::RegisterMenus));
    }

    /** Called when menus are ready to be extended */
    void RegisterMenus()
    {
        // Owner for unregistration purposes
        FToolMenuOwnerScoped ownerScoped(this);

        // Extend the Level Editor's main menu bar
        if (UToolMenu* mainMenu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu"))
        {
            // We want to insert our menu after the "Help" menu
            FToolMenuInsert insertPos("Help", EToolMenuInsertType::After);

            // Add the new "Start Games" pull-down menu
            FToolMenuSection& startGamesSection = mainMenu->FindOrAddSection("StartGamesSection");
            startGamesSection.AddSubMenu(
                "StartGamesMenu",
                INVTEXT("Start Games"),
                INVTEXT("Start Games specific tools and editors"),
                FNewToolMenuDelegate::CreateLambda([](UToolMenu* Menu)
                {
                    FToolMenuSection& section = Menu->AddSection("DataManagement", INVTEXT("Data Management"));
                    section.AddMenuEntry(
                        "OpenDynamicTextAssetBrowser",
                        INVTEXT("Dynamic Text Asset Browser"),
                        INVTEXT("Opens the Dynamic Text Asset Browser window"),
                        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"),
                        FUIAction(FExecuteAction::CreateLambda([]()
                        {
                            FGlobalTabmanager::Get()->TryInvokeTab(SSGDynamicTextAssetBrowser::GetTabId());
                        }))
                    );
                }),
                false, // bInOpenSubMenuOnClick
                FSlateIcon(),
                false // bInShouldCloseWindowAfterMenuSelection
            ).InsertPosition = insertPos; // Tell the system to place this SubMenu before "Help"
        }

        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Registered menu extensions"));
    }

    /** Registers detail customizations for dynamic text asset classes */
    void RegisterDetailCustomizations()
    {
        FPropertyEditorModule& propertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

        // Class layout customization for USGDynamicTextAsset (identity section)
        propertyModule.RegisterCustomClassLayout(
            USGDynamicTextAsset::StaticClass()->GetFName(),
            FOnGetDetailCustomizationInstance::CreateStatic(&FSGDynamicTextAssetIdentityCustomization::MakeInstance));

        // Property type customization for FSGDynamicTextAssetId (read-only ID display)
        propertyModule.RegisterCustomPropertyTypeLayout(
            TEXT("SGDynamicTextAssetId"),
            FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSGDynamicTextAssetIdCustomization::MakeInstance));

        // Property type customization for FSGDynamicTextAssetRef (picker widget)
        propertyModule.RegisterCustomPropertyTypeLayout(
            TEXT("SGDynamicTextAssetRef"),
            FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSGDynamicTextAssetRefCustomization::MakeInstance));

        // Property type customization for FSGDynamicTextAssetTypeId (searchable type picker)
        propertyModule.RegisterCustomPropertyTypeLayout(
            TEXT("SGDynamicTextAssetTypeId"),
            FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSGAssetTypeIdCustomization::MakeInstance));

        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Registered detail customizations"));
    }

    /** Unregisters detail customizations */
    void UnregisterDetailCustomizations()
    {
        if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
        {
            FPropertyEditorModule& propertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
            propertyModule.UnregisterCustomClassLayout(USGDynamicTextAsset::StaticClass()->GetFName());
            propertyModule.UnregisterCustomPropertyTypeLayout(TEXT("SGDynamicTextAssetId"));
            propertyModule.UnregisterCustomPropertyTypeLayout(TEXT("SGDynamicTextAssetRef"));
            propertyModule.UnregisterCustomPropertyTypeLayout(TEXT("SGDynamicTextAssetTypeId"));
        }
    }

    /**
     * Callback invoked when the engine's cook process starts.
     * Automatically cooks all dynamic text assets to flat ID-named binary files with manifest.
     * Errors are logged as warnings but do not fail the cook.
     */
    void OnCookStarted(UE::Cook::ICookInfo& CookInfo)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== Automatic Dynamic Text Asset Cook Started ==="));

        FString outputDirectory = FSGDynamicTextAssetCookUtils::GetCookedOutputRootPath();
        TArray<FString> errors;

        bool bSuccess = FSGDynamicTextAssetCookUtils::CookAllDynamicTextAssets(outputDirectory, FString(), errors);

        if (bSuccess)
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== Automatic Dynamic Text Asset Cook Finished Successfully ==="));
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Automatic dynamic text asset cook completed with errors:"));
            for (const FString& error : errors)
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("  - %s"), *error);
            }
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("=== Automatic Dynamic Text Asset Cook Finished With Errors ==="));
        }
    }

    /**
     * Callback invoked during CollectFilesToCook to add soft-referenced packages from DTA files.
     * Scans all DTA files, deserializes them, walks properties to find TSoftObjectPtr/TSoftClassPtr
     * references, and adds those packages to the cook via FPackageCookRule.
     */
    void OnModifyCook(UE::Cook::ICookInfo& CookInfo, TArray<UE::Cook::FPackageCookRule>& InOutPackageCookRules)
    {
        TArray<FName> packageNames;
        int32 count = FSGDynamicTextAssetCookUtils::GatherSoftReferencesFromAllFiles(packageNames);

        for (const FName& packageName : packageNames)
        {
            UE::Cook::FPackageCookRule rule;
            rule.PackageName = packageName;
            rule.CookRule = UE::Cook::EPackageCookRule::AddToCook;
            rule.InstigatorName = FName(TEXT("SGDynamicTextAssets"));
            InOutPackageCookRules.Add(rule);
        }

        UE_LOG(LogSGDynamicTextAssetsEditor, Log,
            TEXT("Added %d soft-referenced packages to cook from DTA files"), count);
    }

    /**
     * Checks if cooked binary files are tracked by source control and warns the user.
     * These are generated artifacts and should be excluded via .gitignore.
     */
    static void WarnIfCookedFilesInSourceControl()
    {
        FString cookedRoot = FSGDynamicTextAssetFileManager::GetCookedDynamicTextAssetsRootPath();

        // Check if the cooked directory exists
        if (!IFileManager::Get().DirectoryExists(*cookedRoot))
        {
            return;
        }

        // Check if source control is enabled
        if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
        {
            return;
        }

        // Scan for .dta.bin files
        TArray<FString> cookedFiles;
        IFileManager::Get().FindFiles(cookedFiles, *FPaths::Combine(cookedRoot, TEXT("*") + FString(FSGDynamicTextAssetFileManager::BINARY_EXTENSION)), true, false);

        if (cookedFiles.IsEmpty())
        {
            return;
        }

        // Check if any cooked file is tracked by source control
        bool bFoundTrackedFile = false;
        for (const FString& filename : cookedFiles)
        {
            FString fullPath = FPaths::Combine(cookedRoot, filename);
            ESGDynamicTextAssetSourceControlStatus status = FSGDynamicTextAssetSourceControl::GetFileStatus(fullPath);

            if (status == ESGDynamicTextAssetSourceControlStatus::Added
                || status == ESGDynamicTextAssetSourceControlStatus::CheckedOut
                || status == ESGDynamicTextAssetSourceControlStatus::NotCheckedOut
                || status == ESGDynamicTextAssetSourceControlStatus::ModifiedLocally)
            {
                bFoundTrackedFile = true;
                break;
            }
        }

        if (bFoundTrackedFile)
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                TEXT("Cooked dynamic text asset binaries in '%s' are tracked by source control. "
                     "These are generated artifacts and should be excluded. "
                     "Add 'Content/SGDynamicTextAssetsCooked/' to your source control's ignore file."),
                *cookedRoot);

            FNotificationInfo notificationInfo(INVTEXT("SGDynamicTextAssets: Cooked binary files are tracked by source control.\n"
                "Add 'Content/SGDynamicTextAssetsCooked/' to your source control's ignore file."));
            notificationInfo.bFireAndForget = true;
            notificationInfo.ExpireDuration = 10.0f;
            notificationInfo.bUseThrobber = false;
            notificationInfo.Image = FAppStyle::GetBrush(TEXT("Icons.WarningWithColor"));
            FSlateNotificationManager::Get().AddNotification(notificationInfo);
        }
    }

    /**
     * Registers the cooked dynamic text assets directory for staging into packaged builds.
     * Ensures SGDynamicTextAssetsCooked is added to DirectoriesToAlwaysStageAsUFS
     * so that .dta.bin files and dta_manifest.bin are included in the pak file.
     */
    void RegisterCookedDirectoryForStaging()
    {
        static const FString COOKED_DIRECTORY_NAME = TEXT("SGDynamicTextAssetsCooked");

        UProjectPackagingSettings* packagingSettings = GetMutableDefault<UProjectPackagingSettings>();
        if (!packagingSettings)
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to get UProjectPackagingSettings - cooked dynamic text assets may not be staged"));
            return;
        }

        // Check if already registered
        bool bAlreadyRegistered = false;
        for (const FDirectoryPath& directory : packagingSettings->DirectoriesToAlwaysStageAsUFS)
        {
            if (directory.Path.Equals(COOKED_DIRECTORY_NAME, ESearchCase::IgnoreCase))
            {
                bAlreadyRegistered = true;
                break;
            }
        }

        if (!bAlreadyRegistered)
        {
            FDirectoryPath newDirectory;
            newDirectory.Path = COOKED_DIRECTORY_NAME;
            packagingSettings->DirectoriesToAlwaysStageAsUFS.Add(newDirectory);

            // Persist to DefaultGame.ini
            packagingSettings->TryUpdateDefaultConfigFile();

            UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Registered '%s' for staging into packaged builds"), *COOKED_DIRECTORY_NAME);
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("'%s' already registered for staging"), *COOKED_DIRECTORY_NAME);
        }
    }

    /** Handle for the cook started delegate subscription */
    FDelegateHandle CookStartedHandle;

    /** Handle for the ModifyCook delegate subscription (soft reference cook inclusion) */
    FDelegateHandle ModifyCookHandle;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSGDynamicTextAssetsEditorModule, SGDynamicTextAssetsEditor)
