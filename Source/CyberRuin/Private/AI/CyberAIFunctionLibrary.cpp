#include "AI/CyberAIFunctionLibrary.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"

namespace
{
	UNavigationSystemV1* GetNavSys(const UObject* ContextObject)
	{
		UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
		return World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	}
}

bool UCyberAIFunctionLibrary::WarpToNearestNavigable(AActor* Actor, float SearchRadius)
{
	if (!Actor)
	{
		return false;
	}
	UNavigationSystemV1* NavSys = GetNavSys(Actor);
	if (!NavSys)
	{
		return false;
	}

	const FVector From = Actor->GetActorLocation();
	FNavLocation Out;

	// 先试原地投影（多数卡死只差一步），失败再随机找近旁可达点
	if (NavSys->ProjectPointToNavigation(From, Out) ||
		NavSys->GetRandomPointInNavigableRadius(From, SearchRadius, Out))
	{
		return Actor->TeleportTo(Out.Location, Actor->GetActorRotation());
	}
	return false;
}

bool UCyberAIFunctionLibrary::ValidateSpawnPoint(const UObject* WorldContextObject, const FVector& Desired, FVector& OutSpawn)
{
	UNavigationSystemV1* NavSys = GetNavSys(WorldContextObject);
	if (!NavSys)
	{
		return false;
	}

	FNavLocation Out;
	if (NavSys->ProjectPointToNavigation(Desired, Out))
	{
		OutSpawn = Out.Location;
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[赛博遗迹AI] 刷怪点不在导航网格上，跳过刷怪: %s"), *Desired.ToString());
	return false;
}

bool UCyberAIFunctionLibrary::GetFlankPoint(const AActor* Target, float FlankDistance, float SampleArcDegrees, FVector& OutPoint)
{
	if (!Target)
	{
		return false;
	}
	UNavigationSystemV1* NavSys = GetNavSys(Target);
	if (!NavSys)
	{
		return false;
	}

	// 目标背后扇形上均匀取 7 个候选，投影到导航网格，取第一个成功的（背后死角优先）
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector Facing = Target->GetActorForwardVector().GetSafeNormal2D();
	const float HalfArc = FMath::Max(SampleArcDegrees, 1.f);

	for (int32 CandidateIndex = 0; CandidateIndex < 7; ++CandidateIndex)
	{
		const float Degrees = 180.f - HalfArc + (2.f * HalfArc / 6.f) * CandidateIndex;
		const FVector Candidate = TargetLocation + Facing.RotateAngleAxis(Degrees, FVector::UpVector).GetSafeNormal2D() * FlankDistance;

		FNavLocation Out;
		if (NavSys->ProjectPointToNavigation(Candidate, Out))
		{
			OutPoint = Out.Location;
			return true;
		}
	}
	return false;
}
