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
| ChapterLabel | 固定城市标识 THE GREAT ABYSS CITY。 |
| TipLabel / TipText | 固定「大渊城新闻」栏目名与随机新闻正文。正文支持自动换行。 |
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

Open Level With Stage、读档跳转和返回菜单现在统一走提前淡入的加载入口。
Loading 先在旧关卡上淡入；完全显示并经过一帧之后才发起 OpenLevel。
Standalone 将同一界面交给 MoviePlayer，PIE 保持原视口覆盖。新关卡就绪后仍停留 Hold After Load Duration（默认 0.5 秒），
同时确保总显示时间达到 Minimum Display Time，再淡出显示新关卡。不会重新随机图片或新闻，也不会在交接时重新淡入。
Project Settings → Game → VTG Media Loading Page → Transitions：
Fade In Duration 默认 0.4 秒，Fade Out Duration 默认 0.6 秒，设置为 0 可关闭对应渐变。
PIE 的 VTG.PreviewLoading 同样演示淡入和淡出；主菜单加载也支持 PIE；独立运行时使用 MoviePlayer 避免阻塞加载动画。
淡入由 Slate 执行，不依赖蓝图 Tick；淡出使用真实时间，不受游戏时间缩放或暂停影响。
这是 Loading 界面的透明度渐变；视频如需同样渐变，请使用不透明 UMG 背景覆盖视频。

主菜单 WBP_MainMenu 的原始 Open Level (by Object Reference) 已替换为 Open Level With Loading Screen (Object Reference)，目标 L2Bar 和原执行链保持不变。PIE 先在当前视口显示并淡入，再发起跳转；PostLoadMapWithWorld 确认新世界就绪、短暂停留并满足最短显示时间后开始淡出。连续点击会被合并，跳转失败及结束 PIE 时清理界面。


## 简化后的图片和新闻配置

界面只保留 **THE GREAT ABYSS CITY**、**大渊城新闻**、新闻正文以及 **正在载入** 和转圈标记。地点名、地点介绍和每条新闻的独立标题已移除。

打开 **Content/Widget/Loading/DA_LevelLoadingInfo**，现在只需要维护三个字段：

1. **随机图片 / Background Images**：添加导入项目的 Texture2D，每次 Loading 随机选一张。
2. **新闻正文列表 / News Bodies**：每个条目直接填写正文，无需设置标题，也不再按关卡重复填写。正文使用 FText，保留本地化支持。
3. **新闻轮换秒数 / News Rotation Seconds**：默认 5 秒随机切换到另一条；设为 0 就每次只显示一条随机新闻。

所有关卡使用同一个图片和新闻列表。空条目被忽略；只有一条新闻时不会反复切换。图片列表为空时使用 Designer 原有背景；新闻列表为空时显示 Designer 的新闻正文。

固定标签只在 **WBP_VTGLoadingScreen** 的 Designer 维护一次：`ChapterLabel`、`TipLabel`、`LoadingLabel`。新闻正文对应 `TipText`，图片对应 `BackgroundArt`。字体、位置、尺寸和颜色仍在 Designer 调整。

旧配置中的图片和新闻正文已经合并到通用列表，保留原 FText 本地化标识；旧地点介绍和新闻标题不再作为可编辑配置显示。迁移命令 `-run=VTGLoadingScreen -Simplify` 可重复执行，不会重复添加相同正文。

PIE 控制台输入 `VTG.PreviewLoading` 可预览 5 秒。要观察多次轮换，可通过蓝图调用 `Preview Loading Page`，把 Duration 设为 20。默认实际 Loading 最短仅 2.5 秒，因此较快加载时只会看到第一条随机新闻。

新闻轮换在 Slate 层执行，Standalone 阻塞加载时也可以继续切换；淡出阶段停止轮换。PIE 的同步地图加载期间不能绘制新帧，恢复绘制后继续轮换。

开发校验：`UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGLoadingScreen -VerifyRandomContent -nullrhi -unattended`。

### 关卡切换时序调整

使用 **Open Level With Loading Screen (Name)** 或 **Open Level With Loading Screen (Object Reference)**。这两个入口在旧关卡中先显示 Loading，再安排切关卡；Open Level With Stage、正常读档和死亡界面的返回菜单已接入。关卡床铺与近黑市隧道中剩余的普通 Open Level 节点也已替换。

普通 Open Level 的 PreLoadMap 回调仍提供后备 Loading，但它无法回到调用之前补出旧关卡中的淡入帧。新增 Blueprint 切关卡逻辑请使用上面的入口。

Project Settings → Game → **VTG Media Loading Page → Transitions**：

- Fade In Duration：Loading 淡入，默认 0.4 秒。
- Hold After Load Duration：新关卡加载完成后继续显示 Loading 的时长，默认 0.5 秒。
- Fade Out Duration：Loading 淡出显示新场景，默认 0.6 秒。
- Minimum Display Time：整个 Loading 的最低显示时长，仍为 2.5 秒；长时间加载也不会跳过新增加的完成后停留。

Loading Widget 类、数据资产和其引用的图片会在 GameInstance 启动时预加载，避免切关卡时才读取这些资源。地图本身仍然需要加载；这次调整改变等待期间的显示顺序，不缩短地图的数据加载时间。死亡后的同关卡 checkpoint 重试继续使用原来的黑屏淡入淡出。

时序验证：`-run=VTGLoadingScreen -VerifyTiming -nullrhi -unattended`。Blueprint 入口审查：`-run=VTGLoadingScreen -ConnectTransitions -nullrhi -unattended`；加 `-Apply` 仅迁移审查范围内仍使用普通 Open Level 的节点，并编译保存。
