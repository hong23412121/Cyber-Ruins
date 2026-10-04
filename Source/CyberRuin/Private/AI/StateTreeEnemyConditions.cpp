#include "AI/StateTreeEnemyConditions.h"

#include "AI/AntiStuckComponent.h"
#include "AI/CyberEnemyAIController.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

bool FStateTreeIsStuckCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		return false;
	}

	const APawn* Pawn = InstanceData.AIController->GetPawn();
	const UAntiStuckComponent* AntiStuck = Pawn ? Pawn->FindComponentByClass<UAntiStuckComponent>() : nullptr;
	return AntiStuck && AntiStuck->bStuck;
}

bool FStateTreeIsFarFromHomeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	if (!EnemyAIC || !EnemyAIC->GetPawn())
	{
		return false;
	}

	return FVector::Dist2D(EnemyAIC->GetPawn()->GetActorLocation(), EnemyAIC->HomeLocation) > InstanceData.MaxDistance;
}
