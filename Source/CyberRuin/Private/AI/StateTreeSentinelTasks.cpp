#include "AI/StateTreeSentinelTasks.h"

#include "AI/CyberAIFunctionLibrary.h"
#include "AI/CyberEnemyAIController.h"
#include "AI/PatrolRouteComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	/** 寻路组件当前是否在移动 */
	bool IsMoveActive(AAIController* AI)
	{
		return AI && AI->GetPathFollowingComponent()
			&& AI->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving;
	}

	/** MoveToActor 结果转文案（诊断日志用） */
	const TCHAR* PathRequestResultText(EPathFollowingRequestResult::Type Result)
	{
		switch (Result)
		{
		case EPathFollowingRequestResult::Type::AlreadyAtGoal: return TEXT("AlreadyAtGoal");
		case EPathFollowingRequestResult::Type::Failed: return TEXT("Failed");
		default: return TEXT("Moving");
		}
	}
}

// ---------------- 取巡逻点 ----------------

FStateTreeGetPatrolPointTask::FStateTreeGetPatrolPointTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeGetPatrolPointTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = InstanceData.AIController->GetPawn();
	UPatrolRouteComponent* Route = Pawn ? Pawn->FindComponentByClass<UPatrolRouteComponent>() : nullptr;
	if (!Route)
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Point = Route->GetCurrentPoint();
	if (!Point)
	{
		Route->Advance();
		Point = Route->GetCurrentPoint();
	}
	if (!Point)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.OutPoint = Point;
	Route->Advance();
	return EStateTreeRunStatus::Succeeded;
}

// ---------------- 巡逻移动 ----------------

FStateTreePatrolMoveTask::FStateTreePatrolMoveTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePatrolMoveTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = InstanceData.AIController->GetPawn();
	UPatrolRouteComponent* Route = Pawn ? Pawn->FindComponentByClass<UPatrolRouteComponent>() : nullptr;
	if (!Route)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Patrol] %s 没挂 PatrolRouteComponent，巡逻状态失败"), *GetNameSafe(Pawn));
		return EStateTreeRunStatus::Failed; // 没挂巡逻组件：状态失败，转换兜底
	}

	// 任务内自循环：到达/走不通时 Tick 里取下一点继续走，永不主动完成
	AActor* Point = Route->GetCurrentPoint();
	if (!Point)
	{
		Route->Advance();
		Point = Route->GetCurrentPoint();
	}
	if (Point)
	{
		const EPathFollowingRequestResult::Type Result =
			InstanceData.AIController->MoveToActor(Point, InstanceData.AcceptanceRadius);
		UE_LOG(LogTemp, Display, TEXT("[Patrol] %s EnterState → %s MoveTo=%s（路线共 %d 点）"),
			*GetNameSafe(Pawn), *GetNameSafe(Point), PathRequestResultText(Result), Route->Points.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Patrol] %s EnterState：路线为空（%d 点），站桩待命"), *GetNameSafe(Pawn), Route->Points.Num());
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreePatrolMoveTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* AI = InstanceData.AIController;
	if (!AI || !AI->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}

	// 走完了（或这次目标走不通）→ 取下一个点继续：坏点自动跳过，巡逻自愈
	if (!IsMoveActive(AI))
	{
		UPatrolRouteComponent* Route = AI->GetPawn()->FindComponentByClass<UPatrolRouteComponent>();
		if (!Route)
		{
			return EStateTreeRunStatus::Failed;
		}
		Route->Advance();

		if (AActor* NextPoint = Route->GetCurrentPoint())
		{
			const EPathFollowingRequestResult::Type Result =
				AI->MoveToActor(NextPoint, InstanceData.AcceptanceRadius);
			UE_LOG(LogTemp, Display, TEXT("[Patrol] %s 换点 → %s MoveTo=%s"),
				*GetNameSafe(AI->GetPawn()), *GetNameSafe(NextPoint), PathRequestResultText(Result));
		}
	}
	return EStateTreeRunStatus::Running;
}

void FStateTreePatrolMoveTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// 被转换切走（看见敌人/解卡）时停掉在途移动
	if (FInstanceDataType& InstanceData = Context.GetInstanceData(*this); InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
}

// ---------------- 追击目标 ----------------

FStateTreeChaseTargetTask::FStateTreeChaseTargetTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeChaseTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 目标直接来自 AIC（感知回调先写 CurrentTarget 再发事件，顺序有保证）
	const ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	if (!EnemyAIC || !EnemyAIC->CurrentTarget)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.LastTarget = EnemyAIC->CurrentTarget;
	InstanceData.AIController->MoveToActor(EnemyAIC->CurrentTarget, InstanceData.AcceptanceRadius);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeChaseTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	if (!EnemyAIC)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 目标没了 → Failed（转换兜回巡逻）；换目标 → 重新寻路；走完/走不通且目标还在 → 重发
	AActor* Target = EnemyAIC->CurrentTarget;
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Target != InstanceData.LastTarget || !IsMoveActive(InstanceData.AIController))
	{
		InstanceData.LastTarget = Target;
		InstanceData.AIController->MoveToActor(Target, InstanceData.AcceptanceRadius);
	}
	return EStateTreeRunStatus::Running;
}

void FStateTreeChaseTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (FInstanceDataType& InstanceData = Context.GetInstanceData(*this); InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	// LastTarget 留作下次对比，无需清
}

// ---------------- 解卡挪窝 ----------------

FStateTreeWarpUnstuckTask::FStateTreeWarpUnstuckTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeWarpUnstuckTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController || !InstanceData.AIController->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}

	return UCyberAIFunctionLibrary::WarpToNearestNavigable(InstanceData.AIController->GetPawn(), InstanceData.SearchRadius)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}
