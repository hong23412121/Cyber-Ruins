#include "AI/CyberEnemyAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "StateTree.h"

ACyberEnemyAIController::ACyberEnemyAIController()
{
	// StateTree 运行组件（AI 版，Context Actor = 本 AIC）。
	// 同时挂进 AIC 的 Brain 槽：引擎在 Possess 流程自动 StartLogic，与旧 BT 同一套生命周期。
	UStateTreeAIComponent* STComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComp"));
	StateTreeComp = STComp;
	BrainComponent = STComp;

	// 感知组件（方案 §3.3：视觉 20m、正面 140° 锥）——C++ 配置，免蓝图连线
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	UAISenseConfig_Sight* Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	Sight->SightRadius = 2000.f;                    // 看见 20m
	Sight->LoseSightRadius = 2500.f;                // 丢失 25m
	Sight->PeripheralVisionAngleDegrees = 70.f;    // 半角 70°（正面 140° 锥）
	Sight->DetectionByAffiliation.bDetectEnemies = true;
	Sight->DetectionByAffiliation.bDetectNeutrals = true;
	Sight->DetectionByAffiliation.bDetectFriendlies = true;
	Sight->AutoSuccessRangeFromLastSeenLocation = 300.f; // 贴脸必发现
	Sight->SetMaxAge(5.f);
	if (PerceptionComponent)
	{
		PerceptionComponent->ConfigureSense(*Sight);
		PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
		PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ACyberEnemyAIController::HandleTargetPerceptionUpdated);
	}

	// RVO 群体避让参数（方案 §3.3）：AAIController 默认已带 CrowdFollowingComponent，这里只调参。
	// PathFollowingComponent 是 private 成员，用公开的 GetPathFollowingComponent() 取
	if (UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SetCrowdSeparation(true, true);                    // 群体分离：防围攻时重叠
		Crowd->SetCrowdSeparationWeight(1.0f, true);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium, true); // 质量 Medium(2)
	}
}

void ACyberEnemyAIController::OnPossess(APawn* InPawn)
{
	// SetStateTree 在逻辑运行中是 no-op：若组件已随 BeginPlay 自启动，先停再换再启
	if (StateTreeComp && EnemyStateTree)
	{
		const bool bWasRunning = StateTreeComp->IsRunning();
		if (bWasRunning)
		{
			StateTreeComp->StopLogic(TEXT("RebindStateTree"));
		}
		StateTreeComp->SetStateTree(EnemyStateTree);
		if (bWasRunning)
		{
			StateTreeComp->StartLogic();
		}
	}

	// 记录出生点：哨兵 15m / 裁决者 30m 的 leash 基准（StateTree 条件直接读）
	HomeLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;

	Super::OnPossess(InPawn);
}

void ACyberEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	CurrentTarget = Actor;
	bCanSeeTarget = Stimulus.WasSuccessfullySensed();

	UE_LOG(LogTemp, Display, TEXT("[Perception] %s %s %s（距离 %.0fcm）"),
		*GetNameSafe(GetPawn()),
		bCanSeeTarget ? TEXT("看见") : TEXT("丢失"),
		*GetNameSafe(Actor),
		Actor ? FVector::Dist(GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector, Actor->GetActorLocation()) : -1.f);

	if (!StateTreeComp)
	{
		return;
	}

	if (bCanSeeTarget)
	{
		// 看见：事件带目标载荷，追击状态可把 Payload.Target 绑给移动任务
		SeenPayload.Target = Actor;
		StateTreeComp->SendStateTreeEvent(FCyberRuinNativeTags::Get().EnemySeen, FConstStructView::Make(SeenPayload), TEXT("Perception"));
	}
	else
	{
		StateTreeComp->SendStateTreeEvent(FCyberRuinNativeTags::Get().EnemyLost, FConstStructView(), TEXT("Perception"));
	}
}

void ACyberEnemyAIController::FreezeLogic()
{
	StopMovement();
	if (StateTreeComp)
	{
		StateTreeComp->PauseLogic(TEXT("Freeze"));
	}
}

void ACyberEnemyAIController::UnfreezeLogic()
{
	if (StateTreeComp)
	{
		StateTreeComp->ResumeLogic(TEXT("Unfreeze"));
	}
}
