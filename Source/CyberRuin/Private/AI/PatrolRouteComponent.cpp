#include "AI/PatrolRouteComponent.h"

#include "GameFramework/Actor.h"

UPatrolRouteComponent::UPatrolRouteComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
