#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeArbiterTasks.generated.h"

class AAIController;
class AActor;

/** 守盾警戒任务的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeArbiterGuardTaskInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 离岗超过此距离走回岗位点(cm) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ReturnDistance = 500.f;

	/** 开火距离(cm)：目标可见且更近则视为在射程内 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FireRange = 1200.f;

	/** 调试：持续可见多少秒自动破盾（真破盾事件由 Gameplay 层接手后可关） */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float DebugBreakSeconds = 6.f;

	/** 运行期：开火日志节流 */
	UPROPERTY(Transient)
	float LastFireLogTime = -1.f;

	/** 运行期：目标连续可见累计（调试自动破盾用） */
	UPROPERTY(Transient)
	float VisibleAccum = 0.f;
};

/**
 * 守盾警戒（方案 §11.4 裁决者默认态）：站在岗位上，目标可见就面朝并在射程内"开火"（测试期打日志）；
 * 盾未破永远不主动追（追杀转换由 IsShieldBroken 条件门槛控制）；
 * 离岗超距走回岗位点；调试模式连续贴近可见 DebugBreakSeconds 秒自动破盾，形成"守盾→追杀→回岗"可观测循环。
 */
USTRUCT(Meta = (DisplayName = "守盾警戒 (Arbiter Guard)", Category = "AI|Arbiter"))
struct CYBERRUIN_API FStateTreeArbiterGuardTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeArbiterGuardTaskInstanceData;

	FStateTreeArbiterGuardTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
