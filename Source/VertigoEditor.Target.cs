using UnrealBuildTool;

public class VertigoEditorTarget : TargetRules
{
	public VertigoEditorTarget(TargetInfo Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V3;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		Type = TargetType.Editor;
		ExtraModuleNames.Add("Vertigo");
		ExtraModuleNames.Add("VertigoEditor");
	}
}
