using UnrealBuildTool;

public class VertigoEditor : ModuleRules
{
	public VertigoEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "UnrealEd", "PropertyEditor" });
	}
}
