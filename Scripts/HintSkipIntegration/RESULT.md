# WBP_HintLayer：Mail / Skip 共用提示层

实际资产：`/Game/Blueprints/Widget/WBP_HintLayer`。

## 布局与交互

- 原 Mail 的 WB_ControlsHints 保留右侧居中，原 HintText 位置不变。
- 新增 SkipHintPanel，右对齐、底部留 40，默认 Collapsed。长按进度、按键图片和两段显示文本均在同一 WidgetTree。
- 字体复制现有 WB_ControlsHintBlock.Action_Text：Noto Sans SC Medium、18 号、相同颜色和描边。
- 按键图片使用现有模板计算出的 45×45，仍使用统一 Inventory Controls 资源。
- Skip 复用玩家 HintLayerRef 指向的已有层；隐藏 Skip 不会移除整层或清空 Mail。原 Hide Hint 蓝图函数也不会关闭 Skip。
- 移除了原来的独立 Slate 提示文字/图标，只保留上层黑屏遮罩。0.8 秒长按、0.5 秒渐黑、目标 20 倍快进、4 秒闲置隐藏及对白/暂停/失焦/切关卡保护继续使用。
- 保留现有 Allow Skip 范围；未批量开放正式剧情。

## 文案和本地化

打开 WBP_HintLayer → Class Defaults → Skip Hint / Localization：

- Skip Hold Text：Origin 为 Hold，简体中文为 长按。
- Skip Action Text：Origin 为 to skip，简体中文为 跳过。

字段使用 FText，拥有和 Narrative dialogue 相同的 Origin 原文、目标语言编辑和原文变更复核流程。现有目标语言为 en-US、zh-Hans；英文沿用 Origin。项目设置中的语言列表继续控制可编辑语言。

修改译文后，使用现有手工多语言编辑功能编译翻译资源。已加入中文并编译到 Content/Localization/Vertigo。Localization/ManualTranslations.json 只新增两个 VTGHint 条目，保留其余原始条目。

## 验证

- VertigoEditor 构建通过。
- VTGHintIntegration -VerifyOnly：资产保存读回、位置、字体、图片尺寸、Mail/Skip 独立显示、原 Hide Hint 蓝图函数互不影响、中文 locres 和英文编辑入口通过。
- CinematicRegression / SequenceSkipPrompt 两组原有快进回归通过。
- preview.png 为实际 UMG 渲染；Mail 文本仅用来标示原位置，并非游戏内截图。

日志：verify.log / render.log。修改前资产与本地化文件备份在 Backup。未进行 Git push。
