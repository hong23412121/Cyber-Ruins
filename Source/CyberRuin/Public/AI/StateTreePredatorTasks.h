#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreePredatorTasks.generated.h"

class AAIController;

/** 索敌漫游任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreePredatorRoamTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 漫游环带内径/外径(cm)：绕出生点随机可达点 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RoamInnerRadius = 600.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RoamOuterRadius = 1000.f;

	/** 运行期：上次发 MoveTo 的时刻（坏点重选节流） */
	UPROPERTY(Transient)
	float LastRequestTime = -1.f;
};

/**
 * 索敌漫游（方案 §11.3 掠食者默认态）：绕出生点环带随机可达点永续游走，等感知看见玩家。
 * 任务内自循环：到达/走不通冷却半秒后取下一个点，永不主动完成。
 */
USTRUCT(Meta = (DisplayName = "索敌漫游 (Predator Roam)", Category = "AI|Predator"))
struct CYBERRUIN_API FStateTreePredatorRoamTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePredatorRoamTaskInstanceData;

	FStateTreePredatorRoamTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** 绕后接近任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreePredatorFlankTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 绕后距离(cm)，方案 §12.1 初值 450 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FlankDistance = 450.f;

	/** 背后取样半角(度)：取样范围 = 正背后 ± 此值（中心向外优先，见 GetFlankPoint） */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SampleArcDegrees = 60.f;

	/** 到点判定半径(cm) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 160.f;

	/** 重寻路间隔(秒)：目标移动时绕后点跟着刷新 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RepathInterval = 0.5f;

	/** 运行期：当前绕后点与上次刷新时刻 */
	UPROPERTY(Transient)
	FVector FlankPoint = FVector::ZeroVector;

	UPROPERTY(Transient)
	float LastRepathTime = -1.f;
};

/**
 * 绕后接近（方案 §11.3）：GetFlankPoint 取目标背后扇形点 → MoveTo，按间隔刷新跟随目标；
 * 到点 → Succeeded（转背刺判定）；取点失败/目标丢失 → Failed（回索敌漫游）。
 */
USTRUCT(Meta = (DisplayName = "绕后接近 (Predator Flank)", Category = "AI|Predator"))
struct CYBERRUIN_API FStateTreePredatorFlankTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePredatorFlankTaskInstanceData;

	FStateTreePredatorFlankTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** 背刺判定任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreePredatorBackstabTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 背刺距离上限(cm)，方案 §11.3 初值 160 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float BackstabRange = 160.f;
};

/**
 * 贴身背刺（方案 §11.3）：从绕后点（目标背后 450cm）直冲目标，贴到跟前后判背后 90° 扇形
 * → 命中 Succeeded（背刺大伤害由 Gameplay 层后续挂钩，测试期打日志）；未达条件 Failed（回绕后接近再绕）。
 * 被玩家察觉可被 IsSpotted 转换打断（贴近途中玩家扭头看见）。
 */
USTRUCT(Meta = (DisplayName = "贴身背刺 (Predator Backstab)", Category = "AI|Predator"))
struct CYBERRUIN_API FStateTreePredatorBackstabTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePredatorBackstabTaskInstanceData;

	FStateTreePredatorBackstabTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** 被发现游走任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreePredatorSpottedTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 游走时长(秒)：方案 §11.3 初值 3 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WanderSeconds = 3.f;

	/** 环玩家游走半径(cm) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WanderRadius = 400.f;

	/** 运行期：已游走时长与上次取点时刻 */
	UPROPERTY(Transient)
	float Elapsed = 0.f;

	UPROPERTY(Transient)
	float LastRequestTime = -1.f;
};

/**
 * 被发现游走（方案 §11.3 被发现态）：绕玩家环带游走 WanderSeconds 秒后撤离回索敌漫游；
 * 期间目标丢失 → Failed 兜底。
 */
USTRUCT(Meta = (DisplayName = "被发现游走 (Predator Spotted)", Category = "AI|Predator"))
struct CYBERRUIN_API FStateTreePredatorSpottedTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePredatorSpottedTaskInstanceData;

	FStateTreePredatorSpottedTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
