#include "AI/AttackSlotSubsystem.h"

#include "GameFramework/Actor.h"

UAttackSlotSubsystem* UAttackSlotSubsystem::Get(const AActor* Actor)
{
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAttackSlotSubsystem>() : nullptr;
}

bool UAttackSlotSubsystem::TryAcquireSlot(AActor* Attacker, AActor* Target, FVector& OutPoint)
{
	if (!Attacker || !Target)
	{
		return false;
	}

	const float SectorDeg = 360.f / NumSlots;
	// 攻击者相对目标的角度 → 离它最近的扇区编号
	const FVector Dir = (Attacker->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const int32 Wanted = FMath::RoundToInt(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) / SectorDeg);

	TSet<int32> Taken;
	for (const TPair<FObjectKey, int32>& Pair : Claimed)
	{
		Taken.Add(Pair.Value);
	}

	int32 Best = -1;
	float BestCost = FLT_MAX;
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		if (Taken.Contains(Slot))
		{
			continue;
		}
		const int32 Offset = ((Slot - Wanted) % NumSlots + NumSlots) % NumSlots; // 角度距离（扇区数，防负数取模）
		const float Cost = FMath::Min(Offset, NumSlots - Offset) * SectorDeg;
		if (Cost < BestCost)
		{
			BestCost = Cost;
			Best = Slot;
		}
	}

	if (Best < 0)
	{
		return false; // 满员：调用方转游走
	}

	Claimed.Add(FObjectKey(Attacker), Best);
	const float Angle = FMath::DegreesToRadians(Best * SectorDeg);
	OutPoint = Target->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * RingRadius;
	return true;
}

void UAttackSlotSubsystem::Release(AActor* Attacker)
{
	Claimed.Remove(FObjectKey(Attacker));
}
