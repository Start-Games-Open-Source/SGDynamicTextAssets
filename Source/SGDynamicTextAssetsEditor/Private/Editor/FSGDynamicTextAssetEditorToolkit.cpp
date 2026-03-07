// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/FSGDynamicTextAssetEditorToolkit.h"

#include "Browser/SSGDynamicTextAssetBrowser.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetValidationResult.h"
#include "Editor.h"
#include "Editor/SGDynamicTextAssetEditorCommands.h"
#include "Editor/SSGDynamicTextAssetRawView.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Misc/MessageDialog.h"
#include "PropertyEditorModule.h"
#include "ReferenceViewer/SSGDynamicTextAssetReferenceViewer.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "SourceCodeNavigation.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "Utilities/SGDynamicTextAssetSourceControl.h"
#include "Utilities/SGDynamicTextAssetEditorStatics.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGDynamicTextAssetEditorToolkit"

// Static deduplication map definition
TMap<FString, TWeakPtr<FSGDynamicTextAssetEditorToolkit>> FSGDynamicTextAssetEditorToolkit::OPEN_EDITORS;

FSGDynamicTextAssetEditorToolkit::FSGDynamicTextAssetEditorToolkit()
{
}

FSGDynamicTextAssetEditorToolkit::~FSGDynamicTextAssetEditorToolkit()
{
    if (GEditor)
    {
        GEditor->UnregisterForUndo(this);
    }

    if (!FilePath.IsEmpty())
    {
        OPEN_EDITORS.Remove(FilePath);
    }

    // Note: FSGDynamicTextAssetEditorCommands are NOT unregistered here.
    // The commands singleton is owned by the editor module (Register in StartupModule,
    // Unregister in ShutdownModule). Per-instance Unregister() would destroy the singleton
    // while other toolkit instances still reference it, since TCommands is not ref-counted.
}

FName FSGDynamicTextAssetEditorToolkit::GetToolkitFName() const
{
    return FName("SGDynamicTextAssetEditor");
}

FText FSGDynamicTextAssetEditorToolkit::GetBaseToolkitName() const
{
    return INVTEXT("Dynamic Text Asset Editor");
}

FText FSGDynamicTextAssetEditorToolkit::GetToolkitName() const
{
    return GetWindowTitle();
}

FString FSGDynamicTextAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
    return TEXT("DynamicTextAsset");
}

FLinearColor FSGDynamicTextAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
    return FLinearColor::White;
}

void FSGDynamicTextAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

    InTabManager->RegisterTabSpawner(
        SGDynamicTextAssetEditorTabs::DetailsTabId,
        FOnSpawnTab::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::SpawnTab_Details))
        .SetDisplayName(INVTEXT("Details"))
        .SetGroup(USGDynamicTextAssetEditorStatics::GetPluginLocalWorkspaceMenuCategory())
    .SetCanSidebarTab(false);

    InTabManager->RegisterTabSpawner(
        SGDynamicTextAssetEditorTabs::RawViewTabId,
        FOnSpawnTab::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::SpawnTab_RawView))
        .SetDisplayName(INVTEXT("Raw View"))
        .SetGroup(USGDynamicTextAssetEditorStatics::GetPluginLocalWorkspaceMenuCategory())
    .SetCanSidebarTab(true);

    // Start on the details tab
    InTabManager->SetMainTab(SGDynamicTextAssetEditorTabs::DetailsTabId);
}

void FSGDynamicTextAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    InTabManager->UnregisterTabSpawner(SGDynamicTextAssetEditorTabs::DetailsTabId);
    InTabManager->UnregisterTabSpawner(SGDynamicTextAssetEditorTabs::RawViewTabId);
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FSGDynamicTextAssetEditorToolkit::PostRegenerateMenusAndToolbars()
{
    FAssetEditorToolkit::PostRegenerateMenusAndToolbars();

    // Draw the file type with question mark
    TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
    if (!serializer.IsValid())
    {

    }
    const FText description = serializer->GetFormatDescription();
    AddToolbarWidget(SNew(SHorizontalBox)

        // "File Type:" text
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0)
        [
            SNew(STextBlock)
            .Text(INVTEXT("File Type: "))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ]

        // File type format text
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0)
        [
          SNew(STextBlock)
          .Text(serializer->GetFormatName())
          .ColorAndOpacity(FSlateColor::UseForeground())
        ]

        // Help icon to signal hover will tell you what it is
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(5.0f, 0.0f, 15.0f, 0.0f)
        [
            SNew(SImage)
            .Image(FAppStyle::GetBrush("Icons.Help"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .ToolTipText(description)
        ]
    );

    // GenerateToolbar() already ran before PostRegenerateMenusAndToolbars() was called,
    // so we must regenerate now that the widget has been added to ToolbarWidgets.
    GenerateToolbar();
}

void FSGDynamicTextAssetEditorToolkit::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent,
    FProperty* InPropertyThatChanged)
{
    FNotifyHook::NotifyPostChange(InPropertyChangedEvent, InPropertyThatChanged);
    MarkDirty();
}

void FSGDynamicTextAssetEditorToolkit::PostUndo(bool bSuccess)
{
    if (bSuccess)
    {
        MarkDirty();
    }
}

void FSGDynamicTextAssetEditorToolkit::PostRedo(bool bSuccess)
{
    if (bSuccess)
    {
        MarkDirty();
    }
}

void FSGDynamicTextAssetEditorToolkit::InitEditor(EToolkitMode::Type Mode,
    const TSharedPtr<IToolkitHost>& InitToolkitHost,
    USGDynamicTextAssetEditorProxy* Proxy)
{
    check(Proxy);
    FilePath = Proxy->FilePath;
    AssetTypeId = Proxy->AssetTypeId;

    // Create the details view
    FPropertyEditorModule& propertyModule =
        FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

    FDetailsViewArgs detailsViewArgs;
    detailsViewArgs.bAllowSearch       = true;
    detailsViewArgs.bShowOptions       = true;
    detailsViewArgs.bHideSelectionTip  = true;
    detailsViewArgs.bUpdatesFromSelection = false;
    detailsViewArgs.NotifyHook         = this;

    DetailsView = propertyModule.CreateDetailView(detailsViewArgs);

    // Wire property-change callback
    DetailsView->OnFinishedChangingProperties().AddSP(
        this, &FSGDynamicTextAssetEditorToolkit::OnPropertyChanged);

    // Load data from disk, need to do this first to avoid dependent UI not having any data loaded
    LoadFromFile();

    // Build the default layout: Details area and RawView Stacked
    // Don't forget to increment the version number if its layout is changed due to editor caching!
    const TSharedRef<FTabManager::FLayout> defaultLayout =
        FTabManager::NewLayout("SGDynamicTextAssetEditor_v2")
        ->AddArea
        (
            FTabManager::NewPrimaryArea()
            ->SetOrientation(Orient_Vertical)
            ->Split
            (
                FTabManager::NewStack()
                ->SetSizeCoefficient(1.0f)
                ->AddTab(SGDynamicTextAssetEditorTabs::DetailsTabId, ETabState::OpenedTab)
                ->AddTab(SGDynamicTextAssetEditorTabs::RawViewTabId, ETabState::OpenedTab)
                ->SetForegroundTab(SGDynamicTextAssetEditorTabs::DetailsTabId)
            )
        );

    // Initialise via toolkit framework (creates window, toolbar, tab manager)
    FAssetEditorToolkit::InitAssetEditor(
        Mode,
        InitToolkitHost,
        FName("SGDynamicTextAssetEditorApp"),
        defaultLayout,
        /*bCreateDefaultStandaloneMenu*/ true,
        /*bCreateDefaultToolbar*/        true,
        Proxy
    );

    // Focus SGDynamicTextAssetEditorTabs::DetailsTabId but keep the tab ordering of [Details] | [Raw View]
    TabManager->TryInvokeTab(SGDynamicTextAssetEditorTabs::DetailsTabId);

    // Register for undo after toolkit is initialized
    if (GEditor)
    {
        GEditor->RegisterForUndo(this);
    }

    // Extend the standard toolbar with our custom buttons
    ExtendToolbar();

    // Add the parent class hyperlink
    ConstructParentClassHyperlink();

    RegenerateMenusAndToolbars();

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Opened dynamic text asset editor for: %s"), *FilePath);
}

void FSGDynamicTextAssetEditorToolkit::ExtendToolbar()
{
    // Safety net: no-op if already registered by the module's StartupModule().
    FSGDynamicTextAssetEditorCommands::Register();

    const TSharedRef<FUICommandList> commands = GetToolkitCommands();

    commands->MapAction(
        FSGDynamicTextAssetEditorCommands::Get().Revert,
        FExecuteAction::CreateLambda([this]() { LoadFromFile(); }),
        FCanExecuteAction::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::HasUnsavedChanges));

    commands->MapAction(
        FSGDynamicTextAssetEditorCommands::Get().ShowInExplorer,
        FExecuteAction::CreateLambda([this]()
        {
            if (!FilePath.IsEmpty())
            {
                FPlatformProcess::ExploreFolder(*FilePath);
            }
        }));


    commands->MapAction(
        FSGDynamicTextAssetEditorCommands::Get().OpenReferenceViewer,
        FExecuteAction::CreateLambda([this]()
        {
            if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(EditedDynamicTextAsset))
            {
                SSGDynamicTextAssetReferenceViewer::OpenViewer(
                    provider->GetDynamicTextAssetId(),
                    provider->GetUserFacingId());
            }
        }));

    TSharedPtr<FExtender> toolbarExtender = MakeShared<FExtender>();
    toolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        commands,
        FToolBarExtensionDelegate::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::FillToolbar));

    AddToolbarExtender(toolbarExtender);
}

void FSGDynamicTextAssetEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
    ToolbarBuilder.BeginSection("DynamicTextAsset");
    {
        ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().Revert,
            NAME_None, TAttribute<FText>(), TAttribute<FText>(),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Undo"));

        ToolbarBuilder.AddSeparator();

        ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().ShowInExplorer,
            NAME_None, TAttribute<FText>(), TAttribute<FText>(),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"));

        ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().OpenReferenceViewer,
            NAME_None, TAttribute<FText>(), TAttribute<FText>(),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"));
    }
    ToolbarBuilder.EndSection();
}

void FSGDynamicTextAssetEditorToolkit::ConstructParentClassHyperlink()
{
    SetMenuOverlay(SNew(SBox)
        .VAlign(VAlign_Center)
        .Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
        [
            SNew(SHorizontalBox)

            // Class Label
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(INVTEXT("Class:"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]

            // Hyperlink to C++ class
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SHyperlink)
                .Text(GetClassDisplayText())
                .ToolTipText(INVTEXT("Open Dynamic Text Asset class in C++ source"))
                .OnNavigate(this, &FSGDynamicTextAssetEditorToolkit::OnClassLinkClicked)
            ]
        ]);
}

bool FSGDynamicTextAssetEditorToolkit::LoadFromFile()
{
    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Cannot load: empty file path"));
        return false;
    }

    FString fileContents;
    if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, fileContents))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to read file: %s"), *FilePath);
        return false;
    }

    // Extract metadata
    FSGDynamicTextAssetFileMetadata fileMetadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(FilePath);
    if (!fileMetadata.bIsValid)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to extract metadata for: %s"), *FilePath);
        return false;
    }

    // Determine class
    UClass* classToUse = FindFirstObject<UClass>(*fileMetadata.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
    if (classToUse && !classToUse->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
            TEXT("Resolved class '%s' does not implement ISGDynamicTextAssetProvider, ignoring"), *fileMetadata.ClassName);
        classToUse = nullptr;
    }

    if (!classToUse)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to determine class for: %s"), *FilePath);
        return false;
    }

    // Create or reuse the dynamic text asset instance
    UObject* dataObject = EditedDynamicTextAsset;
    if (!dataObject || dataObject->GetClass() != classToUse)
    {
        // Need RF_Transactional so edit changes are captured by the undo buffer
        dataObject = NewObject<UObject>(GetTransientPackage(), classToUse, NAME_None, RF_Transactional);
        EditedDynamicTextAssetStrong = TStrongObjectPtr<UObject>(dataObject);
        EditedDynamicTextAsset = dataObject;
    }

    // Deserialize via the provider interface
    {
        TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
        if (!serializer.IsValid())
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("No serializer found for: %s"), *FilePath);
            return false;
        }

        bool bMigrated = false;
        if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(dataObject))
        {
            if (!serializer->DeserializeProvider(fileContents, provider, bMigrated))
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to deserialize: %s"), *FilePath);
                return false;
            }
            if (bMigrated)
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Migrated dynamic text asset to current version: %s"), *FilePath);
            }
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Loaded object does not implement ISGDynamicTextAssetProvider: %s"), *FilePath);
            return false;
        }
    }

    // Populate UserFacingId from file metadata if not already set
    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(dataObject))
    {
        if (provider->GetUserFacingId().IsEmpty() && !fileMetadata.UserFacingId.IsEmpty())
        {
            provider->SetUserFacingId(fileMetadata.UserFacingId);
        }

        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Loaded dynamic text asset from: %s (UserFacingId: %s)"),
            *FilePath, *provider->GetUserFacingId());
    }

    if (DetailsView.IsValid())
    {
        DetailsView->SetObject(dataObject);
    }

    MarkClean();
    return true;
}

bool FSGDynamicTextAssetEditorToolkit::SaveToFile()
{
    if (!EditedDynamicTextAsset)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Cannot save: no dynamic text asset loaded"));
        return false;
    }

    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Cannot save: empty file path"));
        return false;
    }

    // Validate via provider interface
    ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(EditedDynamicTextAsset);
    if (!provider)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Cannot save: edited object does not implement ISGDynamicTextAssetProvider"));
        return false;
    }

    FSGDynamicTextAssetValidationResult validationResult;
    if (!provider->Native_ValidateDynamicTextAsset(validationResult))
    {
        FString errorMessage = TEXT("Validation failed with the following issues:\n\n");
        errorMessage += validationResult.ToFormattedString();
        errorMessage += TEXT("\nPlease fix the error(s) before saving.");

        FMessageDialog::Open(EAppMsgType::Ok,
            FText::FromString(errorMessage),
            FText::FromString(TEXT("Validation Failed")));

        UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
            TEXT("Cannot save '%s' (%s): validation failed with %d issue(s)"),
            *provider->GetUserFacingId(),
            *provider->GetDynamicTextAssetId().ToString(),
            validationResult.GetTotalCount());
        return false;
    }

    // Serialize via the provider interface
    FString fileOutput;
    {
        TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
        if (!serializer.IsValid())
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("No serializer found for: %s"), *FilePath);
            return false;
        }

        if (!serializer->SerializeProvider(provider, fileOutput))
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to serialize dynamic text asset"));
            return false;
        }
    }

    // Source control auto-checkout
    if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        FString otherUser;
        if (FSGDynamicTextAssetSourceControl::IsCheckedOutByOther(FilePath, otherUser))
        {
            FString errorMessage = FString::Printf(TEXT("Cannot save: file is checked out by %s"), *otherUser);
            FMessageDialog::Open(EAppMsgType::Ok,
                FText::FromString(errorMessage),
                FText::FromString(TEXT("Source Control")));
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("%s"), *errorMessage);
            return false;
        }

        if (!FSGDynamicTextAssetSourceControl::CheckOutFile(FilePath))
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                TEXT("Failed to check out file from source control: %s"), *FilePath);
            // Continue anyway — user may want to save locally
        }
    }

    // Write file
    if (!FSGDynamicTextAssetFileManager::WriteRawFileContents(FilePath, fileOutput))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to write file: %s"), *FilePath);
        return false;
    }

    MarkClean();
    RefreshRawView();

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Saved dynamic text asset to: %s"), *FilePath);
    return true;
}

bool FSGDynamicTextAssetEditorToolkit::CanSaveAsset() const
{
    return HasUnsavedChanges();
}

void FSGDynamicTextAssetEditorToolkit::SaveAsset_Execute()
{
    SaveToFile();
}

void FSGDynamicTextAssetEditorToolkit::FindInContentBrowser_Execute()
{
    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(EditedDynamicTextAsset))
    {
        SSGDynamicTextAssetBrowser::OpenAndSelect(provider->GetDynamicTextAssetId());
    }
}

void FSGDynamicTextAssetEditorToolkit::MarkDirty()
{
    if (!bHasUnsavedChanges)
    {
        bHasUnsavedChanges = true;
        RegenerateMenusAndToolbars();
    }
}

void FSGDynamicTextAssetEditorToolkit::MarkClean()
{
    bHasUnsavedChanges = false;
    RegenerateMenusAndToolbars();
}

void FSGDynamicTextAssetEditorToolkit::OnPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
    MarkDirty();
}

TSharedRef<SDockTab> FSGDynamicTextAssetEditorToolkit::SpawnTab_Details(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .Label(INVTEXT("Details"))
        [
            DetailsView.IsValid() ? DetailsView.ToSharedRef() : SNullWidget::NullWidget
        ];
}

TSharedRef<SDockTab> FSGDynamicTextAssetEditorToolkit::SpawnTab_RawView(const FSpawnTabArgs& Args)
{
    FText dataObjectId = INVTEXT("");
    FText userFacingId = INVTEXT("");
    FText version = INVTEXT("");
    FText rawText = INVTEXT("");

    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(EditedDynamicTextAsset))
    {
        dataObjectId = FText::FromString(provider->GetDynamicTextAssetId().ToString());
        userFacingId = FText::FromString(provider->GetUserFacingId());
        version = FText::FromString(provider->GetVersion().ToString());
    }

    return SNew(SDockTab)
        .Label(INVTEXT("Raw View"))
        [
            SAssignNew(RawView, SSGDynamicTextAssetRawView)
            .DynamicTextAssetId(dataObjectId)
            .UserFacingId(userFacingId)
            .Version(version)
            .JsonText(GetRawText())
            .OnRefreshRequested(FSimpleDelegate::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::RefreshRawView))
        ];
}

void FSGDynamicTextAssetEditorToolkit::OpenEditor(const FString& InFilePath, UClass* InDynamicTextAssetClass,
    const FSGDynamicTextAssetTypeId& InAssetTypeId)
{
    if (InFilePath.IsEmpty() || !InDynamicTextAssetClass)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("OpenEditor: invalid arguments (empty path or null class)"));
        return;
    }

    // Focus existing editor if already open
    if (TWeakPtr<FSGDynamicTextAssetEditorToolkit>* existing = OPEN_EDITORS.Find(InFilePath))
    {
        if (TSharedPtr<FSGDynamicTextAssetEditorToolkit> pinned = existing->Pin())
        {
            pinned->BringToolkitToFront();
            UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Focused existing editor for: %s"), *InFilePath);
            return;
        }
    }

    // Create proxy and toolkit
    USGDynamicTextAssetEditorProxy* proxy = USGDynamicTextAssetEditorProxy::Create(InFilePath, InDynamicTextAssetClass, InAssetTypeId);

    TSharedRef<FSGDynamicTextAssetEditorToolkit> toolkit = MakeShared<FSGDynamicTextAssetEditorToolkit>();
    toolkit->InitEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), proxy);

    OPEN_EDITORS.Add(InFilePath, toolkit);
}

void FSGDynamicTextAssetEditorToolkit::OpenEditorForFile(const FString& InFilePath)
{
    if (InFilePath.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("OpenEditorForFile: empty file path"));
        return;
    }

    FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(InFilePath);
    if (!metadata.bIsValid)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("OpenEditorForFile: failed to extract metadata %s"), *InFilePath);
        return;
    }

    // Resolve class via Asset Type ID (O(1) map lookup), with fallback to class name for legacy files
    UClass* dataObjectClass = nullptr;
    FSGDynamicTextAssetTypeId resolvedTypeId = metadata.AssetTypeId;
    if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
    {
        if (metadata.AssetTypeId.IsValid())
        {
            dataObjectClass = registry->ResolveClassForTypeId(metadata.AssetTypeId);
        }

        // Fallback: resolve by class name for legacy files without a valid Asset Type ID
        if (!dataObjectClass && !metadata.ClassName.IsEmpty())
        {
            TArray<UClass*> allClasses;
            registry->GetAllInstantiableClasses(allClasses);
            for (UClass* registeredClass : allClasses)
            {
                if (registeredClass && registeredClass->GetName() == metadata.ClassName)
                {
                    dataObjectClass = registeredClass;
                    resolvedTypeId = registry->GetTypeIdForClass(registeredClass);
                    break;
                }
            }
        }
    }

    if (!dataObjectClass)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
            TEXT("OpenEditorForFile: failed to resolve class for AssetTypeId '%s' (ClassName '%s') in %s"),
            *metadata.AssetTypeId.ToString(), *metadata.ClassName, *InFilePath);
        return;
    }

    OpenEditor(InFilePath, dataObjectClass, resolvedTypeId);
}

void FSGDynamicTextAssetEditorToolkit::NotifyFileRenamed(const FString& OldFilePath, const FString& NewFilePath)
{
    TWeakPtr<FSGDynamicTextAssetEditorToolkit>* existing = OPEN_EDITORS.Find(OldFilePath);
    if (!existing || !existing->IsValid())
    {
        return;
    }

    TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = existing->Pin();
    if (!toolkit.IsValid())
    {
        return;
    }

    // Remap in the deduplication map
    OPEN_EDITORS.Remove(OldFilePath);
    toolkit->FilePath = NewFilePath;
    OPEN_EDITORS.Add(NewFilePath, toolkit);

    // Reload UserFacingId from the renamed file's metadata
    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(toolkit->EditedDynamicTextAsset))
    {
        FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(NewFilePath);
        if (metadata.bIsValid && !metadata.UserFacingId.IsEmpty())
        {
            provider->SetUserFacingId(metadata.UserFacingId);
        }
    }

    toolkit->MarkClean();
    toolkit->RefreshRawView();

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("NotifyFileRenamed: %s -> %s"), *OldFilePath, *NewFilePath);
}

void FSGDynamicTextAssetEditorToolkit::NotifyFileDeleted(const FString& InFilePath)
{
    TWeakPtr<FSGDynamicTextAssetEditorToolkit>* existing = OPEN_EDITORS.Find(InFilePath);
    if (!existing || !existing->IsValid())
    {
        return;
    }

    TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = existing->Pin();
    if (!toolkit.IsValid())
    {
        return;
    }

    // Remove before CloseWindow so the destructor's OPEN_EDITORS.Remove is a harmless no-op
    OPEN_EDITORS.Remove(InFilePath);
    toolkit->CloseWindow(EAssetEditorCloseReason::AssetUnloadingOrInvalid);

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("NotifyFileDeleted: closed editor for %s"), *InFilePath);
}

bool FSGDynamicTextAssetEditorToolkit::HasOpenEditorWithUnsavedChanges(const FString& InFilePath)
{
    TWeakPtr<FSGDynamicTextAssetEditorToolkit>* existing = OPEN_EDITORS.Find(InFilePath);
    if (!existing || !existing->IsValid())
    {
        return false;
    }

    TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = existing->Pin();
    if (!toolkit.IsValid())
    {
        return false;
    }

    return toolkit->HasUnsavedChanges();
}

bool FSGDynamicTextAssetEditorToolkit::SaveOpenEditor(const FString& InFilePath)
{
    TWeakPtr<FSGDynamicTextAssetEditorToolkit>* existing = OPEN_EDITORS.Find(InFilePath);
    if (!existing || !existing->IsValid())
    {
        // No editor open — nothing to save
        return true;
    }

    TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = existing->Pin();
    if (!toolkit.IsValid())
    {
        return true;
    }

    if (!toolkit->HasUnsavedChanges())
    {
        return true;
    }

    bool bSaved = toolkit->SaveToFile();
    if (bSaved)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("SaveOpenEditor: saved editor for %s"), *InFilePath);
    }
    else
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("SaveOpenEditor: failed to save editor for %s"), *InFilePath);
    }

    return bSaved;
}

FText FSGDynamicTextAssetEditorToolkit::GetWindowTitle() const
{
    FString displayName = TEXT("Dynamic Text Asset");

    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(EditedDynamicTextAsset))
    {
        displayName = provider->GetUserFacingId();
    }

    return FText::Format(INVTEXT("{0}{1}"),
        FText::FromString(displayName),
        bHasUnsavedChanges ? INVTEXT(" *") : INVTEXT(""));
}

FText FSGDynamicTextAssetEditorToolkit::GetClassDisplayText() const
{
    if (EditedDynamicTextAsset)
    {
        return FText::FromString(EditedDynamicTextAsset->GetClass()->GetName());
    }
    return INVTEXT("Unknown");
}

FText FSGDynamicTextAssetEditorToolkit::GetRawText() const
{
    FText rawText = INVTEXT("");

    if (!FilePath.IsEmpty())
    {
        FString rawOutput;
        if (FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, rawOutput))
        {
            rawText = FText::FromString(rawOutput);
        }
    }

    return rawText;
}

void FSGDynamicTextAssetEditorToolkit::OnClassLinkClicked()
{
    if (!EditedDynamicTextAsset)
    {
        return;
    }

    if (UClass* dataClass = EditedDynamicTextAsset->GetClass())
    {
        FString classHeaderPath;
        if (FSourceCodeNavigation::FindClassHeaderPath(dataClass, classHeaderPath))
        {
            FSourceCodeNavigation::OpenSourceFile(classHeaderPath);
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                TEXT("Could not find source file for class: %s"), *dataClass->GetName());
        }
    }
}

FReply FSGDynamicTextAssetEditorToolkit::OnCopyToClipboard(const FString& TextToCopy) const
{
    FPlatformApplicationMisc::ClipboardCopy(*TextToCopy);
    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Copied to clipboard: %s"), *TextToCopy);
    return FReply::Handled();
}

void FSGDynamicTextAssetEditorToolkit::RefreshRawView()
{
    if (!RawView.IsValid())
    {
        return;
    }

    // Re-read the file from disk and update the text content
    RawView->SetRawText(GetRawText());

    // Update identity properties from the in-memory provider
    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(EditedDynamicTextAsset))
    {
        RawView->SetIdentityProperties(
            FText::FromString(provider->GetDynamicTextAssetId().ToString()),
            FText::FromString(provider->GetUserFacingId()),
            FText::FromString(provider->GetVersion().ToString()));
    }
}

#undef LOCTEXT_NAMESPACE
