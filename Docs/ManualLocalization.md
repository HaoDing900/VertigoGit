# 手工多语言编辑（第一版）

本工具只收集原文和保存手工译文，不调用机器翻译。Origin 保留资产或 C++ 内的中英文混合原文；正式英语使用 `en-US`，简体中文使用 `zh-Hans`。原生资源使用 `en` 承载 Origin，不能把正式英语也放进 `en`。

在 **Project Settings → Game → Manual Localization / 手工本地化** 中设置 `Target Languages` 和 `Default Release Language`。可以添加其他有效 UE 语言代码，例如 `ja`；翻译面板和主菜单使用同一列表。新增语言还需在 **Packaging → Localizations to Package** 中加入对应代码。`en` 和 `origin` 保留给开发原文，重复或无效代码会阻止编译。

## 使用

重新启动 Unreal Editor 加载新模块。

### Narrative Dialogue 就地编辑

在 Dialogue 图中选择单个 NPC 或 Player 节点。台词正文显示为上方的 **Origin** 原文编辑区，下方是默认折叠的 **多语言译文**。Player 的 Option Text、Hint Text 和 Alternative Lines 的正文也有各自的折叠区。节点 ID、Speaker ID、事件、条件及其他逻辑字段不加入可编辑的译文区。

Dialogue 的默认设置中，Speakers、Player Speaker Info 和 Party Speaker Info 的 **Speaker Name** 显示名也有同样的折叠区；**Speaker ID** 保持原来的标识字段，不创建翻译。未填写 Speaker Name 时请先填写 Origin 显示名，再编辑译文，不直接翻译作为回退的 Speaker ID。

展开后，English 和简体中文在没有手工译文时显示 Origin 的副本；这时只是跟随原文的默认视图，打开窗口不写入翻译文件，也不将副本标记为已翻译。修改 Origin 后，尚未编辑的语言继续跟随；已经手填的译文保留并标为待复核。

修改译文后离开输入框，会校验并保存合法内容。点击 **保存并应用译文**，确认当前字段的各语言内容并编译资源，再进入游戏切换语言。空译文或与 Origin 相同的内容恢复默认跟随。空的原文/选项/提示先保持不可翻译，避免用译文改变节点的自动选项或路由行为；请先填写 Origin。

语言预览期间，Origin 上方仍显示真正原文，但保持只读；请切回 `origin` 编辑原文。下方手工译文可以继续编辑，避免把 UE 预览中的显示译文误当作 Origin。

占位符和富文本仍使用受保护编号。若原文或文本键在未保存草稿期间改变，会保留草稿并阻止误写；可点击 **重新加载 / 放弃未保存草稿**。译文存储在统一翻译表，不修改原资产的节点 ID 或文本键。

### Level Sequence 字幕就地编辑

打开带有项目 **Subtitles / 字幕** 轨道的 Level Sequence，选择某一段字幕 Section，在 Details 中的 **Text** 和 **Speaker** 下方都有默认折叠的多语言编辑区。每个字段上方是自己的 Origin，未编辑的 English 和简体中文默认复制并跟随该字段的原文；已经手填的内容独立保存。

编辑后点击 **保存并应用译文**，在现有 **字幕预览与样式** 面板中预览 `en-US` / `zh-Hans`，或在游戏中使用下面的控制台命令。语言预览期间 Origin 始终显示真实原文并保持只读。此功能不改字幕时间区间、Show Speaker、轨道字体或全局样式，也不改现有字幕播放规则。

与 Dialogue 一样，译文存入同一份手工翻译表，支持受保护结构校验、原文变更复核和外部文件覆盖保护。多选时不提供就地译文编辑，避免把同一译文写到多段字幕。

### 游戏控制台

在正在运行的游戏或 PIE 窗口中打开控制台，输入：

```text
vtg.Language origin
vtg.Language en
vtg.Language zh
```

`en` 对应正式英语 `en-US`，`zh` 对应简体中文 `zh-Hans`；也可直接输入已配置的目标语言代码。只输入 `vtg.Language` 会在日志中列出当前语言及可用语言。此命令与主菜单共用语言设置服务，保存语言偏好，不切换配音。Origin 仅在带 Editor 支持的开发运行中可用；普通发行构建仍拒绝它。使用前先保存并应用手工译文，未翻译内容继续显示 Origin 回退。

游戏翻译路径配置在 `DefaultGame.ini` 的 `[Internationalization]` 中。编辑器运行时找不到游戏本地化资源会拒绝切换并提示原因，不会只改变语言设置而假装预览成功。

### 通用编辑面板

1. 在字幕 Section、Narrative 节点、UMG 控件或 Data Asset 的 Details 中，点击 Text 字段右侧的 **多语言 / 标识风险** 扩展按钮。按钮可能在行尾的扩展菜单里。
2. 也可通过 Window 中的 **手工多语言编辑** 窗口，点击 **收集 Content Browser 当前选中资产的文本**，再从下拉列表选一个字段。这个入口适合蓝图内部、嵌套结构和 C++ 标签没有直接文本属性编辑入口的情况。
3. Origin、Namespace、Key、原资产来源在面板中只读。未知文本默认不能填写译文；检查它确实只是玩家显示文本后，勾选用途确认。用途确认不代表工具自动完成了蓝图逻辑审计。
4. 手动填写对应语言。没有译文时保留为空，会回退到 Origin。
5. `{Name}`、`{0}` 和富文本标签在译文编辑区显示为 `⟦编号⟧`。结构参考给出原文中标记的位置；编号对应的实际结构由程序保存。允许按语序移动占位符标记，不允许遗漏、重复、改编号，富文本标签的嵌套顺序不能改变。
6. 点击 **校验并保存此语言**。保存意味着你确认当前译文对应面板显示的 Origin。
7. 点击 **保存审核状态并编译全部手工译文**。随后在已有的字幕预览语言功能或编辑器的 Preview Game Language 中预览 `en-US` / `zh-Hans`。修改原文后，重新收集或编译时会检查所记录资产；旧译文标为待复核并停止使用。

译文统一保存到 `Localization/ManualTranslations.json`。游戏仅读取编译后的 `Content/Localization/Vertigo` 中的标准 UE 资源；手工编辑表在 Content 之外，不随正常打包进入游戏。JSON 和编译资源都应纳入版本控制。不要在本目标之外另建一份同键译文，否则不同资源优先级可能造成覆盖。

## 风险与锁定

- String / Name 字段：显示标识风险警示，不创建译文，不修改原字段。
- 无稳定文本键、不可本地化文本和运行时生成结果：显示警示并锁定，先在原编辑器处理其 FText 来源。
- 支持用 `VTGLocalizationLocked="原因"` 属性元数据明确锁定字段；编译同样拒绝此类条目。
- 扫描中同一文本键对应多个不同原文时锁定，避免把翻译错误绑定到另一个字段。
- 原文变更的译文不编译；损坏的 JSON 不允许覆盖；被外部直接修改成错误占位符或标签的译文也会阻止编译。
- 其他编辑器或外部工具修改翻译表后，会阻止当前旧会话覆盖文件；需重新加载或先合并。保存使用临时文件替换，避免中途写入损坏原表。
- Origin 不在这个面板中编辑，防止翻译操作改动原资产。原编辑器仍保留正常编辑能力，警示并不代替 Unreal 自己的权限系统。

## 游戏语言

主菜单右下角增加语言下拉框，也可以在主菜单蓝图中放置名为 `TextLanguageSelector` 的 ComboBoxString 自行安排位置。

编辑器版本默认 Origin，并提供 Origin / English / 简体中文；没有 Editor 支持的打包版本只提供 English / 简体中文，接口也拒绝 Origin。首次发行启动跟随中文系统语言，否则使用英语。之后保存用户选择，与剧情存档独立。

语言切换只改变文本语言，不切换音频或游戏资产。PIE 使用 UE 游戏本地化预览，结束 PIE 恢复之前的编辑器预览设置。Narrative 变量使用 FText 格式保留当时的变量值，语言更新不会重播节点事件。Skip 控件依赖稳定文本键，英文内部标识保持不变。

## 收集与编译命令

使用项目所属 UE 5.3 的 `UnrealEditor-Cmd.exe`：

```text
Vertigo.uproject -run=VTGLocalization -GatherPath=/Game/Narrative -unattended -nop4 -NullRHI
Vertigo.uproject -run=VTGLocalization -unattended -nop4 -NullRHI
```

路径收集只加载常见文本资产类，不保存原资产；面板的选中资产收集不限制资产类。编译命令可以作为发布前的结构检查，但当前不要求译文全部填写，尚不能作为“翻译完成”的发布门槛。

## 本版边界

统一 Details 入口和资产扫描覆盖反射得到的 FText 字段。蓝图图内的临时 String、动态拼接、纹理/视频内文字、没有 FText 身份的节点常量仍需逐项审计与迁移。C++ 扫描目前覆盖 Vertigo runtime 的字面量 NSLOCTEXT，不自动解析任意宏和插件源码。Origin 中实际包含 `⟦编号⟧` 的文本不适合此保护标记语法。

关卡资产名与用户填写的存档名保持原值，未新增地点显示名表，也未修改旧存档结构。多语言字体、主菜单下拉框的实际布局和最终打包显示仍需视觉验收。此版没有添加配音语言系统。

自动化测试：`Vertigo.Localization`（结构保护、FText 身份与格式重建、Details 入口和面板构建）。已有字幕与对话测试用于相关行为回归。

## Narrative Quest 内联编辑

Quest 的默认值面板中，Quest Name 和 Journey Main Description，以及图中分支／状态的 Description、Task 的 Description Override，都使用 Origin 下方的折叠多语言编辑区。英文、中文未填写时继承 Origin；手工编辑后按“保存并应用译文”。控制台继续使用 `vtg.Language origin`、`vtg.Language en`、`vtg.Language zh` 预览。

ID、事件名、目标状态、Task 参数和完成数量不参与翻译，面板显示逻辑风险提示。自动生成的 Task 描述应通过 Description Override 提供可翻译的显示文本；空描述保持为空，不改变任务 UI 的原有回退行为。译文仅保存到翻译表，原 Quest 资产和任务存档身份保持原值。路径收集命令也包含 QuestBlueprint。
