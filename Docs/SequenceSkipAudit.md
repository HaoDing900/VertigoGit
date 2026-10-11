# Sequence 快进检查

检查日期：2026-10-11。只读检查，未修改实现或资产。

## 可复用部分

UVTGSequenceSkipSubsystem：按住 Esc 0.8 秒后，使用 SetPlaybackPosition(..., Play) 逐显示帧推进，而非 Jump 或 GoToEndAndStop。每步运行 Sequencer 待执行动作，遇到 Pause/Stop/对话边界停止快进。单次输入更新预算 6ms，最多 240 步。

Vertigo.Dialogue.CinematicRegression 已执行并成功：起始/中间/末帧 Trigger 一次、Repeater 持续求值、OnFinished 一次、Pause/Resume、不越过活跃对话或选项边界。测试使用 30fps、30 tick resolution 的合成 Sequence；不代表全部真实关卡过场或音频已验证。

## 问题与限制

1. 现行 Esc 流程只创建右下角 Hold Esc / Skipping 提示，没有全屏 Fade。BP_Player_Sa 中找到的 WBP_Fade 创建节点用于 ANM_Sa_Death_Bwd1_Montage 死亡流程，不是 Esc 快进。
2. 起始时扫描所有正在正向播放、非循环、无 VTG.NoSkip 标签的 LevelSequenceActor，可能同时推进其他一次性 Sequence。应改为显式注册当前剧情过场，保留排除标签。
3. 任何活跃 Narrative 对话都会阻止此快进，因此 Narrative 对话行启动的过场可能无法用 Esc 快进。这是当前安全边界，不是事件丢失；后续需要区分固定过场台词和需要玩家交互的选项。
4. 只推进 Sequencer 时间，不推进世界时间。事件内 Delay/Timer、独立 Montage、渐变或异步操作不因快进而完成；可能打乱原本依赖真实时间的后续操作。需要逐场景验证，不能仅凭事件触发就保证完整演出正确。
5. 同一渲染帧内可推进多显示帧，因此后续字幕应该按当前 Sequence 时间解析，不依赖事件次数或独立 Delay。黑屏应持续至结束、暂停边界、取消或切关卡，而不是固定几秒。

## 推荐

保留 AdvanceFrame 与事件安全边界，补当前过场注册、全屏遮罩与生命周期通知，再以 Sequence 当前时间驱动字幕。实现字幕/音频前，选择一个包含事件和暂停的真实过场做集成检查。

证据：Source/Vertigo/Private/UI/VTGSequenceSkipSubsystem.cpp，Source/VertigoEditor/Private/VTGCinematicRegression.cpp；测试输出 Scripts/SequenceSkipAudit/regression.log 和 Report/。
