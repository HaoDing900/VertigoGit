using UnrealBuildTool;

public class VertigoEditor : ModuleRules
{
	public VertigoEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false; // Editor commandlets have independent file-local helpers.

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "UnrealEd", "PropertyEditor", "Vertigo", "Narrative", "ImageCore", "BlueprintGraph", "KismetCompiler", "EnhancedInput", "UMG", "UMGEditor", "MovieScene", "MovieSceneTracks", "Niagara", "AssetTools", "AssetRegistry", "AIModule", "RenderCore", "SlateRHIRenderer", "AnimGraph", "AnimGraphRuntime" });
	}
}
