# 警局动画脚步与合同重开（2026-11-05）

## 动画声音

- `ANM_Bello_Loco_Walk_Fwd_NoRM_Mirror`：保留 0.36、1.11 秒两个 PlaySound Notify，声音改为 `/Game/SFX/Character/SaFootstep/Waves/Footstep_Boots_06`。
- `ANM_Corvos_Walk_Fwd`：在 0.26、0.90 秒增加 `/Game/SFX/FootstepAndFoleySounds/Footsteps/StereoVersions/Wavs/footstep_concrete_walk_04`，独立 Footsteps 轨。
- 原资产备份位于 `Scripts/PoliceContractAudio/Backup`。原动画姿势、长度及其他事件不变。

## 合同重开

用户现象：提前关闭合同后进入重读对话，加入重开时的传送／转向后 Note 不再弹出。

检查发现 `BPLM_PoliceStation` 已在首次进入 `L3_Contract` 时保存 `PendingNoteAsset_0`，但 `Client_OpenFullScreenWidget` 仍连接原 `L3_Contract` 的事件参数（通过 Knot），没有读取保存的资产。`L3_ReopenContractIfNotFinished` 没有 NoteAsset 参数，依赖了另一个入口留下的事件参数帧。

修复：

1. 打开 RPC 改为读取 `PendingNoteAsset_0`。
2. 共用的打开链在 Delay 之后、打开之前，重新设置 `ForceReadAllPages=true`、`Note Close Allowed=false`。
3. 保留用户设置的 `K2Node_CallFunction_65` 传送／转向节点、其变换值及原有提前关闭回到 NPC_46 的回调。未修改 Narrative 全局行为、对话内容、场景 Trigger 或 Note 插件。

隔离测试使用真实生成的 BPLM 的临时副本，保留重开链和 Delay，把最后 UI RPC 换成“传入资产有效”的标记；不在测试中保存临时副本。

- 修复前：正常事件参数可达打开；清空旧事件参数但保留有效缓存后，打开收到无效资产。
- 修复前：缓存 Player Pawn 为空会报告 Accessed None，但不会单独阻止后面的打开链；因此排除“Set Actor 节点因空 Pawn 直接截断流程”的猜测。
- 修复后：旧事件参数清空，打开仍收到有效的缓存资产。
- 两份动画的声音和各两个时间点经资产读取检查通过。

传送位置落在 Corvos Trigger 的包围盒内，这是一项排查线索，尚未证明该 Trigger 在用户这次复现中重入或改变了事件参数。隔离测试也未证明“传送本身使参数失效”；目前确认并修复的是重开读取旧参数的缺陷。完整警局剧情与真实 Note 页码／关闭操作尚未在 PIE 中验证。

工具：`-run=VTGMailSequencePolish -ContractProbe -ApplyContract`；`-VerifyContract` 只读验证保存的资产与重开链。日志位于 `Scripts/PoliceContractAudio`。
