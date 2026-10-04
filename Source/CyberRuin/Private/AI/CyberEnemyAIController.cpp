#include "AI/CyberEnemyAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "StateTree.h"

ACyberEnemyAIController::ACyberEnemyAIController()
{
	// StateTree 运行组件（AI 版，Context Actor = 本 AIC）。
	// 同时挂进 AIC 的 Brain 槽：引擎在 Possess 流程自动 StartLogic，与旧 BT 同一套生命周期。
	UStateTreeAIComponent* STComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComp"));
	StateTreeComp = STComp;
	BrainComponent = STComp;

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

	Super::OnPossess(InPawn);
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
