# Corvos 接近演出被提前结束的回归

## 原因

新增的 Sequence 入口规则会在外部过场开始时 Force Exit 当前 Narrative 对话。此前只在 `UDialogue::PlayDialogueSequence` 创建的相机镜头上加了 Narrative 归属标记，遗漏了项目已有的另一条入口：

`BPA_0SmartTrigger_CorvosTrigger → BPLM_PoliceStation.CorvosTalk → L3PoliceStation_Still / NPC_6 → 1PlayLevelSequence → UNarrativeComponent::PlayDialogueLevelSequence → NLS_L3_CorvosConv_SaApproach`

这条路径生成的 Sequence 被误判为外部过场，触发 Force Exit。结束对话会调用 StopDialogueLevelSequence 并释放输入锁，导致演出在播放启动／求值期间被拆掉。用户现场日志中出现对应脚本调用栈和 `MovieSceneSequencePlayer` 的 `TickManager` ensure。

这是新增退出逻辑漏掉保护路径造成的回归。此前只用带标签的模拟对白镜头验证，未覆盖实际 `1PlayLevelSequence` 事件入口。

## 修复

1. `UNarrativeComponent::PlayDialogueLevelSequence` 在 Prepare / Play 之前给创建的 Actor 添加 Narrative 归属标记。
2. `UVTGSequenceDialogueGate::ShouldEndDialogue` 同时检查 Narrative Component 当前实际拥有的 Sequence Player。即使标签缺失，归属关系也能阻止它结束自己的对话。

修复覆盖所有经此 API 播放的 Narrative 演出。外部地图／游戏过场的退出旧对话开关现为默认关闭（2026-11-04 用户要求），显式启用后才执行，Force Exit 的显式调用也保留原行为。

未改动 Corvos 的地图、Smart Trigger、台词内容、节点计时、Sequence 长度、动画轨道或相机。运行测试里的角色绑定只在独立进程内修改，不保存资产。

## 验证

`-run=VTGMailSequencePolish -CorvosRegression` 使用实际 NPC_6 中的 `1PlayLevelSequence` 事件与实际 SaApproach 资产，在隔离世界中绑定 Sa / Corvos，检查：

- 开始、中段及自然结束时，对话不会被入口钩子提前结束。
- 中段仍保持移动／视角输入锁。
- 没有标签时，真实归属检查仍有效。
- 显式 Force Exit 可正常清理自有演出。
- 外部过场仍可退出旧的不可手动退出对话。

该检查使用受控的对话输入所有者，以便隔离这次退出回归；它不是完整真实地图剧情的端到端 PIE。隔离世界不包含全部地图对象，可能产生缺少场景引用的 Blueprint 警告。测试关闭音频与渲染，不改用户存档。

日志与修复前现场错误摘录位于 `Scripts/CorvosSequenceRegression`。

修复编译完成。Corvos 专项 15 项检查、原 Sequence 入口／Mail 21 项检查和原 Montage 暂停恢复 8 项检查通过。当前 DLL 已更新，可重新打开 Editor 复测警局 Trigger；未执行完整警局剧情 PIE 流程。
