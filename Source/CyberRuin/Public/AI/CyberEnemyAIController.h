#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CyberEnemyAIController.generated.h"

class UStateTree;
class UStateTreeAIComponent;

/**
 * 怪物 AIController C++ 基类（方案 §3.3）：
 * - 默认挂 UStateTreeAIComponent 并接入 AIC 的 Brain 槽：Possess 时引擎自动启动逻辑，
 *   StateTree 资产由派生 BP 在细节面板指定（EnemyStateTree）
 * - 默认配好 RVO 群体避让（CrowdFollowing）：分离开启 + Medium 质量（方案 §3.3 参数）
 * - FreezeLogic / UnfreezeLogic = 过场动画冻结/恢复全场怪：Pause 保留状态不重置
 *
 * 派生 BP 两步走：指定 EnemyStateTree 资产 → 怪 BP 的 AIController Class 选它。
 */
UCLASS()
class CYBERRUIN_API ACyberEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ACyberEnemyAIController();

	virtual void OnPossess(APawn* InPawn) override;

	/** 怪物行为 StateTree 资产：哨兵/掠食者/裁决者各指定自己的（ST_Sentinel / ST_Predator / ST_Arbiter） */
	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UStateTree> EnemyStateTree;

	/** 冻结行为（过场动画/教学暂停）：StateTree 暂停（保留状态）+ 停止移动 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void FreezeLogic();

	/** 恢复行为：从暂停处继续 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void UnfreezeLogic();

protected:
	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeComp;
};
