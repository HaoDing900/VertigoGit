# 手工本地化：使用、覆盖范围与维护

本工具只收集原文和保存手工译文，不调用机器翻译。Origin 保留资产或 C++ 内的中英文混合原文；正式英语使用 `en-US`，简体中文使用 `zh-Hans`。原生资源使用 `en` 承载 Origin，不能把正式英语也放进 `en`。

在 **Project Settings → Game → Manual Localization / 手工本地化** 中设置 `Target Languages` 和 `Default Release Language`。可以添加其他有效 UE 语言代码，例如 `ja`；翻译面板和主菜单使用同一列表。新增语言还需在 **Packaging → Localizations to Package** 中加入对应代码。`en` 和 `origin` 保留给开发原文，重复或无效代码会阻止编译。

## 当前接入范围

以下按当前代码核对；“可收集”不等于已完成翻译，也不等于已逐项检查实际游戏界面。

| 对象／入口 | 已接入的显示文本 | 保留的逻辑数据／边界 |
| --- | --- | --- |
| Narrative Dialogue 节点 | 主台词、Alternative Lines 正文、Player Option Text 和 Hint Text | 节点 ID、Speaker ID、条件、事件不翻译 |
| Dialogue 默认设置 | Speakers、Player／Party Speaker Info 的 Speaker Name | 空显示名不能靠翻译 Speaker ID 解决 |
| 项目 Level Sequence 字幕轨道 | Section 的 Text、Speaker | 时间范围、Show Speaker、字体、样式和音频不参与翻译 |
| Narrative Quest | Quest Name、Journey Main Description、分支／状态 Description、嵌套 Task Description Override | ID、事件名、目标状态、任务参数与数量不翻译；自动描述需另行审计 |
| 地图地点控件 `VTGMapLocationNode` | Location Display Name、Location Description | 只编辑显示层，不改地图资源名、加载路径或解锁标识 |
| 跳过提示控件 `VTGHintLayer` | Skip Hold Text、Skip Action Text | 不改按键绑定、长按时间或跳过判定 |
| Mail DataTable | 新增 SenderDisplayName；邮件界面读取此 FText | 原 SenderName、行名、DialogueID、头像和 Dialogue 引用保留 |
| PDA 文档与页面 | `PDA_NoteText` 的 Name、Pages | Tab 分类枚举、资源引用不翻译 |
| PDA 标签标题 | `PDA_NoteOptions.Text`、`S_TabInfo.Text`、`WB_TabButton.Text` | 枚举值、Widget Index 和分类标识保留 |
| Show Hint | HintDescriptions 中每项的 Text、NewMailHintText | 原 HintDescription 作为查找 ID，ActionName 保留 |
| Actor Interaction | `AC_ActorInteractionComponent` 的 Interaction Text | 交互检测、距离、图标、禁用条件不变 |
| 通用 Details／手工编辑窗口 | 能读取到且有稳定身份的 FText，例如 UMG、Data Asset、Data Table 中的显示文本 | 未知字段需人工确认用途；没有对所有 UI 自动添加折叠区 |
| C++ 文案收集 | Vertigo runtime 源码中的字面量 NSLOCTEXT | 不自动解析所有宏、动态拼接或插件源码 |
| 主菜单与游戏控制台 | 共用文本语言设置，支持即时切换 | 不切换配音；不保证第三方或自行缓存的字符串自动刷新 |

地图和跳过提示的用法与 Dialogue 相同：打开相应控件蓝图的默认值／Details，找到上述字段，展开多语言区。不要把 Widget 名称或地图标识当作显示文本翻译。字幕样式与预览的详细操作见 [字幕预览、样式与本地化](SubtitlePreviewAndLocalization.md)。

## 使用

### 推荐的日常流程

1. 在 Origin 模式编写原文，正常保存 Dialogue、Quest、Sequence 或控件资产。
2. 选中单个对象，展开对应字段的多语言区，手工填写 English／简体中文。
3. 保留受保护编号；点击“保存并应用译文”。仅离开输入框保存译文，不等于已更新游戏资源。
4. 在 PIE 中切换语言，检查当前显示和下一次打开界面的结果；检查长文本、中文字体、换行、按钮尺寸及富文本。
5. 修改 Origin 后复核已有译文，再保存并应用；提交原资产、翻译表和编译资源。

**未编辑时显示 Origin 副本不是已经翻译完成。** 编译成功也不代表所有目标语言都已填写。

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

### Narrative Quest 就地编辑

Quest 的默认值面板中，Quest Name 和 Journey Main Description，以及图中分支／状态的 Description、Task 的 Description Override，都使用 Origin 下方的折叠多语言编辑区。英文、中文未填写时继承 Origin；手工编辑后按“保存并应用译文”。控制台继续使用 `vtg.Language origin`、`vtg.Language en`、`vtg.Language zh` 预览。

ID、事件名、目标状态、Task 参数和完成数量不参与翻译，面板显示逻辑风险提示。自动生成的 Task 描述应通过 Description Override 提供可翻译的显示文本；空描述保持为空，不改变任务 UI 的原有回退行为。译文仅保存到翻译表，原 Quest 资产和任务存档身份保持原值。路径收集命令也包含 QuestBlueprint。

### Mail、PDA、Show Hint 与 Interaction Text

- **Mail**：原 `SenderName` 保留为 Name 标识，新增 `SenderDisplayName` 作为玩家显示的 FText，并在该字段下提供折叠译文区。相同 SenderName 复用同一显示文本身份，避免同一发件人在不同行使用不一致译文；行名、DialogueID、头像与 Dialogue 引用保留。
- **PDA**：`PDA_NoteText` 父类的 Name、Pages 正文接入；Tab 保留枚举／索引，翻译标签配置中的 Text（包括 `PDA_NoteOptions.Text` 和 `S_TabInfo.Text`），不翻译枚举标识。
- **Interaction**：`AC_ActorInteractionComponent` 的 `Interaction Text` 下提供折叠译文区，组件默认值和实例沿用同一规则；运行时覆盖它的文本仍需另外保证来源可本地化。
- **Show Hint**：`WBP_HintLayer` 默认值增加 `Show Hint | Localization`。在 `HintDescriptions` 中用稳定 ID 对应原 `HintDescription` 参数，编辑该项 Text 的 Origin 和译文；原参数与 ActionName 保持不变。找不到条目时回退到原 Name，新增调用时需同步添加显示条目。新邮件提示通过 `NewMailHintText` 编辑。

Show Hint 示例：调用节点的 `HintDescription` 填 `Hint_OpenDoor`，在 WBP_HintLayer 默认值的 HintDescriptions 新增一项，ID 填同一个 `Hint_OpenDoor`，Text 填原文，然后展开 Text 的多语言区手工填写译文。不要翻译 ID，也不要创建重复 ID；重复时运行时使用首个匹配且非空的 Text。动态传入的 Name 也需有对应条目，否则沿用原文回退。

Mail 的 SenderDisplayName 首次从旧 SenderName 复制；以后修改显示名应编辑这个 FText。新增邮件行时也需要填写它的 Origin。旧 SenderName 不自动跟随显示名变化，显示名为空时运行时仍回退到旧 SenderName。运行时另外赋值的 Interaction Text 也需要是有稳定本地化身份的 FText，仅修改组件默认值无法覆盖其他代码的赋值。

Mail／Show Hint 的一次性迁移已于 2026-10-17 完成，6 行 Mail 的原字段校验通过，相关蓝图编译成功。原资产备份位于 `Saved/Backups/DisplayLocalization/FB1285954BBAD2FBA1D95D9F368AE335/`。日常翻译不需要重新运行迁移工具。

维护工具为 `-run=VTGDisplayLocalization -Apply`，仅供后续检查与迁移使用，应在编辑器关闭后运行。它会先备份、核对旧 Mail 字段，再编译相关蓝图并保存；不是整批资产的原子事务，若保存中途失败应查看日志和备份。新增回归测试为 `Vertigo.Localization.AssetDisplayInline`，测试译文写入隔离目录，不写入正式翻译表。

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

## 后续维护

### 文件与职责

| 文件／模块 | 作用与维护原则 |
| --- | --- |
| `Localization/ManualTranslations.json` | 手工译文源表，包含文本身份、原文、来源、审核信息；应备份并纳入版本控制，优先通过编辑器修改 |
| `Content/Localization/Vertigo/` | 游戏读取的 `.locres` 和 `.locmeta`；由工具生成，不直接手改；与源表一起提交 |
| `Config/DefaultGame.ini` | `[Internationalization]` 的资源路径，以及 Packaging 的语言打包列表；原生 `en` 资源仍需保留 |
| `VTGLocalizationSettings`／Project Settings | 目标语言列表与发行默认语言；新增语言时同步打包配置和字体检查 |
| `Source/VertigoEditor/Private/VTGManualLocalization.*` | 收集、保存、结构保护、原文复核与编译；修改会影响所有文本类型 |
| 同目录 `VTG*LocalizationDetails.*` | Dialogue、Subtitle、Quest、Map、Hint 的安全字段判断与折叠编辑入口 |
| `Source/Vertigo/Private/UI/VTGLocalizationSubsystem.cpp` | 运行时语言、控制台命令、用户偏好及 PIE 预览恢复 |
| Narrative 插件的 Details Extension | 保留原插件节点编辑器并追加项目 UI；更新插件时检查扩展声明、广播与模块依赖仍在 |

### 修改原文、复制资产或合并版本时

先保存原资产，再重新收集／编译并处理待复核译文。编译会重新读取已记录的 `/Game/` 来源包，但这不等于扫描整个项目，也不保证能识别所有删除、搬迁和旧条目。重命名资产、复制节点、更换文本键或升级插件后，应重新收集相关目录，检查来源与文本身份，再在游戏中验证；不要仅为保留译文强行复制 Key。

多人修改同一 JSON 时先关闭相关编辑会话，再合并；按 Namespace／Key 核对原文、译文和审核信息，不能直接选择整份文件的一方覆盖。外部文件变更后若工具拒绝保存，应保留自己的草稿、合并磁盘文件并重新打开编辑器加载；不要反复强行保存。编译资源为二进制，合并源表后统一重新生成。

新增一种显示字段时，先确认没有使用文本做条件比较、查找、事件路由或存档身份，再使用独立 FText 显示字段接入。就地编辑既需要 Details UI，也需要共享编辑器的安全字段白名单；只添加 UI 不代表已允许保存。增加验证：默认跟随 Origin、手工保存、结构保护、原文变化、语言切换，以及逻辑标识不变。未知 String／Name 不应直接放进翻译白名单。

### 发布与维护检查

1. 保存相关资产，收集改动目录并编译；检查错误日志，而非只看是否生成文件。
2. 检查每种语言的缺失、待复核及未确认条目。当前工具没有强制“全翻译完成”的发布检查。
3. 在 PIE 和实际打包版本检查主菜单、对话／历史、Quest 日志、字幕、地图及提示；即时切换与重新打开界面都要检查。
4. 确认字体包含中文和目标语言字符，检查长文本、DPI、富文本及布局；确认各语言资源和国际化数据已打包。
5. 提交对应资产、JSON、配置和生成资源；如果编译中途失败，修复后完整重编译，不发布部分更新的资源。

代码修改后可运行 `Vertigo.Localization`，并回归 `Vertigo.Dialogue.History`、`Vertigo.Sequence.Subtitles`。这些测试不能代替实际打包、字体与布局验收。2026-10-17 本次扩展构建成功，10 项本地化／对话历史／字幕测试通过，新增的 Mail 行编辑器与实时译文测试也通过。

## 潜在隐患与排查

| 现象／风险 | 原因与处理 |
| --- | --- |
| 发行版出现混合中英文原文 | Origin 仅从语言选项和接口隐藏；未翻译／待复核文本仍可能回退到原文，原文也仍存在资产与原生资源中。若要求玩家完全看不到 Origin，必须补齐译文并增加发布完整性检查 |
| 保存译文后游戏没变化 | 保存源表与编译资源是两步；先“保存并应用”，再检查语言、文本来源和资源路径 |
| 原文改了，旧译文消失 | 译文仍在源表，但审核原文不匹配时不编译；复核后重新保存，不能把旧审核状态直接当作有效翻译 |
| 同一个翻译出现在多个地方 | 多处共享 Namespace／Key；可能是有意复用，也可能是错误复制。不同原文同键会锁定；相同原文同键的上下文差异仍需人工判断 |
| UI 只在重新打开后更新 | 蓝图或控件可能把 FText 转成 String 或缓存静态结果；保留 FText 身份，必要时监听语言变化刷新，不能假定所有自定义控件都即时更新 |
| 空字段不能编辑译文 | 空描述可能控制隐藏或回退行为，先决定并填写 Origin；当前无法仅在某一种语言新增文本，也无法用空译文隐藏非空 Origin |
| Task 仍显示自动生成的原文 | 自动描述来自任务蓝图／参数，未必属于内联翻译字段；优先用 Description Override。分支 Description 优先于 Task Override，先确认实际显示来源 |
| 编译通过但仍漏字 | 收集受资产类型、来源路径与文本身份限制；临时 String、动态生成、图片文字等不会因此自动本地化 |
| 预览语言影响其他 PIE 窗口 | 编辑器游戏本地化预览使用共享文本管理器；多 PIE 实例独立语言和交错退出应另行验证 |
| 资源不一致或被覆盖 | JSON 有外部覆盖保护，但整套多语言资源不是一次原子发布；失败后完整重编译。同键的其他本地化目标也可能覆盖结果 |

关键警示只是提醒，不会禁止你在原编辑器修改 ID／参数；“安全显示文本”的白名单也不能证明自定义蓝图从未读取这些文本参与逻辑。新增逻辑或更新插件后需要重新审计。

## 本版边界

统一 Details 入口和资产扫描覆盖反射得到的 FText 字段。蓝图图内的临时 String、动态拼接、纹理/视频内文字、没有 FText 身份的节点常量仍需逐项审计与迁移。C++ 扫描目前覆盖 Vertigo runtime 的字面量 NSLOCTEXT，不自动解析任意宏和插件源码。Origin 中实际包含 `⟦编号⟧` 的文本不适合此保护标记语法。

关卡资产名与用户填写的存档名保持原值；地图已有独立的地点显示名／描述字段，不能因此改动用于加载地图的标识。此工具未迁移旧剧情存档结构。多语言字体、主菜单下拉框的实际布局和最终打包显示仍需视觉验收。此版没有添加配音语言系统。

自动化测试：`Vertigo.Localization`（结构保护、FText 身份与格式重建、Details 入口和面板构建）。已有字幕与对话测试用于相关行为回归。
