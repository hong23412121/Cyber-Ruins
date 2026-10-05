#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwarmHive.generated.h"

class ASwarmDrone;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwarmDroneCleared, FVector, Location);

UENUM(BlueprintType)
enum class ESwarmState : uint8
{
	Idle,
	Engage
};

/**
 * 蜂群导演：不做导航寻路，整群 = 锚点 + 蜂群算法（Boids）+ 射线避墙 + sweep 贴墙滑动。
 * 不进 RVO——它自己就是避让算法；"蹲行可穿"由对蹲行玩家失明实现（行为层，非碰撞层）；
 * 锚点→玩家被墙挡住则整群回锚点（断视线 = 玩家的战术解）。
 * 摆放：各区主理人按密度表摆进 _Gameplay（或对应 Data Layer）。
 */
UCLASS()
class CYBERRUIN_API ASwarmHive : public AActor
{
	GENERATED_BODY()

public:
	ASwarmHive();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** EMP 清组入口（道具调用），全灭并广播爆点位置给特效 */
	UFUNCTION(BlueprintCallable, Category = "蜂群")
	void ClearByEMP();

	/** 被哨兵点名：无视蹲行与断视线强制追击 AlertSeconds 秒（呼叫支援的联动入口） */
	UFUNCTION(BlueprintCallable, Category = "蜂群")
	void Alert(float AlertSeconds = 10.f);

	/** 每只无人机消灭时的爆点（粒子特效挂这里） */
	UPROPERTY(BlueprintAssignable)
	FOnSwarmDroneCleared OnCleared;

	UPROPERTY(EditAnywhere, Category = "蜂群")
	TSubclassOf<AActor> DroneClass;

	UPROPERTY(EditAnywhere, Category = "蜂群", Meta = (ClampMin = "4", ClampMax = "32"))
	int32 DroneCount = 16;

	/** 当前状态（供 UI / 调试读取） */
	UPROPERTY(BlueprintReadOnly, Category = "蜂群")
	ESwarmState CurrentState = ESwarmState::Idle;

	UPROPERTY(EditAnywhere, Category = "蜂群")
	float GuardRadius = 900.f; // 封锁范围

	UPROPERTY(EditAnywhere, Category = "蜂群")
	float EngageSpeed = 700.f; // 追玩家速度

	UPROPERTY(EditAnywhere, Category = "蜂群")
	float IdleSpeed = 300.f; // 盘旋速度

	UPROPERTY(EditAnywhere, Category = "蜂群")
	float SeparationRadius = 130.f;

	UPROPERTY(EditAnywhere, Category = "蜂群")
	float WallLookAhead = 160.f; // 避墙探测距离

	/** 低障碍翻越高度：障碍顶低于无人机此高度内 → 向上翻越（箱子可翻）；高于它 → 视为墙顺墙绕行 */
	UPROPERTY(EditAnywhere, Category = "蜂群")
	float ClimbOverHeight = 300.f;

	UPROPERTY(EditAnywhere, Category = "蜂群")
	float HoverZ = 150.f; // 距地悬停高度

private:
	void SpawnDrones();
	void SteerAll(float DeltaTime, const FVector& Target, float Speed);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASwarmDrone>> Drones;

	FVector Anchor = FVector::ZeroVector;
	float AlertedUntil = -1.f;
};
