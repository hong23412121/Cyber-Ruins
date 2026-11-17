#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeSentinelTasks.generated.h"

class AAIController;
class AActor;

/** 取巡逻点任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeGetPatrolPointTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文（同引擎 MoveTo 任务） */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 输出：当前巡逻点（属性绑定喂给移动任务） */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> OutPoint = nullptr;
};

/**
 * 取巡逻点（方案 §五）：从怪的 PatrolRouteComponent 拿当前巡逻点写到输出，
 * 同时推进索引——不可达的坏点会在下一轮被自动跳过（巡逻自愈）。
 */
USTRUCT(Meta = (DisplayName = "取巡逻点 (Get Patrol Point)", Category = "AI|Patrol"))
struct CYBERRUIN_API FStateTreeGetPatrolPointTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeGetPatrolPointTaskInstanceData;

	FStateTreeGetPatrolPointTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** 巡逻移动任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreePatrolMoveTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 到达判定半径(cm) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 60.f;
};

/**
 * 巡逻移动（方案 §五）：任务内自循环——到达或走不通时自动取下一个巡逻点继续走，
 * 永不完成（状态保持 Running，只被事件/卡死转换切走）。坏点自动跳过，巡逻自愈。
 * 自包含零绑定：不依赖跨状态属性绑定。
 */
USTRUCT(Meta = (DisplayName = "巡逻移动 (Patrol Move)", Category = "AI|Patrol"))
struct CYBERRUIN_API FStateTreePatrolMoveTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePatrolMoveTaskInstanceData;

	FStateTreePatrolMoveTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** 追击目标任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeChaseTargetTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 到达判定半径(cm) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 120.f;

	/** 运行期：当前正在追的目标（换了目标自动重新寻路） */
	UPROPERTY(Transient)
	TObjectPtr<AActor> LastTarget = nullptr;
};

/**
 * 追击目标（方案 §五）：直接读 AIC 的 CurrentTarget（感知写入，见 ACyberEnemyAIController），
 * 目标变化或走完/走不通时自动重新寻路。目标丢失返回 Failed（由转换兜回巡逻）。
 * 自包含零绑定。
 */
USTRUCT(Meta = (DisplayName = "追击目标 (Chase Target)", Category = "AI|Patrol"))
struct CYBERRUIN_API FStateTreeChaseTargetTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeChaseTargetTaskInstanceData;

	FStateTreeChaseTargetTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** 解卡挪窝任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeWarpUnstuckTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 搜索半径(cm)：掠食者/审计官用小半径防大位移穿帮 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SearchRadius = 200.f;
};

/**
 * 解卡挪窝（方案 §四）：调 WarpToNearestNavigable 把怪挪回最近的导航网格点。
 * 挪窝失败返回 Running：停留本状态下一 tick 再试，不大位移穿帮。
 */
USTRUCT(Meta = (DisplayName = "解卡挪窝 (Warp Unstuck)", Category = "AI|Patrol"))
struct CYBERRUIN_API FStateTreeWarpUnstuckTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeWarpUnstuckTaskInstanceData;

	FStateTreeWarpUnstuckTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** 待机任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeIdleTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 原地扫描角速度(度/秒)：0 = 完全站桩不转 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ScanDegreesPerSecond = 60.f;
};

/**
 * 原地待机：停移动、原地慢速扫描（等视野锥扫到目标），保持 Running 等事件转换切走。
 * 审计官移动桩的"索敌待机"态用（§九：审计官行为不进 StateTree，此处仅测试其感知→移动链路）。
 */
USTRUCT(Meta = (DisplayName = "待机 (Idle)", Category = "AI|Common"))
struct CYBERRUIN_API FStateTreeIdleTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeIdleTaskInstanceData;

	FStateTreeIdleTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** 近战挥击任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeMeleeAttackTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 命中判定距离(cm)：出招瞬间目标不超出此距离才算命中（主角方距离击中判定的怪侧来源） */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float HitRange = 220.f;

	/** 出招延迟(s)：进入攻击后隔多久做命中判定（对上动画打击点） */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float StrikeDelay = 0.35f;

	/** 运行期：是否已做过命中判定 */
	UPROPERTY(Transient)
	bool bStruck = false;

	/** 运行期：命中判定倒计时 */
	UPROPERTY(Transient)
	float StrikeTimer = 0.f;
};

/**
 * 近战挥击（方案 §五 哨兵攻击 / §九 审计官攻击）：贴身出招——停移动、面向目标、
 * 播主行动动画（PlayPrimaryAction，播完 AIC Tick 自动恢复跑动）；StrikeDelay 后按 HitRange 做距离
 * 命中判定（命中只打日志，玩家侧受伤由主角组接）；动作播完 Succeeded 回追击。
 * 出招时写 AIC->LastAttackTime（追击→攻击的 AttackReady 冷却条件读它，冷却内不再进攻击态）。
 */
USTRUCT(Meta = (DisplayName = "近战挥击 (Melee Attack)", Category = "AI|Combat"))
struct CYBERRUIN_API FStateTreeMeleeAttackTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeMeleeAttackTaskInstanceData;

	FStateTreeMeleeAttackTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
