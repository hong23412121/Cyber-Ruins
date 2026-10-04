// 《赛博遗迹》编辑器目标
using UnrealBuildTool;
using System.Collections.Generic;

public class CyberRuinEditorTarget : TargetRules
{
	public CyberRuinEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CyberRuin");
	}
}
