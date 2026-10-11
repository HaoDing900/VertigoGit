# Sequence 快进开关与事件检查面板

## 使用

编译后的编辑器模块需重新启动 Unreal Editor 才能加载。

打开 Level Sequence 时，自动显示可停靠的 **Sequence 快进检查** 面板。面板标题标明正在检查的资产路径。

- 所有旧、新 Sequence 默认 No Skip，不需要批量添加标签或修改资产。
- 需要快进时，勾选面板中的 **Allow Skip**，并保存该 Sequence。
- 设置存储在资产的 VTGSequenceSettings Asset User Data 中，打包后也生效。
- Actor 上已有的 `VTG.NoSkip` 标签仍优先禁止快进；循环或反向播放仍不参与。
- 此开关控制根 Sequence 播放器，包括它驱动的子 Sequence 演出；单独播放子 Sequence 时使用子资产自己的开关。
- Narrative 活跃对话的快进保护保持原样。

## 检查提示

检查器从当前 Sequence 和子 Sequence 中启用的 Trigger/Repeater Event 入口开始，沿执行连接检查蓝图函数、Custom Event、宏和折叠图。不会扫描未接入执行链的孤立节点。

会提示 Delay、Retriggerable Delay、SetTimer、播放 Montage、Timeline、异步代理与标记 Latent 的调用。每条结果显示 Sequence/事件时间和调用链。外部/原生函数、未解析入口、超出检查上限的链路标为“需确认”。纯 Montage 状态查询不作为播放操作提示。

修改事件图后点 **重新检查事件**。勾选 Allow Skip 只保存许可，不会自动修复异步事件。

没有提示不代表安全：动态绑定、运行时选择对象、原生调用内部行为无法完整静态分析；快进仍不会推进世界 Delay/Timer 或独立 Montage 的时间。

## 验证

- Unreal Editor Win64 Development 编译通过。
- `Vertigo.Dialogue.CinematicRegression` 通过：默认禁止、实际不推进、手动启用，以及事件、完成、暂停和对话边界回归。
- `Vertigo.Sequence.AsyncInspector` 通过：接入链的 Delay/Timer、排除孤立 Montage、真实 Event Track、禁用 Section、嵌套 Sequence、无法解析入口。

本次未改动任何 Sequence 资产，也未新增 Fade 遮罩或字幕。编辑器面板已注册自动打开，但没有在用户的交互式编辑器窗口中进行视觉检查。
