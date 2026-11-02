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

/** 有目标条件的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeHasTargetConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

/**
 * 有目标判断：AIC 的 CurrentTarget 非空即通过。
 * 掠食者索敌漫游重咬目标（背刺/游走结束后目标还在视野记忆里 → 立即再绕后）。
 */
USTRUCT(Meta = (DisplayName = "有目标 (Has Target)", Category = "AI|Common"))
struct CYBERRUIN_API FStateTreeHasTargetCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeHasTargetConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** 被玩家察觉条件的实例数据 */USTRUCT()
struct CYBERRUIN_API FStateTreeIsSpottedConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** 玩家多近内角色面朝它即算"被察觉"(cm) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SpotRadius = 1200.f;

	/** 玩家视线半角(度)：以角色朝向为轴的视线锥内即被察觉（绑角色不绑相机） */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SpotHalfAngleDegrees = 60.f;

	/** 贴身锁定距离：玩家与怪距离小于此值时不判被察觉（背刺扑击发动后转身面对也拦不住，只有走位能躲）；0 = 不启用 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MinDistance = 0.f;
};

/**
 * 被玩家察觉判断（方案 §11.3 掠食者"被察觉"触发）：
 * 玩家在 SpotRadius 内、视线无遮挡、且怪处在**角色朝向**为轴的视线锥内 → 通过（绑角色不绑相机——
 * 第三人称里镜头回头看它不算"看见"，转身脸朝它才算，与背刺"背后 90° 扇形"判定源一致）。
 * 用于掠食者绕后途中被玩家转身看见 → 转"被发现"撤离。
 */
USTRUCT(Meta = (DisplayName = "被玩家察觉 (Is Spotted)", Category = "AI|Predator"))
struct CYBERRUIN_API FStateTreeIsSpottedCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeIsSpottedConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** 护盾已破条件的实例数据 */
USTRUCT()
struct CYBERRUIN_API FStateTreeIsShieldBrokenConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;
};

/**
 * 护盾已破判断（方案 §11.4 裁决者）：读 AIC 的 bShieldBroken（Gameplay 层破盾时写入）。
 * 用于裁决者守盾警戒 → 追杀的事件转换门槛：盾没破看见也不追。
 */
USTRUCT(Meta = (DisplayName = "护盾已破 (Is Shield Broken)", Category = "AI|Arbiter"))
struct CYBERRUIN_API FStateTreeIsShieldBrokenCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeIsShieldBrokenConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
