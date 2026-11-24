#include "AI/CyberEnemyAIController.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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
	// Create Default Subobject：创造出厂默认零件
	// TEXT("StateTreeComp")：属性
	UStateTreeAIComponent* STComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComp"));
	StateTreeComp = STComp;  //protected类型，场监
	BrainComponent = STComp;  //  AAIController（父类）带的大脑插槽，插上放映机（场监）

	// 感知组件（方案 §3.3：视觉 20m、正面 140° 锥）——C++ 配置，免蓝图连线
	// AIPerceptionComponent眼睛；AISenseConfig_Sight视力表
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	UAISenseConfig_Sight* Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	//以下为配置
	Sight->SightRadius = 2000.f;                    // 近于20m标位看到
	Sight->LoseSightRadius = 2500.f;                // 远于25m丢失
	Sight->PeripheralVisionAngleDegrees = 70.f;    // 半角 70°（正面 140° 锥）
	Sight->DetectionByAffiliation.bDetectEnemies = true;  //是否报告敌人呢/中立/友方阵营。由于现在没有分阵营，所以全部报告
	Sight->DetectionByAffiliation.bDetectNeutrals = true;
	Sight->DetectionByAffiliation.bDetectFriendlies = true;
	Sight->AutoSuccessRangeFromLastSeenLocation = 300.f; // 贴脸3m内，就算人物被物体挡住也会保持发现状态
	Sight->SetMaxAge(5.f); // 丢失位置记忆保留5s。好处：裁决者搜寻姿态/玩家出现在同一位置不需要重新跑流程，减少流程时长
	
	if (PerceptionComponent)
	{
		PerceptionComponent->ConfigureSense(*Sight); // 把配置好的视力表装到眼睛上。Sight为视力表的指针类，参数为实体
		PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass()); // 设置主要感知为视觉类的固定类
		// StaticClass() 是每个 UCLASS 自动获得的引擎方法：返回这个类本身的属性列表
		PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ACyberEnemyAIController::HandleTargetPerceptionUpdated);
		// 感知系统。AddDynamic订阅有事喊HandleTargetPerceptionUpdated方法。ACyberEnemyAIController就是本类。取地址是取哪一页，类名是身份的一部分
		// this是对象的部分而不是类的部分，属于是伪实例化，是员工，十个怪物有十个this，但是十个怪通用一个HandleTargetPerceptionUpdated方法
	}

	// RVO ·群体避让参数（方案 §3.3）：AAIController 默认已带 CrowdFollowingComponent，这里只调参。
	// PathFollowingComponent 是 private 成员，用公开的 GetPathFollowingComponent() 取
	// 设置通用的群体避让属性/初值
	if (UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SetCrowdSeparation(true, true);                    // 群体分离：防围攻时重叠
		Crowd->SetCrowdSeparationWeight(1.0f, true);                  // 挤的时候互相推开的力度
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium, true); // 质量 Medium(2)
	}
}

void ACyberEnemyAIController::OnPossess(APawn* InPawn)
{
	// SetStateTree 在逻辑运行中是 no-op：若组件已随 BeginPlay 自启动，先停再换再启
	// 放映机正在运行时，无法调用setstatetree（若调用会没反应），所以要先暂停放映机，插入剧本后继续执行。核心就是插入剧本
	if (StateTreeComp && EnemyStateTree)
	{
		const bool bWasRunning = StateTreeComp->IsRunning();
		if (bWasRunning)
		{
			StateTreeComp->StopLogic(TEXT("RebindStateTree")); // TEXT放的是写给日志的停机原因
		}
		StateTreeComp->SetStateTree(EnemyStateTree);
		if (bWasRunning)
		{
			StateTreeComp->StartLogic();
		}
	}

	// 记录出生点：哨兵 15m / 裁决者 30m 的 leash 基准（StateTree 条件直接读）
	HomeLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	// 获取实物此刻的身体坐标，防御性写法

	Super::OnPossess(InPawn);
	// 把标记好的身体给父类，让他完成剩下的登记流程。结束后大脑就可以接管怪物的身体了
}

void ACyberEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 已死不再感知（死了就是死了，别躺着还喊看见）
	if (bDead)
	{
		return;
	}

	// 敌我过滤（方案 §11 集成约束）：怪只把玩家当目标，怪与怪互不触发追击/绕后，
	// 否则哨兵会去追掠食者、掠食者绕到哨兵背后——测试关卡多怪同屏时尤其致命
	const APawn* SensedPawn = Cast<APawn>(Actor);
	if (!SensedPawn || !SensedPawn->IsPlayerControlled())
	{
		return;
	}

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

void ACyberEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 已死：死亡动画播完（非循环引擎自动冻末帧）→ 留尸倒计时 → 销毁退场；步态/调试全停
	if (bDead)
	{
		if (ActionRemaining > 0.f)
		{
			ActionRemaining -= DeltaSeconds;
			if (ActionRemaining <= 0.f)
			{
				ActionRemaining = 0.f;
				CorpseRemaining = CorpseLifespanSeconds; // 动画播完，进入留尸期（保持倒地姿势躺 5s）
			}
		}
		else if (CorpseRemaining > 0.f)
		{
			CorpseRemaining -= DeltaSeconds;
			if (CorpseRemaining <= 0.f)
			{
				UE_LOG(LogTemp, Display, TEXT("[Death] %s 留尸 %.0fs 期满，销毁退场"), *GetNameSafe(GetPawn()), CorpseLifespanSeconds);
				if (APawn* MyPawn = GetPawn())
				{
					MyPawn->Destroy();
				}
				Destroy(); // 控制器随身体一起退场，别留孤儿 AIC 占内存
			}
		}
		return;
	}

	// 调试自动挨刀（受击链路测试通路，玩家侧攻击就绪后关旗）：玩家贴近 250cm 内每 1.5s 挨 1 刀
	if (bDebugAutoTakeHit)
	{
		UWorld* World = GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
		if (PlayerPawn && GetPawn() && FVector::Dist2D(GetPawn()->GetActorLocation(), PlayerPawn->GetActorLocation()) <= 250.f)
		{
			DebugHitTimer -= DeltaSeconds;
			if (DebugHitTimer <= 0.f)
			{
				DebugHitTimer = 1.5f;
				TryReceiveHit(PlayerPawn, 250.f, 1);
			}
		}
		else
		{
			DebugHitTimer = 1.5f; // 离范围重置满弦：走近后第 1.5s 才挨第一刀
		}
	}

	// 灰盒单节点动画驱动（走 ABP 的怪如哨兵在此直接返回，零开销）：
	// 速度 → BlendSpace 混合位置；一次性动作（背刺/开火/受击）播完恢复跑动
	UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnim();
	if (!SingleNode)
	{
		return;
	}

	if (ActionRemaining > 0.f)
	{
		ActionRemaining -= DeltaSeconds;
		if (ActionRemaining <= 0.f)
		{
			ActionRemaining = 0.f;
			if (LocomotionBlendSpace)
			{
				// 非循环动作播到末尾时引擎自动 SetPlaying(false)（AnimSingleNodeInstanceProxy 非循环分支），
				// 而 SetAnimationAsset 不会拉回播放状态——不显式 SetPlaying(true)，怪会永远冻在动作末帧姿势平移
				SingleNode->SetAnimationAsset(LocomotionBlendSpace, true, LocomotionPlayRate);
				SingleNode->SetPlaying(true);
				// 立即喂当前速度，避免恢复瞬间闪一帧零速待机
				const APawn* MyPawn = GetPawn();
				SingleNode->SetBlendSpacePosition(FVector(MyPawn ? MyPawn->GetVelocity().Size2D() : 0.f, 0.f, 0.f));
			}
		}
		return;
	}

	if (LocomotionBlendSpace)
	{
		const APawn* MyPawn = GetPawn();
		SingleNode->SetBlendSpacePosition(FVector(MyPawn ? MyPawn->GetVelocity().Size2D() : 0.f, 0.f, 0.f));
		SingleNode->SetPlayRate(LocomotionPlayRate);
	}
}

void ACyberEnemyAIController::PlayPrimaryAction()
{
	PlayOneShot(PrimaryActionAnim);
}

void ACyberEnemyAIController::PlayHitReact()
{
	PlayOneShot(HitReactAnim);
}

USkeletalMeshComponent* ACyberEnemyAIController::GetEnemyMesh() const
{
	APawn* MyPawn = GetPawn();
	return MyPawn ? MyPawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

UAnimSingleNodeInstance* ACyberEnemyAIController::GetSingleNodeAnim() const
{
	USkeletalMeshComponent* EnemyMesh = GetEnemyMesh();
	return EnemyMesh ? Cast<UAnimSingleNodeInstance>(EnemyMesh->GetAnimInstance()) : nullptr;
}

void ACyberEnemyAIController::PlayOneShot(UAnimSequence* Anim)
{
	// 只对单节点步态的怪生效（哨兵走 ABP，不在此播灰盒动作）；动作未接或上一发没播完则跳过
	UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnim();
	if (!SingleNode || !Anim || !LocomotionBlendSpace || ActionRemaining > 0.f)
	{
		return;
	}

	SingleNode->SetAnimationAsset(Anim, false, 1.f);
	SingleNode->SetPlaying(true);
	ActionRemaining = Anim->GetPlayLength() + 0.05f;
}

bool ACyberEnemyAIController::TryReceiveHit(AActor* Attacker, float HitRange, int32 Damage)
{
	if (bDead || !Attacker || !GetPawn() || Damage <= 0)
	{
		return false;
	}

	// 距离击中判定（灰盒约定：进圈即中；正式弹道/投掷物/碰撞由 Gameplay 层接管后替换）
	const float Dist = FVector::Dist2D(GetPawn()->GetActorLocation(), Attacker->GetActorLocation());
	if (Dist > HitRange)
	{
		UE_LOG(LogTemp, Display, TEXT("[Hurt] %s 未被 %s 击中（距离 %.0f > %.0f）"),
			*GetNameSafe(GetPawn()), *GetNameSafe(Attacker), Dist, HitRange);
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Hurt] %s 被 %s 击中（距离 %.0f ≤ %.0f）"),
		*GetNameSafe(GetPawn()), *GetNameSafe(Attacker), Dist, HitRange);
	ApplyHit(Damage);
	return true;
}

void ACyberEnemyAIController::ApplyHit(int32 Damage)
{
	if (bDead)
	{
		return;
	}

	Health -= Damage;
	if (Health <= 0.f)
	{
		Die();
	}
	else
	{
		// 受击表现：攻击动作播到一半时挨刀不顶掉攻击（PlayOneShot 在播即跳过）——出招者不轻易硬直
		PlayHitReact();
		UE_LOG(LogTemp, Warning, TEXT("[Hurt] %s 剩余血量 %.0f"), *GetNameSafe(GetPawn()), Health);
	}
}

void ACyberEnemyAIController::Die()
{
	bDead = true;
	StopMovement();
	if (StateTreeComp)
	{
		StateTreeComp->StopLogic(TEXT("Death")); // 行为全停（巡逻/追击/攻击全下岗）
	}
	UE_LOG(LogTemp, Warning, TEXT("[Death] %s 血量归零，播放死亡动画（冻末帧留尸 %.0fs 后销毁）"),
		*GetNameSafe(GetPawn()), CorpseLifespanSeconds);

	// 播死亡动画：非循环播完引擎自动冻末帧；Tick 的 bDead 分支不进跑动恢复，倒地姿势定格到底
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnim(); SingleNode && DeathAnim)
	{
		SingleNode->SetAnimationAsset(DeathAnim, false, 1.f);
		SingleNode->SetPlaying(true);
		ActionRemaining = DeathAnim->GetPlayLength() + 0.05f;
	}
	else
	{
		// 死亡动画没配（或皮不在）：直接以当前姿势进入留尸期
		CorpseRemaining = CorpseLifespanSeconds;
	}
}
