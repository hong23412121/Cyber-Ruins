#include "AI/StateTreeEnemyEvents.h"

#include "GameplayTagsManager.h"

namespace
{
	// DLL 加载时构造：FGameplayTagNativeAdder 构造即向标签管理器登记自己，
	// 引擎启动构建原生标签表时回调 AddTags。必须早于任何 Get() 使用
	// （惰性 static 会错过注册窗口，拿到的将是无效空标签）。
	static FCyberRuinNativeTags GCyberRuinTagsInstance;
}

void FCyberRuinNativeTags::AddTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	EnemySeen = Manager.AddNativeGameplayTag(TEXT("Event.Enemy.Seen"), TEXT("看见敌人（AI 感知事件）"));
	EnemyLost = Manager.AddNativeGameplayTag(TEXT("Event.Enemy.Lost"), TEXT("丢失敌人（AI 感知事件）"));
}

const FCyberRuinNativeTags& FCyberRuinNativeTags::Get()
{
	return GCyberRuinTagsInstance;
}
