# 《赛博遗迹》哨兵寻路测试关卡构建脚本（Docs/build_test_level.py）
# 运行：UnrealEditor-Cmd.exe CyberRuin.uproject -ExecutePythonScript=<本文件绝对路径> -unattended -nullrhi
# 产出：/Game/XuTang/L_Test_AI —— 打开即 PIE：哨兵沿 4 巡逻点往返、看见玩家追击、躲墙后脱战回巡逻

import unreal

LEVEL_PATH = '/Game/XuTang/L_Test_AI'
CUBE = '/Game/LevelPrototyping/Meshes/SM_Cube'

unreal.log('[build_test_level] 开始创建测试关卡 %s' % LEVEL_PATH)

# 新建空白关卡并设为当前
world = unreal.EditorLevelLibrary.new_level(LEVEL_PATH)
assert world is not None, '创建关卡失败'


def spawn(cls, loc, rot=unreal.Rotator(0, 0, 0)):
    return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(*loc), rot)


def make_cube(label, loc, scale):
    actor = spawn(unreal.StaticMeshActor, loc)
    actor.set_actor_label(label)
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(unreal.EditorAssetLibrary.load_asset(CUBE))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor

# ---- 地板：60m x 60m（顶面 Z=0）----
floor = make_cube('Floor', (0, 0, -50), (60, 60, 1))

# ---- 灯光（否则黑屏）----
light = spawn(unreal.DirectionalLight, (0, 0, 1500), unreal.Rotator(-40, 0, 30))
light.set_actor_label('SunLight')

# ---- 玩家出生点（模板第三人称 GameMode 会生成小人）----
player_start = spawn(unreal.PlayerStart, (0, 2500, 150))
player_start.set_actor_label('PlayerStart')

# ---- 巡逻点：15m 见方四角，名字有序（PatrolRoute 自动抓取按名字排序）----
for i, (x, y) in enumerate([(1500, 1500), (1500, -1500), (-1500, -1500), (-1500, 1500)], start=1):
    tp = spawn(unreal.TargetPoint, (x, y, 100))
    tp.set_actor_label('PP_%d' % i)

# ---- 两堵墙：挡视线/制造死角，验证脱战与解卡 ----
make_cube('Wall_A', (0, 0, 200), (2, 16, 4))       # 中央墙：躲墙后断视线 → 脱战
make_cube('Wall_B', (-1500, 1500, 200), (10, 2, 4))  # 斜角墙：制造绕行/死角

# ---- NavMesh 覆盖全场（61x61x30m）----
nav = spawn(unreal.NavMeshBoundsVolume, (0, 0, 300))
nav.set_actor_label('NavMeshBounds')
nav.set_actor_scale3d(unreal.Vector(31, 31, 15))

# ---- 哨兵（程序化生成的 BP）----
sentinel_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/XuTang/BP_Enemy_Sentinel')
assert sentinel_class is not None, '找不到 BP_Enemy_Sentinel'
sentinel = spawn(sentinel_class, (0, -2000, 150))
sentinel.set_actor_label('BP_Enemy_Sentinel')

# ---- 保存 ----
ok = unreal.EditorLevelLibrary.save_current_level()
unreal.log('[build_test_level] 保存 %s：%s' % (LEVEL_PATH, '成功' if ok else '失败'))
assert ok, '关卡保存失败'
unreal.log('[build_test_level] 完成：打开 %s 按 Play——哨兵沿 PP_1~PP_4 往返，靠近会追你，躲到墙后 3 秒脱战回巡逻' % LEVEL_PATH)
