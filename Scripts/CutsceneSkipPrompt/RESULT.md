# Cutscene 跳过提示

## 交互

- 默认隐藏。仅在当前存在 Allow Skip 的、正在正向播放且不循环的 Level Sequence 时提供跳过。
- 任意键、鼠标按键或滚轮唤出右下角提示；无新操作 4 秒后收起，再操作可重新唤出。
- 键盘显示 `Hold [Space 图片] to skip`，手柄使用对应 A 图片。图片来自统一的 CommonInput ControllerData，资源在 `/Game/Inventory/InventoryXBased/UMG/Textures/Controls`。
- 持续按 Space 0.8 秒确认，显示长按进度条。确认前松手取消进度，提示仍保留至超时。
- 确认后即使松手也继续执行：0.5 秒渐黑；完全黑屏后按目标 20 倍速度快进。
- 播放结束后用 0.25 秒解除黑屏。
- Esc 保留原暂停功能。对白期间的 Space 仍走原对白控制。

## 播放与中断

仍用 Sequencer 的 Play 评估逐帧推进，不调用 Jump、GoToEndAndStop。普通播放本身的 1 倍速度加上按真实经过时间预算的额外 19 倍评估，目标为 20 倍；每次限制 6ms 工作量和最多 240 帧，避免长过场卡死程序。不改变全局时间膨胀或 Sequence 原始 PlayRate。

保留事件轨道、正常 OnFinished 和 restore-state 路径。对白/选项、世界暂停、窗口失焦、切关卡会中断并清理黑屏。Sequence 自己 Pause/Stop 或失去跳过资格，也会退出快进。切关卡发生在事件回调内时同样有重入清理保护。

## 当前允许范围

只读检查 `/Game/LevelNSequence`：80 个 Level Sequence 资产中，仅 `/Game/LevelNSequence/In_Level/TestSeq` 开启了 Allow Skip。此次未批量修改正式剧情的权限。

可在现有 `Sequence 快进检查` 面板里审查事件、勾选 Allow Skip 并保存 Sequence。`VTG.NoSkip` Actor 标签、循环 Sequence、活动对白仍会阻止跳过。

## 参数位置

`Source/Vertigo/Private/UI/VTGSequenceSkipSubsystem.cpp` 顶部 `SequenceSkip` 命名空间：

- HoldSeconds = 0.8
- FadeSeconds = 0.5
- RevealSeconds = 0.25
- HintSeconds = 4
- PlaybackMultiplier = 20

## 验证

VertigoEditor 构建通过；`VTGSequenceSkipCheck` 独立命令执行两个回归组均通过：

- CinematicRegression：事件触发、repeater、结束回调、Pause、对白边界与恢复。
- SequenceSkipPrompt：默认隐藏、唤出/闲置/再次出现、Space 提前松手、长按进度、0.5 秒渐黑、全黑后加速、20 倍时间预算、结束揭幕、禁止跳过/循环过滤、失焦、对白和事件内切关卡清理。

最终记录：`final-check.log`。`preview.png` 为实际 Slate 提示布局渲染，不是游戏内截图。

编辑器自动化发现和日志路由在该环境中出现崩溃，故独立命令直接执行同一组同步测试并检查各项断言结果。未完整跑过所有正式剧情，未执行 Git push。

## 嵌套 Sequence

只对实际播放的最外层 Sequence 开启 Allow Skip。子 Sequence 由根播放器按正常 Sequencer 层级评估，不需要独立播放器或逐个开启子资产权限；子资产单独播放时仍遵守它自己的权限。不要手动再调用子事件，否则会产生重复副作用。

嵌套运行回归覆盖两层嵌套、时间区间偏移、不同显示帧率、子 Trigger / Repeater 和根 OnFinished。事件内的 Delay、Timer 或等待动画仍使用原有游戏时钟，不随手动快放同步加速；逐帧触发保证不能替代异步流程的完成保证。互动对白和 Sequence 自己的 Pause / Stop 仍会结束本次快放。

只读检查示例：`-run=VTGSequenceSkipCheck -InspectSequence=/Game/LevelNSequence/In_Level/L3Apartment/LS/Intro/LS_L3_Apt_Intro.LS_L3_Apt_Intro`。
