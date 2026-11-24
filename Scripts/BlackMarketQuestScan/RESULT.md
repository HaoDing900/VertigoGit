# NPC_279 Quest 与 NPC_185 Scan 修复

修改资产：L4_BlackMarketInvestigation_Still_MainInvestigation。
原资产备份：Backup/，保留用户此前已保存的更改。

## NPC_279 Quest
原 NE_BeginQuest 的 Optional Start from ID = QuestBranch_7。
Narrative BeginQuest 只按 State ID 查找；Branch ID 不会进入有效状态，也不广播有效 OnQuestNewState。
此外，BeginQuest 拒绝再次创建已有任务，不能推进已开始的任务。
QuestBranch_7 的所属状态是 QuestState_1，任务内容为“从地下入口离开黑市”。

该节点改用 VTG Ensure Quest State 事件：
- 缺失任务时从 QuestState_1 开始；
- 已有进行中任务进入 QuestState_1，沿用原实例与历史；
- 已到达此状态或任务已经完成时，重播对话不回退进度。
不修改 Narrative Overlay 或整个 Narrative 插件的 BeginQuest 行为。

## NPC_185 Scan
NPC_177 的链是 LookAtInterestPoint → MoveComponentTo（4秒）→ OpenScan。
NPC_185 的 TurnOffScan 链本身连通，但快速推进时，4秒完成回调可能晚于关闭事件，再次打开扫描。

在同一对话蓝图新增 AllowIPHAScan：
- NPC_177 开始设为 true；
- 共享 TurnOffScan 链的所有现有入口设为 false；
- 移动完成先检查许可，再决定是否 OpenScan。
保留移动、对白、Drone Mesh、扫描材质及全局投影逻辑。

## 验证范围
C++ Development Editor 与对话蓝图编译通过。
运行真实 Quest 事件与 Overlay 的 On Quest New State：TaskBox 生成3个子控件。
验证新任务开始、已有任务不重建而推进、重播不回退后续状态。
运行真实节点177/185生成的蓝图回调，并推进4.5秒 latent action：
快速177→185保持关闭；正常177→完成打开→185关闭。
保存后重新加载并复验。未播放完整PIE剧情或进行画面验收。

## 第三拳与HUD建议（未实施新 stamina 系统）
当前徒手第三拳 Montage：ANM_Sa_FightFist03_Inplace1_Montage。
长度1.466667秒，RateScale=1，BlendOut=0.25。
伤害Notify区间0.259640–0.366570秒；ComboWindow区间0.582843–1.466567秒。
攻击逻辑设置 MOVE_None，并通过现有重置/打断路径恢复。
这些时间不直接等于固定0.5秒收招；建议恢复提示读取实际攻击许可与移动恢复状态，
并处理受击、闪避、Montage中断、播放速度、时间缩放和剧情锁定。
