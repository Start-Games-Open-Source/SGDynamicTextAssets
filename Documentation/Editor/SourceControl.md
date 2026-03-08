# Source Control

[Back to Table of Contents](../TableOfContents.md)

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
```

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
    ModifiedLocally
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
