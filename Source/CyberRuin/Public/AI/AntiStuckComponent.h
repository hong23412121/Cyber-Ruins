#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AntiStuckComponent.generated.h"

/**
 * 卡死检测组件（全怪共用，框架无关）：按 tick 间隔采样 AI 位置，
 * 仅在"正在移动"时累计——位移持续低于阈值则 bStuck = true。
 * StateTree 在转换条件里读 bStuck（全状态最高优先 → 解卡状态）；
 * 审计官自建状态机、蜂群不依赖 StateTree，同样直接读本组件。
 * 挂载位置：怪 BP（BP_BaseEnemy 的子类）默认添加。
 */
UCLASS(ClassGroup = (赛博遗迹AI), Meta = (BlueprintSpawnableComponent))
class CYBERRUIN_API UAntiStuckComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAntiStuckComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 卡死判定结果：StateTree 转换条件 / 审计官状态机直接读 */
	UPROPERTY(BlueprintReadOnly, Category = "AntiStuck")
	bool bStuck = false;

	/** 每个采样周期要求的最小位移(cm)，低于即记作"没动"。慢速怪（如裁决者）可调低 */
	UPROPERTY(EditAnywhere, Category = "AntiStuck", Meta = (ClampMin = "0.0"))
	float MinMovePerSample = 10.f;

	/** 连续"没动"多久判定卡死 */
	UPROPERTY(EditAnywhere, Category = "AntiStuck", Meta = (ClampMin = "0.1"))
	float MaxStuckSeconds = 3.f;

private:
	FVector LastLocation = FVector::ZeroVector;
	float StuckTime = 0.f;
};
