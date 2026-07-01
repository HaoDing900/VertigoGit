// Copyright 2026 TOXIC STOCK All rights reserved.

using UnrealBuildTool;
using System.IO;

public class FilmEmulator : ModuleRules
{
    public FilmEmulator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/FilmEmulatorPCH.h";

        PrivateIncludePaths.AddRange(new string[]
        {
            // UE 5.3 Renderer only has Private/ and Public/ (no Internal/ folder; that arrived in 5.4).
            Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private"),
            Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Public"),
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore",
            "RHI",
            "Renderer",
            "Projects",
            "ImageWrapper",
            "ImageCore",
            "Json",
            "JsonUtilities",
            "AssetRegistry"
        });


        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd"
            });
        }

        string ShaderDirectory = Path.Combine(ModuleDirectory, "../../Shaders");
        if (Directory.Exists(ShaderDirectory))
        {
            RuntimeDependencies.Add(Path.Combine(ShaderDirectory, "FilmEmulatorBase.usf"));
            RuntimeDependencies.Add(Path.Combine(ShaderDirectory, "FilmEmulatorColor.usf"));
            RuntimeDependencies.Add(Path.Combine(ShaderDirectory, "FilmEmulatorGrain.usf"));
            RuntimeDependencies.Add(Path.Combine(ShaderDirectory, "FilmEmulatorScratch.usf"));
            RuntimeDependencies.Add(Path.Combine(ShaderDirectory, "FilmEmulatorDirt.usf"));
            RuntimeDependencies.Add(Path.Combine(ShaderDirectory, "FilmEmulatorHalation.usf"));
            RuntimeDependencies.Add(Path.Combine(ShaderDirectory, "FilmEmulatorPrint.usf"));
        }

        string LutDirectory = Path.Combine(ModuleDirectory, "../../Content/LUTs");
        if (!Directory.Exists(LutDirectory))
        {
            LutDirectory = Path.Combine(ModuleDirectory, "../../Resources/LUTs");
        }

        if (Directory.Exists(LutDirectory))
        {
            foreach (string LutFile in Directory.GetFiles(LutDirectory, "*.cube", SearchOption.TopDirectoryOnly))
            {
                RuntimeDependencies.Add(LutFile);
            }
        }

        string PresetDirectory = Path.Combine(ModuleDirectory, "../../Content/Presets");
        if (Directory.Exists(PresetDirectory))
        {
            foreach (string PresetFile in Directory.GetFiles(PresetDirectory, "*.json", SearchOption.TopDirectoryOnly))
            {
                RuntimeDependencies.Add(PresetFile);
            }
        }
    }
}
