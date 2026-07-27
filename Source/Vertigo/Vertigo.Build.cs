using UnrealBuildTool;

public class Vertigo : ModuleRules
{
	public Vertigo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// "Narrative" lets the SaveCoordinator drive the Narrative plugin's own Save/Load in C++.
		// "MoviePlayer" (+ Slate/UMG) drives the tunnel loading-screen transitions.
		PrivateDependencyModuleNames.AddRange(new string[] { "Narrative", "MoviePlayer", "Slate", "SlateCore", "UMG", "DeveloperSettings" });
	}
}
