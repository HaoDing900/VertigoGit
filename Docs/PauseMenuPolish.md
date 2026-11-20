# Pause Menu 与 Sequence 入口调整（2026-11-04）

2026-11-19 补充：新增 Sequence 资产级入口设置，动态创建的播放器也读取该默认值；实例可以开启、关闭或跟随资产。当前操作见 [Mail 与过场入口说明](MailSequencePolish.md)。下方 2026-11-04 记录中的默认关闭，是未配置资产默认值时的行为。

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

## 暂停菜单语言按钮（2026-11-14）

WBP_PauseMenu 继承 UVTGPauseMenu。打开时在 B_Continue 与 B_Exit 之间生成 B_Language，复制 Continue 的按钮样式、字体、颜色和文字内边距，以及 Exit 的列表间距；上下导航连接三个按钮。按钮显示当前语言，点击在 en-US / zh-Hans 之间切换，复用 VTGLocalizationSubsystem 与主菜单共享持久化设置，并监听语言变化立即刷新。无需 world tick，保持现有蓝图暂停/继续/退出事件。

按钮是运行时生成，Designer 中不显示。重新打开不会重复添加，退出控件时解绑。Origin 调试语言首次点击进入简体中文，之后仅两种正式语言切换。未翻译正文继续使用原有 Origin 回退规则。

维护：-run=VTGMailSequencePolish -PauseMenu -LanguageButton；-Verify -LanguageTest 重新加载并测试真实控件切换。资产备份 Saved/QA/PauseLanguage。
2026-11-14: Editor-only PreBeginPIE temporarily remaps an unmodified Escape StopPlaySession binding to Shift+Escape. EndPIE/module shutdown restores the prior chords; custom modified shortcuts are retained. Resolves editor Stop competing with gameplay Escape; game-window focus and UI input modes still determine whether the pause action itself receives input. Full PIE input acceptance remains to be checked.
2026-11-14: WITH_EDITOR pause language cycle is Origin → zh-Hans → en-US → Origin. Non-editor builds retain the Chinese/English cycle and reject Origin through the shared language whitelist.
