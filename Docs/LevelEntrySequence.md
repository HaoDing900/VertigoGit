# 关卡开场 Sequence 组件

## 在新关卡中使用

1. 打开关卡的 BPLM 蓝图，在 Components 面板选择 Add Component，搜索 `VTG Level Entry Sequence`。
2. 选中组件，在 Details 的 **Entry Sequence** 中选择演出资产。
3. 默认开启 **Play On Begin Play**，并在 Actor 的初始化逻辑结束后的下一次 Tick 请求播放。
4. 选择 **Start Policy**：
   - **Immediate**：请求时立刻播放。
   - **When Loading Starts Revealing**：加载遮罩开始淡出时播放（默认）。
   - **After Loading Screen Disappears**：等加载遮罩完全移除后播放。
5. 在 **Playback Settings** 配置输入锁定、隐藏 HUD、播放速度、循环和结束后状态恢复。这里的 Auto Play 会被忽略；自动触发由组件的 Play On Begin Play 控制。
6. 绑定组件的 **On Started** 和 **On Finished**。On Finished 的 `Skipped` 参数区分自然完成与跳过；读档抑制开场时也会报告 Skipped=true。

没有加载遮罩时，三种策略都会直接开始。等待期间不会执行 Play/Pause 或首帧求值，因此不会提前触发 Sequence 的事件轨道与音效。

## 由剧情手动控制

关闭 **Play On Begin Play**，在原有阶段判断通过后调用组件的 **Play Entry**。

如果原图已经创建并保存 Sequence Player、绑定完成事件，可调用 **Play Entry Player** 接管播放时机；它使用传入 Player 的已有播放设置。不要同时对这个 Player 直接调用 Play。

**Create Entry Player** 只使用组件配置创建 Player，不播放，可用于先绑定回调再调用 Play Entry Player 的流程。Get Entry Player 用于取得当前实例。

**Skip Entry** 在等待期间取消请求；播放期间跳到结尾并停止，然后触发组件 On Finished(Skipped=true)。它不会向外部 Player 的 OnFinished 委托伪造自然完成事件，因此新的剧情收尾建议绑定组件的 On Finished。

重复播放请求在等待或播放期间被忽略；完成后允许再次调用。自然完成、显式跳过和关卡销毁会清理组件监听。外部单独 Stop 会在下一次 Tick 作为跳过收尾。

## 读档

默认开启 **Skip On Save Resume**。组件检测 BeginPlay 时的恢复状态与当前地图的 OnSlotLoaded；读档恢复时抑制开场，避免重新覆盖存档站位。

On Finished 的 Skipped=true 可以用于分支处理：需要从存档重建的剧情状态应走恢复逻辑，不要无条件重复新游戏开场之后的剧情。

## 已迁移的街道关卡

`BPLM_L2StreetBarFight` 已增加 **LevelEntrySequence** 组件，Entry Sequence 为 `LS_L2BarStreet_IntroScene`。

- **Play On Begin Play=false**，由原有 `L2_PlayEntrySequence+ActionMode` 阶段逻辑发起。
- **Skip On Save Resume=false**，保留该关卡已有的 phase/checkpoint 恢复判断。
- Start Policy 默认是开始揭幕时播放。
- 原有完成回调、全局 Play Rate 设置和阶段流转继续保留；完成委托改用追加绑定，避免清除组件监听。
- 以后更换街道开场请修改组件的 **Entry Sequence**。原 `L2_EntrySequence` 变量保留兼容，但不再决定这条开场路径的资产。

其他关卡不必使用街道的这些例外设置，通常直接采用组件默认值即可。

## 验证与文件

编译：VertigoEditor Win64 Development。
回归：`-run=VTGLoadingScreen -VerifyEntryComponent`。
资产重载：`-run=VTGLoadingScreen -InstallEntryComponent -VerifyOnly`。

源码：Source/Vertigo/Public/Loading/VTGLevelEntrySequenceComponent.h 与对应 Private/Loading cpp。
资产备份：Scripts/BarBackDiagnosis/BPLM_L2StreetBarFight.before-entry-component.uasset。

尚未执行每个关卡的完整剧情试玩；带自定义事件轨道、相机切换和存档恢复的关卡应在接入后分别试玩。
