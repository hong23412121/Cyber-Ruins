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
