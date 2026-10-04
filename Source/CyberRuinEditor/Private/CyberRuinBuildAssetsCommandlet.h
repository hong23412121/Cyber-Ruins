#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CyberRuinBuildAssetsCommandlet.generated.h"

/**
 * 怪物 AI 资产程序化构建命令（方案 §11 落地）：
 *   UnrealEditor-Cmd.exe CyberRuin.uproject -run=CyberRuinBuildAssets -unattended -nullrhi
 * 产出（全部在 /Game/XuTang/，重复执行幂等覆盖）：
 *   1. ST_Sentinel        —— 哨兵行为 StateTree（取巡逻点/走过去/追击/解卡 四状态，方案 §11.2）
 *   2. BP_AIC_Sentinel    —— 哨兵 AIController（挂 ST_Sentinel）
 *   3. BP_Enemy_Sentinel  —— 哨兵怪壳（BP_BaseEnemy 子类 + AntiStuck/PatrolRoute 组件 + 指定 AIC）
 */
UCLASS()
class UCyberRuinBuildAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCyberRuinBuildAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	/** 建 ST_Sentinel 资产（状态机 + 任务 + 转换 + 绑定 + 编译），返回资产指针 */
	class UStateTree* BuildSentinelStateTree();

	/** 建哨兵 AIC 蓝图（指定 StateTree 资产） */
	class UBlueprint* BuildSentinelAIC(class UStateTree* InStateTree);

	/** 建哨兵怪壳蓝图（BP_BaseEnemy 子类 + SCS 组件 + AIControllerClass） */
	class UBlueprint* BuildSentinelEnemyPawn(class UBlueprint* InAICBlueprint);
};
