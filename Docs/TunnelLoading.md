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

1. **PIE 里不显示 loading screen** —— 必须用 **Standalone**（New Editor Window → Standalone）或打包版测试。
2. **视频要能无缝循环** —— `MT_LoadingLoop` 循环最后一段直到关卡就绪，长加载不穿帮。
3. **PreLoadMap 只对 travel/OpenLevel 触发**，不对 level streaming 子关卡触发。这套是给关卡间硬切用的。
4. **Content/Movies 要打包进去** —— 引擎默认会包含 Movies 目录；确认打包设置里没排除。
5. Movie Names 或 Overlay 至少填一个，否则不注册 loading screen（避免黑屏）。

## 5. 相关

- gameplay 隧道场景实现：[[tunnel-scene-implementation]]
- 背景滚动组件：[[parallax-scroll-component]]
