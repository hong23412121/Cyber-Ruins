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
	const float Dist = FVector::Dist2D(PlayerPawn->GetActorLocation(), Pawn->GetActorLocation());

	// 贴身锁定：距离已进扑击范围时不判被察觉——背刺冲刺发动后即使转身面对也拦不住，只有走位拉开/闪避能躲
	if (Dist < InstanceData.MinDistance)
	{
		return false;
	}
	if (Dist > InstanceData.SpotRadius)
	{
		return false;
	}

	// 玩家视线锥：角色朝向 vs 指向怪的方向（v2.2 改绑角色朝向——原用相机/控制朝向，
	// 第三人称里镜头和角色经常不一致：镜头回头看它不算"看见"，角色脸朝它才算。
	// 与背刺"背后 90° 扇形"判定源一致：转身=防住，转镜头=防不住）
	const FVector ViewDir = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
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
