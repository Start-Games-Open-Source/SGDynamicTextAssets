// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDTAEditorToolkit.h"

#include "Browser/SSGDTABrowser.h"
#include "SGDTAScanSubsystem.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDTAValidationResult.h"
#include "Editor.h"
#include "Editor/SGDTABundleRowExtension.h"
#include "Editor/SGDTAEditorCommands.h"
#include "Editor/SSGDTASaveDialog.h"
#include "Interfaces/IMainFrameModule.h"
#include "Editor/SSGDTARawView.h"
#include "Editor/SSGDynamicTextAssetIcon.h"
#include "Browser/SSGDTASCCBadge.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Management/SGDTAFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ReferenceViewer/SSGDTAReferenceViewer.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Serialization/SGDTAJsonSerializer.h"
#include "SGDTAEditorLogs.h"
#include "SourceCodeNavigation.h"
#include "Statics/SGDynamicTextAssetStatics.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "Utilities/SGDTASCCStatusCache.h"
#include "Utilities/SGDTASourceControl.h"
#include "Utilities/SGDTAEditorStatics.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGDynamicTextAssetEditorToolkit"

// Static deduplication map definition
TMap<FString, TWeakPtr<FSGDynamicTextAssetEditorToolkit>> FSGDynamicTextAssetEditorToolkit::OPEN_EDITORS;
TMap<FString, FSGDynamicTextAssetEditorToolkit::FDirtyObjectCacheEntry> FSGDynamicTextAssetEditorToolkit::DIRTY_OBJECT_CACHE;

// Coarse dirty-state refresh signal (see header). Program-lifetime static.
FOnDirtyStateChanged FSGDynamicTextAssetEditorToolkit::OnDirtyStateChanged;

FSGDynamicTextAssetEditorToolkit::FSGDynamicTextAssetEditorToolkit()
{
}

FSGDynamicTextAssetEditorToolkit::~FSGDynamicTextAssetEditorToolkit()
{
    if (GEditor)
    {
        GEditor->UnregisterForUndo(this);
    }

    // Guarded: the module can tear the SCC status cache down before this toolkit is destroyed during shutdown
    if (SCCStatusChangedHandle.IsValid() && FSGDynamicTextAssetSCCStatusCache::IsAvailable())
    {
        FSGDynamicTextAssetSCCStatusCache::Get().OnStatusChanged.Remove(SCCStatusChangedHandle);
        SCCStatusChangedHandle.Reset();
    }

    // Cache the dirty object so it survives GC until the editor is reopened or changes are discarded
    if (!FilePath.IsEmpty() && bHasUnsavedChanges && EditedDynamicTextAssetStrong.IsValid())
    {
        DIRTY_OBJECT_CACHE.Add(FilePath, FDirtyObjectCacheEntry{
            MoveTemp(EditedDynamicTextAssetStrong),
            LoadedFileFormatVersion
        });
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
        return;
    }
    const FText description = serializer->GetFormatDescription();

    // Compute the current source control tooltip once, at build time. PostRegenerateMenusAndToolbars
    // rebuilds this widget on every toolbar regeneration, and the STORY-017 status-cache subscription
    // triggers a regeneration whenever the file's source control status changes, so a build-time
    // tooltip stays current without any per-paint bound attribute. Empty means no hover text (a clean
    // file or source control disabled).
    FText sourceControlTooltip = FText::GetEmpty();
    if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        const FSourceControlStatePtr sourceControlState =
            ISourceControlModule::Get().GetProvider().GetState(FilePath, EStateCacheUsage::Use);
        if (sourceControlState.IsValid())
        {
            sourceControlTooltip = sourceControlState->GetDisplayTooltip();
        }
    }

    AddToolbarWidget(
        SNew(SHorizontalBox)

        // File type and version text.
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)

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
                .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("Icons.Help"))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    .ToolTipText(description)
                ]
            ]

            // Print the version below the type and help icon
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(0.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .Text(FText::Format(INVTEXT("Version: {0}"), serializer->GetFileFormatVersion().ToText()))
                .ToolTipText(INVTEXT("The latest file format version that this file type is at.\nThis may be different from this specific DTA's file format version."))
            ]
        ]

        // Source control status icon, placed after the file type information. Standalone (no file-type
        // icon): the badge renders the provider status icon and self-collapses for a clean or untracked
        // file. The build-time tooltip reads out the current status on hover.
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(5.0f, 0.0f)
        [
            SNew(SSGDynamicTextAssetSCCBadge)
            .FilePath(FilePath)
            .ToolTipText(sourceControlTooltip)
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

    // Set extension handler for asset bundle icons on soft reference properties
    DetailsView->SetExtensionHandler(MakeShared<FSGDynamicTextAssetPropertyExtensionHandler>());

    // Wire property-change callback
    DetailsView->OnFinishedChangingProperties().AddSP(
        this, &FSGDynamicTextAssetEditorToolkit::OnPropertyChanged);

    // Check for a cached dirty object from a previously closed editor
    if (FDirtyObjectCacheEntry* cachedEntry = DIRTY_OBJECT_CACHE.Find(FilePath))
    {
        EditedDynamicTextAssetStrong = MoveTemp(cachedEntry->Object);
        EditedDynamicTextAsset = EditedDynamicTextAssetStrong.Get();
        LoadedFileFormatVersion = cachedEntry->LoadedFileFormatVersion;
        DIRTY_OBJECT_CACHE.Remove(FilePath);

        if (DetailsView.IsValid())
        {
            DetailsView->SetObject(EditedDynamicTextAsset);
        }

        bHasUnsavedChanges = true;
    }
    else
    {
        // Load data from disk, need to do this first to avoid dependent UI not having any data loaded
        LoadFromFile();
    }

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

    commands->MapAction(
        FSGDynamicTextAssetEditorCommands::Get().SourceControlCheckOut,
        FExecuteAction::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::Internal_CheckOutFile),
        FCanExecuteAction::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::CanCheckOutFile));

    commands->MapAction(
        FSGDynamicTextAssetEditorCommands::Get().SourceControlMarkForAdd,
        FExecuteAction::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::Internal_MarkForAddFile),
        FCanExecuteAction::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::CanMarkForAddFile));

    commands->MapAction(
        FSGDynamicTextAssetEditorCommands::Get().SourceControlRevert,
        FExecuteAction::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::Internal_RevertFile),
        FCanExecuteAction::CreateSP(this, &FSGDynamicTextAssetEditorToolkit::CanRevertFile));

    // Subscribe to the shared SCC status cache so Check Out enables/disables live as the cache changes,
    // without the toolbar being reopened. AddSP binds weakly; the handle is removed in the destructor.
    if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
    {
        SCCStatusChangedHandle = FSGDynamicTextAssetSCCStatusCache::Get().OnStatusChanged.AddSP(
            this, &FSGDynamicTextAssetEditorToolkit::HandleSCCStatusChanged);
    }

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
        ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().ShowInExplorer,
            NAME_None, TAttribute<FText>(), TAttribute<FText>(),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"));

        ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().OpenReferenceViewer,
            NAME_None, TAttribute<FText>(), TAttribute<FText>(),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"));
    }
    ToolbarBuilder.EndSection();

    // Source control affordances. The whole section is skipped (not merely disabled) when source
    // control is off, mirroring how the browser omits its source-control submenu entirely.
    if (FSGDynamicTextAssetSourceControl::IsSourceControlEnabled())
    {
        ToolbarBuilder.BeginSection("RevisionControl");
        {
            // Check Out is a checkout / lock-model action. It is omitted (not merely disabled) on
            // modify-in-place providers such as Git, where there is nothing to check out; Perforce
            // and Subversion still show it. Revert stays for every provider.
            if (FSGDynamicTextAssetSourceControl::ProviderUsesCheckout())
            {
                ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().SourceControlCheckOut,
                    NAME_None, TAttribute<FText>(), TAttribute<FText>(),
                    FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.CheckOut"));
            }

            // Mark For Add is emitted unconditionally: staging / adding an untracked file applies to
            // every provider, including modify-in-place providers such as Git where Check Out is omitted.
            // Its enablement (CanMarkForAddFile) gates on the file being NotInSourceControl.
            ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().SourceControlMarkForAdd,
                NAME_None, TAttribute<FText>(), TAttribute<FText>(),
                FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Add"));

            ToolbarBuilder.AddToolBarButton(FSGDynamicTextAssetEditorCommands::Get().SourceControlRevert,
                NAME_None, TAttribute<FText>(), TAttribute<FText>(),
                FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.Revert"));
        }
        ToolbarBuilder.EndSection();
    }
}

void FSGDynamicTextAssetEditorToolkit::Internal_CheckOutFile()
{
    if (FilePath.IsEmpty())
    {
        return;
    }

    // CheckOutFile runs the provider operation synchronously, so we can act on the real result here.
    const bool bSucceeded = FSGDynamicTextAssetSourceControl::CheckOutFile(FilePath);

    // Invalidate the cached status so the next CanExecute evaluation reflects the new state. The cache
    // broadcast from this invalidation also refreshes the toolbar, disabling Check Out once checked out.
    if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
    {
        FSGDynamicTextAssetSCCStatusCache::Get().InvalidateCachePath(FilePath);
    }

    FNotificationInfo notification(bSucceeded
        ? INVTEXT("Checked out file from source control.")
        : INVTEXT("Failed to check out file from source control. Check the log for details."));
    notification.ExpireDuration = 4.0f;
    notification.Image = bSucceeded
        ? FAppStyle::GetBrush("Icons.SuccessWithColor")
        : FAppStyle::GetBrush("Icons.ErrorWithColor");
    FSlateNotificationManager::Get().AddNotification(notification);

    if (bSucceeded)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Checked out file from source control: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to check out file from source control: %s"), *FilePath);
    }
}

bool FSGDynamicTextAssetEditorToolkit::CanCheckOutFile() const
{
    // Read the live cache each evaluation so enablement tracks the file's current status.
    return FSGDynamicTextAssetSourceControl::IsSourceControlEnabled()
        && FSGDynamicTextAssetSCCStatusCache::IsAvailable()
        && FSGDynamicTextAssetSCCStatusCache::Get().GetCachedStatus(FilePath)
            == ESGDynamicTextAssetSourceControlStatus::NotCheckedOut;
}

void FSGDynamicTextAssetEditorToolkit::Internal_MarkForAddFile()
{
    if (FilePath.IsEmpty())
    {
        return;
    }

    // MarkForAdd runs the provider operation synchronously, so we can act on the real result here.
    const bool bSucceeded = FSGDynamicTextAssetSourceControl::MarkForAdd(FilePath);

    // Invalidate the cached status so the next CanExecute evaluation reflects the new state. The cache
    // broadcast from this invalidation also refreshes the toolbar, disabling Mark For Add once added.
    if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
    {
        FSGDynamicTextAssetSCCStatusCache::Get().InvalidateCachePath(FilePath);
    }

    FNotificationInfo notification(bSucceeded
        ? INVTEXT("Marked file for add in source control.")
        : INVTEXT("Failed to mark file for add in source control. Check the log for details."));
    notification.ExpireDuration = 4.0f;
    notification.Image = bSucceeded
        ? FAppStyle::GetBrush("Icons.SuccessWithColor")
        : FAppStyle::GetBrush("Icons.ErrorWithColor");
    FSlateNotificationManager::Get().AddNotification(notification);

    if (bSucceeded)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Marked file for add in source control: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to mark file for add in source control: %s"), *FilePath);
    }
}

bool FSGDynamicTextAssetEditorToolkit::CanMarkForAddFile() const
{
    // Read the live cache each evaluation so enablement tracks the file's current status. Untracked-only:
    // Mark For Add lights up only for a NotInSourceControl file, matching the browser Mark For Add enable
    // condition so the two surfaces stay consistent.
    return FSGDynamicTextAssetSourceControl::IsSourceControlEnabled()
        && FSGDynamicTextAssetSCCStatusCache::IsAvailable()
        && FSGDynamicTextAssetSCCStatusCache::Get().GetCachedStatus(FilePath)
            == ESGDynamicTextAssetSourceControlStatus::NotInSourceControl;
}

void FSGDynamicTextAssetEditorToolkit::Internal_RevertFile()
{
    if (FilePath.IsEmpty())
    {
        return;
    }

    // Confirmation dialog naming the file; bail if the user declines. Copy is kept consistent with the
    // browser Revert confirmation so the two surfaces do not drift.
    const FText confirmMessage = FText::Format(
        INVTEXT("Revert {0}? This discards all local changes and cannot be undone."),
        FText::FromString(FPaths::GetCleanFilename(FilePath)));
    const EAppReturnType::Type choice = FMessageDialog::Open(EAppMsgType::YesNo,
        confirmMessage, INVTEXT("Revert File"));
    if (choice != EAppReturnType::Yes)
    {
        return;
    }

    // RevertFile runs the provider operation synchronously, so we can act on the real result here.
    const bool bSucceeded = FSGDynamicTextAssetSourceControl::RevertFile(FilePath);

    if (bSucceeded)
    {
        // This toolkit is the open editor for the file, so reload it directly rather than routing through
        // the static NotifyFileReverted (which only exists to find an open editor from an out-of-editor
        // caller). ReloadFromDisk re-reads disk, rebinds the Details view, refreshes the raw view, and
        // clears the dirty flag with no save prompt.
        ReloadFromDisk();

        // Invalidate the cached status so the next CanExecute evaluation reflects the reverted state. The
        // cache broadcast from this invalidation also refreshes the toolbar, disabling Revert once reverted.
        if (FSGDynamicTextAssetSCCStatusCache::IsAvailable())
        {
            FSGDynamicTextAssetSCCStatusCache::Get().InvalidateCachePath(FilePath);
        }
    }

    FNotificationInfo notification(bSucceeded
        ? INVTEXT("Reverted file to the depot version.")
        : INVTEXT("Failed to revert file. Check the log for details."));
    notification.ExpireDuration = 4.0f;
    notification.Image = bSucceeded
        ? FAppStyle::GetBrush("Icons.SuccessWithColor")
        : FAppStyle::GetBrush("Icons.ErrorWithColor");
    FSlateNotificationManager::Get().AddNotification(notification);

    if (bSucceeded)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Reverted file to the depot version: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to revert file: %s"), *FilePath);
    }
}

bool FSGDynamicTextAssetEditorToolkit::CanRevertFile() const
{
    // Read the live cache each evaluation so enablement tracks the file's current status. The Revert set is
    // {Added, ModifiedLocally}: the file must have actual local changes to revert. A clean checkout now maps
    // to CheckedOut (see GetFileStatus) and is deliberately excluded so Revert does not light up with nothing
    // to discard; a genuinely modified checked-out file maps to ModifiedLocally and still enables.
    // MarkedForDelete is omitted here because a single open-file editor never reaches that state.
    if (!FSGDynamicTextAssetSourceControl::IsSourceControlEnabled()
        || !FSGDynamicTextAssetSCCStatusCache::IsAvailable())
    {
        return false;
    }

    const ESGDynamicTextAssetSourceControlStatus status =
        FSGDynamicTextAssetSCCStatusCache::Get().GetCachedStatus(FilePath);

    return status == ESGDynamicTextAssetSourceControlStatus::Added
        || status == ESGDynamicTextAssetSourceControlStatus::ModifiedLocally;
}

void FSGDynamicTextAssetEditorToolkit::HandleSCCStatusChanged()
{
    // Regenerate the toolbar so Check Out, Mark For Add, and Revert re-evaluate their CanExecute predicates against the fresh cache.
    RegenerateMenusAndToolbars();
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
    // Clear any cached dirty object for this file (Revert should always read from disk)
    DIRTY_OBJECT_CACHE.Remove(FilePath);

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

    // Extract file information
    FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(FilePath);
    if (!fileInfo.bIsValid)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to extract file info for: %s"), *FilePath);
        return false;
    }

    // Track the file's format version for minor auto-upgrade detection on save
    LoadedFileFormatVersion = fileInfo.FileFormatVersion;

    // Determine class
    UClass* classToUse = FindFirstObject<UClass>(*fileInfo.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
    if (classToUse && !classToUse->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
            TEXT("Resolved class '%s' does not implement ISGDynamicTextAssetProvider, ignoring"), *fileInfo.ClassName);
        classToUse = nullptr;
    }

    if (!classToUse)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to determine class for: %s"), *FilePath);
        return false;
    }

    // Guard against instantiating an abstract class. A file that references an abstract
    // class would crash NewObject below (StaticAllocateObject rejects abstract classes), so
    // fail gracefully with a log entry and a user-facing toast instead. This protects every
    // open path, since both double-click open and OpenEditorForFile funnel through here.
    if (classToUse->HasAnyClassFlags(CLASS_Abstract))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error,
            TEXT("Cannot open file '%s': class '%s' is abstract and cannot be instantiated"),
            *FilePath, *fileInfo.ClassName);

        FNotificationInfo notification(FText::Format(
            INVTEXT("Cannot open this file: it references the abstract class '{0}', which cannot be instantiated."),
            FText::FromString(fileInfo.ClassName)));
        notification.ExpireDuration = 6.0f;
        notification.Image = FAppStyle::GetBrush("Icons.ErrorWithColor");
        FSlateNotificationManager::Get().AddNotification(notification);

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

    // Populate UserFacingId from file info if not already set
    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(dataObject))
    {
        if (provider->GetUserFacingId().IsEmpty() && !fileInfo.UserFacingId.IsEmpty())
        {
            provider->SetUserFacingId(fileInfo.UserFacingId);
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

bool FSGDynamicTextAssetEditorToolkit::SaveToFile(bool bSkipValidation)
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

    if (!bSkipValidation)
    {
        FSGDynamicTextAssetValidationResult validationResult;
        if (!provider->Native_ValidateDynamicTextAsset(validationResult))
        {
            FString errorMessage = TEXT("Validation failed with the following issues:\n\n");
            errorMessage += validationResult.ToFormattedString();
            errorMessage += TEXT("\nPlease fix the error(s) before saving.");

            for (const FSGDynamicTextAssetValidationEntry& error : validationResult.Errors)
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                    TEXT("Validation error for '%s' [%s]: %s"),
                    *provider->GetUserFacingId(),
                    *error.PropertyPath,
                    *error.Message.ToString());
            }

            UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                TEXT("Cannot save '%s' (%s): validation failed with %d issue(s)"),
                *provider->GetUserFacingId(),
                *provider->GetDynamicTextAssetId().ToString(),
                validationResult.GetTotalCount());

            FMessageDialog::Open(EAppMsgType::Ok,
                FText::FromString(errorMessage),
                INVTEXT("Validation Failed"));

            return false;
        }

        // Show warnings to the user and let them decide whether to proceed
        if (validationResult.HasWarnings())
        {
            for (const FSGDynamicTextAssetValidationEntry& warning : validationResult.Warnings)
            {
                UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                    TEXT("Validation warning for '%s' [%s]: %s"),
                    *provider->GetUserFacingId(),
                    *warning.PropertyPath,
                    *warning.Message.ToString());
            }

            FString warningMessage = TEXT("Validation produced the following warnings:\n\n");
            warningMessage += validationResult.ToFormattedString();
            warningMessage += TEXT("\nDo you want to save anyway?");

            EAppReturnType::Type userChoice = FMessageDialog::Open(EAppMsgType::YesNo,
                FText::FromString(warningMessage),
                INVTEXT("Validation Warnings"));

            if (userChoice != EAppReturnType::Yes)
            {
                return false;
            }
        }
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

        // Minor/patch format version auto-upgrade: if the file was loaded with an older
        // minor/patch version (same major), call MigrateFileFormat() for any structural
        // changes and log the upgrade. No user notification is needed for minor bumps.
        const FSGDynamicTextAssetVersion currentFormatVersion = serializer->GetFileFormatVersion();
        if (LoadedFileFormatVersion.IsValid()
            && LoadedFileFormatVersion != currentFormatVersion
            && LoadedFileFormatVersion.Major == currentFormatVersion.Major)
        {
            serializer->MigrateFileFormat(fileOutput, LoadedFileFormatVersion, currentFormatVersion);

            UE_LOG(LogSGDynamicTextAssetsEditor, Log,
                TEXT("Auto-upgraded file format version %s -> %s for: %s"),
                *LoadedFileFormatVersion.ToString(), *currentFormatVersion.ToString(), *FilePath);
        }
    }

    // Source control pre-flight: block the save if the file is checked out by another
    // user (a dialog is more helpful here than a silent log). The actual checkout is
    // now performed inside WriteRawFileContents via the default bAutoCheckOut=true.
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
    }

    // Write file (auto-checkout of an existing file happens here via bAutoCheckOut=true).
    // Pass the edited asset's GUID so the ON_FILE_CHANGED broadcast carries it (Modified for an
    // existing file, Created for a new one); the direct write keeps its default broadcast.
    if (!FSGDynamicTextAssetFileManager::WriteRawFileContents(FilePath, fileOutput, /*bAutoCheckOut*/ true,
        /*bBroadcastChange*/ true, provider->GetDynamicTextAssetId().GetGuid()))
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to write file: %s"), *FilePath);
        return false;
    }

    MarkClean();
    DIRTY_OBJECT_CACHE.Remove(FilePath);
    RefreshRawView();

    // Incrementally update the project info cache with this file's format version
    if (USGDynamicTextAssetScanSubsystem* scanSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetScanSubsystem>())
    {
        TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
        if (serializer.IsValid())
        {
            scanSubsystem->UpdateProjectInfoForFile(serializer->GetSerializerFormat(), serializer->GetFileFormatVersion());

            // Update tracked version so subsequent saves don't re-log the upgrade
            LoadedFileFormatVersion = serializer->GetFileFormatVersion();
        }
    }

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Saved dynamic text asset to: %s"), *FilePath);
    return true;
}

bool FSGDynamicTextAssetEditorToolkit::HandleCanCloseEditor()
{
    // Collect all dirty DTAs from open editors and the dirty object cache
    TSharedRef<SSGDynamicTextAssetSaveDialog> dialog = SNew(SSGDynamicTextAssetSaveDialog);
    bool bHasAnyDirty = false;

    // From open editors
    for (const TPair<FString, TWeakPtr<FSGDynamicTextAssetEditorToolkit>>& pair : OPEN_EDITORS)
    {
        TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = pair.Value.Pin();
        if (toolkit.IsValid() && toolkit->HasUnsavedChanges())
        {
            FString assetName = TEXT("Unknown");
            FString typeName = TEXT("Unknown");
            if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(toolkit->EditedDynamicTextAsset))
            {
                assetName = provider->GetUserFacingId();
            }
            if (toolkit->EditedDynamicTextAsset)
            {
                typeName = toolkit->EditedDynamicTextAsset->GetClass()->GetName();
            }

            FString relativePath;
            FString contentDir = FPaths::ProjectContentDir();
            if (pair.Key.StartsWith(contentDir))
            {
                relativePath = pair.Key.RightChop(contentDir.Len());
            }
            else
            {
                relativePath = FPaths::GetCleanFilename(pair.Key);
            }

            dialog->AddItem(assetName, relativePath, typeName, pair.Key);
            bHasAnyDirty = true;
        }
    }

    // From dirty object cache (closed editors with unsaved changes)
    for (const TPair<FString, FDirtyObjectCacheEntry>& pair : DIRTY_OBJECT_CACHE)
    {
        FString assetName = TEXT("Unknown");
        FString typeName = TEXT("Unknown");
        if (pair.Value.Object.IsValid())
        {
            if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(pair.Value.Object.Get()))
            {
                assetName = provider->GetUserFacingId();
            }
            typeName = pair.Value.Object->GetClass()->GetName();
        }

        FString relativePath;
        FString contentDir = FPaths::ProjectContentDir();
        if (pair.Key.StartsWith(contentDir))
        {
            relativePath = pair.Key.RightChop(contentDir.Len());
        }
        else
        {
            relativePath = FPaths::GetCleanFilename(pair.Key);
        }

        dialog->AddItem(assetName, relativePath, typeName, pair.Key);
        bHasAnyDirty = true;
    }

    // No dirty DTAs, allow close
    if (!bHasAnyDirty)
    {
        return true;
    }

    // Show modal dialog
    TSharedRef<SWindow> modalWindow = SNew(SWindow)
        .Title(INVTEXT("Save Content"))
        .ClientSize(FVector2D(640, 492))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    modalWindow->SetContent(dialog);
    dialog->SetParentWindow(modalWindow);

    GEditor->EditorAddModalWindow(modalWindow);

    switch (dialog->GetResult())
    {
        case ESGDTASaveDialogResult::SaveSelected:
        {
            TArray<TSharedPtr<FSGDTASaveDialogItem>> checkedItems = dialog->GetCheckedItems();

            // Phase 1: Pre-validate all checked items and separate into passed/failed
            TArray<TSharedPtr<FSGDTASaveDialogItem>> validItems;
            int32 validationFailCount = 0;

            for (const TSharedPtr<FSGDTASaveDialogItem>& item : checkedItems)
            {
                if (!item.IsValid())
                {
                    continue;
                }

                // Find the provider for this item (from open editor or dirty cache)
                ISGDynamicTextAssetProvider* provider = nullptr;

                TWeakPtr<FSGDynamicTextAssetEditorToolkit>* editorEntry = OPEN_EDITORS.Find(item->AbsoluteFilePath);
                if (editorEntry)
                {
                    TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = editorEntry->Pin();
                    if (toolkit.IsValid() && toolkit->EditedDynamicTextAsset)
                    {
                        provider = Cast<ISGDynamicTextAssetProvider>(toolkit->EditedDynamicTextAsset);
                    }
                }

                if (!provider)
                {
                    if (FDirtyObjectCacheEntry* cachedEntry = DIRTY_OBJECT_CACHE.Find(item->AbsoluteFilePath))
                    {
                        if (cachedEntry->Object.IsValid())
                        {
                            provider = Cast<ISGDynamicTextAssetProvider>(cachedEntry->Object.Get());
                        }
                    }
                }

                if (!provider)
                {
                    UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                        TEXT("Shutdown save: no provider found for %s, skipping"), *item->AbsoluteFilePath);
                    continue;
                }

                FSGDynamicTextAssetValidationResult validationResult;
                if (provider->Native_ValidateDynamicTextAsset(validationResult))
                {
                    validItems.Add(item);
                }
                else
                {
                    validationFailCount++;
                    UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                        TEXT("Shutdown save: validation failed for '%s' (%s) with %d issue(s)"),
                        *provider->GetUserFacingId(),
                        *item->AbsoluteFilePath,
                        validationResult.GetTotalCount());

                    for (const FSGDynamicTextAssetValidationEntry& error : validationResult.Errors)
                    {
                        UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                            TEXT("  Validation error [%s]: %s"),
                            *error.PropertyPath,
                            *error.Message.ToString());
                    }
                }
            }

            // Phase 2: Save all items that passed validation (skip re-validation)
            int32 writeFailCount = 0;

            for (const TSharedPtr<FSGDTASaveDialogItem>& item : validItems)
            {
                bool bSaved = false;

                // Try saving via open editor first
                if (TWeakPtr<FSGDynamicTextAssetEditorToolkit>* editorEntry = OPEN_EDITORS.Find(item->AbsoluteFilePath))
                {
                    TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = editorEntry->Pin();
                    if (toolkit.IsValid() && toolkit->HasUnsavedChanges())
                    {
                        bSaved = toolkit->SaveToFile(true);
                        if (!bSaved)
                        {
                            writeFailCount++;
                            UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                                TEXT("Shutdown save: failed to save open editor for %s"), *item->AbsoluteFilePath);
                        }
                        continue;
                    }
                }

                // Save cached dirty objects that have no open editor
                if (FDirtyObjectCacheEntry* cachedEntry = DIRTY_OBJECT_CACHE.Find(item->AbsoluteFilePath))
                {
                    if (cachedEntry->Object.IsValid())
                    {
                        if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(cachedEntry->Object.Get()))
                        {
                            TSharedPtr<ISGDynamicTextAssetSerializer> serializer =
                                FSGDynamicTextAssetFileManager::FindSerializerForFile(item->AbsoluteFilePath);
                            if (serializer.IsValid())
                            {
                                FString serializedContents;
                                // Pass the asset GUID so the ON_FILE_CHANGED broadcast from this
                                // direct write carries it (Modified/Created keyed by pre-existence).
                                if (serializer->SerializeProvider(provider, serializedContents)
                                    && FSGDynamicTextAssetFileManager::WriteRawFileContents(item->AbsoluteFilePath, serializedContents,
                                        /*bAutoCheckOut*/ true, /*bBroadcastChange*/ true, provider->GetDynamicTextAssetId().GetGuid()))
                                {
                                    bSaved = true;
                                }
                            }
                        }
                    }

                    if (!bSaved)
                    {
                        writeFailCount++;
                        UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
                            TEXT("Shutdown save: failed to write cached DTA %s"), *item->AbsoluteFilePath);
                    }
                }
            }

            // Phase 3: Handle validation failures
            if (validationFailCount > 0)
            {
                FString message = FString::Printf(
                    TEXT("%d asset(s) failed validation and could not be saved.\n"
                         "Refer to the log or re-run validation for details.\n\n"
                         "Assets that passed validation have been saved.\n\n"
                         "Discard changes to the failed asset(s) and close the editor?"),
                    validationFailCount);

                EAppReturnType::Type choice = FMessageDialog::Open(EAppMsgType::YesNo,
                    FText::FromString(message),
                    INVTEXT("Validation Failed"));

                if (choice != EAppReturnType::Yes)
                {
                    // User chose Don't Discard - cancel shutdown so they can fix validation
                    return false;
                }
            }

            // Phase 4: Handle write failures
            if (writeFailCount > 0)
            {
                FString message = FString::Printf(
                    TEXT("%d file(s) failed to save properly.\n"
                         "Please refer to logs for information on the failure reason.\n\n"
                         "Continue closing editor?"),
                    writeFailCount);

                EAppReturnType::Type choice = FMessageDialog::Open(EAppMsgType::YesNo,
                    FText::FromString(message),
                    INVTEXT("Save Failed"));

                if (choice != EAppReturnType::Yes)
                {
                    return false;
                }
            }

            // Clear all cached dirty objects (saved or intentionally discarded)
            DIRTY_OBJECT_CACHE.Empty();

            // Mark all open dirty editors clean so they close without re-caching in the destructor
            for (const TPair<FString, TWeakPtr<FSGDynamicTextAssetEditorToolkit>>& pair : OPEN_EDITORS)
            {
                TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = pair.Value.Pin();
                if (toolkit.IsValid() && toolkit->HasUnsavedChanges())
                {
                    toolkit->MarkClean();
                }
            }

            // Cover the bulk cache empty above (which bypasses MarkClean) so cached-dirty
            // stars re-query and clear; no stale star lingers on tiles after shutdown-save.
            OnDirtyStateChanged.Broadcast();

            return true;
        }
        case ESGDTASaveDialogResult::DontSave:
        {
            // Discard all unsaved changes
            DIRTY_OBJECT_CACHE.Empty();

            for (const TPair<FString, TWeakPtr<FSGDynamicTextAssetEditorToolkit>>& pair : OPEN_EDITORS)
            {
                TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = pair.Value.Pin();
                if (toolkit.IsValid() && toolkit->HasUnsavedChanges())
                {
                    toolkit->MarkClean();
                }
            }

            // Cover the bulk cache empty above (which bypasses MarkClean) so cached-dirty
            // stars re-query and clear after a discard.
            OnDirtyStateChanged.Broadcast();

            return true;
        }
        // ESGDTASaveDialogResult::Cancelled
        default:
        {
            return false;
        }
    }
}

bool FSGDynamicTextAssetEditorToolkit::CanSaveAsset() const
{
    // Always allow saving so the user can force re-serialization
    // without requiring property modifications (e.g., testing serialization changes).
    return true;
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

        // Notify observers (browser tile dirty badges) to re-query on the clean->dirty transition
        OnDirtyStateChanged.Broadcast();
    }
}

void FSGDynamicTextAssetEditorToolkit::MarkClean()
{
    bHasUnsavedChanges = false;
    RegenerateMenusAndToolbars();

    // Notify observers (browser tile dirty badges) to re-query on the transition to clean
    OnDirtyStateChanged.Broadcast();
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
    FText dataObjectId = FText::GetEmpty();
    FText userFacingId = FText::GetEmpty();
    FText version = FText::GetEmpty();
    FText fileFormatVersion = FText::GetEmpty();
    FText rawText = FText::GetEmpty();

    // Retrieve values from the DTA
    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(EditedDynamicTextAsset))
    {
        dataObjectId = FText::FromString(provider->GetDynamicTextAssetId().ToString());
        userFacingId = FText::FromString(provider->GetUserFacingId());
        version = FText::FromString(provider->GetVersion().ToString());
        if (TSharedPtr<ISGDynamicTextAssetSerializer> serializer = USGDynamicTextAssetStatics::FindSerializerForDynamicTextAssetId(provider->GetDynamicTextAssetId()))
        {
            fileFormatVersion = serializer->GetFileFormatVersion().ToText();
        }
    }

    return SNew(SDockTab)
        .Label(INVTEXT("Raw View"))
        [
            SAssignNew(RawView, SSGDynamicTextAssetRawView)
            .DynamicTextAssetId(dataObjectId)
            .UserFacingId(userFacingId)
            .Version(version)
            .JsonText(GetRawText())
            .FileFormatVersion(fileFormatVersion)
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

    FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(InFilePath);
    if (!fileInfo.bIsValid)
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("OpenEditorForFile: failed to extract file info %s"), *InFilePath);
        return;
    }

    // Resolve class via Asset Type ID (O(1) map lookup), with fallback to class name for legacy files
    UClass* dataObjectClass = nullptr;
    FSGDynamicTextAssetTypeId resolvedTypeId = fileInfo.AssetTypeId;
    if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
    {
        if (fileInfo.AssetTypeId.IsValid())
        {
            dataObjectClass = registry->ResolveClassForTypeId(fileInfo.AssetTypeId);
        }

        // Fallback: resolve by class name for legacy files without a valid Asset Type ID
        if (!dataObjectClass && !fileInfo.ClassName.IsEmpty())
        {
            TArray<UClass*> allClasses;
            registry->GetAllInstantiableClasses(allClasses);
            for (UClass* registeredClass : allClasses)
            {
                if (registeredClass && registeredClass->GetName() == fileInfo.ClassName)
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
            *fileInfo.AssetTypeId.ToString(), *fileInfo.ClassName, *InFilePath);
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

    // Update dirty object cache key if it exists
    if (FDirtyObjectCacheEntry cachedEntry; DIRTY_OBJECT_CACHE.RemoveAndCopyValue(OldFilePath, cachedEntry))
    {
        DIRTY_OBJECT_CACHE.Add(NewFilePath, MoveTemp(cachedEntry));
    }

    // Reload UserFacingId from the renamed file's file info
    if (ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(toolkit->EditedDynamicTextAsset))
    {
        FSGDynamicTextAssetFileInfo renamedFileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(NewFilePath);
        if (renamedFileInfo.bIsValid && !renamedFileInfo.UserFacingId.IsEmpty())
        {
            provider->SetUserFacingId(renamedFileInfo.UserFacingId);
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
    DIRTY_OBJECT_CACHE.Remove(InFilePath);
    toolkit->CloseWindow(EAssetEditorCloseReason::AssetUnloadingOrInvalid);

    // The file (and any dirty state for it) is gone; tell observers to re-query so no stale star lingers
    OnDirtyStateChanged.Broadcast();

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("NotifyFileDeleted: closed editor for %s"), *InFilePath);
}

void FSGDynamicTextAssetEditorToolkit::NotifyFileReverted(const FString& InFilePath)
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

    toolkit->ReloadFromDisk();

    UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("NotifyFileReverted: reloaded editor for %s"), *InFilePath);
}

void FSGDynamicTextAssetEditorToolkit::ReloadFromDisk()
{
    // Re-read FilePath into EditedDynamicTextAsset. LoadFromFile reuses the existing in-memory
    // object when the class is unchanged and already clears any cached dirty object for this path.
    if (!LoadFromFile())
    {
        UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("ReloadFromDisk: failed to reload %s"), *FilePath);
        return;
    }

    // Force the Details view to rebind to the reloaded object. LoadFromFile calls SetObject without
    // a forced refresh, so when the object identity is unchanged the panel would keep stale values.
    if (DetailsView.IsValid())
    {
        DetailsView->SetObject(EditedDynamicTextAsset, /*bForceRefresh=*/true);
    }

    // Refresh the raw text tab so it reflects the reverted-to disk contents.
    RefreshRawView();

    // Ensure the dirty flag is clear; the reload matches disk exactly, so there is no save prompt.
    MarkClean();
}

bool FSGDynamicTextAssetEditorToolkit::HasOpenEditorWithUnsavedChanges(const FString& InFilePath)
{
    TWeakPtr<FSGDynamicTextAssetEditorToolkit>* existing = OPEN_EDITORS.Find(InFilePath);
    if (existing && existing->IsValid())
    {
        TSharedPtr<FSGDynamicTextAssetEditorToolkit> toolkit = existing->Pin();
        if (toolkit.IsValid())
        {
            return toolkit->HasUnsavedChanges();
        }
    }

    // Also check the dirty object cache for closed-but-dirty editors
    return DIRTY_OBJECT_CACHE.Contains(InFilePath);
}

bool FSGDynamicTextAssetEditorToolkit::SaveOpenEditor(const FString& InFilePath)
{
    TWeakPtr<FSGDynamicTextAssetEditorToolkit>* existing = OPEN_EDITORS.Find(InFilePath);
    if (!existing || !existing->IsValid())
    {
        // No editor open  - nothing to save
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
