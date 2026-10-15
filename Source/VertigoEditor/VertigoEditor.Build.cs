using UnrealBuildTool;

public class VertigoEditor : ModuleRules
{
	public VertigoEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false; // Editor commandlets have independent file-local helpers.

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.Add("Sequencer");
        PrivateDependencyModuleNames.Add("Json");
        PrivateDependencyModuleNames.Add("ContentBrowser");
        PrivateDependencyModuleNames.Add("InventorySystemX");
        PrivateDependencyModuleNames.Add("NarrativeDialogueEditor");
        PrivateDependencyModuleNames.Add("NarrativeQuestEditor");
		PrivateDependencyModuleNames.AddRange(new string[] { "CommonUI", "CommonInput", "GameplayTags", "NarrativeCommonUI", "InputCore", "Slate", "SlateCore", "UnrealEd", "PropertyEditor", "Vertigo", "Narrative", "ImageCore", "BlueprintGraph", "KismetCompiler", "EnhancedInput", "UMG", "UMGEditor", "MovieScene", "LevelSequence", "MovieSceneTracks", "Niagara", "AssetTools", "AssetRegistry", "AIModule", "RenderCore", "SlateRHIRenderer", "AnimGraph", "AnimGraphRuntime" });
	}
}
