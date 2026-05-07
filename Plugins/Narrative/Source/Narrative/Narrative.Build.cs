// Copyright Narrative Tools 2022.

using UnrealBuildTool;

public class Narrative : ModuleRules
{
    public Narrative(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;

        PublicIncludePaths.AddRange(new string[]
        {
            // Add public include paths here if needed
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            // Add private include paths here if needed
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Paper2D",             // ✅ Needed for PaperSprite support
            "LevelSequence",
            "MovieScene",
            "CinematicCamera",
            "AssetRegistry",
            "AnimationCore",
            "AnimGraphRuntime"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Engine",
            "CoreUObject",
            "UMG",
            "Slate",
            "SlateCore",
            "LevelSequence",
            "TraceLog"
        });

        DynamicallyLoadedModuleNames.AddRange(new string[]
        {
            // Add dynamically loaded modules here if needed
        });
    }
}