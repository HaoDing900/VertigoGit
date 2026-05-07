/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

using UnrealBuildTool;

public class InventorySXEditor : ModuleRules
{
	public InventorySXEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { });

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// PrivateDependencyModuleNames.AddRange(new string[] { "InventorySystemX" });
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"ContentBrowser",
			"InventorySystemX",
			"Blutility",
			"Slate",
			"SlateCore",
			"AssetTools"
		});

		PublicIncludePaths.AddRange(
			new string[]
			{
				ModuleDirectory + "/Public",
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
				ModuleDirectory + "/Private",
				ModuleDirectory + "/Private/Actions",
				ModuleDirectory + "/Private/Factories",
				ModuleDirectory + "/Private/Tasks",
				ModuleDirectory + "/Private/ThumbnailRender",
			}
		);
	}
}