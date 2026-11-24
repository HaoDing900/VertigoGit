# Bowerbird 在战后 Sequence 约 17.6 秒消失的检查

## 当前状态：修复已应用

用户关闭编辑器后，已执行 `Apply-BowerbirdBounds.ps1`。写回前确认原资产仍与备份一致；写回后确认 Content 中的文件 SHA256 与已验证暂存文件一致。

从正常 Content 路径独立重载并运行闪回顶点检查，552,540 个采样点出界数为 0，进程退出 0、0 errors。日志：`verify-applied-bounds.log`。渲染 Bounds 修复现已应用，仍需用户启动编辑器进行 PIE 视觉复测。

## PIE 现场日志与已准备的 Bounds 修复

已读取用户复现后的 `Saved/Diagnostics/L2BowerbirdTrace.log`。闪回中 Actor 一直存在、未销毁，Actor Hidden=0、Mesh Visible=1、HiddenInGame=0、无无效骨骼、未被 Controller 隐藏。现场表现是先出现几帧再消失。

进一步发现已证实的渲染包围盒错误：Bowerbird Skeletal Mesh 没有 Physics Asset；组件未设置固定包围盒，但没有物理包围盒时引擎仍回退到 Skeletal Mesh 的导入站姿 Bounds。闪回躺姿超出了该固定范围。蒙皮顶点检查覆盖 552,540 个顶点/时间样本，其中 549,255 个（99.405%）出界。摄像机视锥内仍有角色骨骼、姿势缩放为 1，隔离求值中的 Camera Fade 禁用参数为 1。

该 Bounds 错误能够导致错误的视锥/遮挡剔除，且遮挡反馈有帧延迟，与现场先出现再消失相符。但尚未直接抓取 GPU 剔除结果，最终视觉因果仍需修复后的 PIE 对照确认。

已根据闪回的实际 LOD0 蒙皮顶点范围，加 10cm 余量，准备仅扩展 Bounds 的修复资产：

- `Staged/Bowerbird_RIG_ue_renamed_.uasset`
- Positive Bounds Extension = `(54.874, 0, 0)`
- Negative Bounds Extension = `(24.790, 189.464, 39.124)`
- 独立进程重载修复文件后，相同 552,540 个样本全部被包围，出界数为 0；`verify-staged-bounds2.log`，进程退出 0。

未更改模型顶点、动画、材质、碰撞或 Blueprint。原模型备份在 `Backup/Bowerbird_RIG_ue_renamed_.uasset`。首次直接保存原路径失败，Windows Error 32 表示正在运行的编辑器占用该文件，因此先生成并验证暂存版本；后续已完成应用，见本文开头。实际 Content 资产的复验命令为 `-run=VTGSaCinePose -AfterFightProbe -BirdGameplay -VerifyBirdBounds`。

## Simulation 专项后续

用户补充：编辑器单独播放闪回正常，Simulation 才消失。

已排除单纯因重力下落而消失：该关卡实例 `IsCutscene?(NeedsManualDetect)=false`，BeginPlay 的检测函数确实会把 MovementMode 从 None 改成 Walking，但 Bowerbird 的 GravityScale=0。使用实际 Blueprint、地图位置、地图重力 -980 和 KillZ=-1048575 的五分钟隔离移动测试中，它始终保留原位置，也未被销毁。地面检测未命中，但在零重力条件下不能据此断言它会下落。证据：`physics-before3.log`，退出 0。

尚未捕获完整地图中的消失现场，因此根因仍未确认。增加编辑器专用诊断 `VTGBowerbirdTrace`，已编译通过（`build-trace.log`）：

- 默认启用，仅针对 `L2_StreetOutsideBar` 的 PIE/Simulation；不修改任何角色状态。
- 常规每两秒记录一次；战后主 Sequence 的 17.3–18.9 秒加密采样。
- 记录角色是否存在、Actor/Mesh 显隐、Controller 隐藏列表、移动状态、位置、骨骼异常、包围盒、最近渲染状态和当前镜头。
- 输出到项目 `Saved/Diagnostics/L2BowerbirdTrace.log`。控制台 `vtg.BowerbirdTrace 0` 可关闭。

需要重启编辑器以载入新模块，然后在 Simulation 中重现一次消失，才能从真实现场日志继续定位。本次未更改地图或 Sequence 资产，也未应用未经证实的显隐/物理修复。

## 已确认

- 当前主 Sequence `LS_L2BarAfterFight` 在第 529 帧（17.633333 秒）切入 `FLS_L2BarbackFlashback_Bowerbird`，第 559 帧（18.633333 秒）离开。
- 闪回的 Bowerbird 绑定确实指向当前街道地图 `BP_Bowerbird_Still_C_1`。Actor Visibility 轨道默认值与关键帧都为显示，没有在此处写入隐藏的关键帧。Bowerbird 的动画使用 `LSANM_Bowerbird_Paired_Last`，播放倍率 0.01。
- 地图保存状态中该 Actor 的 Hidden=false，CharacterMesh0 的 Visible=true、HiddenInGame=false，BoundsScale=1、无最大绘制距离限制。
- 关卡中的 `BPLS_L2StreetBarFight_C_1`（类为 `BPLM_L2StreetBarFight`）内 L2_FemaleClient 正确引用 FemaleClient，并未误指向 Bowerbird。已导出的 BPLM 逻辑里，没有找到在此处直接隐藏 Bowerbird 的调用。
- 运行时隔离探针使用实际角色 Blueprint 和当前 Sequence 的角色轨道临时副本。17.633333 秒至 17.883333 秒，Bowerbird 的 Actor Hidden=0、Mesh Visible=1、Mesh HiddenInGame=0，位置约 `(-844.693,5291.227,266.158)`；骨骼包围盒有限且正常，未复现消失。日志 `probe.log`，进程退出 0。

## 一个尚不能当作根因的控制缺口

Actor Visibility 与 Mesh.SetVisibility 是不同层。闪回对过去 Sa 有 `SetActorHiddenInGame(false)` 和 `TurnMeshVisible` 两个事件；Bowerbird 没有对应的 Mesh 显示恢复事件。父类 `BP_StillNPC` 的 `TurnMeshInvisible` 会对 Mesh（及子组件）执行 SetVisibility(false)，而 Actor Visibility 轨道不能恢复这一位。因此若 Bowerbird 的 Mesh 在上游流程被关掉，仅有当前 Actor 显示轨道不足以把它显示出来。

尚未发现当前所查流程里实际调用 Bowerbird.TurnMeshInvisible 的证据，不能将该条件性风险宣称为本次消失的确定原因。隔离探针也没有覆盖完整地图的 Simulation、所有 Blueprint/Timer/物理更新或 GPU 渲染，无法排除现场 Mesh 状态、运行时位置和渲染剔除的问题。

本次只导出、检查和新增诊断记录，没有保存或修改地图、Sequence 或角色 Blueprint。仍需区分症状是切镜头瞬间闪失、整个闪回不可见，还是仅 Simulation 出现，并在对应现场记录 Actor/Mesh 可见性、位置、包围盒和正在播放的 Sequence 实例，才能确定实际根因。
