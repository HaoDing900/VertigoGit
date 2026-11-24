# Action HUD 收招恢复条

已在原 UVTGActionHUD 内加入运行时控件，不修改 WBP_Action、主角蓝图或攻击数值。
显示位置：右下角 Punch 提示下方，Recovery 标签与细进度条。
默认目标为徒手第三拳 ANM_Sa_FightFist03_Inplace1_Montage，可在 Action HUD 的 Combat Hints / Recovery Montage 配置。

行为：伤害 Notify 的最后结束时点开始显示；按当前 Montage 的实际位置计算恢复进度。
当前素材约从0.36657秒开始，到自然播放结束1.46667秒附近恢复；不写死0.5秒。
动作仍锁定时最多显示98%；自然结束且 IsAttacking 已清除、MovementMode 不再为 None 后，
显示满格约0.22秒并淡出。换招、闪避、受击打断、死亡、隐藏角色或剧情锁定时取消提示。
读取现有状态，不修改攻击、输入、伤害、移动、闪避、动画速度或资源消耗。
支持自然blend-out期间的Montage实例跟踪，防止进度提前消失；旧实例的延迟回调不会清除新实例。

验证：Development Editor 构建通过。
VTGRecoveryHUD commandlet 使用真实 WBP_Action、Sa角色与第三拳Montage，验证伤害窗口前隐藏、
窗口结束时近零进度、动画中段进度、播放速率/外部Delta不伪造进度、自然淡出、主动中断、
重复播放、闪避/死亡/剧情隐藏、真正解除锁定后满格与淡出、空Montage设置。
尚未完成完整PIE战斗体验验收。

## Narrative 任务进度用法

正常推进：Begin Quest 只负责首次开始；Quest State 的 Branch 配置任务，完成后进入 Destination State。
任务例子：BPT_PlayDialogueNode、BPT_GoToLocation。
Start from ID 填 State 的 ID，不填 Branch ID。
剧情节点直接切换阶段：Events 添加 VTG Ensure Quest State，设置 Quest 与 StateID。
NPC_279 已配置 Q_L4BlackMarketInvestigation + QuestState_1，包含 QuestBranch_7“从地下入口离开黑市”。
已有任务沿用实例与历史，重播不退回已到达的状态。需要重玩整条任务时另用 RestartQuest。
Narrative 3 本地插件源码明确说明 CompleteNarrativeTask 是旧版兼容接口，不再负责推进新版任务图。
UI 随 OnQuestNewState 更新，不需要直接改 Overlay 文本。

## Scan 修复用法

对话蓝图中的 AllowIPHAScan 为许可标记：
NPC_177 开始设 true；NPC_185 及共享关闭链入口设 false，然后执行 TurnOffScan；
Move Component To 的4秒完成回调先经过 Branch 检查，只有 true 才调用 OpenScan。
因此快速推进到185后，之前尚未结束的移动不会重新开灯；移动本身保留。
