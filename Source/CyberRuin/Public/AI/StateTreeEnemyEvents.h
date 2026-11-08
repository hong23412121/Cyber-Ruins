#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StateTreeEnemyEvents.generated.h"

class AActor;

/** 感知事件标签（StateTree 转换用 OnEvent + Tag 监听） */
struct CYBERRUIN_API FCyberRuinNativeTags : public FGameplayTagNativeAdder
{
	FGameplayTag EnemySeen;  // Event.Enemy.Seen：看见敌人
	FGameplayTag EnemyLost;  // Event.Enemy.Lost：丢失敌人

	virtual void AddTags() override;

	static const FCyberRuinNativeTags& Get();
};

/** "看见敌人"事件的载荷：StateTree 状态里可绑定 Payload.Target 喂给移动任务 */
USTRUCT()
struct CYBERRUIN_API FStateTreeEnemySeenPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> Target = nullptr;
};
