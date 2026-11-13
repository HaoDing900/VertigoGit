# Sequence 内部音频音量倍数

打开 Sequence 后，在现有的「Sequence 快进检查」窗口调整「内部音频音量倍数」。也可以在 Sequence、Movie Scene 或关卡内 Level Sequence Actor 的 Details →「过场音频」中调整同一设置。

窗口中的「过场声音预设」与关卡实例 Details 共用同一份 Sequence 资产设置，可选择保留现场声音、突出剧情（压低环境，淡出音乐）或静音环境与背景音乐。修改后保存 Sequence；切换打开的 Sequence 时显示对应资产的设置，支持撤销。

- 默认 **1**，保持原有音量；**0.5** 减半，**2** 加倍，**0** 静音。输入非负倍数，按 Enter 或离开输入框提交。
- 设置保存在 **Sequence 资产**上，修改后保存该资产。同一资产的所有播放实例共用设置。
- 倍数乘以所有 Audio Track 片段当前的音量曲线，包含绑定到角色的 Audio Track 和嵌套 Sequence。父、子 Sequence 的倍数相乘。
- 原有片段音量关键帧、共享 Sound Wave / Cue 均不改写。从 0 改回 1 可恢复原音量。编辑器预览和游戏播放使用相同逻辑。
- 此设置控制 Sequencer 的 Audio Track；Director 事件或动画 Notify 另行播放的声音不在此范围内。环境声和背景音乐的压低/静音仍使用现有「过场声音预设」。

验证：`Vertigo.Sequence.AudioVolumeMultiplier` 自动化测试覆盖默认值、放大、嵌套相乘、连续求值不累乘、静音恢复、音量曲线及停止后重播。
