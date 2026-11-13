# Stage、黑市敌人与下水道/入口对话修复（2026-11-07）

## Stage 优先级

`AVTGLevelManagerBase::GetActiveStage` 原来无条件优先选择 Test Stage，导致正式切图传入的 Stage 被覆盖。
现在只在 PIE 且本游戏会话尚未通过 `OpenLevelWithStage` 或读取存档明确指定 Stage 时使用 Test Stage。
`UVTGGameInstanceBase::SetCurrentStage` 记录会话内的明确 Stage 标志；`OpenLevelWithStage` 即使收到空 Stage，
也会记录此次切图并保留原 Stage。该标志不保存进存档，每次新建 PIE 会话重新开始。
非 PIE 游戏始终忽略测试 override。
`OnWorldChanged` 也检测离开已开始运行的游戏世界，覆盖普通 OpenLevel/重新载入。
初始空世界/尚未 BeginPlay 的 dummy world 不会误判为一次玩家切图。

## 三名黑市敌人

实际伤害源为各敌人继承组件模板 `AIBehaviorsComponent.MeleeAttackDamage`。

| 敌人 | 原伤害 | 当前伤害（后续指定值） |
| --- | ---: | ---: |
| Misto | 7 | 10 |
| Nara | 7 | 10 |
| Brosky | 25 | 20 |

后续调整脚本：`Scripts/StreetFlowFix/set_damage.py`；增加 `-DamageVerify` 仅验证。
该调整前的蓝图备份：`Scripts/StreetFlowFix/BackupDamageRetune/`。

仅修改这三个敌人各自的继承组件 override。地图无需重新保存。
回归同时检查真实 `BlackMarketStreet` 中已放置的三名敌人组件值，并通过真实碰撞组件的
`ApplyDamageToActor` 验证实际扣血值。

## Brosky 死亡

原 `BP_AIBehaviorsComponent.DeadRagdoll` 的两条路径最终都会进入 ragdoll。
`Use_Dead_Ragdoll=False` 仅表示先播放动画，之后仍通过两段 Delay 进入物理模拟。

组件新增可选的 `AnimationOnlyDeath`，默认 None。只有 Brosky 的组件 override 配置为
`/Game/AI/AIBehaviorSystem/Demo/Animation/Death_Bwd`。
有配置时走 `UVTGCombatStatics::TryAnimationOnlyDeath`，停止 AI/移动，关闭死亡碰撞与物理，
通过非循环单动画播放并保留末帧；跳过原来会再次启用 ragdoll 的整条路径。
其他敌人的 None 配置继续执行原图；父级死亡计数/通知链保留。

运行回归通过真实致死 PointDamage 进入死亡流程，模拟 10 秒，验证播放资产、末帧时间和物理关闭。
无界面动画测试每帧显式推进动画并刷新骨骼。

## 下水道开关卡住

`BP_0ISXInteractiveCollision_L3Switch` 对应地图中的 `BP_0ISXInteractiveCollision_C_1`，
TRIGGER ID 为 `L3_OpenRoundDoor`，开启 `Lock Cam on Interact?`。
该 ID 进入 `L3_RoundDoorSwitch → L3_OpenRoundDoor`，原链只播放开关/门动画。
本应启动 `L3Sewer_Still_DialogueNode_NPC_22` 的 `K2Node_CallFunction_60` 执行输入没有连线。
因此镜头/操作锁定没有正常对话结束作为释放契机。

修复在 `K2Node_CallFunction_9`（调用物理开门事件）之后连接上述 BeginDialogue。
物理开门事件本身保持不变，checkpoint 恢复等直接调用它的路径不会额外播放对话。
回归调用真实开关事件，验证 NPC_22 启动、对话可结束和 DoorOpened checkpoint 被置位。

## 黑市入口 NPC_14 不前进：公共根因

`L4_Entrance_BlackMarket_Freemove` 为 `bFreeMovement=True`，但 `bAlwaysAutoAdvance=False`。
`UVTGDialogueHistorySubsystem` 会把用户的全局 Auto 设置传给新对话。
Auto 关闭时，旧 `UDialogue::FinishNPCDialogue` 把当前可跳过的 NPC 台词置为等待手动继续。
自由移动对话没有普通剧情对话的模态继续入口，于是停在 NPC_14。
NPC_17 负责启用 `ISXColi_Uni_GoToMarketStreet`，因此它也不会执行。

此前 `VTGCombatSewerPolish` 只为 `L3Sewer_Freemove` 设置了 `bAlwaysAutoAdvance`，
解决了 egg comment 的具体资产，但没有修复其他自由移动对话的相同条件。

本次修复公共 `UDialogue`：

- `SetAutoAdvance` 在自由移动对话中保持自动推进。
- `FinishNPCDialogue` 不会把自由移动对话置为等待手动继续，即使设置调用顺序不同。
- 普通 `bFreeMovement=False` 剧情对话继续遵守 Auto 设置。

运行回归使用真实入口对话，主动关闭 Auto，验证 NPC_14 → NPC_16 → NPC_17 → 正常退出，
并确认 NPC_17 的实际绑定事件启用了出口交互，而不是只验证节点编号。
旧 Sewer 回归测试也调整为分别验证普通模式的手动等待和自由移动模式的公共自动规则。

## 复用与限制

应用：`-run=VTGBlackMarketFollowup -StreetFlowFix -unattended -NullRHI`。
重新加载并只验证：增加 `-VerifyOnly`。
旧 Sewer 回归：`-run=VTGCombatSewerPolish -Tests -unattended -NullRHI`。

蓝图原件备份：`Scripts/StreetFlowFix/Backup/`。
历史倍率基线：`Scripts/StreetFlowFix/damage-baseline.txt`。当前工具使用指定的绝对值 10/10/20，重复应用不会累乘。
日志：`Saved/Logs/StreetFlowBuild.log`、`StreetFlowApply.log`、`StreetFlowVerify.log`、`StreetFlowSewerRegression.log`。
不修改用户存档。命令行回归验证逻辑与动画状态，不替代完整地图 PIE 的镜头和动画观感复测。
