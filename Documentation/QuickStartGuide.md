# Quick Start Guide

[Back to Table of Contents](TableOfContents.md)

This guide walks through creating your first dynamic text asset, from C++ class definition to runtime loading.

## 1. Enable the Plugin

Enable **SGDynamicTextAssets** in Edit > Plugins, or add it to your `.uproject` file:

```json
{
  "Plugins": [
    {
      "Name": "SGDynamicTextAssets",
      "Enabled": true
    }
  ]
}
```

Add the runtime module as a dependency in your game module's `.Build.cs`:

```csharp
PublicDependencyModuleNames.Add("SGDynamicTextAssetsRuntime");
```

## 2. Create a Dynamic Text Asset Subclass

Define a C++ class that inherits from `USGDynamicTextAsset`. Add UPROPERTY fields for your configuration data.

```cpp
// WeaponData.h
#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAsset.h"
#include "WeaponData.generated.h"

UCLASS(BlueprintType)
class MYGAME_API UWeaponData : public USGDynamicTextAsset
{
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float BaseDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float AttackSpeed = 1.0f;
};
```

Child classes of registered base dynamic text asset classes are automatically discovered via Unreal's reflection system. You do not need to manually register every subclass.

## 3. Register Root Dynamic Text Asset Classes (If Applicable)

> **Note:** Child classes of already-registered base classes are discovered automatically via reflection. You only need to manually register a class if you are introducing a **new root type** that does not extend an existing registered base class.

If your class introduces a new root hierarchy (e.g., `UWeaponData` is a direct child of `USGDynamicTextAsset`), register it during module startup:

```cpp
// MyGameModule.cpp
#include "Management/SGDynamicTextAssetRegistry.h"
#include "WeaponData.h"

void FMyGameModule::StartupModule()
{
    USGDynamicTextAssetRegistry::GetChecked().RegisterDynamicTextAssetClass(UWeaponData::StaticClass());
}
```

If `UWeaponData` is already registered and you create `USwordData : public UWeaponData`, you do **not** need to register `USwordData`. It will be found automatically.

## 4. Create an Instance in the Browser

1. Open the Dynamic Text Asset Browser from the toolbar or Window menu.
2. Select your class (e.g., `WeaponData`) in the type tree on the left.
3. Click **New** in the toolbar.
4. Enter a UserFacingId (e.g., `"excalibur"`). This becomes the filename.
5. A new `.dta.json` file is automatically created at `Content/SGDynamicTextAssets/WeaponData/excalibur.dta.json`.

## 5. Edit Properties

Double-click the dynamic text asset in the browser to open the Dynamic Text Asset Editor. This shows a standard Unreal details panel with your UPROPERTY fields. Edit values and click **Save** to write changes to the file.

> You can also right-click and select `Open` to open the editor.

The editor tracks unsaved changes and supports **Revert** to reload from disk, **Copy Raw** to copy the file content to clipboard, and a **Raw View** tab to inspect the actual on-disk file content.

## 6. Load at Runtime

Use `USGDynamicTextAssetSubsystem` to load dynamic text assets at runtime. The subsystem is a `UGameInstanceSubsystem` created automatically and accessible from any game instance.

### Synchronous Loading

```cpp
USGDynamicTextAssetSubsystem* Subsystem =
    GetGameInstance()->GetSubsystem<USGDynamicTextAssetSubsystem>();

FSGDynamicTextAssetId WeaponId = FSGDynamicTextAssetId::FromString(TEXT("550e8400-e29b-41d4-a716-446655440000"));
FString FilePath;

// Load weapon data into the subsystem cache
Subsystem->GetOrLoadDynamicTextAsset(WeaponId, FilePath, UWeaponData::StaticClass());

// Retrieve by ID
UWeaponData* Weapon = Subsystem->GetDynamicTextAsset<UWeaponData>(WeaponId);
```

### Asynchronous Loading

```cpp
Subsystem->LoadDynamicTextAssetAsync(WeaponId,
    UWeaponData::StaticClass()/*Optional, can be nullptr*/,
    FOnDynamicTextAssetLoaded::CreateLambda(
        [](TScriptInterface<ISGDynamicTextAssetProvider> Provider, bool bSuccess)
    {
        if (bSuccess)
        {
            UWeaponData* Weapon = Cast<UWeaponData>(Provider.GetObject());
            // Use weapon data...
        }
    }));
```

## 7. Reference from Other Assets

Use `FSGDynamicTextAssetRef` to store references to dynamic text assets in your game classes. The ref stores only the `FSGDynamicTextAssetId` and resolves lazily.

```cpp
// In your character or item class
UPROPERTY(EditAnywhere, meta = (DynamicTextAssetType = "UWeaponData"))
FSGDynamicTextAssetRef WeaponRef;
```

The `DynamicTextAssetType` metadata restricts the editor picker to show only `UWeaponData` and its subclasses.

Resolve the reference at runtime:

```cpp
// Requires world context to access the game instance subsystem
if (UWeaponData* Weapon = WeaponRef.Get<UWeaponData>(GetGameInstance()))
{
    float damage = Weapon->BaseDamage;
}
```

## 8. Blueprint Usage

The `USGDynamicTextAssetStatics` Blueprint function library exposes common operations to Blueprints:

- `Is Valid Dynamic Text Asset Ref`: Check if a reference has a valid GUID.
- `Get Dynamic Text Asset`: Resolve a reference to the loaded object.
- `Load Dynamic Text Asset Ref Async`: Load a referenced object asynchronously with a callback.
- `Make Dynamic Text Asset Ref`: Create a reference from a GUID.
- `Get All Dynamic Text Asset IDs By Class`: Scan the file system for all GUIDs of a given class.
- `Get All Loaded Dynamic Text Assets Of Class`: Get all cached objects of a given type.

Validation results can also be queried in Blueprints using functions like `Validation Result Has Errors`, `Get Validation Result Errors`, and `Validation Result To String`.

## Next Steps

- [Dynamic Text Assets](Core/DynamicTextAssets.md): Deep dive into `USGDynamicTextAsset`, overridable hooks, and best practices.
- [Serialization](Serialization/SerializerInterface.md): Understand the serializer architecture and supported formats.
- [Cook Pipeline](Serialization/CookPipeline.md): Learn how dynamic text assets are packaged for shipping builds.
- [Server Integration](Server/ServerInterface.md): Connect a backend for live data overrides.

[Back to Table of Contents](TableOfContents.md)
