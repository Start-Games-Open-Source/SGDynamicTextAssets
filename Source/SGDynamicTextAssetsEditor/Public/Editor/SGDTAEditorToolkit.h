// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/NotifyHook.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Editor/SGDTAEditorProxy.h"
#include "EditorUndoClient.h"

class IDetailsView;
class SSGDynamicTextAssetRawView;

struct FSGDynamicTextAssetId;
struct FSGDynamicTextAssetTypeId;

/**
 * Tab ID for the Details panel inside the dynamic text asset editor.
 */
namespace SGDynamicTextAssetEditorTabs
{
    static const FName DetailsTabId("Details");
    static const FName RawViewTabId("RawView");
}

/**
 * Standalone asset editor toolkit for editing a single dynamic text asset.
 *
 * Replaces SSGDynamicTextAssetEditor. Because dynamic text assets are plain JSON files
 * (not UAssets), a transient USGDynamicTextAssetEditorProxy is provided as the
 * asset handle. The toolkit owns the window lifecycle, layout persistence,
 * and toolbar via the standard FAssetEditorToolkit machinery.
 *
 * Layout:
 * +---------------------------------------------------------+
 * | Standard toolbar (Save, Revert, Copy Raw, ...)          |
 * +---------------------------------------------------------+
 * | Info bar (class link, GUID)                             |
 * +---------------------------------------------------------+
 * | Details view (IDetailsView)                             |
 * +---------------------------------------------------------+
 * | Status bar (Saved / Unsaved Changes)                    |
 * +---------------------------------------------------------+
 *
 * Call OpenEditor() or OpenEditorForFile() to open or focus a window.
 */
class SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetEditorToolkit
    : public FAssetEditorToolkit
    , public FNotifyHook
    , public FEditorUndoClient
{
public:

    FSGDynamicTextAssetEditorToolkit();
    virtual ~FSGDynamicTextAssetEditorToolkit();

    // FAssetEditorToolkit overrides
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual FText GetToolkitName() const override;   // returns UserFacingId [*] dynamically
    virtual FString GetWorldCentricTabPrefix() const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
    virtual void PostRegenerateMenusAndToolbars() override;
    // ~FAssetEditorToolkit overrides

    // FNotifyHook overrides
    virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent,
                                  FProperty* InPropertyThatChanged) override;
    // ~FNotifyHook overrides

    // FEditorUndoClient overrides
    virtual void PostUndo(bool bSuccess) override;
    virtual void PostRedo(bool bSuccess) override;
    // ~FEditorUndoClient overrides

    /**
     * Initialises this toolkit ready to edit the given proxy asset.
     * Called immediately after construction.
     */
    void InitEditor(EToolkitMode::Type Mode,
                    const TSharedPtr<IToolkitHost>& InitToolkitHost,
                    USGDynamicTextAssetEditorProxy* Proxy);

    /**
     * Opens (or focuses) a dynamic text asset editor for the given file and class.
     *
     * @param FilePath              Absolute path to the .dta.json file
     * @param DynamicTextAssetClass ISGDynamicTextAssetProvider implementor class
     * @param AssetTypeId           The asset type ID for the class
     */
    static void OpenEditor(const FString& FilePath, UClass* DynamicTextAssetClass,
        const FSGDynamicTextAssetTypeId& AssetTypeId);

    /**
     * Opens (or focuses) a dynamic text asset editor, resolving the class from file.
     *
     * @param FilePath  Absolute path to the .dta.json file
     */
    static void OpenEditorForFile(const FString& FilePath);

    /**
     * Notifies any open editor that a file has been renamed.
     * Updates internal FilePath, deduplication map key, and tab title.
     *
     * @param OldFilePath  Previous absolute file path
     * @param NewFilePath  New absolute file path
     */
    static void NotifyFileRenamed(const FString& OldFilePath, const FString& NewFilePath);

    /**
     * Closes any open editor for the given file path.
     * Removes the entry from the deduplication map and closes the editor window.
     *
     * @param FilePath  Absolute file path of the deleted file
     */
    static void NotifyFileDeleted(const FString& FilePath);

    /**
     * Returns true if there is an open editor for the given file path that has unsaved changes.
     * Returns false if no editor is open for that path or if the editor has no unsaved changes.
     *
     * @param FilePath  Absolute file path to check
     */
    static bool HasOpenEditorWithUnsavedChanges(const FString& FilePath);

    /**
     * Saves the open editor for the given file path.
     * Returns true if the save succeeded, if no editor is open (nothing to save),
     * or if the editor has no unsaved changes. Returns false only if SaveToFile() fails.
     *
     * @param FilePath  Absolute file path of the editor to save
     */
    static bool SaveOpenEditor(const FString& FilePath);

    /**
     * Registered with IMainFrameModule::RegisterCanCloseEditor to intercept editor shutdown.
     * Collects all dirty DTAs (open editors + cached dirty objects), shows a modal save dialog,
     * and returns false to block shutdown if the user cancels.
     */
    static bool HandleCanCloseEditor();

    /** Returns the absolute path of the file being edited. */
    const FString& GetFilePath() const { return FilePath; }

    bool HasUnsavedChanges() const { return bHasUnsavedChanges; }

private:

    // FAssetEditorToolkit overrides
    virtual bool CanSaveAsset() const override;
    virtual void SaveAsset_Execute() override;
    virtual void FindInContentBrowser_Execute() override;
    // ~FAssetEditorToolkit overrides

    TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnTab_RawView(const FSpawnTabArgs& Args);

    /** Registers editor commands and adds a toolbar extension. */
    void ExtendToolbar();

    /** Populates the toolbar extension section. */
    void FillToolbar(FToolBarBuilder& ToolbarBuilder);

    /** Constructs the parent class hyperlink in the top-right corner of the editor's menu bar. */
    void ConstructParentClassHyperlink();

    /** Loads the dynamic text asset from FilePath into EditedDynamicTextAsset. */
    bool LoadFromFile();

    /**
     * Saves the in-memory dynamic text asset to its JSON file on disk.
     *
     * @param bSkipValidation  If true, bypasses validation and warning dialogs.
     *                         Used during shutdown saves where validation is handled externally.
     */
    bool SaveToFile(bool bSkipValidation = false);

    void MarkDirty();
    void MarkClean();

    void OnPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);

    FText GetWindowTitle() const;
    FText GetStatusBarText() const;
    FSlateColor GetStatusBarColor() const;
    FText GetIdDisplayText() const;
    FText GetClassDisplayText() const;
    FText GetRawText() const;
    void  OnClassLinkClicked();

    /** Re-reads the file from disk and updates the raw view widget with fresh text and identity properties. */
    void RefreshRawView();

    FReply OnCopyToClipboard(const FString& TextToCopy) const;

    /** Absolute path to the JSON file being edited. */
    FString FilePath;

    /** The asset type ID for the file's class. */
    FSGDynamicTextAssetTypeId AssetTypeId;

    /** The IDetailsView displaying the in-memory dynamic text asset. */
    TSharedPtr<IDetailsView> DetailsView = nullptr;

    /**
     * Live in-memory provider instance.
     * Typed as UObject* because the provider may not be a USGDynamicTextAsset.
     */
    TObjectPtr<UObject> EditedDynamicTextAsset = nullptr;

    /** Keeps EditedDynamicTextAsset alive against GC. */
    TStrongObjectPtr<UObject> EditedDynamicTextAssetStrong = nullptr;

    TSharedPtr<SSGDynamicTextAssetRawView> RawView = nullptr;

    /** Format version of the file as it was last loaded from disk. Used to detect upgrades on save. */
    FSGDynamicTextAssetVersion LoadedFileFormatVersion;

    /** True when the in-memory state differs from the file on disk. */
    uint8 bHasUnsavedChanges : 1 = 0;

    /** Weak references to all currently open toolkits, keyed by file path. */
    static TMap<FString, TWeakPtr<FSGDynamicTextAssetEditorToolkit>> OPEN_EDITORS;

    /** Entry for caching a dirty UObject when an editor tab is closed with unsaved changes. */
    struct FDirtyObjectCacheEntry
    {
        TStrongObjectPtr<UObject> Object;
        FSGDynamicTextAssetVersion LoadedFileFormatVersion;
    };

    /** Cached dirty objects for editors that were closed with unsaved changes. Keyed by file path. */
    static TMap<FString, FDirtyObjectCacheEntry> DIRTY_OBJECT_CACHE;
};
