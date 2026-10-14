# L2 街头酒吧战斗：存档与战后角色状态检查

检查日期：2026-10-14。通过 UE 5.3 Python commandlet 从当前资产重新导出蓝图和两个 Sequence；没有修改游戏源码或 uasset。未进行 PIE 战斗复现。

## 1. FemaleClient 战后隐藏 / 离开结尾位置

BPLM EventGraph：L2_InitializeNPC -> Set BackgroundNPC Initial Location -> Set FNPC Initial Position -> Set Enm Initial Position -> SetActorHiddenInGame(L2_FemaleClient, true)。节点 K2Node_CallFunction_76。

正常开始和读档战斗分支都会调用 L2_InitializeNPC。读档分支执行顺序是 L2_EnterCombat -> L2_InitializeNPC，后者还会把敌人重置回 Initial Position，覆盖刚设置的战斗位置。

战后 FLS_L2BarStreet_AfterFight_Animation 的 FemaleClient Visibility 关键帧：显示帧 376 false、416 true、667 true。Visibility 的 true 表示可见（引擎将其取反写入 bHidden）。所以 Sequence 中会暂时显示她。

FemaleClient Visibility 和 Transform section 均未显式覆盖 CompletionMode，使用 ProjectDefault；项目 Config 无覆盖，UE BaseEngine.ini 的 LevelSequence 默认是 RestoreState。结束时恢复演出前的隐藏和位置。BPLM OnFinished_AfterFight 只调用 CameraWorkLastFrame，没有把 FemaleClient 显示并定位。

FemaleClient 的 Sequence 最后位置约 (-682.458618, 2258.969482, 242)，Yaw -171.954788，Scale 1。这是序列数据的最后关键帧值，需在关卡确认希望保留的实际交互站位。

## 2. Player Sa 战后位置

FLS_L2BarStreet_AfterFight_Animation / BP_Player_Sa / MovieScene3DTransformTrack_1 / MovieScene3DTransformSection_0 明确 EvalOptions=(CompletionMode=RestoreState)。结束会恢复演出前的位置，受玩家打完战斗时站位影响。

BPLM 的 K2Node_CallFunction_60 是 SetActorTransform(Player Pawn)，目标位置 (-402,2119,251)，但 execute 没有 LinkedTo，是断开的节点。AfterFightDialogue 实际路径：LockPlayerInput -> ScreenTransition_Short -> PrintString -> SetIsCutscene(true) -> SetBaseIdle -> BeginDialogue。没有经过该定位节点。

Sequence 末位置约 (-399.274567,2109.115479,248)，Yaw 75.756935，Scale 1.035；与蓝图断开节点的 (-402,2119,251)、Rotation 0、Scale 1 不完全一致。应确定统一的战后交互站位/朝向，避免盲目复制不同来源的 Transform。

## 3. 存档阶段不完整

OnFinished_CombatIntro -> ScreenTransition_Short -> Delay 0.2 -> SetPersistentInt(L2StreetFightPhase,1) -> AutoSave -> ResetIgnoreInputFlags -> L2_EnterCombat。

蓝图只设置 phase=1，未发现战后更新。OnStageBegin 只判断 phase==1 -> L2_EnterCombat -> L2_InitializeNPC；否则重新走开场。战后保存仍会走战斗初始化。

当前 C++ 会在 BeginPlay 前恢复 BPLM 的数值/布尔/枚举进度，但蓝图仍依据这个未更新的 persistent phase 启动战斗。恢复字段不会自动重建对应的角色可见性、Sequence 结尾站位或战后场景。

SaveCoordinator 在地图载入后的 ticker 中恢复 Pawn 的 PlayerTransform，以及 Saveable Actor 的 Transform。因此 BeginPlay 中设置的位置也可能随后被存档位置覆盖。这是可确认的时序风险，尚未通过运行日志证明是本次症状的额外触发因素。

## 建议修复顺序

1. 在 OnFinished_AfterFight 统一执行战后状态：FemaleClient SetActorHiddenInGame(false)，显式放到战后位置；Sa 明确放到确认后的交互位置。再交还摄像机、启动对话或允许移动。不要只依赖 Sequence 最后关键帧。
2. 将断开的 Sa SetActorTransform 接到正确的战后执行路径；确认玩家运动/物理恢复不会继续推动 Pawn。不要在 Sequence 仍控制 Transform 时设置最终位置。
3. 将 L2StreetFightPhase 扩展为 Intro=0、Combat=1、AfterFight=2。战后先完成角色状态，再写 phase=2，再保存。读档增加 AfterFight 分支，重建战后场景，不重新启动战斗或播放已完成的演出。
4. 读档的角色定位放到存档恢复完成之后（如 OnSlotLoaded），以避免 ApplyWorldState 覆盖。保留适用于自由移动存档的玩家当前位置；只在剧情固定站位恢复时强制定位。
5. Combat 恢复先做初始化，再 EnterCombat，避免敌人被后续 Initial Position 重置。
6. OnFinished 回调应在 Play 前绑定；目前战后流程是 Play -> NPC Hype Ends -> Delay 0.2 -> Bind OnFinished，短时播放或快进存在漏接完成事件的风险。此风险未做运行复现。

验证应覆盖：新开关卡正常战斗、战斗中死亡重试、战后保存再读档、Esc 快进战后演出。分别记录 FemaleClient Hidden/Transform、Pawn Transform、phase，以及 Sequence 完成和存档恢复的时序。

证据：同目录 BPLM_L2StreetBarFight.copy、FLS_L2BarStreet_AfterFight_Animation.t3d、sequences.json。C++：Source/Vertigo/Private/VTGLevelManagerBase.cpp；Source/Vertigo/Private/Save/VTGSaveCoordinator.cpp。

## 已实施修复（2026-10-14）

已修改并保存 BPLM_L2StreetBarFight.uasset，新增 L2FightState 原生组件（Source/Vertigo/Public/Save/VTGL2FightStateComponent.h；Private/Save/VTGL2FightStateComponent.cpp）。

- OnFinished_AfterFight -> CompleteAfterFight -> CameraWorkLastFrame；明确显示 FemaleClient，并设定战后站位；Sa 使用原 BP 指定的 (-402,2119,251)，朝向为 Sequence 末尾 Yaw 75.756935，单位缩放，停止剩余移动速度。
- 两个最终 Transform 可在 BPLM 的 L2FightState 组件 Details 中调整。
- 移除断开的旧 Sa 定位节点，由统一收尾组件负责定位。
- 战斗恢复分支改为 InitializeNPC -> EnterCombat，避免初始化覆盖战斗布局。
- 增加 phase=2 战后恢复分支；通过 OnSlotLoaded 等待完整存档恢复，再显示和定位 FemaleClient、清理战斗敌人和战斗区域；保留玩家存档位置。
- 设置 L2StreetFightAfterDialoguePending，追踪战后对话是否结束。未完成时读档恢复战后对话（Narrative 不保存正在播放的对话）；已完成则恢复自由移动，不重复对话。
- 战后 OnFinished 在 Play 前绑定，移除原本绑定前的 Delay。
- 保持已有自动存档时机；收尾更新 phase=2，之后的手动/自动存档会记录它。修复前已经生成、phase 仍为1的旧存档仍恢复战斗，未自动改写用户存档。

验证：VertigoEditor Development C++ 编译通过；修复工具编译蓝图、保存，再以独立进程 VerifyOnly 从磁盘重载复验成功。回归覆盖隐藏 NPC 恢复、带速度的 Pawn 定位并停止、实际 Complete 写入 phase=2、PendingLoad 期间暂缓恢复、OnSlotLoaded 后恢复 NPC 且保留保存的 Pawn Transform、重复收尾不覆盖站位/对话状态，以及阶段序列化。

日志：build.log、apply.log、verify.log。没有进行完整 PIE 战斗、真实剧情对话或真实 Esc 快进演出验证；这些场景仍需在编辑器实际试玩确认。

资产备份：BPLM_L2StreetBarFight.before_fix.uasset。不要仅回滚 C++ 并保留修复后的蓝图，它引用新增原生组件；若回滚应一并恢复资产。
