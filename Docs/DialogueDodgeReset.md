# 对话结束后脚步消失：闪避标志复位（2026-11-05）

## 现场与根因

用户在失声 PIE 中运行 `Scripts/PoliceNotifyObservation/live_snapshot.py`。记录显示 `IsDodging?=True`，没有活动 Montage、没有当前 Narrative 对话、`InCombat=False`，Actor 碰撞开启，动画为正常 ABP_Sa 且 IsCutscene=False。`UVTGPlayerAudioComponent::CanPlayFootstep` 在 IsDodging? 为 True 时拒绝播放。

`BP_Player_Sa.EventGraph` 的 NPCDialogueLineFinished 处理器会先设置 IsDodging?=True。EndSequenceWhenNDialogueFinishes? 为 True 时，它枚举 LevelSequenceActor 并逐个销毁；旧连线从每次 DestroyActor 后进入 Delay 1 秒，再设置 IsDodging?=False。ForEachLoop 的 Completed 未连接。因此没有 Sequence Actor 时，整个复位链不执行。

原资产上的实际生成 Blueprint 回调已复现：清理开关开启、Sequence 数量 0，等待 1.1 秒后仍为 IsDodging?=True，脚步播放条件仍为 False。

## 修改

只保存 `/Game/ThirdPerson/Blueprints/BP_Player_Sa`。

- 断开 `K2Node_CallFunction_239`（DestroyActor）的 then → `K2Node_Knot_82.InputPin`。
- 改接 `K2Node_MacroInstance_0.Completed` → 同一 InputPin。
- 原 False 分支、逐个销毁操作、1 秒延迟和标志复位节点保留。复位现在在枚举结束后执行一次，包含空数组。
- 原资产备份：`Scripts/DialogueDodgeReset/Backup/BP_Player_Sa.uasset`。

没有调整碰撞、脚步材质分类、Notify 或 Narrative 全局行为。

## 验证

工具：`-run=VTGMailSequencePolish -DodgeReset`；`-Verify` 在新进程中只读验证保存资产。

在临时游戏世界中，对真实生成的 NPCDialogueLineFinished 回调测试：开关 False/True × Sequence 数量 0/1/3，共六种情况。每种都验证原来的 0.9 秒内仍保持抑制、1.1 秒后解除 IsDodging?、脚步播放条件恢复；开启时销毁全部测试 Sequence Actor，关闭时保留它们。未修改玩家保存文件。

已运行中的旧 PIE 实例不会自动应用磁盘上的蓝图更新；新实例使用已保存修复。本次未停止用户 PIE，也未写入其运行时标志。
