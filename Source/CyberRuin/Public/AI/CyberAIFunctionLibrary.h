#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CyberAIFunctionLibrary.generated.h"

class AActor;

/**
 * 怪物 AI 公共函数库：解卡瞬移、刷怪点校验、掠食者绕后取点。
 * 框架无关（不依赖 StateTree），StateTree BP 任务、审计官状态机、刷怪逻辑都可调用。
 */
UCLASS()
class CYBERRUIN_API UCyberAIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 解卡兜底：把怪挪到最近的导航网格点。掠食者/审计官 = 设定内的相位位移；哨兵也可用，观感差异小 */
	UFUNCTION(BlueprintCallable, Category = "赛博遗迹AI")
	static bool WarpToNearestNavigable(AActor* Actor, float SearchRadius = 200.f);

	/** 刷怪点自检：投影不到导航网格的点直接拒绝刷怪，防止"出生即卡死" */
	UFUNCTION(BlueprintCallable, Category = "赛博遗迹AI", Meta = (WorldContext = "WorldContextObject"))
	static bool ValidateSpawnPoint(const UObject* WorldContextObject, const FVector& Desired, FVector& OutSpawn);

	/** 掠食者绕后取点：目标背后 ±SampleArcDegrees 扇形、FlankDistance 处投影导航网格。false = 全被挡
	 *  注意：参数 2/3 不带默认值（输出参数在后，C++ 禁止默认参数后跟无默认参数）；BP 调用方显式传 450 / 60 */
	UFUNCTION(BlueprintCallable, Category = "赛博遗迹AI")
	static bool GetFlankPoint(const AActor* Target, float FlankDistance, float SampleArcDegrees, FVector& OutPoint);
};
