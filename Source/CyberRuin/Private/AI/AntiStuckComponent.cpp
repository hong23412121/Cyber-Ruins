#include "AI/AntiStuckComponent.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"

UAntiStuckComponent::UAntiStuckComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.5f; // 采样频率 = tick 频率，编辑器里可调
}

void UAntiStuckComponent::BeginPlay()
{
	Super::BeginPlay();
	LastLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UAntiStuckComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;

	// 只在移动中计时：待机/攻击原地不动不算卡死
	const bool bMoving = AI && AI->GetPathFollowingComponent()
		&& AI->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving;
	if (!bMoving)
	{
		StuckTime = 0.f;
		bStuck = false;
		return;
	}

	const FVector Current = Pawn->GetActorLocation();
	const float MovedCm = FVector::Dist2D(Current, LastLocation);
	LastLocation = Current;

	StuckTime = (MovedCm < MinMovePerSample) ? StuckTime + DeltaTime : 0.f;
	bStuck = StuckTime >= MaxStuckSeconds;
}
