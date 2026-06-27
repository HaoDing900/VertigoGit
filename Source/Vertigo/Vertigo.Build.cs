using UnrealBuildTool;

public class Vertigo : ModuleRules
{
	public Vertigo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// "Narrative" lets the SaveCoordinator drive the Narrative plugin's own Save/Load in C++.
		PrivateDependencyModuleNames.AddRange(new string[] { "Narrative" });
	}
}
