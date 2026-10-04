#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEnemyConditions.generated.h"

class AAIController;

/** 卡死判断条件的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeIsStuckConditionInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

/**
 * 卡死判断（方案 §四）：直接读怪身上 AntiStuckComponent 的 bStuck。
 * 用于所有状态的最高优先转换（OnTick → 解卡状态）。
 */
USTRUCT(Meta = (DisplayName = "卡死判断 (Is Stuck)", Category = "AI|Patrol"))
struct CYBERRUIN_API FStateTreeIsStuckCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeIsStuckConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** 离家过远条件的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeIsFarFromHomeConditionInstanceData
{
	GENERATED_BODY()

	/** 由 AI Schema 自动绑定的上下文 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 距离阈值(cm)：哨兵 1500，裁决者资产里改 3000 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxDistance = 1500.f;
};

/**
 * 离家过远判断（方案 §五 leash）：怪距出生点超过 MaxDistance 即通过。
 * 用于追击状态转回巡逻（哨兵教学定位，不追远）。
 */
USTRUCT(Meta = (DisplayName = "离家过远 (Is Far From Home)", Category = "AI|Patrol"))
struct CYBERRUIN_API FStateTreeIsFarFromHomeCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeIsFarFromHomeConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
