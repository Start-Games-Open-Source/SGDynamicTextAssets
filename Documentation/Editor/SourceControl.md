# Source Control

[Back to Table of Contents](../TableOfContents.md)

This page is the central hub for the source control integration that ships with the dynamic text asset tooling. It covers the end to end workflow for a dynamic text asset (DTA) file, the visual status surfaces in the browser and editor, how status stays current on its own, the interim diff support, the automatic author-level operations, and the provider-awareness rules that keep the experience faithful to Perforce, Subversion, and Git. The reference API at the bottom documents the `FSGDynamicTextAssetSourceControl` wrapper for callers that need to drive source control directly.

DTA files are plain text (or cooked binary) files on disk rather than Unreal `.uasset` packages, so the tooling integrates with whatever provider is configured in the editor through the same `ISourceControlModule` the rest of the editor uses. When no provider is enabled, every operation still works and simply skips the source control step.

## Source Control Workflow

The typical loop for editing a tracked DTA file mirrors the native asset workflow:

1. **Check out.** On a checkout provider (Perforce, Subversion), select the file in the browser and use **Revision Control > Check Out**, or simply start editing in the [Dynamic Text Asset Editor](DynamicTextAssetEditor.md). Saving a tracked file that is not yet checked out will check it out automatically. On Git there is no checkout step, since Git is modify-in-place.
2. **Edit.** Open the file in the editor and change its properties. While an editor holds unsaved changes, the browser tile shows an unsaved-change star and the editor shows its own status indicator.
3. **Diff.** Use **Revision Control > Diff Against Depot** to compare your on-disk file against the head depot revision in your configured external text-diff tool.
4. **Revert.** If you want to discard your changes, use **Revision Control > Revert**. Any open editor for the reverted file reloads so its content and unsaved-change star match disk.
5. **Check in.** Use **Revision Control > Check In** to open the engine check-in dialog for the selected files and submit.

The browser is the primary surface for status visuals and the Revision Control submenu; see the [Dynamic Text Asset Browser](DynamicTextAssetBrowser.md) for the tile grid and context menu, and the [Dynamic Text Asset Editor](DynamicTextAssetEditor.md) for the in-editor status indicator and save behavior.

## Tile Status Visuals

Each browser tile can carry two independent overlays: a source-control badge in the top-left corner and an unsaved-change star in the top-right corner. Its tooltip adds source-control and unsaved-change lines to the base asset information.

### Source Control Badge

The tile renders the source control **provider's own status icon**, the same overlay the native Content Browser shows for that provider. The badge brush comes directly from the provider's `ISourceControlState::GetIcon()` for the file, so it is engine-faithful per provider. There is no plugin-specific icon set and no fixed mapping from status to icon; whatever the active provider draws is what appears.

Because the icon comes from the provider, the meaning follows that provider's model:

- **Perforce and Subversion** use a checkout / lock model, so their icons distinguish states like checked out by you and checked out (locked) by another user.
- **Git** is modify-in-place, so its icons reflect the working-tree state, such as a locally modified file.

The states that produce a visible marker across providers include added (marked for add), locally modified, marked for delete, checked out by you (on checkout providers), and checked out by another user. An up-to-date, clean tracked file shows **no badge**: the provider returns an empty icon for that state, which the tile treats as "no overlay". When source control is disabled, there is no provider to query, so the tile shows **no badge** at all.

![A browser tile showing a provider source-control badge in the top-left corner and the unsaved change star in the top-right corner.](images/SCCBadgeExample.png)

### Unsaved-Change Star

Separately from source control, a tile whose file is open in an editor with unsaved changes shows a star overlay in the top-right corner (the editor's standard dirty icon). This overlay is driven purely by editor dirty state and is independent of the provider, so it appears whether or not source control is enabled. In the tile tooltip this surfaces as the line:

> Has unsaved changes.

### Tile Tooltip

The tile tooltip starts with the asset's display name, id, and type. Below a divider it appends, when applicable:

- The **unsaved-change line** (`Has unsaved changes.`) when an open editor for the file has unsaved edits.
- The **source-control line**, taken verbatim from the provider's `ISourceControlState::GetDisplayTooltip()` for the file. This is skipped entirely when source control is disabled, and it agrees with the tile badge because both read from the same provider state.
- Referencer and dependency counts for the asset.

## Automatic Status Refresh

Source control status stays current without a manual refresh click. Two triggers converge the badge, tooltip, editor indicator, and Revision Control menu enablement onto the correct state:

- **On editor focus-gain.** When the editor application regains focus (for example after you staged files with Git in an external terminal and alt-tabbed back), the browser requests a fresh status update for the files it is showing. Backgrounding the editor does not trigger a refresh.
- **After any local write.** Immediately after the tooling writes a DTA file (save, rename, duplicate, or format conversion), the affected path's status is re-evaluated. This matters for providers, notably Git, that only report a saved edit as modified after a status refresh; without it the badge, tooltip, and Revert enablement would stay stale until the next focus change.

Together these mean that a modified file converges to the correct badge, tooltip, and correct Revert / Check Out enablement on its own, including on Git.

You can also force a status update for a selection at any time with **Revision Control > Refresh**.

## Diff Against Depot vs Planned Semantic Diff

**Diff Against Depot (available now).** The **Revision Control > Diff Against Depot** entry launches your configured external text-diff tool comparing the on-disk file to its head depot revision. It fetches the depot revision to a temporary file and shells out to the tool set in Editor Preferences, both of which operate on plain files. Because a diff is pairwise, this is enabled only for a single selected file that is tracked and whose provider allows diffing against the depot. If no external diff tool is configured or it cannot launch, a notification explains how to set one:

> Could not launch the external diff tool. Set a valid text-diff tool in Editor Preferences and try again.

This is a line-level, format-agnostic view of the raw file text.

**Semantic diff (planned, not yet available).** A DTA-aware, property-level semantic diff is a separate, later effort. It is not shipped today. The interim raw text diff above is the only diff currently available, and the two are intended to coexist as distinct views once the semantic diff lands.

## File Manager Auto Source Control (Author Level)

At author time in the editor, the file manager keeps source control in step with local file mutations automatically, so an author does not have to run separate source control commands for routine edits:

- **Save / write** checks out a tracked file that is not already checked out (on providers that use checkout).
- **Create** marks the new file for add.
- **Rename** marks the old path for delete and the new path for add.
- **Duplicate** marks the new file for add.
- **Convert file format** marks the new-format file for add and the old-format file for delete.
- **Delete** marks the file for delete.

These operations only run when source control is enabled; when it is not configured, the file operations proceed normally with no source control interaction. See the [File Manager](../Runtime/FileManager.md) page for the file operations these hooks ride on.

## Provider Awareness

The integration adapts to the active provider's model rather than assuming a checkout workflow. `FSGDynamicTextAssetSourceControl::ProviderUsesCheckout()` returns true for Perforce and Subversion and false for Git (and false whenever source control is disabled).

This is why:

- The **Check Out** action and checkout-specific status appear on Perforce and Subversion but are hidden on Git, where checking out has no meaning.
- A **clean tracked Git file reads as not-checked-out** rather than checked-out. Git aliases its checked-out query to "is source controlled", so without this gate a clean tracked Git file would misreport as checked out. Gating the checked-out state on `ProviderUsesCheckout()` lets Git fall through to not-checked-out while leaving Perforce and Subversion unchanged.

## Presentation vs Gating

The integration keeps two concerns deliberately separate:

- **Presentation reads provider state directly.** The tile badge, the tile and editor tooltips, and the editor status indicator read the provider's `ISourceControlState` (`GetIcon()`, `GetDisplayName()`, `GetDisplayTooltip()`) so what you see always matches the native editor and the active provider.
- **Gating uses the plugin enum.** The `ESGDynamicTextAssetSourceControlStatus` enum is used only to decide which Revision Control actions are enabled, most importantly Check Out and Revert. It is a coarse, gate-friendly classification and is never used to pick an icon or compose a tooltip string.

Keeping these split means the visuals never drift from the provider, while action enablement stays simple and predictable.

---

## FSGDynamicTextAssetSourceControl

A static utility class that wraps Unreal's `ISourceControlModule` for managing source control operations on dynamic text asset files. Supports any source control provider configured in the editor (Perforce, Git, SVN, etc.).

```cpp
class FSGDynamicTextAssetSourceControl
{
    // All members are static; no instances
};
```

## File Operations

### Single File

```cpp
// Check out for editing
static bool CheckOutFile(const FString& FilePath);

// Revert local changes
static bool RevertFile(const FString& FilePath);

// Mark new file for add
static bool MarkForAdd(const FString& FilePath);

// Mark file for deletion
static bool MarkForDelete(const FString& FilePath);
```

### Batch Operations

```cpp
// Batch checkout
static bool CheckOutFiles(const TArray<FString>& FilePaths);

// Batch add
static bool MarkFilesForAdd(const TArray<FString>& FilePaths);
```

## Status Queries

```cpp
static ESGDynamicTextAssetSourceControlStatus GetFileStatus(const FString& FilePath);
static bool IsCheckedOutByOther(const FString& FilePath, FString& OutUsername);
static bool IsSourceControlEnabled();

// True when the active provider uses a checkout / lock workflow (Perforce, SVN);
// false for Git (modify-in-place) and false when source control is disabled.
static bool ProviderUsesCheckout();
```

`GetFileStatus` resolves the status in a fixed precedence so the most specific state wins: not in source control, then added, then marked for delete, then checked out by another user, then locally modified, then checked out (only when the file is checked out and the provider uses checkout), otherwise not checked out. Modification is tested before checkout so `CheckedOut` means specifically "checked out but unmodified", and the checkout branch is gated on `ProviderUsesCheckout()` so a clean tracked Git file falls through to `NotCheckedOut`.

### Status Enum

```cpp
enum class ESGDynamicTextAssetSourceControlStatus
{
    // Status not yet queried
    Unknown,
    // File not tracked
    NotInSourceControl,
    // Marked for add
    Added,
    // Checked out by current user
    CheckedOut,
    // Checked out by another user
    CheckedOutByOther,
    // Tracked but not checked out
    NotCheckedOut,
    // Local modifications
    ModifiedLocally,
    // Marked for delete
    MarkedForDelete
};
```

## Automatic Integration

The source control integration hooks into editor workflows automatically:

| Action | Source Control Operation |
|--------|------------------------|
| **Save** in the Dynamic Text Asset Editor | Auto-checkout if not already checked out |
| **Create** new dynamic text asset | Auto-mark for add |
| **Delete** dynamic text asset | Auto-mark for delete |
| **Rename** dynamic text asset | Mark old path for delete, mark new path for add |
| **Duplicate** dynamic text asset | Auto-mark the new file for add |
| **Convert file format** | Mark new-format file for add, mark old-format file for delete |

These automatic operations only trigger when source control is enabled in the editor. If source control is not configured, all operations proceed normally without source control interaction.

## Conflict Detection

Before saving, the editor checks for conflicts using `IsCheckedOutByOther()`:

```cpp
static bool IsCheckedOutByOther(const FString& FilePath, FString& OutUsername);
```

If another user has the file locked, the returned `OutUsername` identifies who has the lock. The editor displays a warning before allowing the save to proceed.

## Batch Operations

For multi-file operations (e.g., bulk format conversion, batch deletes), the source control wrapper provides batch methods:

```cpp
static bool CheckOutFiles(const TArray<FString>& FilePaths);
static bool MarkFilesForAdd(const TArray<FString>& FilePaths);
```

These are more performant than single-file loops as they issue a single source control command for the entire batch.

[Back to Table of Contents](../TableOfContents.md)
