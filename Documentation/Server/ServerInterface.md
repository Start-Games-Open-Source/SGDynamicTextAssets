# Server Interface

[Back to Table of Contents](../TableOfContents.md)

## Overview

The server interface provides an abstraction layer for connecting to a game backend to fetch live data overrides. Games implement this interface to enable server side updates to dynamic text assets without requiring a client patch.

## USGDynamicTextAssetServerInterface

An abstract UObject class that games subclass to provide their backend integration.

```cpp
UCLASS(Abstract, ClassGroup = "Start Games")
class USGDynamicTextAssetServerInterface : public UObject
```

### Pure Virtual Methods

```cpp
virtual void FetchAllDynamicTextAssets(
    const TArray<FSGServerDynamicTextAssetRequest>& LocalObjects,
    FOnServerFetchComplete OnComplete)
PURE_VIRTUAL(USGDynamicTextAssetServerInterface::FetchAllDynamicTextAssets,);

virtual void CheckDynamicTextAssetExists(
    const FSGDynamicTextAssetId& Id,
    FOnServerExistsComplete OnComplete)
PURE_VIRTUAL(USGDynamicTextAssetServerInterface::CheckDynamicTextAssetExists,);
```

### Delegate Types

```cpp
// Bulk fetch completion
DECLARE_DELEGATE_TwoParams(FOnServerFetchComplete,
    bool /*bSuccess*/,
    const TArray<FSGServerDynamicTextAssetResponse>& /*Updates*/);

// Single exists check completion
DECLARE_DELEGATE_ThreeParams(FOnServerExistsComplete,
    bool /*bSuccess*/,
    bool /*bExists*/,
    const FString& /*UpdatedJson*/);
```

### Request/Response Types

```cpp
USTRUCT(BlueprintType)
struct FSGServerDynamicTextAssetRequest
{
    // Dynamic text asset to query
    FSGDynamicTextAssetId Id;
    
    // Client's current version
    FSGDynamicTextAssetVersion LocalVersion;
};

USTRUCT(BlueprintType)
struct FSGServerDynamicTextAssetResponse
{
    // Dynamic text asset being updated
    FSGDynamicTextAssetId Id;
    
    // Updated data (empty if object does not exist on server)
    FString Data;
    
    // Server's version
    FSGDynamicTextAssetVersion ServerVersion;
};
```

## USGDynamicTextAssetServerNullInterface

The **null object pattern** implementation of the server interface. Provides immediate no-op responses for all server operations.

```cpp
UCLASS()
class USGDynamicTextAssetServerNullInterface : public USGDynamicTextAssetServerInterface
```

When no real server backend is configured, the subsystem uses this interface so that loading flows proceed uniformly **without null checks**. All responses indicate success with empty data, causing the subsystem to fall through to local file loading.

The subsystem creates a `USGDynamicTextAssetServerNullInterface` on initialization and always maintains a valid `ServerInterface` pointer.

## Integration with Loading

When server integration is enabled, the subsystem loading flow becomes:

1. **Check cache**: Return immediately if already loaded.
2. **Check server**: Query the server interface for updates (no-op if using null interface).
3. **Apply server data**: If the server provides updated data, use it instead of the local file.
4. **Handle deletions**: If the server indicates the object is deleted (empty `Data` in response), return `ESGLoadResult::ServerDeletedObject`.
5. **Fall back to local**: If the server is unavailable, fall back to the local file.

Server failures result in `ESGLoadResult::ServerFetchFailed`. The system can be configured to continue with local data on failure.

## Configuration

Server integration is controlled via a `USGDynamicTextAssetSettingsAsset`:

| Setting | Default | Description |
|---------|---------|-------------|
| `bEnableServerOverride` | `false` | Master switch for server integration |
| `ServerRequestTimeoutSeconds` | `5.0` | Timeout for server requests (1-60 seconds) |
| `bDeleteLocalOnServerMismatch` | `false` | Delete local files when server says object is deleted |

See [Settings](../Configuration/Settings.md) for configuration details.

## Implementation Guide

1. Create a subclass of `USGDynamicTextAssetServerInterface` (parent is a `UObject`):

```cpp
UCLASS()
class UMyGameServerInterface : public USGDynamicTextAssetServerInterface
{
    GENERATED_BODY()

public:

    // USGDynamicTextAssetServerInterface interface
    virtual void FetchAllDynamicTextAssets(
        const TArray<FSGServerDynamicTextAssetRequest>& LocalObjects,
        FOnServerFetchComplete OnComplete) override
    {
        // Build HTTP request to your backend
        // Compare local versions with server versions
        // Return responses for objects that have changed
    }

    virtual void CheckDynamicTextAssetExists(
        const FSGDynamicTextAssetId& Id,
        FOnServerExistsComplete OnComplete) override
    {
        // Query your backend to check if the object exists
    }
    // ~USGDynamicTextAssetServerInterface interface
};
```

2. Register your server interface implementation with the subsystem during game initialization.

3. Enable server override in your settings asset (`bEnableServerOverride = true`).

## Type Manifest Overrides

The server interface includes support for dynamically modifying the type system at runtime:

```cpp
virtual void FetchTypeManifestOverrides(FOnServerTypeOverridesComplete OnComplete)
PURE_VIRTUAL(USGDynamicTextAssetServerInterface::FetchTypeManifestOverrides,);
```

### Response Format

```json
{
  "manifests": {
    "<rootTypeId>": {
      "types": [
        { "typeId": "...", "className": "UNewWeaponType", "parentTypeId": "..." }
      ]
    }
  }
}
```

### Callback Semantics

- `bSuccess=true` with valid `OverrideData`: overrides applied to matching manifests
- `bSuccess=true` with null `OverrideData`: no overrides available (no-op)
- `bSuccess=false`: fetch failed, no changes made

### Integration Flow

The subsystem provides a convenience method that combines fetch and apply:

```cpp
void FetchAndApplyServerTypeOverrides(FOnServerTypeOverridesComplete OnComplete = {});
```

This calls the server interface's `FetchTypeManifestOverrides()`, and on success routes the JSON data through `USGDynamicTextAssetRegistry::ApplyServerTypeOverrides()`. The overlay is stored separately per manifest and never modifies local files. An entry with an empty `className` disables that type.

Server type overrides are skipped in editor builds (no-op).

See [Type Manifest System](../Advanced/TypeManifestSystem.md) for full details on overlay behavior and resolution priority.

[Back to Table of Contents](../TableOfContents.md)
