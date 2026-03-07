// Copyright Start Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SGDynamicTextAssetsRuntime : ModuleRules
{
    public SGDynamicTextAssetsRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // By default USGDynamicTextAssetStatics::LogRegisteredSerializers is disabled in Shipping builds.
        // Projects can opt-in by defining SG_LOG_SERIALIZERS_SHIPPING=1 in your own Build.cs
        // before/after depending on this module.
        //
        // To enable in your project's Build.cs:
        // PublicDefinitions.Add("SG_DYNAMIC_TEXT_ASSETS_LOG_SERIALIZERS_SHIPPING=1");
        PublicDefinitions.Add("SG_DYNAMIC_TEXT_ASSETS_LOG_SERIALIZERS_SHIPPING=0");

        // fkYAML: header-only YAML library (MIT license, no linking required)
        PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "..", "ThirdParty", "fkYAML", "include"));

        // Enable C++ exceptions for fkYAML library usage
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "DeveloperSettings",
            "Engine",
            "Json",
            "JsonUtilities",
            "XmlParser",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "SlateCore",
        });
    }
}
