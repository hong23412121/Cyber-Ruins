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

	// 目标背后扇形上取 7 个候选：从正背后（180°）中心向外对称外扩，投影到导航网格，
	// 取第一个成功的（背后死角优先）。注意 SampleArcDegrees 是半弧：取样范围 180°±SampleArcDegrees。
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector Facing = Target->GetActorForwardVector().GetSafeNormal2D();
	const float HalfArc = FMath::Max(SampleArcDegrees, 1.f);

	for (int32 CandidateIndex = 0; CandidateIndex < 7; ++CandidateIndex)
	{
		// 顺序：0°→±1/3 弧→±2/3 弧→±整弧，正背后永远最先试
		const int32 Ring = (CandidateIndex + 1) / 2;
		const float Sign = (CandidateIndex % 2 == 0) ? 1.f : -1.f;
		const float Degrees = 180.f + Sign * Ring * (HalfArc / 3.f);
		const FVector Candidate = TargetLocation + Facing.RotateAngleAxis(Degrees, FVector::UpVector).GetSafeNormal2D() * FlankDistance;

		FNavLocation Out;
		// 投影距离校验：投影点离候选 >150cm 说明被墙/箱子挤到了障碍另一侧的 navmesh（穿墙投影），
		// 走过去必然绕大圈甚至贴墙卡住——弃用，换下一个候选
		if (NavSys->ProjectPointToNavigation(Candidate, Out)
			&& FVector::Dist2D(Out.Location, Candidate) <= 150.f)
		{
			OutPoint = Out.Location;
			return true;
		}
	}
	return false;
}
