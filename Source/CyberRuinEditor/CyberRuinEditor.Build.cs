// 《赛博遗迹》编辑器模块：怪物 AI 资产程序化构建工具
using UnrealBuildTool;

public class CyberRuinEditor : ModuleRules
{
	public CyberRuinEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",              // 编辑器工具
			"Kismet",                 // FKismetEditorUtilities::CreateBlueprint（程序化建 BP）
			"AIModule",
			"GameplayTags",
			"StateTreeModule",
			"StateTreeEditorModule",  // UStateTreeEditorData / FStateTreeCompiler（程序化建 ST）
			"GameplayStateTreeModule",
			"PropertyBindingUtils",   // FPropertyBindingPath（任务参数绑定）
			"PropertyBindingUtilsEditor",
			"CyberRuin"               // 运行时模块（任务/条件/AIC 基类定义）
		});
	}
}
