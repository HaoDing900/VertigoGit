# WBP_Fade 参数说明

资产：`/Game/2DArt/ScreenTransition/WBP_Fade`。在 Class Defaults 的 Fade Settings 分类，或 Create Widget 节点上配置。旧 Create Widget 节点若未出现新增参数，可右键 Refresh Node。

| 显示名称 | 变量名 | 含义 |
|---|---|---|
| Fade Duration (seconds) | FadeDuration | 从纯黑变透明的持续秒数。默认 1.5；0 或负数立即透明。 |
| Total Widget Lifetime (seconds) | TimeBeforeKill | 从 Construct 开始到 RemoveFromParent 的总时间。默认 2。0 或负数在淡出完成后自动移除。正数是固定期限，可以截断等待或动画。 |
| Reverse Fade On Appear (1 second) | ReverseFadeOnAppear | 默认 false：直接纯黑。true：用现有动画反向播放，在 1 秒内从透明变纯黑。 |
| Black Hold Before Fade (seconds) | HoldDurationUntilFade | 达到纯黑后等待多少秒，再开始变透明。默认 0；负数按 0。 |
| Keep Black (disable fade out) | DisableFade? | 保留旧变量名。true 时不变透明；总时长为正仍定时移除，总时长为 0 则等待外部移除。 |

例如：Reverse Fade=true，Hold=1，Fade Duration=2，Total Lifetime=5：0–1 秒渐黑，1–2 秒纯黑，2–4 秒变透明，4–5 秒保持透明，5 秒从屏幕移除。

TimeBeforeKill 在修改前没有连接到移除逻辑，现在实际生效。玩家 BP_Player_Sa 的旧调用从 3.5 调整为 4 秒，以容纳原来的 2.5 秒等待 + 1.5 秒淡出。旧变量名称及默认值保留。

RemoveFromParent 是从界面移除，并非立即销毁被其他对象引用的 UObject。动画和延迟会在移除时取消；再次 Construct 时重置状态。

验证：蓝图编译通过；11 个实际 Slate/UMG 时序用例覆盖正反向动画、等待、自动移除、独立寿命、提前截断、零时长和持续黑屏，保存后重新加载也通过。未进行游戏内视觉验证。

原资产备份和日志：Scripts/FadeCustomization/。
