using UnrealBuildTool;

public class Vertigo : ModuleRules
{
	public Vertigo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UMG", "Narrative", "InventorySystemX", "CommonUI", "NarrativeCommonUI" });

		// "Narrative" lets the SaveCoordinator drive the Narrative plugin's own Save/Load in C++.
		// "InventorySystemX" lets it save the player's inventory (FInventorySaveData is in its public headers).
		// "MoviePlayer" (+ Slate/UMG) drives the tunnel loading-screen transitions.
		PrivateDependencyModuleNames.AddRange(new string[] { "Narrative", "MoviePlayer", "Slate", "SlateCore", "UMG", "DeveloperSettings", "InputCore", "LevelSequence", "MovieScene", "Niagara", "PhysicsCore", "AudioMixer" });
	}
}
