#include "AI/StateTreeEnemyConditions.h"

#include "AI/AntiStuckComponent.h"
#include "AI/CyberEnemyAIController.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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

bool FStateTreeIsSpottedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIController* AIC = InstanceData.AIController.Get();
	const APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->GetPawn())
	{
		return false;
	}

	const APawn* PlayerPawn = PC->GetPawn();
	const FVector ToPredator = Pawn->GetActorLocation() - PlayerPawn->GetActorLocation();
	if (FVector::Dist2D(PlayerPawn->GetActorLocation(), Pawn->GetActorLocation()) > InstanceData.SpotRadius)
	{
		return false;
	}

	// 玩家视线锥：控制朝向 vs 指向怪的方向
	const FVector ViewDir = PC->GetControlRotation().Vector().GetSafeNormal2D();
	if (FVector::DotProduct(ViewDir, ToPredator.GetSafeNormal2D()) < FMath::Cos(FMath::DegreesToRadians(InstanceData.SpotHalfAngleDegrees)))
	{
		return false;
	}

	// 中间有墙不算被看见
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PlayerPawn);
	Params.AddIgnoredActor(Pawn);
	Params.TraceTag = TEXT("PredatorSpottedLOS");
	return !World->LineTraceSingleByChannel(
		Hit, PlayerPawn->GetActorLocation(), Pawn->GetActorLocation(), ECC_Visibility, Params);
}

bool FStateTreeIsShieldBrokenCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	return EnemyAIC && EnemyAIC->bShieldBroken;
}

bool FStateTreeHasTargetCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ACyberEnemyAIController* EnemyAIC = Cast<ACyberEnemyAIController>(InstanceData.AIController.Get());
	return EnemyAIC && EnemyAIC->CurrentTarget != nullptr;
}
