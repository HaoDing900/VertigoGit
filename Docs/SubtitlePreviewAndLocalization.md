# 字幕预览、样式与本地化

重启 Unreal Editor 加载编译后的模块。

## 编辑器预览

打开 TestSeq，在 **Subtitles / 字幕轨道右键 → 字幕预览与样式**。

面板中可勾选/取消 **编辑器字幕预览**；拖动 Sequencer 播放头会显示当前区间字幕。此开关只影响面板，游戏字幕仍始终启用。

预览使用与游戏相同的 WBP_SequenceSubtitles，按 1920×1080 的参考区域缩放显示。它是独立面板，不叠加到编辑器场景视口；背景不代表实际场景。预览高度缩小会同比缩小文字，因此最终仍需在游戏分辨率/DPI 下查看。

## 样式

面板下方可直接编辑当前字幕轨道的 **Subtitle Style**：

- Subtitle Font：字体资源、Typeface、字号等。
- Speaker Font：说话者字体与字号。
- Text Color：文字颜色。
- Outline Enabled：是否描边。
- Outline Size：描边宽度，0–16。
- Outline Color：描边颜色。

效果实时反映在预览；保存 Sequence 后同一配置用于游戏。描边由轨道设置统一覆盖 Font 内的描边参数。布局/位置继续在 WBP_SequenceSubtitles 的 Designer 中修改；字体由轨道配置覆盖。

## 本地化

Text 和 Speaker 是 FText，不是普通 String。新增字幕有独立的 Namespace/Key，避免所有默认“字幕文本”共享同一个翻译键。TestSeq 的三条现有字幕已设置独立标识，文本与时间不变。

正式翻译需在 **Localization Dashboard** 创建游戏本地化目标：

1. 确定原生语言及目标语言。
2. 配置从资产收集文本，路径至少包括 Content/LevelNSequence；说话者和字幕文本一起收集。
3. Gather，填写或导入翻译，Compile。
4. 在打包设置中包含目标语言/本地化资源。

字体需要包含相应语言字符。多语言可使用 Composite Font 配置 fallback 或 culture-specific 字体，避免缺字方框。字幕自动换行，但长译文仍需检查断行与时长。

面板支持输入语言代码后点击 **预览语言**。这是 Unreal 的游戏文本预览，会影响编辑器中的其他游戏文本，不修改游戏语言设置；点击“恢复语言”或关闭面板会恢复之前的预览设置。未配置游戏本地化目标时会明确提示；有目标但条目无译文时显示原文。

没有自动生成译文，也没有新增游戏语言设置菜单。游戏内语言切换仍需由游戏设置逻辑触发，已显示字幕使用 FText，可随已加载本地化资源更新。

## 验证

编译通过。Vertigo.Sequence.AsyncInspector、SubtitleEditorPreview、Subtitles 测试通过；覆盖实际 Sequencer 面板预览、拖动播放头、开关、关闭清理、字号/描边、独立本地化键及已有播放/快进规则。尚未检查实际译文资源或用户编辑器中的视觉布局。
