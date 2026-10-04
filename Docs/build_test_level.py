# 《赛博遗迹》哨兵寻路测试关卡构建脚本（Docs/build_test_level.py）v3
# 运行：UnrealEditor-Cmd.exe CyberRuin.uproject -ExecutePythonScript=<本文件绝对路径> -unattended -nullrhi
# v3 变更：不再从空白关卡程序化搭场景——改为以 main 默认地图 /Game/ThirdPerson/Lvl_ThirdPerson 为底版
#          另存为 L_Test_AI（自然光照/天空/地形全继承，解决全黑）；哨兵实体外观已烘焙进
#          BP_Enemy_Sentinel（由 CyberRuinBuildAssets 命令生成，本脚本不再逐实例补 Mesh）；
#          测试要素（巡逻点/演示墙/NavMesh/哨兵）相对 PlayerStart 动态布置并射线探测落地，适配任意底版

import os
import shutil

import unreal

SOURCE_LEVEL = '/Game/ThirdPerson/Lvl_ThirdPerson'   # main 默认地图（底版，保持只读不动）
LEVEL_PATH = '/Game/XuTang/L_Test_AI'                # 测试关卡产出（dev-tang 目录内）
CUBE = '/Game/LevelPrototyping/Meshes/SM_Cube'
SENTINEL_BP = '/Game/XuTang/BP_Enemy_Sentinel'

unreal.log('[build_test_level] v3 开始：以默认地图为底版构建 %s' % LEVEL_PATH)

# ---- 幂等清理：旧 L_Test_AI + 历史外部 Actor 残留（若被本机编辑器打开占用会失败——先关编辑器再跑） ----
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
    unreal.EditorAssetLibrary.delete_asset(LEVEL_PATH)
# delete_asset 在命令行模式可能只标记不落盘——直接删文件兜底
old_umap = os.path.join(unreal.Paths.project_content_dir(), 'XuTang', 'L_Test_AI.umap')
if os.path.isfile(old_umap):
    os.remove(old_umap)
    unreal.log('[build_test_level] 已删除旧关卡文件 %s' % old_umap)
ext_actor_dir = os.path.join(unreal.Paths.project_content_dir(), '__ExternalActors__', 'XuTang')
if os.path.isdir(ext_actor_dir):
    shutil.rmtree(ext_actor_dir)
    unreal.log('[build_test_level] 已清理外部 Actor 目录 %s' % ext_actor_dir)
# 注册表缓存可能残留已删文件的陈旧条目（NewLevelFromTemplate 会拒绝）——强制重扫与磁盘对齐
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/XuTang'], force_rescan=True)
assert not unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH), \
    '资产注册表仍残留 L_Test_AI 条目：请打开编辑器让其完成一次资产扫描后关闭，再重跑本脚本'

# ---- 以 main 默认地图为底版：从模板新建 L_Test_AI（光照/天空/地形/WorldSettings 全继承） ----
created = unreal.EditorLevelLibrary.new_level_from_template(LEVEL_PATH, SOURCE_LEVEL)
assert created, '从默认地图模板创建 L_Test_AI 失败'
world = unreal.EditorLevelLibrary.get_editor_world()
assert world is not None, '获取编辑器世界失败'
unreal.log('[build_test_level] 默认地图已作为底版生成 %s' % LEVEL_PATH)


def spawn(cls, loc, rot=unreal.Rotator(0, 0, 0)):
    return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(*loc), rot)


def ground_z(x, y):
    """从高处向下射线，返回 (x, y) 处地面 Z；探不到按 0 兜底"""
    start = unreal.Vector(x, y, 2000.0)
    end = unreal.Vector(x, y, -2000.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.ECC_VISIBILITY,
        False, [], unreal.DrawDebugTrace.NONE, True)
    # 不同引擎版本 HitResult 的命中标志属性名有差异，运行时探测兼容
    blocked = bool(getattr(hit, 'b_blocking_hit', getattr(hit, 'blocking_hit', False)))
    if not blocked:
        return 0.0
    point = getattr(hit, 'impact_point', getattr(hit, 'location', unreal.Vector(0, 0, 0)))
    return point.z


def make_cube(label, loc, scale):
    actor = spawn(unreal.StaticMeshActor, loc)
    actor.set_actor_label(label)
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(unreal.EditorAssetLibrary.load_asset(CUBE))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


# ---- 定位 PlayerStart，测试要素以其为基准布置 ----
player_start = None
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if isinstance(actor, unreal.PlayerStart):
        player_start = actor
        break
origin = player_start.get_actor_location() if player_start else unreal.Vector(0, 0, 0)
unreal.log('[build_test_level] PlayerStart 基准点：%s' % origin)

# ---- 巡逻区：PlayerStart 前方 8m 为中心的 10m 见方四角（相邻点距 10m，符合方案 §3.4 的 4~12m 摆放规则） ----
center = unreal.Vector(origin.x, origin.y - 800.0, 0.0)
half = 500.0
for i, (dx, dy) in enumerate([(half, half), (half, -half), (-half, -half), (-half, half)], start=1):
    x, y = center.x + dx, center.y + dy
    tp = spawn(unreal.TargetPoint, (x, y, ground_z(x, y) + 100))
    tp.set_actor_label('PP_%d' % i)

# ---- 两堵演示墙：挡视线/制造死角，验证脱战与解卡（贴地放置，不悬空） ----
wa_x, wa_y = center.x + 1400.0, center.y
make_cube('Wall_A', (wa_x, wa_y, ground_z(wa_x, wa_y) + 200), (2, 12, 4))    # 巡逻区东侧竖墙：躲墙后断视线 → 脱战
wb_x, wb_y = center.x - 1400.0, center.y - 1000.0
make_cube('Wall_B', (wb_x, wb_y, ground_z(wb_x, wb_y) + 200), (12, 2, 4))   # 巡逻区西南横墙：制造绕行/死角

# ---- NavMesh：覆盖全场景静态几何（排除天空球），留 5m 余量；底版刷子 200cm ----
min_v = [1e12, 1e12, 1e12]
max_v = [-1e12, -1e12, -1e12]
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    if comp is None or comp.static_mesh is None or 'sky' in comp.static_mesh.get_name().lower():
        continue
    b_origin, b_extent = actor.get_actor_bounds(False)
    min_v[0] = min(min_v[0], b_origin.x - b_extent.x)
    max_v[0] = max(max_v[0], b_origin.x + b_extent.x)
    min_v[1] = min(min_v[1], b_origin.y - b_extent.y)
    max_v[1] = max(max_v[1], b_origin.y + b_extent.y)
    min_v[2] = min(min_v[2], b_origin.z - b_extent.z)
    max_v[2] = max(max_v[2], b_origin.z + b_extent.z)
margin = 500.0
nav_center = unreal.Vector((min_v[0] + max_v[0]) / 2, (min_v[1] + max_v[1]) / 2, (min_v[2] + max_v[2]) / 2)
nav_size = unreal.Vector(max_v[0] - min_v[0] + margin, max_v[1] - min_v[1] + margin, max_v[2] - min_v[2] + 1000.0)
nav = spawn(unreal.NavMeshBoundsVolume, (nav_center.x, nav_center.y, nav_center.z))
nav.set_actor_label('NavMeshBounds')
nav.set_actor_scale3d(unreal.Vector(nav_size.x / 200.0, nav_size.y / 200.0, nav_size.z / 200.0))
unreal.log('[build_test_level] NavMesh 覆盖：中心 %s 尺寸 %s' % (nav_center, nav_size))

# ---- 哨兵：巡逻区南侧 4m 处出生（距全部巡逻点 <25m，满足 PatrolRoute 自动抓取半径） ----
sentinel_class = unreal.EditorAssetLibrary.load_blueprint_class(SENTINEL_BP)
assert sentinel_class is not None, '找不到 BP_Enemy_Sentinel（请先跑 -run=CyberRuinBuildAssets 生成资产）'
sx, sy = center.x, center.y - 400.0
sentinel = spawn(sentinel_class, (sx, sy, ground_z(sx, sy) + 100))
sentinel.set_actor_label('BP_Enemy_Sentinel')
unreal.log('[build_test_level] 哨兵出生点：(%s, %s)' % (sx, sy))

# ---- 保存 ----
ok = unreal.EditorLevelLibrary.save_current_level()
unreal.log('[build_test_level] 保存 %s：%s' % (LEVEL_PATH, '成功' if ok else '失败'))
assert ok, '关卡保存失败'
unreal.log('[build_test_level] 完成 v3：默认地图底版（自然光亮）+ 巡逻点 PP_1~4 + 演示墙 + NavMesh + 哨兵（自带 Manny 实体）')
