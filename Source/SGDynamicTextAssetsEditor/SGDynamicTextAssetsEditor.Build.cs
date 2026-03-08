// Copyright Start Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SGDynamicTextAssetsEditor : ModuleRules
{
    public SGDynamicTextAssetsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "SGDynamicTextAssetsRuntime"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "DeveloperSettings",
            "DeveloperToolSettings",
            "UnrealEd",
            "Slate",
            "SlateCore",
            "EditorStyle",
            "EditorSubsystem",
            "PropertyEditor",
            "AssetRegistry",
            "SourceControl",
            "InputCore",
            "ToolMenus",
            "Json",
            "JsonUtilities",
            "LevelEditor",
            "WorkspaceMenuStructure",
            "ToolWidgets"
        });
    }
}
