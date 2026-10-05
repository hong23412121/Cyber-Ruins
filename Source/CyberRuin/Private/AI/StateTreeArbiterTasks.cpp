#include "AI/StateTreeArbiterTasks.h"

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
}

FStateTreeArbiterGuardTask::FStateTreeArbiterGuardTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeArbiterGuardTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	if (!EnemyAIC)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.LastFireLogTime = -1.f;
	InstanceData.VisibleAccum = 0.f;

	// 调试循环：回岗即视为换了一面新盾（真循环里破盾→击杀/丢失后回岗同理）
	if (EnemyAIC->bDebugAutoBreakShield)
	{
		EnemyAIC->bShieldBroken = false;
	}

	UE_LOG(LogTemp, Display, TEXT("[裁决者] %s 进入守盾警戒（岗位点 %s）"),
		*GetNameSafe(EnemyAIC->GetPawn()), *EnemyAIC->HomeLocation.ToCompactString());
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeArbiterGuardTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	UWorld* World = EnemyAIC ? EnemyAIC->GetWorld() : nullptr;
	AActor* Target = EnemyAIC ? EnemyAIC->CurrentTarget.Get() : nullptr;
	if (!EnemyAIC || !EnemyAIC->GetPawn() || !World)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector PawnLoc = EnemyAIC->GetPawn()->GetActorLocation();

	// 离岗超距 → 走回岗位点（不朝向目标，专心回岗）
	if (FVector::Dist2D(PawnLoc, EnemyAIC->HomeLocation) > InstanceData.ReturnDistance)
	{
		EnemyAIC->ClearFocus(EAIFocusPriority::Gameplay);
		if (!IsMoveActive(EnemyAIC))
		{
			EnemyAIC->MoveToLocation(EnemyAIC->HomeLocation, 100.f, true, true, true);
		}
		InstanceData.VisibleAccum = 0.f;
		return EStateTreeRunStatus::Running;
	}

	// 在岗位上：目标可见就面朝
	const bool bVisible = EnemyAIC->bCanSeeTarget && Target != nullptr;
	if (bVisible)
	{
		EnemyAIC->SetFocus(Target);
		const float Dist = FVector::Dist2D(PawnLoc, Target->GetActorLocation());

		// 射程内开火（大伤害由 Gameplay 层挂钩，测试期节流打日志）
		if (Dist <= InstanceData.FireRange && World->GetTimeSeconds() - InstanceData.LastFireLogTime > 1.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[裁决者] %s 对 %s 开火！距离 %.0fcm（盾未破不追击）"),
				*GetNameSafe(EnemyAIC->GetPawn()), *GetNameSafe(Target), Dist);
			InstanceData.LastFireLogTime = World->GetTimeSeconds();
		}

		// 调试自动破盾：目标持续在射程内可见 → 视为火力压制破盾
		if (EnemyAIC->bDebugAutoBreakShield && !EnemyAIC->bShieldBroken && Dist <= InstanceData.FireRange)
		{
			InstanceData.VisibleAccum += DeltaTime;
			if (InstanceData.VisibleAccum >= InstanceData.DebugBreakSeconds)
			{
				EnemyAIC->bShieldBroken = true;
				UE_LOG(LogTemp, Warning, TEXT("[裁决者] %s 护盾被持续火力击破！转入追杀"),
					*GetNameSafe(EnemyAIC->GetPawn()));
			}
		}
	}
	else
	{
		EnemyAIC->ClearFocus(EAIFocusPriority::Gameplay);
		InstanceData.VisibleAccum = 0.f;
	}
	return EStateTreeRunStatus::Running;
}
