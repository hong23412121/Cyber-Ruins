#include "AI/PatrolRouteComponent.h"

#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

UPatrolRouteComponent::UPatrolRouteComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPatrolRouteComponent::BeginPlay()
{
	Super::BeginPlay();

	// 摆点即用：没拖引用时自动抓取附近 TargetPoint（按名字排序，PP_1/PP_2... 天然有序）
	if (Points.Num() == 0 && bAutoCollectNearbyPoints && GetWorld())
	{
		const AActor* Owner = GetOwner();
		const FVector Origin = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

		TArray<AActor*> Found;
		for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Origin) <= AutoCollectRadius)
			{
				Found.Add(*It);
			}
		}
		Found.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });

		for (AActor* Point : Found)
		{
			Points.Add(Point);
		}
	}
}

AActor* UPatrolRouteComponent::GetCurrentPoint() const
{
	return Points.IsValidIndex(Index) ? Points[Index].Get() : nullptr;
}

void UPatrolRouteComponent::Advance()
{
	if (Points.Num() == 0)
	{
		return;
	}

	if (bPingPong && Points.Num() > 1)
	{
		if (bGoingUp)
		{
			if (Index + 1 < Points.Num())
			{
				++Index;
			}
			else
			{
				// 端点折返：跳过刚走过的端点
				bGoingUp = false;
				Index = FMath::Max(0, Index - 1);
			}
		}
		else
		{
			if (Index - 1 >= 0)
			{
				--Index;
			}
			else
			{
				// 起点折返：跳过刚走过的端点
				bGoingUp = true;
				Index = FMath::Min(Points.Num() - 1, Index + 1);
			}
		}
	}
	else
	{
		Index = (Index + 1) % Points.Num();
	}
}
