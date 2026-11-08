// 《赛博遗迹》主模块：怪物 AI 寻路件所在模块（详见 Docs/赛博遗迹_怪物寻路方案_v1.md）
using UnrealBuildTool;

public class CyberRuin : ModuleRules
{
	public CyberRuin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",              // AAIController / CrowdFollowing / AIPerception / EPathFollowingStatus
			"NavigationSystem",      // UNavigationSystemV1 / FNavLocation
			"GameplayTags",          // 原生 GameplayTag（Event.Enemy.Seen/Lost）
			"StateTreeModule",       // UStateTree 资产类型 / FStateTreeTaskBase / FStateTreeConditionBase
			"GameplayStateTreeModule" // UStateTreeAIComponent / FStateTreeAITaskBase / 引擎 MoveTo 任务
		});
	}
}
