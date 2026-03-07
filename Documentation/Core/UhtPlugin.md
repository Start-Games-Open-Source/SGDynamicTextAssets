# UHT Plugin

[Back to Table of Contents](../TableOfContents.md)

## Overview

The SG Dynamic Text Assets plugin includes a **UHT exporter plugin**, a C# module that runs during the Unreal Header Tool phase of the build process. Its purpose is to enforce the [hard reference prevention rule](Validation.md#hard-reference-prevention) at compile time, ensuring `USGDynamicTextAsset` subclasses never contain hard reference UPROPERTYs.

Unlike runtime validation (which catches issues during editor saves or CI commandlets), the UHT plugin catches violations **before C++ compilation begins**, making it impossible for hard references to reach serialization or packaging.

## File Structure

All UHT plugin files live in a dedicated source directory:

```
Source/SGDynamicTextAssetsUhtPlugin/
    SGDynamicTextAssetHardRefValidator.cs        # Validation logic (required)
    SGDynamicTextAssetsUhtPlugin.ubtplugin.csproj  # MSBuild project (required)
    obj/                                    # Build artifacts (gitignored)
```

| File | Required | Description |
|------|----------|-------------|
| `SGDynamicTextAssetHardRefValidator.cs` | Yes | All validation logic. Contains the `[UhtExporter]` entry point and the `Validator` class that walks UHT type trees and checks properties. |
| `SGDynamicTextAssetsUhtPlugin.ubtplugin.csproj` | Yes | MSBuild project targeting .NET 8.0. References engine assemblies (`EpicGames.UHT`, `EpicGames.Build`, `EpicGames.Core`, `UnrealBuildTool`) via the `$(EngineDir)` MSBuild property. |
| `SGDynamicTextAssetsUhtPlugin.ubtplugin.csproj.props` | No | Optional IDE fallback that provides a hardcoded `$(EngineDir)` value. Only needed for manual C# compilation in an IDE. See [Creating .csproj.props for IDE Support](#optional-creating-csprojprops-for-ide-support). |
| `obj/` | No | Auto-generated .NET build artifacts. Should be in `.gitignore`. |

## How UBT Discovers the Plugin

Unreal Build Tool (UBT) **automatically discovers** UHT exporter plugins by scanning for files matching the `*.ubtplugin.csproj` naming convention. No entry in `.uplugin` or any build configuration is required.

The discovery process:

1. UBT scans the project's `Source/` directories (including plugin source directories) for any `*.ubtplugin.csproj` files.
2. It compiles the C# project using .NET 8.0.
3. The compiled assembly is loaded and scanned for `[UhtExporter]`-decorated methods.
4. The `ModuleName` parameter in the `[UhtExporter]` attribute ties the exporter to a specific Unreal module:

```csharp
[UhtExporter(
    Name = "SGDynamicTextAssetHardRefValidator",
    ModuleName = "SGDynamicTextAssetsRuntime",
    ...)]
```

This means the validator runs whenever `SGDynamicTextAssetsRuntime` (or any module that depends on it) is compiled.

## Engine Path Resolution

The `.csproj` references engine assemblies using the `$(EngineDir)` MSBuild property:

```xml
<Reference Include="EpicGames.UHT">
    <HintPath>$(EngineDir)\Binaries\DotNET\UnrealBuildTool\EpicGames.UHT.dll</HintPath>
</Reference>
```

**How `$(EngineDir)` is provided:**

| Context | How EngineDir Is Set |
|---------|---------------------|
| UBT builds (normal workflow) | UBT passes `/p:EngineDir=<path>` as an MSBuild property automatically. No developer action needed. |
| Manual IDE compilation | Developer must provide `$(EngineDir)` via a `.csproj.props` file or IDE-level MSBuild properties. |

The `.csproj` includes a **conditional import** for the optional `.csproj.props` file:

```xml
<Import Project="SGDynamicTextAssetsUhtPlugin.ubtplugin.csproj.props"
        Condition="Exists('SGDynamicTextAssetsUhtPlugin.ubtplugin.csproj.props')" />
```

The `Condition="Exists(...)"` ensures the build does not fail if the file is absent. Since UBT always provides `$(EngineDir)` during normal builds, the `.csproj.props` fallback is only relevant for IDE-based C# compilation.

## Team and Plugin Distribution

| Scenario | EngineDir Resolution | Developer Action |
|----------|---------------------|-----------------|
| **Team (same engine version)** | UBT provides `$(EngineDir)` automatically during builds | None. The plugin works out of the box for all team members. |
| **Team (IDE C# editing)** | Not provided by default in IDE | Each developer who wants IDE IntelliSense for the C# code creates a local `.csproj.props` pointing to their engine installation. See template below. |
| **External plugin distribution** | UBT provides `$(EngineDir)` automatically during builds | None for normal builds. Document that recipients can create `.csproj.props` if they want IDE support. |

**Key insight:** For normal builds through UBT (which is the standard workflow), the plugin works automatically everywhere regardless of engine installation path. No per-machine configuration is needed.

## Optional: Creating .csproj.props for IDE Support

If you need C# IntelliSense or want to compile the UHT plugin manually from an IDE (Visual Studio, Rider, etc.), create a `.csproj.props` file next to the `.csproj`:

**File:** `Source/SGDynamicTextAssetsUhtPlugin/SGDynamicTextAssetsUhtPlugin.ubtplugin.csproj.props`

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="Current" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
    <PropertyGroup>
        <EngineDir Condition="'$(EngineDir)' == ''">C:\Program Files\Epic Games\UE_5.6\Engine</EngineDir>
    </PropertyGroup>
</Project>
```

Replace the path with your local Unreal Engine installation's `Engine` directory (e.g., `D:\Epic Games\UE_5.6\Engine`).

**Notes:**

- The `Condition="'$(EngineDir)' == ''"` ensures this only sets `EngineDir` when it has not already been provided (e.g., by UBT).
- This file is **machine-specific** and should be in `.gitignore` since each developer's engine path may differ.
- The `.csproj` already has `Condition="Exists(...)"` on the import, so the build works correctly whether or not this file exists.

## Build Artifacts

The `obj/` directory inside `Source/SGDynamicTextAssetsUhtPlugin/` contains auto-generated .NET build artifacts. This directory is created during compilation and should be added to `.gitignore`:

```
# UHT plugin build artifacts
Source/SGDynamicTextAssetsUhtPlugin/obj/
```

The compiled plugin assembly is output to `Binaries/DotNET/UnrealBuildTool/Plugins/SGDynamicTextAssetsUhtPlugin/` as configured in the `.csproj` `OutputPath` property.

[Back to Table of Contents](../TableOfContents.md)
