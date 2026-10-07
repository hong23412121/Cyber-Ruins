#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwarmDrone.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * 蜂群无人机个体：纯被驱动的壳，运动由 SwarmHive 全权接管（个体零 Tick）。
 * 与玩家无硬碰撞（"蹲行可穿"），仅保留对 WorldStatic 的 Block 供 sweep 防穿墙
 * ——依赖墙体保持默认碰撞（方案 §3.4 W1/W2）。
 */
UCLASS()
class CYBERRUIN_API ASwarmDrone : public AActor
{
	GENERATED_BODY()

public:
	ASwarmDrone();

	/** 当前速度，由 Hive 的蜂群算法驱动 */
	FVector Vel = FVector::ZeroVector;

	/** 翻越记忆：正在翻的低障碍"安全改平高度"（箱顶+ClimbClearance）；没爬到此高度前 Hive 持续给升力，防止水平射线脱靶后机身在顶棱高度俯冲撞回箱面。<=0 = 未在翻越 */
	float ClimbFloorZ = -1.f;

	UPROPERTY(VisibleAnywhere, Category = "蜂群")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "蜂群")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
