# 《赛博遗迹》L_Seq_False_Check 演出关构建脚本（Docs/build_seq_false_check.py）
# 目的：以 /Game/XuTang/L_Seq_Escape 为底版，生成一份一模一样的演出关 L_Seq_False_Check
#      （灯光 / 平地 / 全部 Actor / WorldSettings 原样继承），供 LS_False_by_Check 演出使用。
# 运行方式A（推荐，编辑器开着即可）：
#   ① 先 Ctrl+S 保存当前关卡的所有未保存改动
#   ② 编辑器菜单 Tools → Execute Python Script → 选本文件
#   注意：脚本会把编辑器当前打开的关卡切到新生成的 L_Seq_False_Check（原 L_Seq_Escape 文件不动），
#        跑完想回原关卡用 File → Open Recent 切回即可。
# 运行方式B（无头，须先关闭 UnrealEditor，否则项目锁失败）：
#   "E:\UE5.8\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\SDC\Cyber-Ruins\CyberRuin.uproject"
#     -ExecutePythonScript=<本文件绝对路径> -unattended -nullrhi
# 幂等：可重复运行，已存在则先删旧的再生成。

import unreal

SOURCE_LEVEL = '/Game/XuTang/L_Seq_Escape'    # 底版（保持只读不动）
LEVEL_PATH = '/Game/XuTang/L_Seq_False_Check'  # 产出

unreal.log('[build_seq_false_check] 开始：以 %s 为底版生成 %s' % (SOURCE_LEVEL, LEVEL_PATH))

# ---- 前置：底版必须存在（防止注册表缓存骗过磁盘检查，先强制重扫） ----
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/XuTang'], force_rescan=True)
assert unreal.EditorAssetLibrary.does_asset_exist(SOURCE_LEVEL), \
    '找不到底版 %s：请确认 L_Seq_Escape 已保存到磁盘' % SOURCE_LEVEL

# ---- 幂等清理：旧 L_Seq_False_Check（NewLevelFromTemplate 拒绝覆盖已存在资产） ----
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
    unreal.EditorAssetLibrary.delete_asset(LEVEL_PATH)
    unreal.log('[build_seq_false_check] 已删除旧的 %s' % LEVEL_PATH)
# delete_asset 可能只标记不落盘——磁盘文件兜底
import os
old_umap = os.path.join(unreal.Paths.project_content_dir(), 'XuTang', 'L_Seq_False_Check.umap')
if os.path.isfile(old_umap):
    os.remove(old_umap)
    unreal.log('[build_seq_false_check] 已删除旧关卡文件 %s' % old_umap)
# 注册表缓存可能残留已删文件条目——再次强制重扫与磁盘对齐
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/XuTang'], force_rescan=True)
assert not unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH), \
    '资产注册表仍残留 L_Seq_False_Check 条目：请在 Content Browser 刷新一次后重跑本脚本'

# ---- 以 L_Seq_Escape 为底版从模板新建（灯光/平地/Actor/WorldSettings 全继承） ----
created = unreal.EditorLevelLibrary.new_level_from_template(LEVEL_PATH, SOURCE_LEVEL)
assert created, '从 L_Seq_Escape 模板创建 L_Seq_False_Check 失败'
world = unreal.EditorLevelLibrary.get_editor_world()
assert world is not None, '获取编辑器世界失败'
unreal.log('[build_seq_false_check] 已生成：%s（当前编辑器已切换到新关卡）' % world.get_path_name())

# ---- 保存 ----
ok = unreal.EditorLevelLibrary.save_current_level()
unreal.log('[build_seq_false_check] 保存 %s：%s' % (LEVEL_PATH, '成功' if ok else '失败'))
assert ok, '关卡保存失败'
unreal.log('[build_seq_false_check] 完成：Content/XuTang/L_Seq_False_Check 与 L_Seq_Escape 完全一致（灯光+平地），'
           '打开 Sequencer 面板把 LS_False_by_Check 拖进关卡即可开始摆演出')
