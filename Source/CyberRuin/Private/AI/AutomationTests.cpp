// 怪物 AI 寻路件自动化测试（方案 §五 巡逻折返 / §十 攻击名额）
// 运行方式（无头）：
//   UnrealEditor-Cmd.exe CyberRuin.uproject -ExecCmds="Automation RunTests CyberRuin; Quit" -TestExit="Automation Test Queue Empty" -unattended -nullrhi

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/AttackSlotSubsystem.h"
#include "AI/PatrolRouteComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"

namespace
{
	UWorld* MakeTestWorld(const TCHAR* WorldName)
	{
		// CreateWorld 内部已自带持久关卡与子系统初始化（含 WorldSettings 生成）。
		// 不要再手动调 InitializeNewWorld：会导致 WorldSettings 二次生成 →
		// "Cannot generate unique name for 'WorldSettings'" 致命错（UE 5.8 实测）。
		return UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
	}

	ATargetPoint* SpawnPoint(UWorld* World, const FVector& Location)
	{
		ATargetPoint* Point = World ? World->SpawnActor<ATargetPoint>() : nullptr;
		if (Point)
		{
			Point->SetActorLocation(Location);
		}
		return Point;
	}
}

// ---- 巡逻往返折返：Num=3 期望 GetCurrentPoint 序列 1,2,1,0,1,2（端点折返跳过、无重复无死循环）----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyberRuinPatrolPingPongTest, "CyberRuin.AI.PatrolRoute.PingPong",
	EAutomationTestFlags::EngineFilter | EAutomationTestFlags::EditorContext)

bool FCyberRuinPatrolPingPongTest::RunTest(const FString& Parameters)
{
	UWorld* World = MakeTestWorld(TEXT("CyberRuinPatrolRouteTest"));
	if (!TestNotNull(TEXT("测试世界"), World))
	{
		return false;
	}

	UPatrolRouteComponent* Route = NewObject<UPatrolRouteComponent>(World);
	ATargetPoint* P0 = SpawnPoint(World, FVector(0.f, 0.f, 0.f));
	ATargetPoint* P1 = SpawnPoint(World, FVector(400.f, 0.f, 0.f));
	ATargetPoint* P2 = SpawnPoint(World, FVector(800.f, 0.f, 0.f));
	Route->Points.Add(P0);
	Route->Points.Add(P1);
	Route->Points.Add(P2);

	TestTrue(TEXT("初始点"), Route->GetCurrentPoint() == P0);
	Route->Advance(); TestTrue(TEXT("→1"), Route->GetCurrentPoint() == P1);
	Route->Advance(); TestTrue(TEXT("→2"), Route->GetCurrentPoint() == P2);
	Route->Advance(); TestTrue(TEXT("折返→1"), Route->GetCurrentPoint() == P1);
	Route->Advance(); TestTrue(TEXT("→0"), Route->GetCurrentPoint() == P0);
	Route->Advance(); TestTrue(TEXT("折返→1"), Route->GetCurrentPoint() == P1);
	Route->Advance(); TestTrue(TEXT("→2"), Route->GetCurrentPoint() == P2);

	// 循环模式：2 → 0 → 1 → 2
	Route->bPingPong = false;
	Route->Advance(); TestTrue(TEXT("循环→0"), Route->GetCurrentPoint() == P0);
	Route->Advance(); TestTrue(TEXT("循环→1"), Route->GetCurrentPoint() == P1);
	Route->Advance(); TestTrue(TEXT("循环→2"), Route->GetCurrentPoint() == P2);

	// 单点：恒定
	UPatrolRouteComponent* RouteSingle = NewObject<UPatrolRouteComponent>(World);
	RouteSingle->Points.Add(P0);
	RouteSingle->Advance();
	TestTrue(TEXT("单点恒定"), RouteSingle->GetCurrentPoint() == P0);

	// 无路线：返回空
	UPatrolRouteComponent* RouteEmpty = NewObject<UPatrolRouteComponent>(World);
	RouteEmpty->Advance();
	TestNull(TEXT("无路线→空"), RouteEmpty->GetCurrentPoint());

	World->DestroyWorld(false);
	return true;
}

// ---- 攻击名额：8 方向 8 名额互异、满员拒绝、释放后复用 ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyberRuinAttackSlotTest, "CyberRuin.AI.AttackSlot.Sectors",
	EAutomationTestFlags::EngineFilter | EAutomationTestFlags::EditorContext)

bool FCyberRuinAttackSlotTest::RunTest(const FString& Parameters)
{
	UWorld* World = MakeTestWorld(TEXT("CyberRuinAttackSlotTest"));
	UAttackSlotSubsystem* Slots = World ? World->GetSubsystem<UAttackSlotSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("名额子系统"), Slots))
	{
		return false;
	}

	ATargetPoint* Target = SpawnPoint(World, FVector(0.f, 0.f, 0.f));

	// 8 个攻击者站在 45° 间隔、半径 300 的环上：每人应占到不同扇区
	TArray<ATargetPoint*> Attackers;
	TArray<FVector> AcquiredPoints;
	for (int32 AngleDeg = 0; AngleDeg < 360; AngleDeg += 45)
	{
		ATargetPoint* Attacker = SpawnPoint(World,
			FVector(FMath::Cos(FMath::DegreesToRadians(AngleDeg)), FMath::Sin(FMath::DegreesToRadians(AngleDeg)), 0.f) * 300.f);
		Attackers.Add(Attacker);

		FVector OutPoint = FVector::ZeroVector;
		TestTrue(*FString::Printf(TEXT("角度 %d 占位成功"), AngleDeg), Slots->TryAcquireSlot(Attacker, Target, OutPoint));
		AcquiredPoints.Add(OutPoint);

		// 站位都落在半径 160 的站位环上
		TestTrue(*FString::Printf(TEXT("角度 %d 站位在环上"), AngleDeg),
			FMath::IsNearlyEqual(OutPoint.Size2D(), 160.f, 1.f));
	}

	// 8 个站位两两互异
	for (int32 i = 0; i < AcquiredPoints.Num(); ++i)
	{
		for (int32 j = i + 1; j < AcquiredPoints.Num(); ++j)
		{
			TestTrue(*FString::Printf(TEXT("站位 %d 与 %d 不重合"), i, j), !(AcquiredPoints[i] == AcquiredPoints[j]));
		}
	}

	// 第 9 个：满员，拒绝（调用方应转游走，不硬挤）
	ATargetPoint* Ninth = SpawnPoint(World, FVector(1000.f, 1000.f, 0.f));
	FVector Ignored = FVector::ZeroVector;
	TestFalse(TEXT("第 9 名额满员拒绝"), Slots->TryAcquireSlot(Ninth, Target, Ignored));

	// 释放 0 号方向后：同方向新攻击者能占到位置
	Slots->Release(Attackers[0]);
	ATargetPoint* Replacement = SpawnPoint(World, FVector(305.f, 0.f, 0.f));
	TestTrue(TEXT("释放后复用"), Slots->TryAcquireSlot(Replacement, Target, Ignored));

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
