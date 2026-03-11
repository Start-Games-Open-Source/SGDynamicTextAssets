# Property Customizations

[Back to Table of Contents](../TableOfContents.md)

## FSGDynamicTextAssetRefCustomization

A custom property editor for `FSGDynamicTextAssetRef` fields that replaces the default struct view with a user friendly dropdown picker.

### Picker Dropdown

When an `FSGDynamicTextAssetRef` property is displayed in a Details panel, the customization shows:

- A **searchable dropdown** listing all available dynamic text assets.
- Each entry shows the UserFacingId and ClassName in the format `"UserFacingId (ClassName)"`.
- Text search filters the list as you type.

### Type Filtering

The picker respects the `DynamicTextAssetType` UPROPERTY metadata to restrict which classes appear:

```cpp
// Only shows UWeaponData and subclasses in the picker
UPROPERTY(EditAnywhere, meta = (DynamicTextAssetType = "UWeaponData"))
FSGDynamicTextAssetRef WeaponRef;
```

The customization reads this metadata via `GetTypeFilterClass()` and filters entries accordingly. If no `DynamicTextAssetType` metadata is specified, the picker defaults to showing all `USGDynamicTextAsset` subclasses.

An additional **editor local class filter** dropdown appears at the top of the picker menu, allowing you to narrow the list to a specific class without affecting the serialized data. This dropdown shows "All Types" plus all registered instantiable classes matching the type filter.

### Action Buttons

| Button          | Action                                                                   |
|-----------------|--------------------------------------------------------------------------|
| **Open Editor** | Opens the [Dynamic Text Asset Editor](DynamicTextAssetEditor.md) for the referenced object (enabled only when the reference is valid) |
| **Copy ID**     | Copies the `FSGDynamicTextAssetId` string to the clipboard                     |
| **Clear**       | Resets the reference to invalid (empty ID)                               |

### Paste from Clipboard

The customization supports pasting a dynamic text asset reference from the clipboard. The paste logic auto detects the format of the clipboard text:

1. **GUID format**: If the text parses as a valid GUID, it is treated as an `FSGDynamicTextAssetId` and the matching dynamic text asset is looked up.
2. **UserFacingId format**: If the text does not parse as a GUID, it is matched against the UserFacingId of all available dynamic text assets.
3. If neither format matches, a warning is logged.

### Picker Entry Structure

Each entry in the picker internally stores:

```cpp
struct FPickerEntry
{
    FSGDynamicTextAssetId Id;
    FString UserFacingId;
    FString ClassName;
    FString FilePath;
};
```

### Registration

The customization is registered during editor module startup:

```cpp
PropertyModule.RegisterCustomPropertyTypeLayout(
    FSGDynamicTextAssetRef::StaticStruct()->GetFName(),
    FOnGetPropertyTypeCustomizationInstance::CreateStatic(
        &FSGDynamicTextAssetRefCustomization::MakeInstance));
```

## FSGDynamicTextAssetIdentityCustomization

A detail customization for `USGDynamicTextAsset` that enhances the display of identity fields in the Dynamic Text Asset Editor.

### Customized Fields

| Field             | Display                                             |
|-------------------|-----------------------------------------------------|
| **DynamicTextAssetId**  | Read only text with a copy to clipboard button      |
| **UserFacingId**  | Read only text with a copy to clipboard button      |
| **Version**       | Read only formatted text (`Major.Minor.Patch`) with a copy to clipboard button |

Copy buttons use the standard Unreal copy icon. The customization refreshes when the underlying data changes via `RefreshId()`, `RefreshUserFacingId()`, and `RefreshVersion()`.

### Registration

The identity customization is registered for the `USGDynamicTextAsset` class:

```cpp
PropertyModule.RegisterCustomClassLayout(
    USGDynamicTextAsset::StaticClass()->GetFName(),
    FOnGetDetailCustomizationInstance::CreateStatic(
        &FSGDynamicTextAssetIdentityCustomization::MakeInstance));
```

[Back to Table of Contents](../TableOfContents.md)
