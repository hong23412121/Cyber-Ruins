#include "AI/SwarmHive.h"

#include "AI/SwarmDrone.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

ASwarmHive::ASwarmHive()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASwarmHive::BeginPlay()
{
	Super::BeginPlay();

	Anchor = GetActorLocation();
	// 30Hz 足够（蜂群是氛围+威胁，不是精确博弈），省一半移动开销
	SetActorTickInterval(1.f / 30.f);
	SpawnDrones();
}

void ASwarmHive::SpawnDrones()
{
	UWorld* World = GetWorld();
	if (!World || !DroneClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < DroneCount; ++i)
	{
		const FVector Offset = FVector(
			FMath::FRandRange(-1.f, 1.f),
			FMath::FRandRange(-1.f, 1.f),
			FMath::FRandRange(0.f, 1.f)).GetClampedToMaxSize(1.f) * 250.f;

		ASwarmDrone* Drone = World->SpawnActor<ASwarmDrone>(
			DroneClass, Anchor + Offset + FVector(0.f, 0.f, HoverZ), FRotator::ZeroRotator, Params);
		if (Drone)
		{
			Drones.Add(Drone);
		}
	}
}

void ASwarmHive::Alert(float AlertSeconds)
{
	// 哨兵呼叫支援 = 点名期内蹲行不再"隐身"、断视线也继续追
	if (UWorld* World = GetWorld())
	{
		AlertedUntil = World->TimeSeconds + AlertSeconds;
	}
}

void ASwarmHive::ClearByEMP()
{
	for (ASwarmDrone* Drone : Drones)
	{
		if (Drone)
		{
			OnCleared.Broadcast(Drone->GetActorLocation());
			Drone->Destroy();
		}
	}
	Drones.Empty();
}

void ASwarmHive::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	const bool bCrouched = Player && Player->GetCharacterMovement() && Player->GetCharacterMovement()->IsCrouching();
	const bool bInZone = Player && FVector::Dist2D(Player->GetActorLocation(), Anchor) <= GuardRadius;
	const bool bAlerted = World->TimeSeconds < AlertedUntil;

	bool bAnchorLOS = false;
	if (Player)
	{
		// 玩家胶囊默认阻挡 Visibility，必须忽略玩家自身，否则视线永远"被挡"
		FHitResult LosHit;
		FCollisionQueryParams LosParams;
		LosParams.AddIgnoredActor(Player);
		LosParams.TraceTag = TEXT("SwarmAnchorLOS");
		bAnchorLOS = !World->LineTraceSingleByChannel(
			LosHit, Anchor + FVector(0.f, 0.f, HoverZ), Player->GetActorLocation(), ECC_Visibility, LosParams);
	}

	// 核心规则：蹲行 = 对蜂群隐身；墙后断视线 = 脱离封锁；被哨兵点名 = 无视前两条
	const bool bEngage = bAlerted || (bInZone && !bCrouched && bAnchorLOS);
	CurrentState = bEngage ? ESwarmState::Engage : ESwarmState::Idle;

	const FVector Target = (bEngage && Player)
		? Player->GetActorLocation() + FVector(0.f, 0.f, 160.f) // 悬在头顶一圈
		: Anchor + FVector(0.f, 0.f, HoverZ);

	SteerAll(DeltaSeconds, Target, bEngage ? EngageSpeed : IdleSpeed);
}

void ASwarmHive::SteerAll(float DeltaTime, const FVector& Target, float Speed)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (ASwarmDrone* Drone : Drones)
	{
		if (!Drone)
		{
			continue;
		}

		// ---- Boids 三规则（群内两两比较，n≤32 时 O(n²) 完全够用）----
		FVector Sep = FVector::ZeroVector, Align = FVector::ZeroVector, Center = FVector::ZeroVector;
		int32 Neighbors = 0;

		for (ASwarmDrone* Other : Drones)
		{
			if (!Other || Other == Drone)
			{
				continue;
			}
			const FVector Diff = Drone->GetActorLocation() - Other->GetActorLocation();
			const float Dist = Diff.Size();
			if (Dist < SeparationRadius && Dist > 1.f)
			{
				Sep += Diff / Dist * (SeparationRadius - Dist) / SeparationRadius; // 越近推得越狠
				Align += Other->Vel;
				Center += Other->GetActorLocation();
				++Neighbors;
			}
		}

		FVector Steer = (Target - Drone->GetActorLocation()).GetSafeNormal();
		if (Neighbors > 0)
		{
			Steer += Sep.GetSafeNormal() * 1.6f;                                                // 分离权重最高
			Steer += (Align / Neighbors - Drone->Vel).GetSafeNormal() * 0.4f;                  // 对齐
			Steer += (Center / Neighbors - Drone->GetActorLocation()).GetSafeNormal() * 0.5f;  // 聚合
		}

		// ---- 避墙射线：提前转向的"顺滑层"；防穿墙硬保证在下方 sweep 移动 ----
		FHitResult AvoidHit;
		const FVector TraceEnd = Drone->GetActorLocation() + Drone->Vel.GetSafeNormal() * WallLookAhead;
		if (World->LineTraceSingleByChannel(AvoidHit, Drone->GetActorLocation(), TraceEnd, ECC_Visibility)
			&& AvoidHit.Normal.Z < 0.7f) // 忽略地面/坡面，只躲竖直墙
		{
			Steer += AvoidHit.Normal.GetSafeNormal2D() * 2.5f;
		}

		Drone->Vel = (Drone->Vel + Steer * DeltaTime * 900.f).GetClampedToMaxSize(Speed);

		// sweep 移动是防穿墙硬保证：撞上即停在墙面（依赖墙在 WorldStatic 通道，方案 §3.4 W1/W2）
		FHitResult MoveHit;
		const FVector NewLocation = Drone->GetActorLocation() + Drone->Vel * DeltaTime;
		if (!Drone->SetActorLocation(NewLocation, true, &MoveHit) && MoveHit.IsValidBlockingHit())
		{
			// 贴墙滑动：速度投影到墙面切线继续走 + 轻微离墙推力，避免"趴墙抖动"
			Drone->Vel = FVector::VectorPlaneProject(Drone->Vel, MoveHit.Normal) + MoveHit.Normal * 50.f;
		}
	}
}
