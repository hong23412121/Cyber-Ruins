#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CyberRuinBuildAssetsCommandlet.generated.h"

/**
 * 怪物 AI 资产程序化构建命令（方案 §11 落地）：
 *   UnrealEditor-Cmd.exe CyberRuin.uproject -run=CyberRuinBuildAssets -unattended -nullrhi
 * 产出（全部在 /Game/XuTang/，重复执行幂等覆盖）：
 *   1. ST_Sentinel + BP_AIC_Sentinel + BP_Enemy_Sentinel —— 哨兵（巡逻/追击/解卡，方案 §11.2）
 *   2. ST_Predator + BP_AIC_Predator + BP_Enemy_Predator —— 掠食者（索敌漫游/绕后/背刺/被发现，§11.3）
 *   3. ST_Arbiter + BP_AIC_Arbiter + BP_Enemy_Arbiter —— 裁决者（守盾警戒/追杀/解卡，§11.4，调试自动破盾）
 *   4. ST_AuditorStub + BP_AIC_Auditor + BP_Enemy_Auditor —— 审计官移动测试桩（§九，索敌待机/追击/解卡）
 *   5. BP_Drone + BP_SwarmHive —— 蜂群（DroneClass 赋值 + 无人机可见网格体，§七）
 *   6. BS_Sentinel_Patrol / BS_Predator_Prowl / BS_Arbiter_Plod / BS_Auditor_Stride —— 程序生成"纯走"混合空间（§十四 步态差异化），
 *      四怪怪壳统一切单节点模式由 AIC Tick 驱动（ABP_Unarmed 对 AI Pawn 不产步行姿态，实机发现后弃用，正式 AnimBP 到位后由模型侧接管）
 */
/**
 * 单怪灰盒表现包（步态差异化）：跑动混合空间 + 播放速率 + 主行动/受击动画。
 * LocomotionBlendSpace 为空 = 走 ABP_Unarmed 路线（哨兵），非空 = 怪壳切单节点模式。
 */
struct FMonsterVisuals
{
	TObjectPtr<class UBlendSpace> LocomotionBlendSpace;
	float LocomotionPlayRate = 1.f;
	TObjectPtr<class UAnimSequence> PrimaryActionAnim;
	TObjectPtr<class UAnimSequence> HitReactAnim;
};

UCLASS()
class UCyberRuinBuildAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCyberRuinBuildAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	/** 建 ST_Sentinel 资产（巡逻/追击/解卡，自包含任务零绑定），返回资产指针 */
	class UStateTree* BuildSentinelStateTree();

	/** 建 ST_Predator 资产（索敌漫游/绕后接近/背刺判定/被发现游走/解卡，§11.3） */
	class UStateTree* BuildPredatorStateTree();

	/** 建 ST_Arbiter 资产（守盾警戒/追杀/解卡，§11.4：盾不破不追、追杀离家 30m 折返） */
	class UStateTree* BuildArbiterStateTree();

	/** 建 ST_AuditorStub 资产（索敌待机/追击/解卡——测试审计官感知→移动链路的最小行为壳） */
	class UStateTree* BuildAuditorStubStateTree();

	/**
	 * 建怪物 AIC 蓝图（ACyberEnemyAIController 子类，指定 StateTree 资产 + 灰盒表现包；裁决者开调试自动破盾）。
	 * Visuals.LocomotionBlendSpace 非空时 AIC Tick 按速度驱动单节点混合；为空则不驱动（哨兵走 ABP）。
	 */
	class UBlueprint* BuildEnemyAIC(const FString& AssetName, class UStateTree* InStateTree, bool bDebugAutoBreakShield, const FMonsterVisuals& Visuals);

	/**
	 * 建怪壳蓝图（BP_BaseEnemy 子类 + AntiStuck 组件 + 可选 PatrolRoute + 指定 AIC + 烘焙 Manny 网格体）。
	 * MaxWalkSpeed > 0 时覆盖移速（掠食者 420）。
	 * LocomotionBlendSpace 非空时网格体切单节点模式跑该混合空间（步态差异化），为空保持 ABP_Unarmed。
	 */
	class UBlueprint* BuildEnemyPawn(class UBlueprint* InAICBlueprint, const FString& AssetName, float MaxWalkSpeed, bool bAddPatrolRoute, const FString& LabelText = FString(), const FColor& LabelColor = FColor::White, float MeshScale = 1.f, class UBlendSpace* LocomotionBlendSpace = nullptr);

	/**
	 * 程序生成"纯走"1D 混合空间：MF_Unarmed_Walk_Fwd 拉伸铺满 [0, MaxSpeed]（越界速度被钳制），
	 * 零速回落 MM_Idle。全程不出现跑步——小怪整体步行（方案 §十四）。
	 */
	class UBlendSpace1D* BuildWalkOnlyBlendSpace(const FString& AssetName, float MaxSpeed);

	/** 载入小白人动画并校验与 SKM_Manny_Simple 同骨骼（不匹配/找不到返回空，AIC 拿到空即不播该动作） */
	class UAnimSequence* LoadMannequinAnim(const FString& AssetPath);

	/** 建 BP_Drone：ASwarmDrone 子类，CDO 烘焙引擎立方体网格体（无人机可见） */
	class UBlueprint* BuildSwarmDroneBlueprint();

	/** 建 BP_SwarmHive：ASwarmHive 子类，DroneClass 赋 BP_Drone（C++ 缺省为空，必须显式指定） */
	class UBlueprint* BuildSwarmHiveBlueprint(class UBlueprint* InDroneBlueprint);
};
