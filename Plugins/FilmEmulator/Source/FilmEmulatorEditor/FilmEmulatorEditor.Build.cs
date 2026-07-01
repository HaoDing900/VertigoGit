// Copyright 2026 TOXIC STOCK All rights reserved.

using UnrealBuildTool;
using System.IO;

public class FilmEmulatorEditor : ModuleRules
{
    public FilmEmulatorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "FilmEmulator"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "InputCore",
            "UnrealEd",
            "AssetRegistry",
            "AssetTools",
            "WorkspaceMenuStructure",
            "PropertyEditor",
            "Projects"
        });

        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Editor/WorkspaceMenuStructure/Public"));
    }
}
