# 黑市战斗时序、统一清理与敌人头顶名称（2026-11-07）

## 战斗开始

前置对话的末尾 routing node `DialogueNode_NPC_195` 调用 `L4_GoonInCombat`。
它在 Narrative 的节点事件调用栈内部执行，此时 CurrentDialogue 尚未清空，玩家的对话结束回调尚未完成。
原入口会立即移动/激活敌人，再进入 InitializeAlleyCombat。

`BPLM_BlackMarket` 的 `L4_GoonInCombat`、`StartAlleyConflict` 和 `InitializeAlleyCombat`
入口现在调用 `AVTGLevelManagerBase::DeferEncounterUntilDialogueEnds`。
存在当前对话时停止执行入口体，监听该 Narrative 组件的结束通知。
结束广播之后的下一帧再检查 CurrentDialogue，只有真正没有对话时才恢复原入口一次。
如果结束通知表示切换到新对话，会继续等待新对话结束。恢复 checkpoint 的重建过程直接执行。
Level Manager EndPlay 时解除绑定，异步恢复使用弱对象回调。

## 三人统一消失

Misto/Nara 原来在 ragdoll 后按 `DeadLifespan=5` 各自销毁。
Brosky 的动画死亡路径没有进入 ragdoll，也没有那个独立计时，因此清理不同步。

三人的继承组件 `DeadLifespan` 都设为 0，保留尸体到本场战斗统一处理。
`OnDefeated` 原有计数降至 0 的胜利路径启动 `RemoveActorsTogether`，
在最后一人被击败后的 5 秒同时销毁这三个战斗敌人。
计时使用弱引用，已经被其他剧情清理的对象会跳过；离开世界时计时器随世界结束。
原胜利剧情仍然继续。Brosky 的 Death_Bwd 动画死亡及其余两人的 ragdoll 方式不变。

## 显示名称与字体

- 仅三名战斗敌人的 EnemyDisplayName 改成可本地化的 `Street Goon`，内部资产名不变。
- 共享 `WBP_EnemyHealthBar.EnemyName` 使用 `/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Light_Font`。
- 公共 `UVTGEnemyIdentityComponent::RefreshName` 同时为敌人的自定义 EnemyName 控件应用该字体，
  避免其他敌人使用自己的控件时退回旧字体。保留字号、颜色、描边和布局。
- 伤害继续为 Misto 10、Nara 10、Brosky 20。

## 验证

应用：`-run=VTGBlackMarketFollowup -StreetCombatPresentation -unattended -NullRHI`。
新进程重新加载并验证：增加 `-VerifyOnly`。

回归复制真实 Level Manager 蓝图，保留入口 gate、死亡计数和统一清理调用。
测试副本把激活体替换为计数器，隔离与本问题无关的演出：

- 对话未结束时不执行激活体；多次请求只恢复一次。
- 对话结束并替换为另一个对话时仍不执行。
- CurrentDialogue 清空且结束回调返回后的下一帧执行一次。
- 前两次击败后即使经过 6 秒，三名敌人仍存在；第三次击败后 4.9 秒仍存在，超过 5 秒同时移除。
- 在三名真实敌人实例的实际头顶控件上验证 `Street Goon` 和 Light 字体。

`Scripts/StreetCombatPresentation/verify_map.py` 在新进程加载真实 BlackMarketStreet，
验证已放置敌人的名称、独立清理关闭、伤害和 Brosky 死亡动画配置。
回归不替代完整地图 PIE 的战前/战后演出观感复测。

备份：`Scripts/StreetCombatPresentation/Backup/`。
日志：`Saved/Logs/StreetCombatBuild.log`、`StreetCombatPresentationApply.log`、`StreetCombatPresentationVerify.log`。
