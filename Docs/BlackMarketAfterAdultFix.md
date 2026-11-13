# BlackMarketStreet：AfterAdult 入巷与战斗存档修复

地图：`/Game/ThirdPerson/Maps/Street/BlackMarketStreet`。

## 首次入巷需要重新进入

`BPA_0SmartTrigger` 的可重复触发路径经过 `UVTGSmartTriggerComponent::CanAccept`。
AfterAdult 的自由移动对话仍在播放时，对话保护会拒绝首次 overlap，结束对话也不会重新发送 overlap。

`BPLM_BlackMarket.VTG_RestoreAlleyTrigger` 现在根据 `L4_FirstTimeTriggerAlley?` 设置
`SetAllowDuringDialogue`。首次入巷可以进入原来的退出对话、锁定输入和播放 sequence 链；
首次分支消费前即关闭例外。碰撞、busy、同次进入去重和后续对话保护仍然生效。

## 前置对话结束后进入战斗崩溃

崩溃记录 `Saved/Crashes/UECC-Windows-E8514EEC4C8414B4815DCFBB4B584E6F_0002/CrashContext.runtime-xml`
指向 `UNarrativeComponent::Save` 对空 `CurrentState` 的读取。调用链来自
`InitializeAlleyCombat → SaveEncounterCheckpoint → SaveCheckpoint → AutoSave`。

AfterAdult 初始化的 `K2Node_CallFunction_90` 把 `BeginQuest.StartFromID` 填成了
`QuestBranch_9`，但此参数只接受 State ID。现在使用该分支目标 `QuestState_1`。
Narrative 保存时也检查任务当前状态；异常任务记录警告并跳过，避免破坏其他任务的存档。

## 验证与复用

应用：`UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGBlackMarketFollowup -AfterAdultRepair -unattended -NullRHI`

只验证：同一命令增加 `-VerifyOnly`。

回归使用真实 Trigger 蓝图和 Level Manager 的临时副本，验证对话中的首次 overlap 能到达入场分支，
后续对话中的 overlap 被阻止，以及战斗期间禁用、胜利恢复和离巷销毁行为。
入场分支后的演出链在测试副本中替换为计数器，因此此测试不包含完整 PIE sequence 画面验证。

存档回归从真实黑市任务类复制状态，确认旧 Branch ID 无法解析、新 State ID 有效，
并将有效任务与空状态任务一起保存，读回有效任务和 MasterTaskList。测试使用唯一临时存档名，结束后删除。

修复前蓝图备份：`Scripts/BlackMarketAfterAdult/Backup/BPLM_BlackMarket.before.uasset`。
构建、应用和验证日志：`Saved/Logs/BlackMarketAfterAdult*.log`。
