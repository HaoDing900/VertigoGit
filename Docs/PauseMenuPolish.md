# Pause Menu 与 Sequence 入口调整（2026-11-04）

## Sequence

- “进入时结束 Narrative 对话”默认关闭。既有未设置实例和新增实例都不自动退出对话。
- 手动勾选保存 Actor Tag `VTG.EndNarrativeDialogueOnStart`；旧 `VTG.KeepNarrativeDialogueOnStart` 继续作为关闭覆盖。
- `1PlayLevelSequence` 的统一 `PlayDialogueLevelSequence` 入口在播放前标记 Narrative 所有权；入口退出规则始终排除这些播放器，即使有显式开启标签。标签丢失时仍通过 Narrative Component 持有的播放器引用保护。
- 显式 `Force Exit Dialogue` 的正常清理语义保留。
- 更新 Corvos 实际事件测试 15 项通过，入口／Mail 测试 21 项通过。包含默认关闭、显式开启、保护优先级、第一帧新对话保留、暂停恢复不重复退出。

## Pause Menu

仅保存 `/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu`。`WBP_MainMenu` 只读取，修改工具检查其磁盘 MD5 保持一致。

- 原 `B_Continue`、`B_Exit` 控件与事件图保留；复制 Main Menu 的 Normal / Hovered / Pressed 样式（Default / Hover / Selected 贴图）、字体、字号、文字颜色、阴影及按钮内边距。
- 按钮宽度采用 Main Menu 的 345.38 设计单位并保持居中；原标题保留。
- Canvas 层级：BackgroundBlur（Z=0，强度 6）→ 黑色 Image（Z=1，Alpha=0.5）→ PauseArtwork（Z=2，Pause_copy 原始 Alpha，Tint/RenderOpacity=1）→ 标题（Z=3）→ 按钮（Z=4）。
- 背景层均不接收输入。修改前资产备份在 `Scripts/PauseMenuPolish/Backup`。

资产工具：`-run=VTGMailSequencePolish -PauseMenu`；加 `-Verify` 仅回读验证。`-Preview=绝对路径.png -AllowCommandletRendering` 渲染实际 UMG。预览使用人工色块背景检查叠层，不是完整游戏 PIE 截图。

构建、资产编译与重新加载检查通过；完整游戏暂停／继续流程未在 PIE 中运行。
