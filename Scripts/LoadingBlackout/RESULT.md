# Loading 黑屏过渡（2026-10-15）

沿用现有 WBP_VTGLoadingScreen、随机图片、新闻轮换、本地化和 MoviePlayer / PIE 交接，不修改图片数据或 Widget 布局。

当前节奏：黑屏1.5秒 → 用1秒显出 Loading 图片 → 原有图片展示/加载 → 用1.5秒渐黑 → 完全黑屏至少1.5秒并等待就绪 → 用1秒显示关卡。

Project Settings → Game → **VTG Media Loading Page → Transitions** 可维护全部时长。Minimum Display Time 仍为2.5秒，从图片完全显示的时间起计；极快加载整套最短约9秒。配置在 `Config/DefaultGame.ini`。

显示场景前检查目标 World 初始化、BeginPlay、子关卡与 World Partition 流送、异步包加载、SaveCoordinator 世界恢复，以及当前纹理/网格初始化和流送操作。就绪需跨帧稳定0.25秒，并经过完整黑屏帧。等待较久时每5秒在日志列出阻塞步骤或资源；不使用可能永远无法满足的“全部想要的 mip 均已加载”计数。

源码维护：

- `Public/Loading/VTGMediaLoadingPageSettings.h`：设置和默认值。
- `Public/Loading/VTGLoadingTransitionTiming.h`：不依赖 World 的时间与就绪稳定性逻辑。
- `Private/Loading/SVTGLoadingFade.h`：黑色底层、图片透明度、新闻轮换与最终揭幕；Slate 线程不读取 UObject。
- `Private/Loading/VTGMediaLoadingPageSystem.cpp`：切图、MoviePlayer/viewport 交接、就绪检查、异常清理。
- `Private/Loading/VTGLoadingBlackoutPreview.cpp`：仅编辑器构建的显式离屏游戏切图检查入口，需命令行 VTGLoadingProbe 才能执行并退出测试进程；正常游戏不运行。

验证日志：`build-probe.log`、`timing.log`、`content.log`、`travel-final.log`。完整维护说明见 `Docs/TunnelLoading.md` 顶部的新时序章节。

最终验证通过：VertigoEditor 构建成功；VerifyTiming 与 VerifyRandomContent 均 exit 0；独立离屏 -game 进程在启动 Loading 完全结束后再次执行真实 OpenLevel，确认旧世界先显示界面、世界已替换且 BeginPlay 完成、结束覆盖层移除，实测9.06秒（最短9.00秒），进程 exit 0。验证地图为 Engine Entry，未完整试玩实际项目大关卡。

就绪检查不代表任意自定义蓝图异步任务、未来才请求的全部贴图或完整关卡试玩。未执行 Git push。
