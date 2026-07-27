using UnrealBuildTool;

public class TextureBatchTools : ModuleRules
{
	public TextureBatchTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",       // editor
			"ContentBrowser", // right-click menu extender
			"AssetRegistry",  // FAssetData
			"Slate",
			"SlateCore"
		});
	}
}
