#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/StateTreeEnemyEvents.h"
#include "CyberEnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAnimSequence;
class UBlendSpace;
class UAnimSingleNodeInstance;
class UStateTree;
class UStateTreeAIComponent;

/**
 * 怪物 AIController C++ 基类（方案 §3.3）：
 * - 默认挂 UStateTreeAIComponent 并接入 AIC 的 Brain 槽：Possess 时引擎自动启动逻辑，
 *   StateTree 资产由派生 BP 在细节面板指定（EnemyStateTree）
 * - 默认挂 AIPerception + 视觉感知（C++ 配置，无需蓝图连线）：
 *   看见/丢失目标时自动向 StateTree 发送 Event.Enemy.Seen / Event.Enemy.Lost 事件
 * - 默认配好 RVO 群体避让（CrowdFollowing）：分离开启 + Medium 质量（方案 §3.3 参数）
 * - FreezeLogic / UnfreezeLogic = 过场动画冻结/恢复全场怪：Pause 保留状态不重置
 * - HomeLocation 记录出生点（leash 判断基准），CurrentTarget/bCanSeeTarget 供调试与 UI 读取
 * - 战斗底座：Health + TryReceiveHit(距离击中判定，玩家侧攻击时调) + Die(死亡动画冻末帧，留尸数秒后销毁)；
 *   LastAttackTime 是近战出招时刻，由近战任务写入、攻击冷却条件（AttackReady）读取
 *
 * 派生 BP 只需一步：指定 EnemyStateTree 资产。
 */
UCLASS()
class CYBERRUIN_API ACyberEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ACyberEnemyAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 怪物行为 StateTree 资产：哨兵/掠食者/裁决者各指定自己的（ST_Sentinel / ST_Predator / ST_Arbiter） */
	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UStateTree> EnemyStateTree;

	/** 出生点（leash 判断基准，StateTree 的"离家过远"条件直接读） */
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FVector HomeLocation = FVector::ZeroVector;

	/** 感知当前目标（调试 / UI 可读） */
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	TObjectPtr<AActor> CurrentTarget = nullptr;

	/** 当前是否看得见目标（调试 / UI 可读） */
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	bool bCanSeeTarget = false;

	/** 裁决者护盾是否已破（Gameplay 层破盾时写 true；守盾警戒状态据此放行追杀转换） */
	UPROPERTY(BlueprintReadWrite, Category = "AI")
	bool bShieldBroken = false;

	/** 测试用：守盾警戒时玩家持续贴近开火将自动破盾（真破盾事件由 Gameplay 层接手后置 false） */
	UPROPERTY(EditAnywhere, Category = "AI|Debug")
	bool bDebugAutoBreakShield = false;

	/** 血量（AI 灰盒最小实现，资产按怪覆盖：哨兵3/掠食者2/裁决者5/审计官4；正式 Gameplay 血量接入后迁移对接） */
	UPROPERTY(EditAnywhere, Category = "AI|战斗")
	float Health = 3.f;

	/** 是否已死亡（死亡后感知回调/步态驱动全停，StateTree 已 StopLogic） */
	UPROPERTY(BlueprintReadOnly, Category = "AI|战斗")
	bool bDead = false;

	/** 上次出招时刻（世界秒，负值 = 从未出招；近战任务写，AttackReady 冷却条件读） */
	UPROPERTY(BlueprintReadOnly, Category = "AI|战斗")
	float LastAttackTime = -1000.f;

	/** 测试用：玩家贴近 250cm 内自动每 1.5s 挨 1 刀（玩家侧攻击就绪前验证受击→死亡链路；就绪后关） */
	UPROPERTY(EditAnywhere, Category = "AI|Debug")
	bool bDebugAutoTakeHit = false;

	/** 冻结行为（过场动画/教学暂停）：StateTree 暂停（保留状态）+ 停止移动 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void FreezeLogic();

	/** 恢复行为：从暂停处继续 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void UnfreezeLogic();

	/** 灰盒表现：主行动动画一次性播放（掠食者=背刺击 / 裁决者=开火），播完自动恢复跑动混合 */
	UFUNCTION(BlueprintCallable, Category = "表现")
	void PlayPrimaryAction();

	/** 灰盒表现：受击表现（裁决者护盾被破等），播完自动恢复跑动混合 */
	UFUNCTION(BlueprintCallable, Category = "表现")
	void PlayHitReact();

	/** 是否有一次性动作正在播放（裁决者开火节流用） */
	UFUNCTION(BlueprintPure, Category = "表现")
	bool IsActionPlaying() const { return ActionRemaining > 0.f; }

	/** 距离击中判定（玩家侧攻击时调）：攻击者距本怪 HitRange 内才算命中——扣血、播受击；血量归零转死亡。返回是否命中 */
	UFUNCTION(BlueprintCallable, Category = "AI|战斗")
	bool TryReceiveHit(AActor* Attacker, float HitRange = 250.f, int32 Damage = 1);

	/** 灰盒步态：单节点跑动混合空间（空 = 网格走 ABP 路线，本类不做动画驱动） */
	UPROPERTY(EditAnywhere, Category = "表现")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

	/** 灰盒步态播放速率（裁决者 0.8 沉重 / 掠食者 0.9 鬼祟 / 默认 1.0） */
	UPROPERTY(EditAnywhere, Category = "表现")
	float LocomotionPlayRate = 1.f;

	/** 主行动动画（掠食者背刺 / 裁决者开火），由 PlayPrimaryAction 播 */
	UPROPERTY(EditAnywhere, Category = "表现")
	TObjectPtr<UAnimSequence> PrimaryActionAnim;

	/** 受击动画（破盾等），由 PlayHitReact 播 */
	UPROPERTY(EditAnywhere, Category = "表现")
	TObjectPtr<UAnimSequence> HitReactAnim;

	/** 死亡动画（血量归零时播，非循环播完引擎自动冻末帧；留尸 CorpseLifespanSeconds 秒后销毁） */
	UPROPERTY(EditAnywhere, Category = "表现")
	TObjectPtr<UAnimSequence> DeathAnim;

protected:
	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeComp;

private:
	/** 感知回调：写目标状态 + 向 StateTree 发事件（Seen 带目标载荷，Lost 清状态） */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** "看见敌人"事件载荷（成员存续，避免悬垂视图） */
	UPROPERTY()
	FStateTreeEnemySeenPayload SeenPayload;

	/** 一次性动作剩余时长（>0 = 动作播放中，Tick 里倒数归零后恢复跑动混合） */
	float ActionRemaining = 0.f;

	/** 扣血并处理后果（受击动画 / 死亡），供 TryReceiveHit 与调试通路共用 */
	void ApplyHit(int32 Damage);

	/** 死亡收尾：停行为/停移动/播死亡动画冻末帧，留尸 CorpseLifespanSeconds 秒后销毁 */
	void Die();

	/** 尸体留存时长(s)：死亡动画播完后冻末帧躺这段时间再销毁（灰盒不做淡出） */
	static constexpr float CorpseLifespanSeconds = 5.f;

	/** 留尸倒计时（>0 = 尸体期；死亡动画播放期间为 0） */
	float CorpseRemaining = 0.f;

	/** 调试自动挨刀计时器（bDebugAutoTakeHit 用，离范围即重置满弦） */
	float DebugHitTimer = 1.5f;

	USkeletalMeshComponent* GetEnemyMesh() const;
	UAnimSingleNodeInstance* GetSingleNodeAnim() const;
	void PlayOneShot(UAnimSequence* Anim);
};
