# Narrative 驱动 Slideshow 使用笔记

用途：游戏内看 PPT、电视画面、图文叙事，以及结局幻灯片。图片由对话节点或其他蓝图事件切换；没有计时自动翻页。UMG 自动创建，无需自己搭 Widget。

## 第一部分：Slideshow Director 的设置

### 创建与放入关卡

1. 创建 `VTGSlideshowDirector` 的 Blueprint 子类，或复制已有的 Slideshow 蓝图。
2. 在蓝图 Class Defaults 配置图片和动效，然后将蓝图拖入需要播放的关卡。
3. 选中场景实例，确认它的 Slideshow ID 和 Slides。实例可能覆盖蓝图默认值，运行时使用的是实例设置。

蓝图资产只是配置模板，**场景中必须有实例**，对话才找得到它。可以在蓝图中自行添加 Billboard，方便在编辑器定位；Actor 坐标不影响屏幕图片位置。

### Director 参数

| 参数 | 怎么设置 |
|---|---|
| Slideshow ID | 给这套图片一个名字，例如 `Soap_ep1`。同一世界内不能有两个同 ID 的 Director，包括已加载子关卡。对话事件填写同一个 ID |
| Slides | 按顺序添加图片条目，每项填 Image、Fade Seconds、Motion |
| Viewport Z Order | 默认 -1，让图片处于对话 UI 下方；只有出现层级问题时才调整 |
| Keep Black Screen On Close | **默认不勾**：结束后淡出并移除画面，返回游戏。结局需要停在黑屏时勾选 |
| Owning Player | 单人游戏通常留空，自动使用第一个本地 PlayerController |

### 图片编号：最容易弄错的一点

每张图都有可编辑的 **Image Index**。Narrative 的 `Slide Number` 和蓝图 `Show Slide` 填这个编号。

旧图片默认保留 1、2、3… 的编号；新添加的图片自动分配未使用的编号。数组 `[0]`、`[1]` 只是排列位置。例如 `[0]` 的 Image Index 改成 10 后，对话填写 10 即可显示它。

调整顺序或删除图片不会改变其他图片已有的编号。编号必须唯一且大于 0；填写 0 会自动分配。重复编号、找不到编号或 Image 未填写时，保持当前画面并输出警告。`Next Slide` / `Previous Slide` 按数组顺序切换，支持不连续编号。

### 每项 Slides 的设置

- **Image**：该项要显示的 Texture2D。
- **Fade Seconds**：切到这张图的淡入时长，0 表示立即切换。
- **Motion**：该图的缩放、位移、旋转、震动等设置，详见第三部分。

## 第二部分：在关卡中通过 Narrative 开始播放

关卡负责开始对话；对话中的节点负责显示、切换和关闭图片。开始 Dialogue 本身不会自动显示图片，必须有节点调用 Show Slide。

### A. Level Blueprint / BPLM：开始 Dialogue

如果已经持有 Narrative Comp 引用，直接调用它的 **Begin Dialogue**。

如果没有引用，可以按现有 BaseLevel 示例获取：

```text
关卡触发事件
 → Get Player Character
 → Get Component By Class（Component Class = NarrativeComponent）
 → Cast to NarrativeComponent
 → Begin Dialogue
```

上面的 Get Player Character 和 Get Component By Class 是数据节点：角色返回值接组件查询的 Target，组件返回值接 Cast 的 Object；触发事件的执行线接 Cast，成功执行线接 Begin Dialogue。

Begin Dialogue 的设置：
- **Dialogue**：选择要播放的 Dialogue 蓝图类。
- **Play Params → Start From ID**：通常留空，从对话根节点开始；要指定入口，就填该对话节点的 ID。
- 若在 BeginPlay 自动启动，确保角色及 Narrative UI 已经完成初始化。BaseLevel 示例使用 1 秒 Delay，这只是该示例的启动安排。

### B. 对话节点：换图（当前示例采用的方式）

打开 Dialogue，双击要换图的台词节点。Narrative 会在 Event Graph 中创建/定位该节点对应的开始事件。按下面连接：

```text
对话节点开始事件
 → Branch（Condition = Is Valid 的结果）
 → True：Show Slide（Slide Number = 要显示的编号）
```

数据线：

```text
Find Slideshow By ID（ID = 场景 Director 的 Slideshow ID）
 ├ Return Value → Is Valid（Object）→ Branch.Condition
 └ Return Value → Show Slide.Target
```

这里的 Is Valid 是返回布尔值的纯函数版本。没有换图需求的台词节点不用接这组蓝图，当前图片会继续显示和运动。

**当前插件的节点开始事件只在台词开始时触发。** 即使旧事件名字含 `Started/Finished`，也不要用它判断台词结束。

新增台词节点时，先双击新节点，让 Narrative 生成它自己的开始事件，再复制 Find / Is Valid / Branch / Show Slide 这组逻辑。不要把旧节点的开始事件一起复制过去，以免事件没有绑定到新台词。

### C. 更简短的换图方式：节点 Details → Events

如果不需要在 Event Graph 写额外逻辑，也可以选中台词节点，在 Details → Events 添加：

- **VTG Show Slide**
- Slideshow ID = 场景 Director 的 ID。
- Slide Number = 从 1 开始的图片编号。
- Event Runtime = **Start**。

两种换图方式选一种即可；不要在同一节点同时配置，否则可能重复换图并重启动效。

### D. 最后一个节点：关闭 Slideshow

选中最后一个台词节点，在 **Details → Events** 添加：

- **VTG Close Slideshow**
- Slideshow ID = 同一个 ID。
- Fade Seconds = 比如 0.5。
- Event Runtime = **End**。

必须显式添加关闭事件；仅仅结束 Dialogue 不会自动移除 Slideshow。

Director 的 Keep Black Screen On Close 决定结果：
- 不勾（默认）：幻灯片连同黑底、黑边一起淡出并移除，回到原游戏画面。
- 勾选：淡到黑色并保留，适合结局转场。

### E. 现成模板：BaseLevel / testSlideShow

| 项目 | 当前设置 |
|---|---|
| 关卡 | `/Game/ThirdPerson/Maps/BaseLevel` |
| Slideshow 蓝图 | `/Game/2DArt/SlideShow/SlideShow_L4BlackMarket_SoapTV` |
| 场景 Slideshow ID | `Soap_ep1` |
| Dialogue | `/Game/2DArt/SlideShow/testSlideShow` |
| Level BP 启动事件 | `StartSoapTVSlideshow` |
| Begin Dialogue 的 Start From ID | `testSlideShow_DialogueNode_NPC_1` |

| 对话节点 | 配置 |
|---|---|
| NPC_1 开始 | Show Slide(1)，显示 Index 0 |
| NPC_2 | 不换图，继续显示 Index 0 |
| NPC_3 开始 | Show Slide(2)，显示 Index 1 |
| NPC_4 开始 | Show Slide(3)，显示 Index 2 |
| NPC_5 结束 | VTG Close Slideshow，Runtime=End |

BaseLevel 的 BeginPlay 新增了一个 Sequence 分支，延迟 1 秒后调用 StartSoapTVSlideshow。如果要改成玩家互动时才开始，断开该自动启动分支，在交互事件里调用 StartSoapTVSlideshow。

复制 testSlideShow 做新 Dialogue 后，检查节点 ID 和 Begin Dialogue 的 Start From ID；不要直接沿用旧对话入口 ID。

## 第三部分：图片自适应和动效预览

### 一键自适应

先填 Image，再使用每张图预览下面的按钮：

| 按钮 | 效果 |
|---|---|
| Auto Fit (Cover) | 保持图片比例并铺满 16:9 画面，超出部分裁切 |
| Fit Inside | 保持比例显示整张图片，空余部分为黑底 |
| Top to Bottom | 自动铺满，并从竖图顶部滚动到底部；只需设置 Duration |

按钮重置 Start / End Scale 为 (1,1)、Offset 为 (0,0)、Angle 为 0。Top to Bottom 还会关闭震动和往返循环。Duration 保留，整次操作可以 Undo。建议先点按钮，再添加自己的动效。

底层设置位于 Motion：**Image Fit**（Stretch / Cover / Contain）与 **Pan Top To Bottom**。未使用自适应的旧配置默认仍为 Stretch。

### 直接预览，无需进入游戏

展开 Slides 的单项：
- **Start / End**：查看起点和终点构图。
- **时间滑条**：查看中间任意位置。
- **Play / Pause**：播放或暂停该张图的动效。
- 修改 Motion 参数后，预览实时更新。

预览最大 480×270，窄面板中等比缩小，保持 16:9。游戏使用固定的 1920×1080 逻辑画布，再等比适配实际窗口；窗口比例不同时留黑边。预览展示单张图片的动效，不展示图片之间的交叉淡入。

### Motion 参数速查

| 参数 | 意义 |
|---|---|
| Duration | 起点到终点所用秒数；结束后停在终点，不会自动翻页 |
| Start / End Scale | 起点、终点缩放；自适应模式下是额外缩放，1.1 表示放大 10% |
| Start / End Offset | 起点、终点位移，按 1920×1080 逻辑画布计算；X 正值向右，Y 正值向下 |
| Start / End Angle | 起点、终点旋转，单位为度 |
| Ease In Out | 缓慢起步、缓慢结束；关闭时匀速插值 |
| Loop Ping Pong | 在起点、终点之间往返，每程为 Duration |
| Shake Amplitude | 震动幅度，0 关闭 |
| Shake Frequency | 震动频率，Hz |
| Shake Duration | 震动衰减到零的秒数；0 表示持续震动 |

常用设置：
- **静止图**：Auto Fit 后，两端 Scale 均为 (1,1)，两端 Offset / Angle 为 0，Shake Amplitude 为 0。
- **缓慢放大**：Auto Fit 后，Start Scale=(1,1)，End Scale=(1.1,1.1)，Duration=10。
- **竖图从上往下看**：Top to Bottom，Duration=15，无需手算图片高度或位移。
- **短暂震动**：Amplitude=8，Frequency=10，Shake Duration=0.8。

图片平移/旋转/缩小后可能露出黑底，可适当放大留出余量。Top to Bottom 对没有超出画面高度的图不会产生竖向滚动。

## 第四部分：结束通知、普通蓝图控制和注意事项

### 结束后接其他流程

Director 的 **On Slideshow Closed** 在关闭渐变完成后触发。要接 Credits / Open Level，可以在开始播放前绑定：

```text
Director 引用 → Bind Event to On Slideshow Closed
                    Event → 自定义事件 HandleSlideshowClosed

HandleSlideshowClosed → Credits / Open Level / 其他后续逻辑
```

默认返回游戏模式会先移除旧 Widget，再发送通知。保留黑屏模式在通知触发时黑底仍然存在。

### 其他蓝图也可以控制

| 节点 | 用途 |
|---|---|
| Find Slideshow By ID | 获取当前世界中匹配 ID 的 Director；找不到或 ID 重复时返回空引用 |
| Show Slide | 显示指定编号；重复调用同一编号会重启动效 |
| Next Slide / Previous Slide | 切换到相邻图片；到边界不循环 |
| Set Current Motion | 更改当前图片的动效，从新设置的 Start 重新开始 |
| Stop Current Motion | 冻结当前动效位置并停止震动 |
| Close Slideshow | 按时长关闭，使用 Keep Black Screen On Close 开关 |
| Remove Slideshow | 立即移除画面，不发送 On Slideshow Closed |
| On Slide Changed | 成功换图时发出通知，参数为从 1 开始的图片编号 |

### 对话、输入与存档

- 玩家点击推进的可见台词可以设 **Line Duration=Never + Is Skippable**。动效 Duration 和台词 Duration 是两套独立设置。
- 此功能不会主动暂停游戏、禁用玩家移动或改变输入模式。Narrative 正常结束时沿用原有恢复逻辑；你额外禁用的移动/暂停应由你的流程恢复。
- 中途退出或取消 Dialogue 时，如果未走到最后一个关闭节点，需要你的退出流程额外调用 Close Slideshow 或 Remove Slideshow。
- 切关卡会清理本关 Director 的 Widget。
- 当前不自动保存图片编号或动效进度。若需要中途续播，存下编号并在读档后调用 Show Slide；不能仅依赖 Refire On Load。

### 排查速查

| 问题 | 先检查 |
|---|---|
| 不显示图片 | 场景里是否有 Director 实例、ID 是否匹配且唯一、编号是否为 index+1、Image 是否已填 |
| 动效总重新开始 | 是否同时挂了 VTG Show Slide 和节点开始蓝图，或重复调用 Show Slide |
| 结束后一直黑屏 | Keep Black Screen On Close 是否勾选；最后节点是否确实调用 Close |
| 对话结束后图片还在 | 最后节点是否有 VTG Close Slideshow，Runtime 是否为 End |
| 台词不会结束 | Never 模式是否允许跳过，是否正确接入现有 Narrative 推进输入 |
| 图片遮住字幕 | Director 的 Viewport Z Order 和项目 Narrative / CommonUI 层级 |
| 回到游戏但角色不能动 | 原流程是否禁用了输入或暂停，是否有对应恢复 |
| 开始对话但没到目标节点 | Begin Dialogue 的 Start From ID 是否属于当前 Dialogue |

具体示例见 [SoapTVSlideshowExample.md](SoapTVSlideshowExample.md)。可粘贴节点文本见 [启动与结束通知](Slideshow_StartEnding_A.nodes.txt) 和 [直接切图/关闭](Slideshow_DirectCalls.nodes.txt)。
