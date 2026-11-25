using UnrealBuildTool;
public class OrthodoxVigilEditorTarget : TargetRules
{
	public OrthodoxVigilEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("OrthodoxVigil");
	}
}
