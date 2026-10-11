# Sequence 字幕第一版

## 样板

打开 `/Game/LevelNSequence/In_Level/TestSeq`，字幕轨道中已有三段：

| 显示帧范围（不含结束帧） | 文本 |
|---|---|
| 0–50 | 11111 |
| 50–100 | 22222 |
| 100–150 | 33333 |

原 Sequence 内容和播放范围不变；未打开 Allow Skip。实际帧范围基于当前 150 帧样板。

## 编辑

重启 Unreal Editor 加载新模块。Sequencer 的 **+ Track → Subtitles / 字幕** 可添加轨道；字幕轨道右键 **Add subtitle at playhead / 添加字幕**，在当前播放头处添加一段默认 2 秒字幕。拖动 Section 改时间与长度；选中 Section，在 Details 中修改 **Text**、**Speaker**、**Show Speaker**。Text 和 Speaker 都是 FText，支持后续本地化。

Speaker 默认隐藏，即使填了名字也不显示。勾选 Show Speaker 才显示；名字为空时始终自动隐藏。

打开时的 Sequence 快进检查面板也会提示同一字幕轨道中重叠的区间。当前重叠时只显示起点较晚的一段，建议避免重叠。

## 显示和播放

共用 Widget：`/Game/Widget/Subtitles/WBP_SequenceSubtitles`。可以在 UMG Designer 调整位置与布局；字体、字号、颜色和描边由字幕轨道的 Subtitle Style 设置统一控制。保留变量 TextBlock **SubtitleText** 与 **SpeakerText**。

游戏或 PIE 内播放有字幕轨道的 LevelSequenceActor，会自动显示字幕。没有额外字幕关闭开关。字幕按播放器当前时间解析（转换到 Tick Resolution），不依赖 Delay 或 Event 触发次数；暂停保持字幕，快进跟随实际时间，停止/结束后清理，切关卡时移除。

显示层为 -1，位于默认 ZOrder 0 的 WBP_Fade 下方。此工作没有创建额外快进 Fade。

第一版支持一个正在播放/暂停的根 Sequence。不解析子 Sequence 的字幕；建议先把台词放在根字幕轨道。多个同时播放的字幕 Sequence 不混排，当前保持已有播放器。编辑器可在字幕轨道右键打开“字幕预览与样式”面板，使用开关查看当前播放头字幕；游戏屏幕 Widget 在游戏/PIE 播放时显示。

## 验证

编译通过；保存后重新加载验证三段字幕。`Vertigo.Sequence.Subtitles` 测试覆盖不同 Tick/Display Rate、区间边界、停用/重叠、实际 Sequence 快进贯穿三段、暂停/停止、Widget 文本和说话者开关。未在实际交互式关卡中检查视觉布局。

样板修改前备份：`Scripts/Subtitles/TestSeq.before_subtitles.uasset`。

