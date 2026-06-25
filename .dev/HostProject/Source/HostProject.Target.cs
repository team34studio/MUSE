using UnrealBuildTool;
using System.Collections.Generic;

public class HostProjectTarget : TargetRules
{
	public HostProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HostProject");
	}
}
