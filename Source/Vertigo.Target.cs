using UnrealBuildTool;

public class VertigoTarget : TargetRules
{
	public VertigoTarget(TargetInfo Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V3;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		Type = TargetType.Game;
		ExtraModuleNames.Add("Vertigo");
	}
}
