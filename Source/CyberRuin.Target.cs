// 《赛博遗迹》游戏目标
using UnrealBuildTool;
using System.Collections.Generic;

public class CyberRuinTarget : TargetRules
{
	public CyberRuinTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CyberRuin");
	}
}
