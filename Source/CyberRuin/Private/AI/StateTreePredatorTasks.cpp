#include "AI/StateTreePredatorTasks.h"

#include "AI/CyberAIFunctionLibrary.h"
#include "AI/CyberEnemyAIController.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	bool IsMoveActive(AAIController* AI)
	{
		return AI && AI->GetPathFollowingComponent()
			&& AI->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving;
	}

	/** 环带随机点：绕 Origin 的 [Inner, Outer] 半径环带取随机朝向点 */
	FVector RandomRingPoint(const FVector& Origin, float Inner, float Outer)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::FRandRange(Inner, Outer);
		return Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
	}
}

// ---------------- 索敌漫游 ----------------

FStateTreePredatorRoamTask::FStateTreePredatorRoamTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePredatorRoamTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.LastRequestTime = -1.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreePredatorRoamTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* AI = InstanceData.AIController.Get();
	UWorld* World = AI ? AI->GetWorld() : nullptr;
	if (!AI || !AI->GetPawn() || !World)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 到达/走不通 → 冷却半秒取下一个环带点继续游走（坏点自愈，永续漫游）
	if (!IsMoveActive(AI) && World->GetTimeSeconds() - InstanceData.LastRequestTime > 0.5f)
	{
		const ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(AI);
		const FVector Origin = EnemyAIC ? EnemyAIC->HomeLocation : AI->GetPawn()->GetActorLocation();
		const FVector Point = RandomRingPoint(Origin, InstanceData.RoamInnerRadius, InstanceData.RoamOuterRadius);
		AI->MoveToLocation(Point, 100.f, true, true, true);
		InstanceData.LastRequestTime = World->GetTimeSeconds();
	}
	return EStateTreeRunStatus::Running;
}

// ---------------- 绕后接近 ----------------

FStateTreePredatorFlankTask::FStateTreePredatorFlankTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePredatorFlankTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	UWorld* World = EnemyAIC ? EnemyAIC->GetWorld() : nullptr;
	if (!EnemyAIC || !EnemyAIC->CurrentTarget || !World)
	{
		return EStateTreeRunStatus::Failed;
	}

	FVector Point;
	if (!UCyberAIFunctionLibrary::GetFlankPoint(EnemyAIC->CurrentTarget, InstanceData.FlankDistance, InstanceData.SampleArcDegrees, Point))
	{
		UE_LOG(LogTemp, Display, TEXT("[掠食者] %s 绕后取点失败（背后全被挡），回索敌漫游"), *GetNameSafe(EnemyAIC->GetPawn()));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.FlankPoint = Point;
	InstanceData.LastRepathTime = World->GetTimeSeconds();
	EnemyAIC->MoveToLocation(Point, InstanceData.AcceptanceRadius, true, true, true);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreePredatorFlankTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	UWorld* World = EnemyAIC ? EnemyAIC->GetWorld() : nullptr;
	AActor* Target = EnemyAIC ? EnemyAIC->CurrentTarget.Get() : nullptr;
	if (!EnemyAIC || !Target || !EnemyAIC->GetPawn() || !World)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 到达绕后点 → 转背刺判定
	if (!IsMoveActive(InstanceData.AIController.Get())
		&& FVector::Dist2D(EnemyAIC->GetPawn()->GetActorLocation(), InstanceData.FlankPoint) <= InstanceData.AcceptanceRadius + 60.f)
	{
		UE_LOG(LogTemp, Display, TEXT("[掠食者] %s 已绕到目标背后，进背刺判定（距目标 %.0fcm、距绕后点 %.0fcm、绕后点=%s、掠食者=%s、目标朝向 %.0f°）"),
			*GetNameSafe(EnemyAIC->GetPawn()),
			FVector::Dist2D(EnemyAIC->GetPawn()->GetActorLocation(), Target->GetActorLocation()),
			FVector::Dist2D(EnemyAIC->GetPawn()->GetActorLocation(), InstanceData.FlankPoint),
			*InstanceData.FlankPoint.ToCompactString(),
			*EnemyAIC->GetPawn()->GetActorLocation().ToCompactString(),
			Target->GetActorRotation().Yaw);
		return EStateTreeRunStatus::Succeeded;
	}

	// 目标在动，按间隔刷新绕后点跟随
	if (World->GetTimeSeconds() - InstanceData.LastRepathTime >= InstanceData.RepathInterval)
	{
		FVector Point;
		if (UCyberAIFunctionLibrary::GetFlankPoint(Target, InstanceData.FlankDistance, InstanceData.SampleArcDegrees, Point))
		{
			InstanceData.FlankPoint = Point;
			EnemyAIC->MoveToLocation(Point, InstanceData.AcceptanceRadius, true, true, true);
		}
		InstanceData.LastRepathTime = World->GetTimeSeconds();
	}
	return EStateTreeRunStatus::Running;
}

// ---------------- 贴身背刺 ----------------

FStateTreePredatorBackstabTask::FStateTreePredatorBackstabTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePredatorBackstabTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	AActor* Target = EnemyAIC ? EnemyAIC->CurrentTarget.Get() : nullptr;
	if (!EnemyAIC || !Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 从绕后点（目标背后 450cm）直冲目标，贴到背刺距离的 3/4 处开始判定
	EnemyAIC->MoveToActor(Target, InstanceData.BackstabRange * 0.75f);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreePredatorBackstabTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	AActor* Target = EnemyAIC ? EnemyAIC->CurrentTarget.Get() : nullptr;
	if (!EnemyAIC || !Target || !EnemyAIC->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}

	// 还在贴近途中；走完/走不动了才判定（防目标移动导致的反复重发把判定挤掉）
	if (IsMoveActive(EnemyAIC))
	{
		return EStateTreeRunStatus::Running;
	}

	const FVector Offset = EnemyAIC->GetPawn()->GetActorLocation() - Target->GetActorLocation();
	const float Distance = Offset.Size2D();
	const FVector ToPredator = Offset.GetSafeNormal2D();
	const FVector Facing = Target->GetActorForwardVector().GetSafeNormal2D();
	const float Dot = FVector::DotProduct(Facing, ToPredator);
	const bool bInRange = Distance <= InstanceData.BackstabRange;
	// 背后 90° 扇形：目标前向与"目标→掠食者"方向夹角 > 45°（点积 < -cos45°）
	const bool bBehind = Dot < -0.707f;

	if (bInRange && bBehind)
	{
		// 大伤害结算由 Gameplay 层挂钩（BPI_Damageable），测试期打日志 + 灰盒背刺动画
		UE_LOG(LogTemp, Warning, TEXT("[掠食者] %s 背刺命中 %s！距离 %.0fcm"),
			*GetNameSafe(EnemyAIC->GetPawn()), *GetNameSafe(Target), Distance);
		EnemyAIC->PlayPrimaryAction();
		return EStateTreeRunStatus::Succeeded;
	}

	UE_LOG(LogTemp, Display, TEXT("[掠食者] %s 贴近后背刺条件不满足（距离 %.0fcm、dot=%.2f、掠食者=%s、目标=%s、目标朝向 %.0f°、目标在动=%s），再绕一圈"),
		*GetNameSafe(EnemyAIC->GetPawn()), Distance, Dot,
		*EnemyAIC->GetPawn()->GetActorLocation().ToCompactString(),
		*Target->GetActorLocation().ToCompactString(),
		Target->GetActorRotation().Yaw,
		Target->GetVelocity().Size() > 10.f ? TEXT("是") : TEXT("否"));
	return EStateTreeRunStatus::Failed;
}

// ---------------- 被发现游走 ----------------

FStateTreePredatorSpottedTask::FStateTreePredatorSpottedTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePredatorSpottedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Elapsed = 0.f;
	InstanceData.LastRequestTime = -1.f;

	UE_LOG(LogTemp, Display, TEXT("[掠食者] %s 被玩家察觉！掉头撤离 %.1f 秒后重新找时机"),
		*GetNameSafe(InstanceData.AIController ? InstanceData.AIController->GetPawn() : nullptr), InstanceData.WanderSeconds);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreePredatorSpottedTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	UWorld* World = EnemyAIC ? EnemyAIC->GetWorld() : nullptr;
	AActor* Target = EnemyAIC ? EnemyAIC->CurrentTarget.Get() : nullptr;
	if (!EnemyAIC || !Target || !World)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Elapsed += DeltaTime;
	if (InstanceData.Elapsed >= InstanceData.WanderSeconds)
	{
		UE_LOG(LogTemp, Display, TEXT("[掠食者] %s 撤离完毕，回索敌漫游再找时机"), *GetNameSafe(EnemyAIC->GetPawn()));
		return EStateTreeRunStatus::Succeeded;
	}

	// 掉头撤离：沿"玩家→掠食者"方向再退 WanderRadius~2×WanderRadius（重取点时方向随位置刷新）。
	// 原版围着玩家转圈游走，观感上就是"在离我不远的地方晃悠还不来偷背"——被看见的正确观感是逃走另找时机
	if (!IsMoveActive(InstanceData.AIController.Get()) && World->GetTimeSeconds() - InstanceData.LastRequestTime > 0.5f)
	{
		const FVector AwayDir = (EnemyAIC->GetPawn()->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
		const FVector Point = EnemyAIC->GetPawn()->GetActorLocation()
			+ AwayDir * FMath::FRandRange(InstanceData.WanderRadius, InstanceData.WanderRadius * 2.f);
		EnemyAIC->MoveToLocation(Point, 100.f, true, true, true);
		InstanceData.LastRequestTime = World->GetTimeSeconds();
	}
	return EStateTreeRunStatus::Running;
}
