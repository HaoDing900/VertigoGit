# L2 战后 Sequence 18.60 秒附近的状态切换

检查的是当前磁盘资产 `/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarAfterFight`。

## 已实施修复

在 `FLS_L2BarStreet_AfterFight_Animation` 的 BP_Player_Sa 绑定下新增 `Sa: refresh pose after shot return` 事件轨道，在子时间线第 523 帧、Sequence 求值结束后调用 `RefreshSequenceCharacterPose`。该帧对应主时间线 18.6333 秒返回战后镜头的位置。它也会在闪回前正常经过第 523 帧时执行一次同样的零增量刷新。

新增的原生 Blueprint 函数仅执行 `TickAnimation(0, false)` 和 `RefreshBoneTransforms()`，把当前 Sequencer 已设置的动画位置同步到骨骼；不更换 AnimInstance、不停止 Montage、不清除过场状态。使用实际绑定的 Sa，不依赖 Player 0 或全局角色查找。原有 Director Blueprint 的事件及连线保留，通过标准 MovieScene Event 编译流程创建入口，支持保存、重载和后续重新编译。

主 Sequence、镜头切点、动画片段、原有关键帧及 Completion Mode 保持原样。修改的子 Sequence 原文件备份在 `Backup/FLS_L2BarStreet_AfterFight_Animation.uasset`。

曾测试两帧 Pre-roll，但测试发现从 Pre-roll 切入正式求值时又恢复了默认姿势，因此未将此方案保存到资产。

验证入口：`-run=VTGSaCinePose -AfterFightRepair -VerifyOnly`；运行时回归：`-run=VTGSaCinePose -AfterFightProbe -RequireFixed`，加 `-MeshFirst` 复现之前有单帧延迟的顺序。修复后该顺序在 18.6333 秒已得到演出姿势，后续帧亦不回退到默认姿势。验证使用隔离运行环境，尚未进行完整地图的可视 Simulation 试玩。

最终编辑器编译通过（`build-repair-final.log`）。两种更新顺序均通过，进程退出码为 0（`test-final-meshfirst.log`、`test-final-sequencefirst.log`），每次运行报告 0 errors；资产独立重载验证见 `verify-refresh2.log`。工程原有资产加载警告仍存在。

## 已完成的界面修改

`Sequence 快进检查` 新增「过场声音预设」，三个选项与关卡实例 Details 相同，直接读写同一个 `UVTGCutsceneAudioSettings::Preset`。修改保存 Sequence 资产，支持撤销。打开下拉框会重新同步当前资产，避免切换 Sequence 或撤销后保留前一资产的选中项；只打开菜单不会创建设置或弄脏资产。

## 时间线事实

主 Sequence 为 30 fps。战后动画被分成多个 cinematic shot，中间插入闪回：

| 主时间线帧范围（右端不含） | 子 Sequence |
| --- | --- |
| 209–529 | FLS_L2BarStreet_AfterFight_Animation |
| 529–559（17.6333–18.6333 秒） | FLS_L2BarbackFlashback_Bowerbird |
| 559–755（18.6333 秒起） | FLS_L2BarStreet_AfterFight_Animation |

返回镜头的 StartFrameOffset 为 416800 ticks，子 Sequence 的 playback start 为第 2 帧，返回时局部位置约为第 523 帧（17.4333 秒）。这里没有完整子 Sequence 一直保持控制的条件。

## 隔离运行结果

入口：`-run=VTGSaCinePose -AfterFightProbe`，加 `-MeshFirst` 测试骨骼先于 Sequence 更新的顺序。

测试使用真实 BP_Player_Sa、BP_Cine_Sa_Past 和当前资产的 MovieScene 临时副本，保留 Sa 的动画、Transform、bool 轨道及主镜头时间范围；移除摄像机、事件和无关对象轨道，未保存或修改原始 uasset。临时玩家初始位置为原点，因此日志中的原点仅代表“恢复测试前的位置”，不是实际关卡玩家站位。未运行完整关卡、Director 事件、物理或真实 Simulation。

两种更新顺序都确认：闪回期间战后玩家恢复初始位置，18.6333 秒被重新定位到 `(-399.275, 2109.115, 240.663)`。

`probe4.log`（先 Sequence 后骨骼）：18.6333 秒更新到返回演出的姿势，手臂相对参考姿势最大旋转差约 0.748395 rad。

`probe-meshfirst.log`（先骨骼后 Sequence）：18.6333 秒位置已经返回，但骨骼仍是之前的默认姿势（约 0.403783 rad）；18.6500 秒才变成演出姿势（约 0.748395 rad）。证明此镜头边界存在单帧位置/姿势不同步的条件。

这支持运行时镜头释放/重新接管和更新顺序导致闪姿势的解释。测试没有得到严格的参考 T-pose（旋转差为零），因此不能宣称已在真实 Simulation 完整复现用户的 T-pose，也不能排除完整关卡中的事件、ABP 状态和实际 tick 顺序的额外影响。单独播放连续子 Sequence 不经过这个镜头实例边界，所以单独预览正常并不能排除主 Sequence 的切换问题。

以上为修复前的诊断记录。后续实施的局部同步刷新修复见本文开头。

编辑器模块已编译通过。`audit.json` 与两个 `.t3d` 导出文件记录原始时间线；诊断日志记录逐帧结果。
