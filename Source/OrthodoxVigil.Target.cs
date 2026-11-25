using UnrealBuildTool;
public class OrthodoxVigilTarget : TargetRules
{
	public OrthodoxVigilTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("OrthodoxVigil");
	}
}
