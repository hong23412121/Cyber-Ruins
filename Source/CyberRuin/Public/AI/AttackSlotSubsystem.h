#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "AttackSlotSubsystem.generated.h"

class AActor;

/**
 * 攻击名额：把玩家周围按角度切成 8 个站位扇区，每个扇区同时只放一个近战，
 * 防止围攻怪挤在同一条接近线上互撞（"轮流上"观感）。满员时 TryAcquireSlot
 * 返回 false，调用方应转游走/警戒。哨兵×N + 掠食者×2 + W6 守卫活性 +50% 时启用。
 */
UCLASS()
class CYBERRUIN_API UAttackSlotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAttackSlotSubsystem* Get(const AActor* Actor);

	/** 占最接近自身角度的名额；OutPoint 为站位世界坐标。满员返回 false（调用方转游走，别硬挤） */
	UFUNCTION(BlueprintCallable, Category = "赛博遗迹AI")
	bool TryAcquireSlot(AActor* Attacker, AActor* Target, FVector& OutPoint);

	/** 攻击结束/脱战/死亡时必须调用，否则名额泄漏 */
	UFUNCTION(BlueprintCallable, Category = "赛博遗迹AI")
	void Release(AActor* Attacker);

private:
	static constexpr int32 NumSlots = 8;
	static constexpr float RingRadius = 160.f; // 站位环半径(cm)

	TMap<FObjectKey, int32> Claimed;
};
