#include "CyberRuinBuildAssetsCommandlet.h"

#include "AI/AntiStuckComponent.h"
#include "AI/CyberEnemyAIController.h"
#include "AI/PatrolRouteComponent.h"
#include "AI/StateTreeArbiterTasks.h"
#include "AI/StateTreeEnemyConditions.h"
#include "AI/StateTreeEnemyEvents.h"
#include "AI/StateTreePredatorTasks.h"
#include "AI/StateTreeSentinelTasks.h"
#include "AI/SwarmDrone.h"
#include "AI/SwarmHive.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	/** 资产已存在时，启动期资产注册表扫描可能让包处于"部分加载"状态——读写前先补全加载 */
	void EnsurePackageFullyLoaded(UPackage* Package)
	{
		FlushAsyncLoading();
		if (Package && !Package->IsFullyLoaded())
		{
			Package->FullyLoad();
		}
	}

	/** 给 BP 烘焙一个头顶彩色名牌（SCS 节点 NameTag，幂等：已存在则只更新模板属性）。
	 *  测试关四种地面怪共用 Manny 皮，靠名牌颜色 + 字样 + 体型区分谁是谁。 */
	void AddOrUpdateNameTag(UBlueprint* BP, const FString& LabelText, const FColor& LabelColor, float RelativeZ)
	{
		if (!BP || !BP->SimpleConstructionScript || LabelText.IsEmpty())
		{
			return;
		}
		USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
		USCS_Node* LabelNode = SCS->FindSCSNode(TEXT("NameTag"));
		if (!LabelNode)
		{
			LabelNode = SCS->CreateNode(UTextRenderComponent::StaticClass(), TEXT("NameTag"));
			if (LabelNode)
			{
				SCS->AddNode(LabelNode);
			}
		}
		UTextRenderComponent* LabelTemplate = LabelNode ? Cast<UTextRenderComponent>(LabelNode->ComponentTemplate) : nullptr;
		if (LabelTemplate)
		{
			LabelTemplate->SetText(FText::FromString(LabelText));
			LabelTemplate->SetTextRenderColor(LabelColor);
			LabelTemplate->SetWorldSize(30.f);
			LabelTemplate->SetHorizontalAlignment(EHTA_Center);
			LabelTemplate->SetVerticalAlignment(EVRTA_TextCenter);
			LabelTemplate->SetRelativeLocation(FVector(0.f, 0.f, RelativeZ));
		}
	}

	/** 在指定资产路径创建（或复用）包里的主对象 */
	template<typename T>
	T* GetOrCreateAsset(const FString& AssetPath, const FName AssetName)
	{
		UPackage* Package = CreatePackage(*AssetPath);
		EnsurePackageFullyLoaded(Package);
		if (T* Existing = FindObject<T>(Package, *AssetName.ToString()))
		{
			return Existing;
		}
		return NewObject<T>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
	}

	/** 保存资产到磁盘 */
	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = CastChecked<UPackage>(Asset->GetOuter());
		EnsurePackageFullyLoaded(Package);
		Package->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (FileName.IsEmpty())
		{
			return false;
		}

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone | RF_Transactional;
		return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
	}

	/** 建蓝图（已存在则直接复用） */
	UBlueprint* GetOrCreateBlueprint(UClass* ParentClass, const FString& AssetPath, const FName AssetName)
	{
		UPackage* Package = CreatePackage(*AssetPath);
		EnsurePackageFullyLoaded(Package);
		if (UBlueprint* Existing = FindObject<UBlueprint>(Package, *AssetName.ToString()))
		{
			return Existing;
		}

		return FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, AssetName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	}

	/** 建一棵新 StateTree（换新 EditorData = 幂等重建）并挂 AI Schema */
	UStateTree* CreateStateTree(const FString& AssetPath, const FName AssetName)
	{
		UStateTree* StateTree = GetOrCreateAsset<UStateTree>(AssetPath, AssetName);

		// 每次重建：整体换新 EditorData（状态/绑定都以它为 Outer，换掉即全部重置，天然幂等）
		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
		StateTree->EditorData = EditorData;

		// AI Schema：保证任务能访问 AIController（Context 自动绑定）
		EditorData->Schema = NewObject<UStateTreeAIComponentSchema>(EditorData);
		return StateTree;
	}

	/** 编译 StateTree（EditorData → 运行时数据），失败返回 nullptr */
	UStateTree* CompileStateTree(UStateTree* StateTree, const TCHAR* TreeName)
	{
		UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData.Get());
		EditorData->ReparentStates();
		EditorData->UpdateBindings();
		FStateTreeCompilerLog CompileLog;
		FStateTreeCompiler Compiler(CompileLog);
		const bool bCompiled = Compiler.Compile(StateTree);
		UE_LOG(LogTemp, Display, TEXT("[CyberRuinBuildAssets] %s 编译 %s"), TreeName, bCompiled ? TEXT("成功") : TEXT("失败"));
		if (!bCompiled)
		{
			CompileLog.DumpToLog(LogTemp);
			return nullptr;
		}
		return StateTree;
	}

	/** 卡死 → 解卡：各树共用的最高优先解卡转换（Unstick 状态自身也带，挪窝失败原地重试） */
	void AddStuckTransitions(UStateTreeState& State, UStateTreeState& Unstick)
	{
		FStateTreeTransition& StuckTransition = State.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Unstick);
		StuckTransition.Priority = EStateTreeTransitionPriority::High;
		StuckTransition.AddConditionWithOuter<FStateTreeIsStuckCondition>(&State);
	}
}

UCyberRuinBuildAssetsCommandlet::UCyberRuinBuildAssetsCommandlet()
{
	IsClient = false;
	LogToConsole = true;
}

UStateTree* UCyberRuinBuildAssetsCommandlet::BuildSentinelStateTree()
{
	UStateTree* StateTree = CreateStateTree(TEXT("/Game/XuTang/ST_Sentinel"), TEXT("ST_Sentinel"));
	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData.Get());

	// ---------- 状态机结构（方案 §11.2 哨兵表，自包含任务、零属性绑定） ----------
	UStateTreeState& Root = EditorData->AddRootState();

	// 状态 1：巡逻（任务内自循环：到达/走不通自动取下一点，永不主动完成）
	UStateTreeState& Patrol = Root.AddChildState(TEXT("巡逻"));
	Patrol.AddTask<FStateTreePatrolMoveTask>();  // AcceptanceRadius 默认 60

	// 状态 2：追击（直接读 AIC 的 CurrentTarget，目标变化自动重新寻路）
	UStateTreeState& Chase = Root.AddChildState(TEXT("追击"));
	Chase.AddTask<FStateTreeChaseTargetTask>();  // AcceptanceRadius 默认 120

	// 状态 3：解卡（挪窝）
	UStateTreeState& Unstick = Root.AddChildState(TEXT("解卡"));
	Unstick.AddTask<FStateTreeWarpUnstuckTask>().GetInstanceData().SearchRadius = 200.f;

	// ---------- 转换（卡死最高优先，每状态一条） ----------
	AddStuckTransitions(Patrol, Unstick);
	AddStuckTransitions(Chase, Unstick);
	AddStuckTransitions(Unstick, Unstick);

	// 巡逻 → 追击：看见敌人事件（感知 → AIC 发送）
	Patrol.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemySeen, EStateTreeTransitionType::GotoState, &Chase);

	// 追击 → 巡逻：丢失目标 / 目标为空兜底（不限距离追击：不做离家折返，追到真看不见为止）
	Chase.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemyLost, EStateTreeTransitionType::GotoState, &Patrol);
	Chase.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Patrol);

	// 解卡 → 回巡逻
	Unstick.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Patrol);

	return CompileStateTree(StateTree, TEXT("ST_Sentinel"));
}

UStateTree* UCyberRuinBuildAssetsCommandlet::BuildPredatorStateTree()
{
	UStateTree* StateTree = CreateStateTree(TEXT("/Game/XuTang/ST_Predator"), TEXT("ST_Predator"));
	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData.Get());

	// ---------- 状态机结构（方案 §11.3 掠食者表） ----------
	UStateTreeState& Root = EditorData->AddRootState();

	// 状态 1：索敌漫游（绕出生点环带永续游走，等感知看见玩家）
	UStateTreeState& Roam = Root.AddChildState(TEXT("索敌漫游"));
	Roam.AddTask<FStateTreePredatorRoamTask>();

	// 状态 2：绕后接近（GetFlankPoint 取目标背后点 → 跟随刷新，到点转背刺）
	UStateTreeState& Flank = Root.AddChildState(TEXT("绕后接近"));
	Flank.AddTask<FStateTreePredatorFlankTask>();

	// 状态 3：贴身背刺（从绕后点直冲目标，贴近后判背后 90° 扇形）
	UStateTreeState& Backstab = Root.AddChildState(TEXT("贴身背刺"));
	Backstab.AddTask<FStateTreePredatorBackstabTask>();

	// 状态 4：被发现游走（环玩家游走 3 秒后撤离）
	UStateTreeState& Spotted = Root.AddChildState(TEXT("被发现游走"));
	Spotted.AddTask<FStateTreePredatorSpottedTask>();

	// 状态 5：解卡（挪窝，小半径防大位移穿帮）
	UStateTreeState& Unstick = Root.AddChildState(TEXT("解卡"));
	Unstick.AddTask<FStateTreeWarpUnstuckTask>().GetInstanceData().SearchRadius = 100.f;

	// ---------- 转换 ----------
	AddStuckTransitions(Roam, Unstick);
	AddStuckTransitions(Flank, Unstick);
	AddStuckTransitions(Backstab, Unstick);
	AddStuckTransitions(Spotted, Unstick);
	AddStuckTransitions(Unstick, Unstick);

	// 索敌漫游 → 绕后接近：看见敌人事件；或目标仍在视野（背刺/游走结束后延迟 1.5s 再绕，
	// 给"再绕一圈"留出走位节奏，防 0.6s 一轮的高频贴脸循环）
	Roam.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemySeen, EStateTreeTransitionType::GotoState, &Flank);
	FStateTreeTransition& Reacquire = Roam.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Flank);
	Reacquire.AddConditionWithOuter<FStateTreeHasTargetCondition>(&Roam);
	Reacquire.bDelayTransition = true;
	Reacquire.DelayDuration = 1.5f;

	// 绕后接近 → 被发现游走：玩家视线锥内看见掠食者（中优先，先于到点判定）
	FStateTreeTransition& SpottedFromFlank = Flank.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Spotted);
	SpottedFromFlank.Priority = EStateTreeTransitionPriority::Medium;
	SpottedFromFlank.AddConditionWithOuter<FStateTreeIsSpottedCondition>(&Flank);

	// 贴身背刺 → 被发现游走：贴近途中被玩家扭头看见同样打断
	FStateTreeTransition& SpottedFromBackstab = Backstab.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Spotted);
	SpottedFromBackstab.Priority = EStateTreeTransitionPriority::Medium;
	SpottedFromBackstab.AddConditionWithOuter<FStateTreeIsSpottedCondition>(&Backstab);

	// 绕后接近 → 贴身背刺：到点完成（目标丢失时任务 Failed 也会走这条，背刺无目标即 Failed 兜回漫游）
	Flank.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Backstab);

	// 贴身背刺 → 索敌漫游：命中/未中都回漫游（命中日志由任务打，大伤害后续由 Gameplay 挂钩）
	Backstab.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Roam);

	// 被发现游走 → 索敌漫游：游走完毕 / 目标丢失
	Spotted.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Roam);

	// 解卡 → 回索敌漫游
	Unstick.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Roam);

	return CompileStateTree(StateTree, TEXT("ST_Predator"));
}

UStateTree* UCyberRuinBuildAssetsCommandlet::BuildArbiterStateTree()
{
	UStateTree* StateTree = CreateStateTree(TEXT("/Game/XuTang/ST_Arbiter"), TEXT("ST_Arbiter"));
	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData.Get());

	// ---------- 状态机结构（方案 §11.4 裁决者表） ----------
	UStateTreeState& Root = EditorData->AddRootState();

	// 状态 1：守盾警戒（站岗面朝目标开火，盾不破永远不追；离岗超距走回）
	UStateTreeState& Guard = Root.AddChildState(TEXT("守盾警戒"));
	Guard.AddTask<FStateTreeArbiterGuardTask>();

	// 状态 2：追杀（破盾后才进得来，复用哨兵追击任务）
	UStateTreeState& Chase = Root.AddChildState(TEXT("追杀"));
	Chase.AddTask<FStateTreeChaseTargetTask>();  // AcceptanceRadius 默认 120

	// 状态 3：解卡（挪窝，小半径防穿帮）
	UStateTreeState& Unstick = Root.AddChildState(TEXT("解卡"));
	Unstick.AddTask<FStateTreeWarpUnstuckTask>().GetInstanceData().SearchRadius = 100.f;

	// ---------- 转换 ----------
	AddStuckTransitions(Guard, Unstick);
	AddStuckTransitions(Chase, Unstick);
	AddStuckTransitions(Unstick, Unstick);

	// 守盾警戒 → 追杀：盾破事件门槛（IsShieldBroken 读 AIC bShieldBroken，Gameplay 破盾时写入）
	FStateTreeTransition& BreakTrans = Guard.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Chase);
	BreakTrans.Priority = EStateTreeTransitionPriority::Medium;
	BreakTrans.AddConditionWithOuter<FStateTreeIsShieldBrokenCondition>(&Guard);

	// 追杀 → 守盾警戒：目标丢失 / 追出 30m 折返回岗（方案 §11.4：leash 比哨兵翻倍）
	Chase.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemyLost, EStateTreeTransitionType::GotoState, &Guard);
	FStateTreeTransition& LeashTrans = Chase.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Guard);
	LeashTrans.Priority = EStateTreeTransitionPriority::Medium;
	LeashTrans.AddConditionWithOuter<FStateTreeIsFarFromHomeCondition>(&Chase).GetInstanceData().MaxDistance = 3000.f;
	Chase.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Guard);

	// 解卡 → 回守盾警戒
	Unstick.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Guard);

	return CompileStateTree(StateTree, TEXT("ST_Arbiter"));
}

UStateTree* UCyberRuinBuildAssetsCommandlet::BuildAuditorStubStateTree()
{
	UStateTree* StateTree = CreateStateTree(TEXT("/Game/XuTang/ST_AuditorStub"), TEXT("ST_AuditorStub"));
	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData.Get());

	// ---------- 状态机结构（§九 审计官移动测试桩：索敌待机/追击/解卡） ----------
	UStateTreeState& Root = EditorData->AddRootState();

	// 状态 1：索敌待机（原地站着，看见玩家就走）
	UStateTreeState& Idle = Root.AddChildState(TEXT("索敌待机"));
	Idle.AddTask<FStateTreeIdleTask>();

	// 状态 2：追击（复用哨兵追击任务，贴到玩家跟前）
	UStateTreeState& Chase = Root.AddChildState(TEXT("追击"));
	Chase.AddTask<FStateTreeChaseTargetTask>();

	// 状态 3：解卡（挪窝）
	UStateTreeState& Unstick = Root.AddChildState(TEXT("解卡"));
	Unstick.AddTask<FStateTreeWarpUnstuckTask>().GetInstanceData().SearchRadius = 100.f;

	// ---------- 转换 ----------
	AddStuckTransitions(Chase, Unstick);
	AddStuckTransitions(Unstick, Unstick);

	// 索敌待机 → 追击：看见敌人
	Idle.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemySeen, EStateTreeTransitionType::GotoState, &Chase);

	// 追击 → 索敌待机：目标丢失 / 兜底
	Chase.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemyLost, EStateTreeTransitionType::GotoState, &Idle);
	Chase.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Idle);

	// 解卡 → 回索敌待机
	Unstick.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Idle);

	return CompileStateTree(StateTree, TEXT("ST_AuditorStub"));
}

UBlueprint* UCyberRuinBuildAssetsCommandlet::BuildEnemyAIC(const FString& AssetName, UStateTree* InStateTree, bool bDebugAutoBreakShield)
{
	UBlueprint* AICBlueprint = GetOrCreateBlueprint(
		ACyberEnemyAIController::StaticClass(), FString::Printf(TEXT("/Game/XuTang/%s"), *AssetName), *AssetName);
	if (!AICBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 建 %s 失败"), *AssetName);
		return nullptr;
	}

	// 默认值：指定 StateTree 资产 + 调试自动破盾开关（裁决者测试用）
	if (ACyberEnemyAIController* AICCDO = Cast<ACyberEnemyAIController>(AICBlueprint->GeneratedClass->GetDefaultObject()))
	{
		AICCDO->EnemyStateTree = InStateTree;
		AICCDO->bDebugAutoBreakShield = bDebugAutoBreakShield;
	}
	FKismetEditorUtilities::CompileBlueprint(AICBlueprint);
	return AICBlueprint;
}

UBlueprint* UCyberRuinBuildAssetsCommandlet::BuildEnemyPawn(UBlueprint* InAICBlueprint, const FString& AssetName, float MaxWalkSpeed, bool bAddPatrolRoute, const FString& LabelText, const FColor& LabelColor, float MeshScale)
{
	// 父类 = 洪韵然的 BP_BaseEnemy（Core 框架）
	UBlueprint* BaseEnemyBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Core/BaseClasses/BP_BaseEnemy.BP_BaseEnemy"));
	if (!BaseEnemyBP || !BaseEnemyBP->GeneratedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 找不到 BP_BaseEnemy（Content/Core/BaseClasses）"));
		return nullptr;
	}

	UBlueprint* EnemyBP = GetOrCreateBlueprint(
		BaseEnemyBP->GeneratedClass, FString::Printf(TEXT("/Game/XuTang/%s"), *AssetName), *AssetName);
	if (!EnemyBP)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 建 %s 失败"), *AssetName);
		return nullptr;
	}

	// SCS 加组件：卡死检测必带；巡逻路线只有巡逻型怪带（哨兵）
	if (USimpleConstructionScript* SCS = EnemyBP->SimpleConstructionScript)
	{
		if (!SCS->FindSCSNode(TEXT("AntiStuck")))
		{
			if (USCS_Node* Node = SCS->CreateNode(UAntiStuckComponent::StaticClass(), TEXT("AntiStuck")))
			{
				SCS->AddNode(Node);
			}
		}
		if (bAddPatrolRoute && !SCS->FindSCSNode(TEXT("PatrolRoute")))
		{
			if (USCS_Node* Node = SCS->CreateNode(UPatrolRouteComponent::StaticClass(), TEXT("PatrolRoute")))
			{
				SCS->AddNode(Node);
			}
		}
	}

	// 默认值：AIController 指定 + 移速覆盖（掠食者 420）
	if (ACharacter* EnemyCharCDO = Cast<ACharacter>(EnemyBP->GeneratedClass->GetDefaultObject()))
	{
		EnemyCharCDO->AIControllerClass = Cast<UClass>(InAICBlueprint->GeneratedClass);
		if (MaxWalkSpeed > 0.f && EnemyCharCDO->GetCharacterMovement())
		{
			EnemyCharCDO->GetCharacterMovement()->MaxWalkSpeed = MaxWalkSpeed;
		}

		// 挤压起跳根治：胶囊默认"可被踩踏"（CanCharacterStepUpOn），怪物互挤时会往对方胶囊上"踏上"一步，
		// 观感 = 卡住时疯狂起跳，且位移持续让 AntiStuck 永不触发。禁掉怪物间踩踏后，硬卡 3s 由解卡 Warp 兜底接管
		if (EnemyCharCDO->GetCapsuleComponent())
		{
			EnemyCharCDO->GetCapsuleComponent()->SetCanCharacterStepUpOn(false);
		}

		// 默认值：实体外观（出厂自带骨骼网格体，不再只是碰撞胶囊）
		// 玩家默认是 SKM_Quinn_Simple，怪用 SKM_Manny_Simple 天然区分敌我；动画复用项目 ABP_Unarmed
		USkeletalMeshComponent* EnemyMesh = EnemyCharCDO->GetMesh();
		USkeletalMesh* MannyMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		UBlueprint* AnimBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed"));
		if (EnemyMesh && MannyMesh && AnimBP && AnimBP->GeneratedClass)
		{
			EnemyMesh->SetSkeletalMesh(MannyMesh, false);
			EnemyMesh->SetAnimInstanceClass(Cast<UClass>(AnimBP->GeneratedClass));

			// 对齐胶囊体：复制模板角色 BP_ThirdPersonCharacter 的 Mesh 相对变换（Z 下移 + 朝向偏转），避免半截埋地/悬浮
			if (UBlueprint* TemplateCharBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter")))
			{
				if (const ACharacter* TemplateCDO = Cast<ACharacter>(TemplateCharBP->GeneratedClass->GetDefaultObject()))
				{
					if (const USkeletalMeshComponent* TemplateMesh = TemplateCDO->GetMesh())
					{
						EnemyMesh->SetRelativeLocation(TemplateMesh->GetRelativeLocation(), false);
						EnemyMesh->SetRelativeRotation(TemplateMesh->GetRelativeRotation(), false);
					}
				}
			}

			// 体型区分（只缩 Mesh 不缩胶囊，寻路/碰撞与已验证基线完全一致）：掠食者矮壮、裁决者魁梧、审计官高大
			EnemyMesh->SetRelativeScale3D(FVector(FMath::Max(MeshScale, 0.1f)));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CyberRuinBuildAssets] 缺少 SKM_Manny_Simple 或 ABP_Unarmed，%s 将没有实体外观"), *AssetName);
		}
	}

	// 头顶彩色名牌：同皮四怪在测试关靠名牌分清谁是谁（Z 随体型抬升）
	AddOrUpdateNameTag(EnemyBP, LabelText, LabelColor, 60.f + 130.f * FMath::Max(MeshScale, 0.1f));

	FKismetEditorUtilities::CompileBlueprint(EnemyBP);
	return EnemyBP;
}

UBlueprint* UCyberRuinBuildAssetsCommandlet::BuildSwarmDroneBlueprint()
{
	UBlueprint* DroneBP = GetOrCreateBlueprint(
		ASwarmDrone::StaticClass(), TEXT("/Game/XuTang/BP_Drone"), TEXT("BP_Drone"));
	if (!DroneBP)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 建 BP_Drone 失败"));
		return nullptr;
	}

	// CDO 烘焙可见网格体：引擎基础形状立方体缩到 50cm（原生 Mesh 组件缺省无网格，不可见没法测）
	if (ASwarmDrone* DroneCDO = Cast<ASwarmDrone>(DroneBP->GeneratedClass->GetDefaultObject()))
	{
		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/SM_Cube.SM_Cube"));
		if (!CubeMesh)
		{
			CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		if (CubeMesh && DroneCDO->Mesh)
		{
			DroneCDO->Mesh->SetStaticMesh(CubeMesh);
			DroneCDO->Mesh->SetRelativeScale3D(FVector(0.5f));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CyberRuinBuildAssets] 找不到引擎立方体网格，无人机将不可见"));
		}
	}
	FKismetEditorUtilities::CompileBlueprint(DroneBP);
	return DroneBP;
}

UBlueprint* UCyberRuinBuildAssetsCommandlet::BuildSwarmHiveBlueprint(UBlueprint* InDroneBlueprint)
{
	UBlueprint* HiveBP = GetOrCreateBlueprint(
		ASwarmHive::StaticClass(), TEXT("/Game/XuTang/BP_SwarmHive"), TEXT("BP_SwarmHive"));
	if (!HiveBP)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 建 BP_SwarmHive 失败"));
		return nullptr;
	}

	// DroneClass C++ 缺省为空——BeginPlay SpawnDrones 全靠它，必须显式赋值
	if (ASwarmHive* HiveCDO = Cast<ASwarmHive>(HiveBP->GeneratedClass->GetDefaultObject()))
	{
		HiveCDO->DroneClass = Cast<UClass>(InDroneBlueprint->GeneratedClass);
	}
	// 蜂群锚点也挂名牌，测试关一眼认出封锁区
	AddOrUpdateNameTag(HiveBP, TEXT("SwarmHive"), FColor(0, 255, 120), 160.f);
	FKismetEditorUtilities::CompileBlueprint(HiveBP);
	return HiveBP;
}

int32 UCyberRuinBuildAssetsCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("[CyberRuinBuildAssets] 开始构建怪物 AI 资产……"));

	// ---------- 哨兵 ----------
	UStateTree* SentinelTree = BuildSentinelStateTree();
	if (!SentinelTree || !SaveAsset(SentinelTree))
	{
		return 1;
	}
	UBlueprint* SentinelAIC = BuildEnemyAIC(TEXT("BP_AIC_Sentinel"), SentinelTree, false);
	if (!SentinelAIC || !SaveAsset(SentinelAIC))
	{
		return 1;
	}
	UBlueprint* SentinelPawn = BuildEnemyPawn(SentinelAIC, TEXT("BP_Enemy_Sentinel"), 0.f, true, TEXT("Sentinel"), FColor(200, 200, 255), 1.0f);
	if (!SentinelPawn || !SaveAsset(SentinelPawn))
	{
		return 1;
	}

	// ---------- 掠食者 ----------
	UStateTree* PredatorTree = BuildPredatorStateTree();
	if (!PredatorTree || !SaveAsset(PredatorTree))
	{
		return 1;
	}
	UBlueprint* PredatorAIC = BuildEnemyAIC(TEXT("BP_AIC_Predator"), PredatorTree, false);
	if (!PredatorAIC || !SaveAsset(PredatorAIC))
	{
		return 1;
	}
	UBlueprint* PredatorPawn = BuildEnemyPawn(PredatorAIC, TEXT("BP_Enemy_Predator"), 420.f, false, TEXT("Predator"), FColor(255, 70, 0), 0.85f);
	if (!PredatorPawn || !SaveAsset(PredatorPawn))
	{
		return 1;
	}

	// ---------- 裁决者 ----------
	UStateTree* ArbiterTree = BuildArbiterStateTree();
	if (!ArbiterTree || !SaveAsset(ArbiterTree))
	{
		return 1;
	}
	UBlueprint* ArbiterAIC = BuildEnemyAIC(TEXT("BP_AIC_Arbiter"), ArbiterTree, true);
	if (!ArbiterAIC || !SaveAsset(ArbiterAIC))
	{
		return 1;
	}
	UBlueprint* ArbiterPawn = BuildEnemyPawn(ArbiterAIC, TEXT("BP_Enemy_Arbiter"), 0.f, false, TEXT("Arbiter"), FColor(0, 150, 255), 1.3f);
	if (!ArbiterPawn || !SaveAsset(ArbiterPawn))
	{
		return 1;
	}

	// ---------- 审计官（移动测试桩） ----------
	UStateTree* AuditorTree = BuildAuditorStubStateTree();
	if (!AuditorTree || !SaveAsset(AuditorTree))
	{
		return 1;
	}
	UBlueprint* AuditorAIC = BuildEnemyAIC(TEXT("BP_AIC_Auditor"), AuditorTree, false);
	if (!AuditorAIC || !SaveAsset(AuditorAIC))
	{
		return 1;
	}
	UBlueprint* AuditorPawn = BuildEnemyPawn(AuditorAIC, TEXT("BP_Enemy_Auditor"), 0.f, false, TEXT("Auditor"), FColor(220, 0, 255), 1.55f);
	if (!AuditorPawn || !SaveAsset(AuditorPawn))
	{
		return 1;
	}

	// ---------- 蜂群 ----------
	UBlueprint* DroneBP = BuildSwarmDroneBlueprint();
	if (!DroneBP || !SaveAsset(DroneBP))
	{
		return 1;
	}
	UBlueprint* HiveBP = BuildSwarmHiveBlueprint(DroneBP);
	if (!HiveBP || !SaveAsset(HiveBP))
	{
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[CyberRuinBuildAssets] 完成：/Game/XuTang/ 下哨兵/掠食者/裁决者/审计官三件套 + BP_Drone + BP_SwarmHive"));
	return 0;
}
