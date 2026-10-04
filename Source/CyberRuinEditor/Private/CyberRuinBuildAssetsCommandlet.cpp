#include "CyberRuinBuildAssetsCommandlet.h"

#include "AI/AntiStuckComponent.h"
#include "AI/CyberEnemyAIController.h"
#include "AI/PatrolRouteComponent.h"
#include "AI/StateTreeEnemyConditions.h"
#include "AI/StateTreeEnemyEvents.h"
#include "AI/StateTreeSentinelTasks.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
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
}

UCyberRuinBuildAssetsCommandlet::UCyberRuinBuildAssetsCommandlet()
{
	IsClient = false;
	LogToConsole = true;
}

UStateTree* UCyberRuinBuildAssetsCommandlet::BuildSentinelStateTree()
{
	const FString AssetPath = TEXT("/Game/XuTang/ST_Sentinel");

	UStateTree* StateTree = GetOrCreateAsset<UStateTree>(AssetPath, TEXT("ST_Sentinel"));

	// 每次重建：整体换新 EditorData（状态/绑定都以它为 Outer，换掉即全部重置，天然幂等）
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;

	// AI Schema：保证任务能访问 AIController（Context 自动绑定）
	EditorData->Schema = NewObject<UStateTreeAIComponentSchema>(EditorData);

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
	auto AddStuckTransitions = [&Unstick](UStateTreeState& State)
	{
		FStateTreeTransition& StuckTransition = State.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Unstick);
		StuckTransition.Priority = EStateTreeTransitionPriority::High;
		StuckTransition.AddConditionWithOuter<FStateTreeIsStuckCondition>(&State);
	};
	AddStuckTransitions(Patrol);
	AddStuckTransitions(Chase);
	AddStuckTransitions(Unstick);

	// 巡逻 → 追击：看见敌人事件（感知 → AIC 发送）
	Patrol.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemySeen, EStateTreeTransitionType::GotoState, &Chase);

	// 追击 → 巡逻：丢失目标 / 目标为空兜底（不限距离追击：不做离家折返，追到真看不见为止）
	Chase.AddTransition(EStateTreeTransitionTrigger::OnEvent, FCyberRuinNativeTags::Get().EnemyLost, EStateTreeTransitionType::GotoState, &Patrol);
	Chase.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Patrol);

	// 解卡 → 回巡逻
	Unstick.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Patrol);

	// ---------- 编译（EditorData → 运行时数据） ----------
	EditorData->ReparentStates();
	EditorData->UpdateBindings();
	FStateTreeCompilerLog CompileLog;
	FStateTreeCompiler Compiler(CompileLog);
	const bool bCompiled = Compiler.Compile(StateTree);
	UE_LOG(LogTemp, Display, TEXT("[CyberRuinBuildAssets] ST_Sentinel 编译 %s"), bCompiled ? TEXT("成功") : TEXT("失败"));
	if (!bCompiled)
	{
		CompileLog.DumpToLog(LogTemp);
		return nullptr;
	}

	return StateTree;
}

UBlueprint* UCyberRuinBuildAssetsCommandlet::BuildSentinelAIC(UStateTree* InStateTree)
{
	UBlueprint* AICBlueprint = GetOrCreateBlueprint(
		ACyberEnemyAIController::StaticClass(), TEXT("/Game/XuTang/BP_AIC_Sentinel"), TEXT("BP_AIC_Sentinel"));
	if (!AICBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 建哨兵 AIC 蓝图失败"));
		return nullptr;
	}

	// 默认值：指定 StateTree 资产
	if (ACyberEnemyAIController* AICCDO = Cast<ACyberEnemyAIController>(AICBlueprint->GeneratedClass->GetDefaultObject()))
	{
		AICCDO->EnemyStateTree = InStateTree;
	}
	FKismetEditorUtilities::CompileBlueprint(AICBlueprint);
	return AICBlueprint;
}

UBlueprint* UCyberRuinBuildAssetsCommandlet::BuildSentinelEnemyPawn(UBlueprint* InAICBlueprint)
{
	// 父类 = 洪韵然的 BP_BaseEnemy（Core 框架）
	UBlueprint* BaseEnemyBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Core/BaseClasses/BP_BaseEnemy.BP_BaseEnemy"));
	if (!BaseEnemyBP || !BaseEnemyBP->GeneratedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 找不到 BP_BaseEnemy（Content/Core/BaseClasses）"));
		return nullptr;
	}

	UBlueprint* EnemyBP = GetOrCreateBlueprint(
		BaseEnemyBP->GeneratedClass, TEXT("/Game/XuTang/BP_Enemy_Sentinel"), TEXT("BP_Enemy_Sentinel"));
	if (!EnemyBP)
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 建哨兵怪壳蓝图失败"));
		return nullptr;
	}

	// SCS 加组件：卡死检测 + 巡逻路线（方案 §3.3/§11.0，怪壳出厂自带）
	if (USimpleConstructionScript* SCS = EnemyBP->SimpleConstructionScript)
	{
		if (!SCS->FindSCSNode(TEXT("AntiStuck")))
		{
			if (USCS_Node* Node = SCS->CreateNode(UAntiStuckComponent::StaticClass(), TEXT("AntiStuck")))
			{
				SCS->AddNode(Node);
			}
		}
		if (!SCS->FindSCSNode(TEXT("PatrolRoute")))
		{
			if (USCS_Node* Node = SCS->CreateNode(UPatrolRouteComponent::StaticClass(), TEXT("PatrolRoute")))
			{
				SCS->AddNode(Node);
			}
		}
	}

	// 默认值：AIController 指定哨兵 AIC
	if (APawn* PawnCDO = Cast<APawn>(EnemyBP->GeneratedClass->GetDefaultObject()))
	{
		PawnCDO->AIControllerClass = Cast<UClass>(InAICBlueprint->GeneratedClass);
	}

	// 默认值：实体外观（哨兵出厂自带骨骼网格体，不再只是碰撞胶囊）
	// 玩家默认是 SKM_Quinn_Simple，哨兵用 SKM_Manny_Simple 天然区分敌我；动画复用项目 ABP_Unarmed
	if (ACharacter* EnemyCharCDO = Cast<ACharacter>(EnemyBP->GeneratedClass->GetDefaultObject()))
	{
		USkeletalMeshComponent* EnemyMesh = EnemyCharCDO->GetMesh();
		USkeletalMesh* SentinelMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		UBlueprint* AnimBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed"));
		if (EnemyMesh && SentinelMesh && AnimBP && AnimBP->GeneratedClass)
		{
			EnemyMesh->SetSkeletalMesh(SentinelMesh, false);
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
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CyberRuinBuildAssets] 缺少 SKM_Manny_Simple 或 ABP_Unarmed，哨兵将没有实体外观"));
		}
	}

	FKismetEditorUtilities::CompileBlueprint(EnemyBP);
	return EnemyBP;
}

int32 UCyberRuinBuildAssetsCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("[CyberRuinBuildAssets] 开始构建怪物 AI 资产……"));

	UStateTree* ST = BuildSentinelStateTree();
	if (!ST)
	{
		return 1;
	}
	if (!SaveAsset(ST))
	{
		UE_LOG(LogTemp, Error, TEXT("[CyberRuinBuildAssets] 保存 ST_Sentinel 失败"));
		return 1;
	}

	UBlueprint* AICBP = BuildSentinelAIC(ST);
	if (!AICBP || !SaveAsset(AICBP))
	{
		return 1;
	}

	UBlueprint* EnemyBP = BuildSentinelEnemyPawn(AICBP);
	if (!EnemyBP || !SaveAsset(EnemyBP))
	{
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[CyberRuinBuildAssets] 完成：/Game/XuTang/ 下 ST_Sentinel + BP_AIC_Sentinel + BP_Enemy_Sentinel"));
	return 0;
}
