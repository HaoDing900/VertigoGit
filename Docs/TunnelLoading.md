# 隧道 Loading 过渡系统（Tunnel Loading）

> 2026-07 添加。关卡切换之间播一段 2-3 秒的隧道骑行画面（王家卫《堕落天使》那种浪漫感），同时后台加载目标关卡。用引擎 MoviePlayer，独立线程渲染，阻塞加载时照播不卡。

核心类：
- `UVTGMediaLoadingPageSystem`（GameInstanceSubsystem，`Source/Vertigo/.../Loading/`）
- `UVTGMediaLoadingPageSettings`（Project Settings 面板）

> 命名：这是个**通用的 loading 时播 media 系统**，隧道只是旗舰用法。名字不带 tunnel。

---

## 1. 为什么是 Media 而不是 Level Sequence

Loading 过渡的本质：**你正处在两个关卡之间**——旧世界在拆、新世界还没起来，中间没有活着的 World 能播 Sequence。而且加载时 CPU/GPU 忙着流关卡，再跑实时场景在抢资源。

所以用**预渲染视频** + 引擎 MoviePlayer：它在独立线程渲染，阻塞加载时照播；王家卫质感（抽帧、重动态模糊、霓虹拖尾、浓调色）用 Movie Render Queue 离线渲一遍随便加。

> 实时隧道技术（[[parallax-scroll-component]]、Toon 明灭）留给 **gameplay 隧道关卡**，那时引擎正常跑，Sequence 合适。两套分工。

## 2. 为什么是 Subsystem 不是 Component

过渡跨越关卡边界，ActorComponent 会随关卡销毁。GameInstanceSubsystem 活整个游戏周期，能在切图那一刻 arm MoviePlayer。

## 3. 如何使用

### 准备视频
1. 预渲染一段 **可无缝循环**的隧道骑行（3-5 秒，Movie Render Queue）。
2. 放进 **`Content/Movies/`**（没有就建这个文件夹）。Windows 默认支持 mp4。

### 配置（Project Settings → Game → "VTG Tunnel Loading"）
| 设置 | 说明 | 建议 |
|---|---|---|
| Enabled | 总开关 | 勾 |
| （面板名） | Project Settings → Game → **"VTG Media Loading Page"** | — |
| Movie Names | Content/Movies 里的文件名（无扩展名/路径） | 你的 mp4 名 |
| Minimum Display Time | 最短显示秒数（加载再快也不闪） | 2.5 |
| Loop Until Loaded | 循环最后一段直到关卡就绪 | 勾 |
| Skippable | 关卡就绪后允许玩家跳过 | 看需求 |
| Overlay Widget Class | 视频上叠的 UMG（暗角/标题/颗粒/字幕） | 可选 |

### 触发切关卡
- **推荐**：用 subsystem 的 BP 节点 **`Open Level With Loading Page`** 代替 `Open Level`。
- **自动**：什么都不改也行——subsystem hook 了 `PreLoadMap`，任何 `Open Level`/travel 都会自动带上（除非关掉）。
- 运行时临时关：`Set Loading Page Enabled(false)`。

## 4. 关键约束

1. **PIE 使用 UMG 视口加载界面**，Standalone / 打包版使用 MoviePlayer。主菜单已接入支持 PIE 的入口。PIE 同步加载期间画面会短暂停住，淡入在加载前完成，淡出在加载后执行。
2. **视频要能无缝循环** —— `MT_LoadingLoop` 循环最后一段直到关卡就绪，长加载不穿帮。
3. **PreLoadMap 只对 travel/OpenLevel 触发**，不对 level streaming 子关卡触发。这套是给关卡间硬切用的。
4. **Content/Movies 要打包进去** —— 引擎默认会包含 Movies 目录；确认打包设置里没排除。
5. Movie Names 或 Overlay 至少填一个，否则不注册 loading screen（避免黑屏）。

## 5. 相关

- gameplay 隧道场景实现：[[tunnel-scene-implementation]]
- 背景滚动组件：[[parallax-scroll-component]]


## 可编辑 Loading 界面

已配置 `/Game/Widget/Loading/WBP_VTGLoadingScreen`，不需要视频也能使用。
用 Content Browser 打开该 Widget Blueprint，在 **Designer** 中直接编辑并 Compile / Save：

| 控件 | 用途 |
| --- | --- |
| BackgroundArt | 全屏背景。Brush → Image 指定导入的贴图；将 Color and Opacity 改为白色可以显示原图颜色。默认纯色背景。 |
| EditableLayout | 1920×1080 设计画布，子控件可拖拽调整位置和尺寸。 |
| Title / Subtitle / ChapterLabel | 主标题、副标题和章节文字，直接修改 Text、Font、Color。 |
| TipLabel / TipText | 提示标题和正文。TipText 支持自动换行。 |
| TunnelLeft / TunnelTop / TunnelRight | 临时的通道线条美术，可自行替换或隐藏。 |
| LoadingSpinner / LoadingLabel | 原生 Slate 旋转标记和加载文字，可调整大小、颜色和摆放。 |

背景铺满屏幕，ResponsiveLayout 让画布等比例适配分辨率。超宽屏会保留更多背景区域。
模板没有运行时代码强制重写控件排版；也可以新建自己的 Widget Blueprint，在
**Project Settings → Game → VTG Media Loading Page → Overlay Widget Class** 中替换整个界面。
Minimum Display Time 设置最短停留时间（默认 2.5 秒）；真正加载未完成时不会提前消失。
`/Game/Widget/Loading` 已加入 Always Cook；新模板和依赖资源建议放在这个目录。

### 预览与接入

- PIE 中按控制台键，输入 `VTG.PreviewLoading`：显示同一模板 5 秒。
- 蓝图也可 Get Game Instance Subsystem → VTGMediaLoadingPageSystem → Preview Loading Page，设置预览时长；Hide Loading Page Preview 可提前关闭。
- 预览不暂停游戏、不修改输入模式。主菜单在 PIE、Standalone Game 和打包版均已接入；其他蓝图若需在 PIE 中先淡入再加载，请调用 Open Level With Loading Page。
- 普通 Open Level 已自动接入，也可调用 Open Level With Loading Page。子关卡 Streaming 的进度不属于此流程。
- 加载期间游戏线程可能阻塞：布局和文字应在显示前准备好，不要依赖 Tick、角色引用或 UMG Timeline 动画。模板使用的 Circular Throbber 由 Slate 绘制。
- 不显示虚构百分比。正在载入标记表示等待，加载完成后自动结束。
- 如果继续使用 Movie Names，模板默认不透明背景会遮住视频；将 BackgroundArt 隐藏或调低透明度即可显示视频。

开发校验：`UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGLoadingScreen -VerifyOnly -nullrhi -unattended`。
不传 VerifyOnly 时只会补建缺失的模板，不会覆盖美术已编辑的现有模板。


### 淡入 / 淡出

普通 Open Level、Open Level With Stage、读档跳转和返回菜单都会触发同一个自动加载流程。
Loading 内容在加载开始时淡入；实际加载完成且达到 Minimum Display Time 后，
同一界面转交新关卡的视口并淡出，结束时自动移除。
Project Settings → Game → VTG Media Loading Page → Transitions：
Fade In Duration 默认 0.4 秒，Fade Out Duration 默认 0.6 秒，设置为 0 可关闭对应渐变。
PIE 的 VTG.PreviewLoading 同样演示淡入和淡出；主菜单加载也支持 PIE；独立运行时使用 MoviePlayer 避免阻塞加载动画。
淡入由 Slate 执行，不依赖蓝图 Tick；淡出使用真实时间，不受游戏时间缩放或暂停影响。
这是 Loading 界面的透明度渐变；视频如需同样渐变，请使用不透明 UMG 背景覆盖视频。

主菜单 WBP_MainMenu 的原始 Open Level (by Object Reference) 已替换为 Open Level With Loading Screen (Object Reference)，目标 L2Bar 和原执行链保持不变。PIE 先在当前视口显示并淡入，再发起跳转；PostLoadMapWithWorld 确认新世界就绪后开始淡出。连续点击会被合并，跳转失败及结束 PIE 时清理界面。


## 按目标关卡编辑 Loading 文案

打开 **Content/Widget/Loading/DA_LevelLoadingInfo**（Data Asset），编辑 **关卡列表 / Levels**：

1. 展开一个条目，或按 `+` 新增条目。
2. **目标关卡 / Level**：用资源选择器指定加载目的地的地图，不是当前所在地图。
3. 展开 **该关卡文案 / Content**，填写地点名称、地区或章节、地点介绍、新闻或提示标题、新闻或提示正文。介绍与正文支持多行。
4. 保存数据资产。下次加载该关卡或重新预览时自动读取，无需修改按钮蓝图。

目前建立了 L2Bar、L_SewerUnderApartment、L0PastBunker 和 BaseLevel 四个条目。
L2Bar 已从你修改后的界面原样读取现有文案；其他条目只提供可编辑的地点名称，正文留空待填写。
空字段会清空对应文字，保留控件布局。每张地图只建一个条目。
未配置地图使用 **Default Content**，不沿用酒吧或上一关的文案。

### 与你当前排版的对应关系

| 数据字段 | 当前控件名 |
| --- | --- |
| Location：地点名称 | Title |
| Region：地区/章节 | ChapterLabel |
| Introduction：介绍 | Subtitle |
| NewsHeading：新闻/提示标题 | TipLabel |
| NewsBody：新闻/提示正文 | TipText |

WBP_VTGLoadingScreen 继续负责所有美术排版，运行时只调用 SetText；不会修改位置、尺寸、字体、颜色、背景或控件层级。
以后如果重命名文字控件，请同步调整同一数据资产的 **Widget Names**；设为 None 表示不接管该控件。
增加其他文字块时，可在每关的 **Extra Texts** 里填「控件名 → 文案」。支持 Text Block 和 Rich Text Block。
已绑定字段的正式游戏文案以数据资产为准，Designer 中的静态 Text 仅作为排版样例。

### 不切关卡就预览指定文案

运行 PIE，在控制台输入：

```
VTG.PreviewLoading L2Bar
VTG.PreviewLoading L_SewerUnderApartment
```

也可以在蓝图调用 **Preview Loading Page**，填写新增的 Level Name 参数。
不填参数时预览当前关卡的文案。蓝图需要自行排版时，可从数据资产调用 **Get Content For Level** 获取整组字段。
数据资产在 Project Settings → Game → VTG Media Loading Page → Level Loading Info 中配置，可以替换为同类型的其他资产。
已放在 Always Cook 的 Loading 目录中。查询仅比较地图软引用，不提前加载整张地图。
