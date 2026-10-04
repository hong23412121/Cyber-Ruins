#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PatrolRouteComponent.generated.h"

class AActor;

/**
 * 哨兵巡逻路线：点位数组（编辑器里拖 TargetPoint 进列表）+ 往返/循环两种走法。
 * StateTree 巡逻状态任务调用：GetCurrentPoint 取点 → MoveToActor → 到达后 Advance。
 * 点位为空时可选自动抓取半径内的 TargetPoint 当巡逻点（摆点即用，无需拖引用）。
 * 摆放规则（方案 §3.4）：巡逻点必须落在导航网格上，两两间距 4~12m。
 */
UCLASS(ClassGroup = (CyberRelicAI), Meta = (BlueprintSpawnableComponent))
class CYBERRUIN_API UPatrolRouteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPatrolRouteComponent();

	virtual void BeginPlay() override;

	/** 当前巡逻点，无路线返回 nullptr */
	UFUNCTION(BlueprintCallable, Category = "巡逻")
	AActor* GetCurrentPoint() const;

	/** 推进到下一个巡逻点；往返模式在两端折返（跳过刚走过的端点，不原地空走） */
	UFUNCTION(BlueprintCallable, Category = "巡逻")
	void Advance();

	UPROPERTY(EditAnywhere, Category = "巡逻")
	TArray<TObjectPtr<AActor>> Points;

	/** true=走到头折返（走廊怪）；false=循环（大厅怪） */
	UPROPERTY(EditAnywhere, Category = "巡逻")
	bool bPingPong = true;

	/** Points 为空时，BeginPlay 自动抓取半径内 TargetPoint（按名字排序）当巡逻点 */
	UPROPERTY(EditAnywhere, Category = "巡逻")
	bool bAutoCollectNearbyPoints = true;

	/** 自动抓取半径(cm) */
	UPROPERTY(EditAnywhere, Category = "巡逻", Meta = (ClampMin = "100"))
	float AutoCollectRadius = 2500.f;

private:
	int32 Index = 0;
	bool bGoingUp = true;
};
