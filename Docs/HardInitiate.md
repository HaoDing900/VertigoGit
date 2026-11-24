# Hard Initiate · 关卡初始化的统一入口

> 一句话：**关卡的布置代码放在 hard initiate 里，不要放在 `On Stage Begin` 上。**
> 因为恢复战斗检查点的读档会把 `On Stage Begin` 整段跳过。

贴 Confluence 用 `HardInitiate.zh-CN.confluence.html`（内容与本文一致）。

---

## 一、使用

### 两步接上

1. 在 BPLM 里写一个**无参数的 Custom Event**，名字随便取（约定用 `HardInitiate`）
2. Class Defaults → `Hard Initiate Event` 填这个名字

就这样，基类会在正确的时机按名字调它。**留空就是不启用**，没做这个事件的 BPLM 行为和以前完全一样。

### 三个节点

| 节点 | 类型 | 作用 |
|---|---|---|
| `Hard Initiate Event` | Class Defaults 字段 | 你的事件名。留空 = 不启用 |
| `Did Enter From Load` | BlueprintPure | 这次进图是不是从存档来的（普通读档和检查点恢复都算 true）。事件里随便哪儿都能读，不占引脚 |
| `Run Hard Initiate` | BlueprintCallable | 手动跑一次。基类会替你调，这个是留给「换场景后重新布置」「不走存档直接测试」用的 |
| `Story Checkpoint` | BlueprintCallable | 在玩家到达某个点时调：记下 sub-stage + 要一次自动存档。见下一节 |
| `Get Sub Stage` | BlueprintPure | 这次该从哪个检查点重建。None = 这个 stage 的默认流程 |
| `Clear Sub Stage` | BlueprintCallable | 丢掉检查点。基本用不到，嵌套的 switch 已经替你挡掉了过期值 |

### 一次性剧情怎么分开

同一条链上插一个 Branch 就行，刀口放在你想放的位置：

```
Event HardInitiate
   → 灯光 / MPC / NPC 摆位          ← 每次进图都跑
   → Branch (Did Enter From Load)
       False → 一次性演出            ← 只有正常进图跑
       True  → （空）
```

需要按事件逐个记（同一条链上多个一次性节点各自独立判断）时，用 `EnterSavedStoryEvent("id")`，
它记在存档里，跨进出都有效。

**「摆玩家」也要走这一刀。** `ApplyWorldState` 已经把存档里的玩家位置还原好了，而且跑在
hard initiate **之前** —— 所以 hard initiate 里再 `Set Actor Transform(Player Pawn)` 一次，
就是把存档里的位置盖掉。自由探索的段落通常不想要这个效果：

```
Event HardInitiate → ... → Branch (Did Enter From Load)
                              False → 摆玩家 + 摆同行 NPC       ← 只有第一次进图跑
                              True  → （跳过）
                           → 汇合：布景 / 跟随 / 标志位          ← 两边都要跑
```

范本在 `BPLM_BlackMarket` 的 `L4_AftAdtStartLocation`（图里有注释气泡）。两件事原来串在一条线上，
拆成「摆位置」和「布置场景」两段，只有前一段被 Branch 挡住。

同行 NPC 的摆位可以和玩家一起跳过 —— 后面的「跟随玩家」会把它带到玩家身边。

> 反过来，**剧情过场的 sub-stage 该摆就摆** —— 那种段落的位置是演出要求的，不是玩家选的。
> 判断标准：这一刻玩家是在自由行动，还是在被编排。

### 范本：BPLM_L2StreetBarFight

```
Event On Stage Begin
  → Set Stage                       ← 只剩这个

Event Hard Initiate
  → Switch on Name (Get Stage)
      Stage_BarStreet_L2 → Set L2_CurrentStage → L2_Activate Level Manager
      Default            → Print String（警告）
```

图里有两个橙色注释框标着这次改动。照抄到别的 BPLM 时，下面四条都适用：

| # | 要点 | 为什么 |
|---|---|---|
| 1 | **不要自己调 `Run Hard Initiate`** | 基类已经会调。手动调会绕过 C++ 里的一次性守卫，变成跑两次（L2 原来是三次，而 Activate 里面有 Enter Combat） |
| 2 | **Switch 读 BPLM 自己缓存的 stage 变量** | 不要用 `Get Active Stage`。它读 GameInstance 的 stage，而那个在 PIE 直接进图时是 None |
| 3 | **状态变量的赋值也搬进来** | `L2_CurrentStage` 门控着 Activate 内部五个分支。搬进来之后这条链自给自足，不靠上一局存了什么 |
| 4 | **Switch 的 Default 要接一个 Print String** | 不接的话 stage 对不上就什么都不发生、没有任何痕迹 —— 和它要修的那个 bug 是同一种失败方式 |

> **`On Stage Begin` 原样保留**，现有 BPLM 一个都不用改。两个都实现的话两个都会跑。

---

## 一点五、Sub-stage：一个 stage 里的存档点

> 一句话：**stage 说"第几章"，sub-stage 说"这一章读到哪"。**

玩家在一张图里走到一半存了档，读回来 hard initiate 会按**这个 stage 的默认样子**重新布置 ——
玩家被传回起点、开场对话重播。Sub-stage 就是告诉它"不，从这个点重建"。

### 三件事，就这些

| | 东西 | 做什么 |
|---|---|---|
| 1 | `Story Checkpoint("<名字>")` | 在玩家到达那个点时调。**记下名字 + 要一次自动存档** |
| 2 | `Get Sub Stage` | 在 hard initiate 里 switch 它 |
| 3 | 那个 switch 的 **Default** 针脚 | 其余一切（没设过 / 别的 stage 留下的 / 还没写 case 的）全都落回原来的流程 |

### 形状（**switch 必须嵌在 stage 的 case 里面**）

```
Event HardInitiate
  → 公共部分：灯光 / MPC / NPR
  → Switch on Name (Get Stage)                      ← 原来就有的，不动
       ├ L5ClinicFirstVisit
       │    → Switch on Name (Get Sub Stage)        ← 新加，只加在需要的 stage 下面
       │         ├ "L5.Corridor" → 走廊的摆场
       │         └ Default ─────→ 原来 L5ClinicFirstVisit 那条链
       └ Default → Print String（stage 对不上的警告）
```

**嵌进去，不要和 stage switch 并列。** 这一条是整个设计能成立的原因：

- 同一张图在流程里出现两次（`BlackMarketStreet` 的 BeforeAdult / AfterAdult 就是），
  前一次留下的 sub-stage 会落到后一个 stage 的 switch 里，**找不到 case，自动走 Default**。
- 所以**不需要任何"换 stage 时清掉它"的代码**。并列着放就必须手动清，而手动清一定会被忘。

没有 sub-stage 的 stage 一个节点都不用加，行为和现在完全一样。

### 名字就是存档的 beat

`Story Checkpoint` 把名字同时当成两样东西：**还原点**和**自动存档的去重键**。
所以：

- **两个检查点不能同名** —— 同名的第二次会被存档系统当成"同一个 beat"直接拒绝
- 名字**带上 stage 前缀**（`L5.Corridor`、`L4.AfterAdult.Cyan`），图内唯一，不可能撞
- 想"只存档、不改还原点"（比如非线性的三个互动点共用一套摆场）：
  **给它们三个不同的名字，然后把三根 switch 针脚接到同一条链上**

### 和存档的关系

它是 `FVTGLevelProgressRecord::SubStage`，**C++ 里的一个独立字段**，不是蓝图变量。
`RestoreLevelProgress` 在 `BeginPlay` 开头就把它放好 —— 远早于 hard initiate 读它。

为什么不做成蓝图变量：BPLM 自己身上的变量才会被进度快照自动收，基类上的会被**静默丢掉**；
放 `BPLM_C` 上又要靠一个"SaveGame"勾选框，而那个勾选框在图里看不见、取消了也不报错。

**老存档没有这个字段 → 读出来是 None → 就是这个 stage 的默认流程。** 不需要升版本、不需要迁移。

### `Do Once` 的状态不进存档（2026-11-23 实测）

`-run=VTGBPLMInitiate -Captured` 会 spawn 真实的 BPLM、调真实的 `CaptureLevelProgress`，
把存档里真正会有的键打出来。14 个 BPLM 跑下来结论一致：

> **存进去的只有你自己声明的变量，外加 `BPLM_C` 的 `GeneralLevelPhase`。**

`BPLM_BlackMarket` 有 4 个 `Do Once` 节点，37 个变量，捕获到 17 个 —— **一个 DoOnce 的隐藏
状态都没有**。所以：

- **读档后每个 `Do Once` 都是重新打开的**，它挡的那件事会再做一次
- 要让"只做一次"跨存档生效，**必须用显式的 bool 变量**，不能靠 `Do Once`

同一张图里两种混用是最危险的：显式 bool 会被记住、`Do Once` 不会，读档回来一半的门开着
一半关着。要么都用 bool，要么在 sub-stage 的还原链里把那组 bool 一起重置回初始值。

### 整段不让手动存档：`Set Manual Save Blocked`

有些段落没法从任意一刻重建 —— 典型是黑市小巷，它 `Destroy Actor` 很多，而**被销毁的 actor 会进
存档、读档时再销毁一次，hard initiate 还原不回来**。与其把每个销毁都改成可逆，不如不让玩家在
那段里存档。

```
段落开始 → Set Manual Save Blocked (True)
段落结束 → Set Manual Save Blocked (False)
```

| | |
|---|---|
| 挡住的 | 终端的手动存档、剧情自动存档（它会等，超时后放弃） |
| **不挡的** | **战斗检查点** —— `SaveEncounterCheckpoint` 走 `SaveCheckpoint → AutoSave`，**从不问 `CanSaveNow`**。所以段落里打架打到一半死了，照样能在检查点复活 |
| 玩家看到 | 终端整页换成 "Not available during this sequence."，样式和"战斗中""过场中"一致 |

**它是运行时状态，进不了存档**（C++ 字段，进度快照只收你声明的蓝图变量），所以不可能出现
"读档回来终端永远是死的"。每次进图都是开着的。

**反过来那一面要自己接**：如果某个 sub-stage 还原时把玩家放回那段**里面**，那条还原链也要
再 `Set Manual Save Blocked (True)` 一次 —— 新实例是开着的，不会自动记得。

日志过滤 `VTG LEVEL` 能看到每次开关；运行时也能在 BPLM 的 Details 里看 `b Manual Save Blocked`。

### 怎么确认它在工作

日志过滤 `VTG LEVEL`，两行里都有 `subStage=`：

```
VTG LEVEL BPLM_Clinic BeginPlay: activeStage=L5ClinicFirstVisit subStage=L5.Corridor ...
VTG LEVEL BPLM_Clinic hard initiate: ... subStage=L5.Corridor -> called
VTG LEVEL BPLM_Clinic story checkpoint: subStage=L5.Corridor stage=... -> autosave scheduled
```

`Story Checkpoint` 返回 false 不代表没记下来 —— sub-stage **一定**被记下，
false 只是说这次没安排自动存档（名字空、同一个 beat、读档中、或者编辑器里
`VTG.AutoSaveInDevRuns` 是 0）。原因由存档协调器自己打在日志里。

运行时想看它现在是什么：选中关卡里的 BPLM，Details 面板 Stage 分类下的 `Sub Stage`（只读）。

### 回归

和 hard initiate 同一条命令：`-run=VTGAlleyRepair -HardInitiate`。
覆盖：默认是 None、没有协调器也照样记下、空名字被拒且不擦掉已有值、进得了进度快照、
读档后在 hard initiate 跑之前就位、老存档读出 None。

---

## 二、维护

### 工具

| 命令 | 做什么 |
|---|---|
| `-run=VTGAlleyRepair -HardInitiate` | 无头回归，验证基类契约。不碰磁盘存档 |
| `-run=VTGL2HardInitiate -Apply` | 把 L2 那张图改成范本形状；不带 `-Apply` 即只验证 |
| `-run=VTGBPLMInitiate` | 一行一个 BPLM 报现状（有没有事件 / switch 读什么 / Default 接没接） |
| `-run=VTGBPLMInitiate -Dump` | 把每个 BPLM 的整张 event graph 按 exec 链打出来，只读 |
| `-run=VTGBPLMInitiate -Apply` | 把还没转的 BPLM 转成范本形状；已经转过的只补 Default 警告 |

`-Only=<名字片段>` 可以只跑一个，`-Root=<Content 下的目录>` 换扫描目录。
改前备份在 `Scripts/L2HardInitiate/Backup/`（L2）和 `Scripts/BPLMHardInitiate/Backup/`（其余）。

### `In_Level` 下全部转完了（2026-11-22）

14 个 BPLM 现在都是范本形状，`-run=VTGBPLMInitiate` 一次看完。转换做的四件事：

1. `On Stage Begin` 只剩 `Set <stage 变量>`，后面整条链搬到新的 `HardInitiate` 自定义事件
2. 没有 stage 变量的（Clinic / L2TunnelCine / Tunnel_NearMarket / ShoppingCenter_ClinicYard /
   Tunnel_MarketToFactory）**新建了一个 `Stage` (Name) 变量**，并补上 `Set Stage`。
   L4 那三个本来就叫 `CurrentStage`，沿用原名
3. Switch 的 Selection 从「事件的 Stage 引脚」改成「读 BPLM 自己的 stage 变量」——
   检查点恢复根本不走 `On Stage Begin`，那个引脚永远不会送到值
4. Switch 补了 Default → Print String

> **`BPLM_C` 的 Class Defaults 里 `Hard Initiate Event` 填的是 `HardInitiate`，所有子类都继承。**
> 所以在这之前，没做这个事件的 BPLM 每次进图都在日志里报一条 Error。新建 BPLM 子类时，
> 要么做一个叫 `HardInitiate` 的无参事件，要么把自己那份字段清空。

L4 的 `BPLM_AdultStore` 和 `BPLM_L4BlackMarket` 是**重定向器**（2 KB 左右），
指向 `AdultStore/BPLM_AdultStore` 和 `BPLM_BlackMarket`，工具会跳过，别照着它们改。

### 回归覆盖了什么

- 正常进图：恰好触发一次，且**不在 BeginPlay 当场**触发
- 多跑 8 帧仍然只有一次
- **检查点恢复**（`EncounterResumeEvent` 已设 → `OnStageBegin` 被跳过）：照样触发，`DidEnterFromLoad` 为 true
- 事件名留空：干净地什么都不做
- 名字拼错 / 事件带参数：拒绝执行并打 Error，不会假装成功
- 手动 `RunHardInitiate` 能跑第二次，且不会让基类重复触发

**没覆盖**：和真实存档恢复的 `OnSlotLoaded` 之间的先后顺序（需要活的 save coordinator，无头世界里
`GI->GetSubsystem` 返回 null），以及「触发之后布置出来的场景对不对」—— 那只能进游戏看。

### 改 BPLM 时的检查项

- 事件改名之后记得同步 `Hard Initiate Event` 字段。忘了会在日志里看到：

  ```
  <BPLM>: hard initiate event "xxx" must exist on this Blueprint and take no parameters
          - the level will not be set up
  ```

  按名字反射失败本来是静默的，这条 Error 是专门加的。
- 日志里还有两行诊断，过滤 `VTG LEVEL` 就能看到：BeginPlay 时报 `activeStage`、`OnStageBegin`
  有没有被跳过、事件名；触发时报有没有真的调到。

### 关于 stage（容易踩）

| 进图方式 | `GI->CurrentStage` |
|---|---|
| 正常流程（`Open Level with Stage`） | 有值 |
| PIE 直接进图 | **None** |

L2 的真实入口是酒吧的对话资产 `L2_Bar_NPC`，里面是
`Open Level with Stage(L2_StreetOutsideBar, Stage_BarStreet_L2)`。

PIE 直接进图想要 stage，用 BPLM 上的 `bOverrideStageForTesting` + `TestStage`。
它**只在 PIE、且没人显式传过 stage 时**生效，不可能影响正常流程或打包版本。

> **PIE 里存的档不要当数据用。** `TestStage` 不写 `GI->CurrentStage`，所以 `Meta.Stage` 存下来还是
> None；而且读档会无条件把 `bHasExplicitStage` 设成 true，从那一刻起 `TestStage` 覆盖就失效了。
> **验证存读请走正常流程。**

### 写无头测试时踩过的两个坑（框架问题，不是产品代码）

- `AActor::ProcessEvent` 在 `World->AreActorsInitialized()` 为 false 时**静默不执行**。
  要先 `InitializeActorsForPlay`，否则事件报告「called」但函数体根本没跑。
- `FTimerManager::Tick` 同一帧内第二次调用直接返回，而 commandlet 不会自己推进 `GFrameCounter`。
  不手动加的话，「只触发一次」那条断言是**空的**。

---

## 三、原理

### 为什么 OnStageBegin 不能放初始化

**原因一：检查点读档时它整段不跑。**

```cpp
// AVTGLevelManagerBase::BeginPlay
if (!bEncounterResumePending)
{
    OnStageBegin(Stage);   // ← 恢复战斗检查点时整段跳过
    ...
}
```

`bEncounterResumePending` 来自存档记录里的 `EncounterResumeEvent`，而那个名字是 BPLM 自己调
`SaveEncounterCheckpoint(Id, ResumeEvent, ...)` 时写进去的。所以**任何设过战斗检查点的 BPLM**，
在那之后存的档读回来时，挂在 `OnStageBegin` 下面的整条链（灯光、MPC、NPC 摆位）一起不跑。

**原因二：它跑在存档恢复之前。**

`OnStageBegin` 在 BeginPlay 里，而 `ApplyWorldState` 在 `HandlePostLoadMap` 的 ticker 里，晚一拍。
恢复过程中会对每个实现 `VTGSaveable` 且有记录的 actor 做 `SetActorTransform` ——
BPLM 刚摆好的 NPC 位置会被盖掉。

### 触发时机

| 进图方式 | OnStageBegin | Hard Initiate |
|---|---|---|
| 正常进图 | 跑 | BeginPlay 之后下一拍 |
| 普通读档 | 跑 | `ApplyWorldState` 完成之后 |
| 战斗检查点恢复 | **不跑** | `ApplyWorldState` 之后、`ResumeSavedEncounter()` **之前** |

最后一格的顺序是有意的：恢复战斗是在布置好的场景**之上**重建，而不是取代它。

### 实现

- BeginPlay 里，若 `Save->HasPendingWorldRestore()` 或 `bEncounterResumePending`，绑 `OnSlotLoaded`；
  `HandleEncounterSlotLoaded` 先 `FireLevelReady(true)` 再 `ResumeSavedEncounter()`。
- 同时**无条件**挂一个 next-tick 的 `PollLevelReadyAfterRestore`：还在恢复中就继续等下一拍，
  恢复结束（或本来就没有恢复）就触发。这既是正常进图的触发点，也是「恢复中途被取消、广播没发出来」
  时的兜底 —— 否则整个关卡的初始化会静默丢失。
- `FireLevelReady` 用 `bLevelReadyFired` 保证只跑一次，两条路径谁先到算谁，然后记下
  `bEnteredFromLoad` 并调 `RunHardInitiate()`。
- `RunHardInitiate` 按名字 `FindFunction`，要求无参数，找不到就打 Error。

### 代价

正常进图也等一拍，是为了让蓝图两种进法拿到的帧位置一致。代价是有一帧 NPC 停在关卡摆放的原位上；
读档那边有加载黑屏盖住，正常进图是一帧。**必须在第 0 帧就正确的东西，留在 `On Stage Begin`。**
