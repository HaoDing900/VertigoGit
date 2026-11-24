# 自由存档（随时存／随时读）可行性检阅

核对日期：2026-11-19 · 范围：`Content/ThirdPerson/Maps` 下所有地图 + 其中放置的 BPLM 实例 + 现有 `VTGSaveCoordinator`

你给的那份《战斗临时存档：使用与维护》并没有过时，它描述的机制在代码里都还在。
但它只覆盖"一场战斗的重试"。要做"自由活动时随时存／读"，缺的不是修补，而是三块结构性的东西。

---

## 1. 地图 / BPLM 实例清单

所有 BPLM 资产都在 `Content/LevelNSequence/In_Level/` 下，全部继承 `BPLM`（`/Game/LevelNSequence/BPLM`），
而 `BPLM` 继承 C++ 的 `AVTGLevelManagerBase`。**没有一张地图用关卡流式子关卡**，每张地图就一个 BPLM 实例。

| 地图 | BPLM 类 | 备注 |
|---|---|---|
| `SaApartment/SaApartment` | `BPLM_SaApartment_C` | |
| `SaApartment/Sa_AptDownAlley` | `BPLM_AptAlley_C` | |
| `L_SewerUnderApartment` | `BPLM_SewerUnderApt_C` | 已接战斗检查点 |
| `L2Bar` | `BPLM_BarBack_C` | |
| `Street/L2_StreetOutsideBar` | `BPLM_L2StreetBarFight_C` | ⚠ 实例名是 `BPLM_L2StreetBarFight1`（带 1） |
| `Street/BlackMarketEntrance` | `BPLM_BlackMarketEntrance_C` | 唯一用了 `EnterSavedStoryEvent` 的 |
| `Street/BlackMarketStreet` | `BPLM_BlackMarket_C` | ⚠ 地图里还残留旧名 `BPLM_L4BlackMarket` 字符串 |
| `Street/AdultStore_BlackMarketStreet` | `BPLM_AdultStore_C` | 指向 `AdultStore/BPLM_AdultStore`；父目录那个 2.6KB 的是重定向器 |
| `Street/Clinic/Clinic` | `BPLM_Clinic_C` | |
| `Street/Clinic/ShoppingCenter_Yard` | `BPLM_ShoppingCenter_ClinicYard_C` | |
| `PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream` | `BPLM_PoliceStation_C` | ⚠ 地图里还残留旧名 `BPLM_L3_PoliceStation_C` |
| `Tunnel/L2TunnelCine` | `BPLM_L2TunnelCine_C` | |
| `Tunnel/Tunnel_NearBlackMarket` | `BPLM_Tunnel_NearMarket_C` | |
| `Tunnel/Tunnel_ClinicToFactory` | `BPLM_Tunnel_MarketToFactory_C` | |

**没有 BPLM 的地图**：`Tunnel/Cine_Tunnel_SouthEnd`、`Tunnel/Tunnel_SouthEnd1`、`MainMenu/M_MainMenu`、
`BaseLevel`、`CQB1`、`L0PastBunker`、`L_CombatTest`、`Level0SubmarineInterior`、`FilmBooth_*`、`Sketches/*`、`DefaultMapEmpty`。
在没有 BPLM 的地图里存档不会报错，但读回来没有任何关卡侧状态可恢复。

**重定向器（空壳 .umap，不是真地图）**：`Street/Clinic/Clinic_Yard` → `ShoppingCenter_Yard`、
`Tunnel/Tunnel_SouthEnd` → `Cine_Tunnel_SouthEnd`、`Tunnel/L3TunnelCine` → `L2TunnelCine`。

> 为什么在意实例名：`FVTGLevelProgressRecord` 是按 **BPLM Actor 的 FName** 存的，还要核对 **类的完整路径**。
> 上面三个 ⚠ 说明这些地图经历过改名。改名／删掉重放 BPLM 会让旧存档的关卡进度静默失效（不报错，只是不恢复）。

---

## 2. 四条需求逐条对账

### ① Narrative 已选择的对话选项 — 已经可以，但是副作用式的

Narrative 的 `UDialogue` 在每个节点播完时会自动调
`CompleteNarrativeDataTask("PlayDialogueNode", <NodeID>)`（`Dialogue.cpp:1469` NPC 节点、`:1514` 玩家节点）。
这些进 `MasterTaskList`，而 `UNarrativeSaveGame` 会把 `MasterTaskList` 整个存盘，
Coordinator 又把它写到 `VTG_Narrative_<槽位>`。所以"哪些节点／哪个选项走过"是存下来的。
项目里 `Cond_PlayedNode` 和 `WBP_NarrativeButton_DialogueOption_ProfilePicture` 已经在读它。

要注意的：
- NodeID 是对话资产里自动生成的 ID。**重排／重建对话节点会换 ID**，旧存档的"走过"记录就对不上了。
- Narrative 存的是**任务 + 任务列表**，不存"当前正在进行的一段对话"。在对话中途存档，读回来对话不会续接。
- 存的是"播过"，不是"选了哪个分支后世界变成什么样"。世界侧还得靠 ②③。

### ② BPLM 各关卡的变量状态 — 机制有，但只存当前这一张地图

`CaptureLevelProgress` 会自动抓 BPLM 上所有受支持的 Blueprint 值变量（bool／数值／enum／name／string／text、
纯值结构、以及它们的 Array／Set／Map），不需要逐个勾 SaveGame。这部分很好。

问题在 `GatherWorldState`：每次存档都新建一个 `UVTGSaveGame`，`LevelProgress` 只装
**当前世界里的 BPLM**。同理 `InteractionEnabled`、`DestroyedActors` 也只有当前地图的。
`RestoreLevelManager` 还要求 `当前地图名 == Meta.LevelName` 才恢复。

结果：
- 在 Clinic 存档 → 这份存档里**没有** SaApartment／BlackMarket 的 BPLM 进度。读回来再走回公寓，公寓从零开始。
- 更要命的是**正常走关也一样**：`DestroyedActorIds` 在每次 world init 时 `Reset()`；玩家进度组件
  （`Counters`／`Flags`）挂在 Pawn 上、随地图重建。普通 Open Level 之后这些全是默认值。
- 目前真正跨地图活着的只有：GameInstance 的 `CurrentStage`、Coordinator 的
  `PersistentInts`／`PersistentNames`、以及 Narrative 的任务和 `MasterTaskList`。

### ③ 哪些事件触发过、到什么分支状态 — 基本等于没有

- `EnterSavedStoryEvent` / `StartedEvents` 这套"已进入事件"台账，**14 个关卡 BPLM 里只有 `BPLM_BlackMarketEntrance` 用了**。
- `DoOnce` 的内部状态不存。读档后所有 DoOnce 重新可用，所以靠 DoOnce 表达"这段演过了"的地方，读档会重演。
- 正在跑的 Sequence、Delay、Timer、活动对话都不存。
- 结论：现在"事件到什么分支"只在 BPLM 自己的 bool／enum 变量里有，而那些变量又受 ② 的单地图限制。

### ④ 当天选了 Leashed 还是 Raw — 已经通了（2026-11-20）

当初以为要补两处，查完发现**第二处本来就接好了**，只缺存档那一半。

完整链路（已验证）：

```
BPLM 父类 (BPLM_C) 的 Event BeginPlay
  → Detect LeashedOrRaw?            (K2Node_CustomEvent_18)
      Pawn.IsMedicated? = GameInstance.Leashed?
```

另外两个父类事件负责写入：
`Set Leashed` (`K2Node_CustomEvent_15`) 和 `Set Raw` (`K2Node_CustomEvent_17`)
都是**先写 GameInstance 的 `Leashed?`，再写 Pawn 的 `IsMedicated?`**——
也就是说 GameInstance 一直就是写入侧的唯一来源，Pawn 只是它的镜像。

所以加上 GI 快照之后，顺序是：

1. 读档：`RestoreGameInstanceValues()` 在**开图之前**把 `Leashed?` 写回 GameInstance
2. 新地图加载，BPLM 的 BeginPlay 跑 `Detect LeashedOrRaw?`，把它同步到 Pawn 的 `IsMedicated?`
3. `Cond_Sa_Leashed` 读 Pawn 的 `IsMedicated?` —— 拿到的是读档后的值

**需求 ④ 到这里闭环了**，不需要再改蓝图。

已经把"BPLM 的 BeginPlay 必须能走到 `Detect LeashedOrRaw?`"写成断言放进
`-run=VTGAlleyRepair -GameInstanceSnapshot` 了——这一环是内容不是代码，
谁在图里把它断开，回归会立刻报，而不是等到读档后对话走错分支才发现。

**还剩两个要在编辑器里眼确认的：**

- `Detect LeashedOrRaw?` 里写 Pawn 用的是 BPLM 的 `Player Pawn` 缓存变量
  （`K2Node_VariableGet_4`）。如果这个缓存是 BeginPlay 里更晚才填的，
  这次 Set 可能打在空指针上、静默失败。顺序要看一眼。
- 扫了 14 个 BPLM 的图，只找到 `BPLM_BlackMarket` 调 `Set Leashed`，**没找到任何地方调 `Set Raw`**。
  但我的扫描只看蓝图节点，Narrative 的对话事件 / 按名字调用都看不到，
  所以更可能是从对话里驱动的，不是真的没人调。要确认"选 Raw"那条路确实会写 GameInstance。
---

## 3. 现在实际存下来的东西（对照用）

| 内容 | 存？ | 来源 |
|---|---|---|
| 地图名 / Stage | ✅ | `Meta.LevelName` / `Meta.Stage` |
| `PersistentInts` / `PersistentNames` | ✅ 跨地图累积 | 读档时在开图**之前**恢复，BeginPlay 可读 |
| Narrative 任务 + `MasterTaskList`（含对话节点走过记录） | ✅ | 独立文件 `VTG_Narrative_N` |
| 玩家 ISX 背包 | ✅ | `UInventoryComponent::GetSaveData`，不需要蓝图接线 |
| 玩家 Transform | ✅ | 手动存档用当前位置；检查点用传入的重生点 |
| 玩家 BP `Health` | ⚠ 只有手动槽（1–9） | 自动存档槽 0 故意不存，重试满血 |
| `UVTGPlayerProgressComponent`（Health／MaxHealth／Counters／Flags） | ⚠ 只在读档时恢复 | 普通过图不保留 |
| 勾了 SaveGame 的 `VTGSaveable` Actor 变量 | ⚠ 只当前地图 | |
| 被销毁的关卡内 Saveable Actor | ⚠ 只当前地图 | |
| 故事交互 `CanInteract?`（`BP_0ISXInteractiveCollision` 子类） | ⚠ 只当前地图 | 按 Actor FName |
| 玩家邮件字段（5 个硬编码名） | ⚠ 只当前 Pawn | `MailFields` |
| BPLM 值变量 + `StartedEvents` + 战斗恢复入口 | ⚠ 只当前地图 | |
| **GameInstance 的 BP 变量（`Leashed?` 等）** | ❌ | |
| 世界里的敌人状态／临时生成物／正在播的 Sequence／活动对话／DoOnce | ❌ | 按设计不存 |
| `OnSaveSubsystems` / `OnLoadSubsystems` | 没人绑 | 背包已由 C++ 直存，这两个委托目前空置 |

存档入口现在只有两个：终端的 SAVE/LOAD 页（槽 1–9，`SaveTerminalSlot`）和自动存档槽 0。
**自由活动时没有存档入口**（没有暂停菜单存档）。

---

## 4. 做"随时存／随时读"要补的东西（按优先级）

### P0 — 跨地图 BPLM 状态：审计做完了，不建台账（2026-11-19 修订）

初版这里写的是"建一个跨地图进度台账"。重新看过之后，**这个结论下得太早了**，
但不是因为游戏线性所以不需要，而是因为问题的性质被我判断错了。三个事实：

**(1) 不要依赖"Stage 决定进入地图时的状态"这个说法。**
`CurrentStage` 是 GameInstance 上**一个全局 FName**，不是每张地图一份。
而且 14 个关卡 BPLM 里只有 4 个引用了任何 `Stage_*` 名字：

| BPLM | 引用的 stage |
|---|---|
| `BPLM_BarBack` | `Stage_Bar_Intro` |
| `BPLM_L2StreetBarFight` | `Stage_BarStreet_L2` |
| `BPLM_BlackMarket` | `Stage_BeforeAdult`、`Stage_AfterAdult` |
| `BPLM_AdultStore` / `BPLM_BlackMarketEntrance` | `Stage_AfterAdult` / `Stage_BeforeAdult` |

其余的（SaApartment、AptAlley、SewerUnderApt、Clinic、PoliceStation、三条隧道、ShoppingCenter_Yard）
目前**不按 stage 分支**，每张图只有一条无条件流程。按作者的说明，这是**内容还没做到**，
不是设计上不打算做：这些图现在算一本道，以后会给它们加 stage。

那么这条事实该怎么用：它不说明 stage 这套机制有问题，它说明**现在还不能拿"stage 已经决定了
进入状态"当作已经成立的前提来推论存档**——这个前提在 4/14 张图上成立，在另外 10 张图上
是"以后会成立"。更要紧的是方向：以后 stage 加得越多，同一张图在不同 stage 下被重进的次数就越多，
**往返也就越多**，下面 (2) 那条就越吃紧，而不是越宽松。

另外 `OpenLevelWithStage` 的 Stage 引脚留空时**不改变 stage**，所以"同一个 stage 内重进同一张图"
在结构上本来就允许。

**(2) 走图关系已经是双向的了，不是单向链。** 从各 BPLM 实际会打开的地图看：

- `BPLM_SaApartment` → `Sa_AptDownAlley`，`BPLM_AptAlley` → `SaApartment` —— **往返**
- `BPLM_BlackMarket` → `AdultStore_BlackMarketStreet`，`BPLM_AdultStore` → `BlackMarketStreet` —— **往返**
- `BPLM_Clinic` → `Tunnel_ClinicToFactory`，`BPLM_Tunnel_MarketToFactory` → `Clinic` —— **往返**
- `BPLM_SewerUnderApt` → `SaApartment` / `Sa_AptDownAlley`，`BPLM_Tunnel_NearMarket` → `BlackMarketEntrance` / `Clinic` / `ShoppingCenter_Yard`

**(3) 14 个 BPLM 里只有 `BPLM_L2StreetBarFight` 把状态存在了自己实例之外**
（用 `SetPersistentInt` / `GetPersistentInt`）。其他全部只有自己的实例变量。

(2) + (3) 合起来的意思是：**"地图内 BPLM 状态在往返后丢失"不是存档的问题，是现在正常游玩就已经存在的行为。**
公寓 → 小巷 → 公寓，`BPLM_SaApartment` 是个新实例，所有变量回默认值，
被捡走的东西回来了（`DestroyedActorIds` 每次 world init 都 `Reset()`），
`CanInteract?` 也回到资产默认。之所以没炸，大概是实测路径事实上还是线性的。

所以真正该做的第一件事不是改架构，是**审计**：

> 逐张地图列出"玩家在这张图里做完、离开再回来应该还是做完"的事实。
> 捡走的物品、开过的门、用过一次的互动、聊过的 NPC、看过的 Note。

然后按这张表的长度决定：

- **表很短（每张图 0–2 条，总共十几条以内）** → 现有设计不用动。
  把这些事实搬到**已经能跨图活着的两个地方**：GameInstance 的变量，或 `PersistentInts` / `PersistentNames`。
  这条路**完全不需要改 C++**，照 `BPLM_L2StreetBarFight` 现成的写法抄就行。
- **表很长（几十条）** → 手工维护几十个 persistent key 比自动化更糟，这时候才值得建上面那个台账。

**审计结果（2026-11-19，从资产二进制里读出的候选清单，语义待在编辑器里确认）：**

| 地图 / BPLM | 看起来必须活过往返的变量 | 性质 |
|---|---|---|
| `BPLM_SaApartment` | `IsDay?`、`IsDim?`、`IsSaNaked?`、`IsMedicated?`、`HasNewMail`、`CanLeaveRoom?` | 当天状态，**这张图是唯一有真正"每日状态"的** |
| `BPLM_SewerUnderApt` | `Progress`（注释写的是打死的怪蛋数量） | 计数 |
| `BPLM_BlackMarket` | `CanGoToNextMap`、`ReadyToLeaveThe Alley` | 一次性闸门 |
| `BPLM_PoliceStation` | `First time Open` | 一次性 |
| `BPLM_BlackMarketEntrance` | `BkgDialogue Finished` | 一次性 |
| `BPLM_Clinic` | `FirstAfterExploreConv` | 一次性 |
| `BPLM_BarBack` | `CanInteract?` | 一次性 |
| `BPLM_L2StreetBarFight` | `LevelPhaseForL2StreetFight` | **已经在 `PersistentInts` 里，不用动** |
| `BPLM_AptAlley`、`BPLM_L2TunnelCine`、`BPLM_Tunnel_NearMarket`、`BPLM_Tunnel_MarketToFactory`、`BPLM_ShoppingCenter_ClinicYard`、`BPLM_AdultStore` | 没有 | 纯过场／纯跳图 |

**结论：短表。** 8 张图、12 个事实左右，其中 1 个已经解决。而且除了公寓那一组，
剩下的全是"第一次怎样、以后怎样"的一次性布尔——**这正是 stage 以后会接管的那一类**。
所以不建台账是对的，而且理由比"现在线性"更强：这些事实本来就该归 stage 管。

公寓那一组是例外，它是真正的**当天状态**（白天/黑夜、吃没吃药、穿没穿衣服、有没有新邮件），
不是剧情进度，stage 接管不了，而且它和需求 ④（Leashed／Raw）是同一件事。
所以**公寓这组应该跟着 GameInstance 快照一起做**（见下面的 P0 — GameInstance 快照），
而不是靠 BPLM 的实例变量。其余 6 个一次性布尔，等对应的 stage 做出来时顺手归到 stage 里就行。

### P0 — 存档格式按地图分键（已做，2026-11-19）

即使不建台账，也先把存档格式改成按地图分键了——**只填当前这一张，行为一点没变**：

```
UVTGSaveGame::ProgressByMap : TMap<FString /*LevelName*/, FVTGMapProgress>
  └ FVTGMapProgress { TMap<FName, FVTGLevelProgressRecord> Managers;
                      TMap<FName, bool>                    InteractionEnabled;
                      TArray<FName>                        DestroyedActors; }
```

具体做法：

- `VTG_SAVE_VERSION` 从 2 升到 3；读到 v2 存档时把它扁平的 `LevelProgress` /
  `InteractionEnabled` / `DestroyedActors` 折进 `ProgressByMap[Meta.LevelName]`（旧存档不作废）。
- `UVTGSaveCoordinator::ProgressByMap` 是个 subsystem 成员：读档时从存档里 seed，
  每次存档时由 `AccumulateMapProgress()` 把当前这张图的记录写进去——
  所以走 A → B → 存档，**A 的记录不再被丢掉**，它留在文件里。
- 扁平的那三个字段**原样保留**，恢复路径和 7 个编辑器回归 commandlet 读的还是它们。
  当前这张图的数据在文件里重复一份，代价很小，换来的是不用改那些 commandlet。
- **没有任何地方读其他地图的条目**，所以读档行为与改动前逐字相同。
- 新建游戏／`ClearPersistentFlags()` 会把 `ProgressByMap` 一起清掉。

现在做的理由只有一个：**存档结构是以后改起来最贵的那部分**（要写迁移，并且让所有测试存档作废），
而行为那一半（重进地图时在 `OnWorldInitializedActors` 或 `AVTGLevelManagerBase::BeginPlay`
里把对应条目应用回去）任何时候加都不影响格式。现在数据已经在文件里躺着了，
以后真要做往返恢复，就只是加读取的那几行，不用再动格式、不用再写迁移。

已编译通过（`Build.bat VertigoEditor Win64 Development`），只有既有的 deprecation warning。

### 一条现在就该立的规矩（零成本，而且这才是"以后自由度更高"的真正答案）

> 一个事实如果必须活过换图，就放 GameInstance 变量或 `PersistentInts` / `PersistentNames`；
> BPLM 的变量只用来表达"这张图内部走到哪了"。

从现在起所有新内容都守这条，以后不管自由度怎么涨都很便宜。
反过来，如果跨图的事实继续散落在 14 个 BPLM 的实例变量里，等到要提高自由度时，
你会同时被迫做台账 + 存档迁移 + 回头改 14 张图。

另外顺带确认过：`UVTGPlayerProgressComponent` 的 `Counters` / `Flags`（本来就是设计给跨图用的）
目前没有任何地方在用。所以现在**没有**已经依赖跨图存储的东西，改规矩的成本是最低的时候。

### P0 — GameInstance 快照（已做，2026-11-20，需求 ④ 到此闭环）

**已实现的部分（C++，编译通过 + 回归测试通过）：**

```
UVTGSaveGame::GameInstanceValues / GameInstanceTypes : TMap<FName, FString>
UVTGSaveCoordinator::CaptureGameInstanceValues()   // SaveToSlot 里调用
UVTGSaveCoordinator::RestoreGameInstanceValues()   // LoadFromSlot 里、OpenLevel 之前调用
UVTGSaveCoordinator::ExcludedGameInstanceVariables // 默认排除 ISX 的 SaveData
```

- 过滤规则和 BPLM 进度快照**共用同一份定义**，提到了
  `UVTGSaveStatics::IsValueProperty` / `IsBlueprintValueProperty` /
  `CaptureValueProperties` / `ApplyValueProperties`。
  `AVTGLevelManagerBase` 那边的 `IsProgressValue` 现在只是转调它，所以两边不会再各自漂移。
  （BPLM 自己的 `BPLM_C` 父类排除规则保留在 BPLM 侧，没动。）
- 收的是 GameInstance 上**蓝图声明的**值变量：bool / 数字 / enum / name / string / text、
  这些类型的容器、以及全部成员都是值类型的结构。对象引用永远不收。
  native 的 `CurrentStage` 不在里面（它本来就在 `Meta.Stage`）。
- **恢复时机是开图之前**，和 `PersistentInts` 同一个位置。GameInstance 跨 travel 活着，
  所以这是唯一能保证下一张图的 BeginPlay 和对话条件读到的是读档后的值。
  放到换图之后（`PendingLoad` 应用的地方）就晚了一帧。
- 回归测试：`-run=VTGAlleyRepair -GameInstanceSnapshot`
  （`Source/VertigoEditor/Private/VTGGameInstanceSnapshot.inl`）。
  用**真的** `BP_VTG_GameInstance` 类、走**真的** SaveGame 序列化，验证
  `Leashed?` 在一个全新实例上被恢复、`SaveData` 和 `CurrentStage` 被排除、
  类型变了的变量被跳过而不是误读。

**需求 ④ 不用再做蓝图了。** 初版这里写的是"还要让 `BP_Player_Sa.IsMedicated?` 从 GameInstance 读"——
查完发现**这条链本来就存在**：BPLM 父类的 `Event BeginPlay` → `Detect LeashedOrRaw?` 已经在做这件事，
而 `Set Leashed` / `Set Raw` 也一直是先写 GameInstance 再写 Pawn。详见上面 §2④。
所以快照一补上，这条就闭环了。

**还剩一件在编辑器里做的事：**

**把公寓当天状态搬到 GameInstance。** 审计查出来的 `BPLM_SaApartment` 的
`IsDay?`、`IsDim?`、`IsSaNaked?`、`IsMedicated?`、`HasNewMail`、`CanLeaveRoom?`
——在 `BP_VTG_GameInstance` 上建同名变量，让 BPLM 读写 GameInstance 而不是自己的实例变量。
它们和 Leashed／Raw 是同一类事实（"今天是什么情况"），不是剧情进度，stage 接管不了，
而公寓又是全项目唯一会被反复进出的家。搬完这一组，审计表里最难的那一行就解决了，
而且**不需要再改 C++**——快照是按类反射收的，加了变量就自动进存档。

注意 `BPLM_SaApartment` 的 `IsMedicated?` 和 Pawn 上的 `IsMedicated?` 是两个不同的变量
（前者是 BPLM 的实例变量，后者在 `BP_Player_Sa` 上，`Cond_Sa_Leashed` 读的是后者）。
搬的时候别把两者合并，先确认 BPLM 那个到底是不是同一个意思。

### P0 — 自动存档：进入新 map 或换 stage 时存，自己一个槽（已做，2026-11-20）

**触发单位是「beat」= 当前 map + 当前 stage。两者任一变化就存，别的都不存。**

为什么不只看 stage：`CurrentStage` 只有两个地方会改——读档时，和 `OpenLevelWithStage`（也就是换图时），
蓝图里没人改它。所以 **stage 变化永远发生在换图时，「新 stage」是「新 map」的子集**。
而现在只有 4/14 张图设 stage，光看 stage 的话自动存档几乎不触发，等于没用。
同时保留 stage 这一半，是为了盖住"同一张图重开但 stage 推进了"这种情况。

Stage 为空也算一个合法的 beat（10 张没 stage 的图照样该存）。

**只记住最后一个 beat，不记访问历史。** 所以：
- 一张图里多个 BPLM 都报 stage begin → 只存一次
- 公寓 → 小巷 → 公寓 → **存 3 次**，每次覆盖上一次

第二条是刻意的：这个槽的含义是"**你现在在哪**"，不是"这一章的开头"。
对一张安全网来说，离你最近比离章节开头近更有用。如果以后想两个都要，那就再开一个槽。

先说清楚改之前是什么：**根本没有"剧情自动存档"。** `AutoSave()` 写槽位 0，而它唯一的调用者是
`SaveEncounterCheckpoint()`——所以槽位 0 的语义一直是"**战斗重试点**"，蓝图里只有
`BPLM_L2StreetBarFight` 碰过。

**评估过但没做：每段长对话结束后自动存。** 三个理由：
(1) 它会反复覆盖槽位 0，也就是覆盖战斗检查点——打到一半出去聊天，检查点就没了；
(2) "长对话"没有可靠判定，最后还是得在每个 Dialogue 资产上打标记，那就已经是手工存档点了；
(3) 对话刚结束是最不稳定的一帧——`OnDialogueFinished` 同帧可能起下一段对话
（见 [[narrative-dialogue-reentrancy]] 那个坑）、开 sequence、换图。

**为什么挂在 stage begin 这个时机上：** 它就是 `AVTGLevelManagerBase::BeginPlay()` 里那一行，
每进一张图必跑一次，而那一刻按定义是
"玩家刚进入一个确定的初始状态"——没有对话在跑、没有 sequence 在播，最适合存档。
以后给剩下 10 张图加 stage 时，stage 那一半的判定会跟着免费变细，不用另外维护一张存档点表。

现在是三层，互不覆盖：

| 层 | 槽位 | 何时 | 谁触发 |
|---|---|---|---|
| 战斗检查点 | 0 | 战斗分段 | `SaveEncounterCheckpoint()`，原有 |
| **剧情自动存档** | **10** | **进入新 map 或换 stage** | **`StoryAutoSave()`，新加** |
| 手动存档 | 1–9 | 玩家在终端 | `SaveTerminalSlot()`，原有 |

实现：

- `UVTGSaveCoordinator::StoryAutoSave(UWorld* World, FName Stage)`，由
  `AVTGLevelManagerBase::BeginPlay()` 在 `OnStageBegin(Stage)` 之后调用。
- `MaxSlots` 10 → 11，新增 `StoryAutoSaveSlot = 10`。加了
  `IsManualSlot(Slot)`（= 不是 0、不是 10、在范围内），`SaveTerminalSlot` 和
  `SaveFromTerminal` 都改成用它，所以**玩家永远存不到这两个保留槽**。
- **写入延后一个 tick。** stage begin 发生在 `BeginPlay` 里：那时读档的世界还没恢复完
  （`PendingLoad` 要等 `HandlePostLoadMap` 后一个 ticker tick 才应用），pawn 有时还不存在。
  延后之后会**重新检查一遍所有条件**，因为状态可能已经变了。
- 四种情况拒绝存：没有世界（算不出 beat）、`PendingLoad` 还在（读档中）、`bRetrying`（战斗重试中）、
  以及**同一个 beat 再来一次**——一张图里多个 BPLM 都会报 stage begin，
  或者 BeginPlay 又跑了一遍而 map 和 stage 都没变。
- 判重是靠 `PersistentNames` 里的 `VTG.StoryAutoSaveStage`（存的是 beat），**写档前先盖章**，
  所以这个档被读回来时不会立刻又把自己覆盖一遍。写失败会把章恢复原样。
- `ClearPersistentFlags()`（新游戏）会清掉它，所以下一轮第一个 stage 还会存。

页面上槽位 10 显示成 "STORY AUTOSAVE"、详情写 "Read-only, written on entering a new stage"（文案可改），
**可以读、不能存**（和槽位 0 一样）。主菜单 Continue 的 `GetLatestValidSlot()` 三层都认。

回归：`-run=VTGAlleyRepair -StoryAutoSave`。刻意**不碰磁盘**——写入是 ticker 干的，
测试从不 pump 它，所以不会动玩家真实的存档文件；断言的是"要不要写"这半边，
因为那才是会悄悄毁掉一轮游戏的部分。

### P2 — 顺手修掉的：story gate 形状检查太死（已做，2026-11-20）

`-run=VTGAlleyRepair -LevelProgress -VerifyOnly` 之前在
`VTGLevelProgressRepair.inl` 断言失败，查出来**不是存档的问题**：

- `BPLM_BlackMarketEntrance` 的 4 个 story gate，3 个还直接挂在事件的 `then` 上，
  `BlackMarket.Intro` 那个前面被插进了一个
  `L4Entrance Character Location at Start` 调用。
- 但 gate 本身**没有变成孤立节点**：它和它的 Branch 仍然由
  `K2Node_CustomEvent_0`（`Initialize L4BlackMarketEntrance`）驱动，照样会跑。
- 换句话说是**检查太死**——它要求 gate 必须是事件的直接后继，而存档真正需要的只是
  "这个事件上 gate 还会跑"。

改法：把那一行从比对单条连线改成沿 exec 链向前走（`VTGReachesStoryGate`）。
现在 `-LevelProgress -VerifyOnly` 通过。资产一个字节都没动。

### P1 — 事件台账铺开

`EnterSavedStoryEvent` 只有一个 BPLM 用。两个选择，建议后者：

- 把它铺到所有 BPLM 的故事入口；或者
- **放弃 DoOnce，改用显式的 enum／int 阶段变量**——BPLM 的值变量本来就会自动存，
  一个 `L4_AlleyPhase (enum)` 比一个 DoOnce 加一个 bool 更能表达"到了哪个分支状态"，
  而且读档后恢复场景时你需要的正是这个阶段值。

### P1 — 存档门禁 `CanSaveNow()`（已做，见 §7）

自由存档最容易坏在"在不该存的时候存了"。在 Coordinator 上加一个 BlueprintPure：
对话进行中（`NarrativeComponent->GetCurrentDialogue()` 非空）、有 Sequence 在播、
`HasPendingWorldRestore()`、`bRetrying`、战斗中、加载页开着、Note／终端等 UI 模式下 → 返回 false。
暂停菜单的 Save 按钮据此灰掉，并给一句"现在不能存档"的提示。

### P1 — 存档入口：终端页已通，主菜单 Continue 也走它了（2026-11-20）

**终端那条（`WB_TabSwitcher` 里的 SAVE / LOAD 页）本来就通，不用改。** 它只通过三个
coordinator 接口说话：

| 页面做的事 | 调的接口 |
|---|---|
| 列槽位 | `GetAllSlotMetas()` |
| SAVE GAME | `SaveTerminalSlot(Slot, Result)` → 内部就是 `SaveToSlot()` |
| LOAD GAME | `LoadFromSlot(Slot)` |

所以这两天加的东西——`AccumulateMapProgress()`、`CaptureGameInstanceValues()`、
`RestoreGameInstanceValues()`——全都在 `SaveToSlot` / `LoadFromSlot` **里面**，
页面自动吃到，一行都不用动。页面本身也不是蓝图：`UVTGTerminalSavePage::RebuildWidget()`
在 C++ 里自己搭的（槽位滚动列表 + 详情面板 + SAVE/LOAD/CANCEL/CLOSE），
挂进终端是靠 `UVTGTerminalSaveLibrary` 往 tab 数组里插一个合成 tab（`SaveTabIndex = 254`）。

顺带注意 `SaveTerminalSlot` 已经有一道**粗糙的存档门禁**：要求有 world、有 player pawn、
`PendingLoad` 为空、不在 `bRetrying`，而且槽位必须 1..MaxSlots-1（所以永远存不到自动存档槽 0）。
下面 P1 的 `CanSaveNow()` 是把这道门禁做细，不是从零开始。

**主菜单 Continue 改成走同一个页面（已做）。** 之前 `UVTGMainMenu::ContinueGame()` 直接
`ContinueLatestSave()`——闷头读最新那个槽，玩家没得选。现在：

- 给页面加了 `OpenForMainMenu(Menu)`：**只读档模式**。隐藏 SAVE 按钮（主菜单里没有 world 和 pawn，
  `SaveTerminalSlot` 本来就会拒绝，不如别让玩家点进一个必然的错误），状态行改成
  "Select a save to continue."，CLOSE 把屏幕交回菜单而不是去关一个不存在的终端。
- `ContinueGame()` 现在 `CreateWidget` 出这个页面、`AddToViewport(50)`、收起菜单、切 UI Only + 显示鼠标。
  读档成功时页面本来就会 `LoadFromSlot` + 切回 Game Only，主菜单模式下再额外禁用并收起菜单。
- 页面建不出来时**回退到原来的 `ContinueLatestSave()`**，不会把按钮变成死路。
- 没有改任何蓝图。`WBP_MainMenu` 继续用 `UVTGMainMenu` 作父类，Continue 按钮继续叫 `Button`。

回归：`-run=VTGAlleyRepair -TerminalSave -VerifyOnly` 里加了 `MainMenuPage()`，断言
`WBP_MainMenu` 仍然派生自 `UVTGMainMenu`、仍然有名为 `Button` 的按钮（绑定是按名字找的，
改名了编译不会报错）、页面以只读档模式打开且 SAVE 被隐藏、CLOSE 把菜单还回来。

**还没做：** 暂停菜单（`UVTGPauseMenu`）还没挂这个页面。做法和主菜单那条一样，
但暂停菜单在游戏里，所以应该用 `OpenForTerminal`／带 SAVE 的完整模式，
而且要先有 P1 的 `CanSaveNow()` 把"战斗中、对话中、过场中不许存"挡住。

### P2 — 读档后"值恢复了但世界没变"

这是现有机制的根本限制，文档里也写了：恢复变量不会重放副作用。
随时存档会把这个问题放大到每一扇门、每一个触发器、每一个 NPC 位置。
建议每个 BPLM 都有一个**幂等的 `VTG_RebuildScene` 事件**：只读阶段变量，把门／碰撞／触发器／NPC／相机
设到该阶段应有的样子，不播任何剧情。正常流程和读档恢复都调它。
（战斗那两个恢复入口其实就是这个模式的特例。）

### P2 — 改名脆弱性（已修，见 §7 中等 4）

`LevelProgress` 按 Actor FName + 类完整路径匹配。给 `AVTGLevelManagerBase` 加一个
`UPROPERTY(EditAnywhere) FName LevelManagerSaveId`，按它匹配（留 FName 作为回退），
并把 `ClassPath` 的严格相等放宽成类名比较或加迁移表。
在此之前：**不要重命名 BPLM 资产、不要删掉重放地图里的 BPLM 实例。**
顺手把 `BPLM_L4BlackMarket`、`L4BlackMarket/BPLM_AdultStore` 这两个重定向器修掉
（Fix Up Redirectors），但要在修完后重新建一份存档验证。

### P3 — 小事

`GetLatestValidSlot` 为了比时间戳把每个槽的完整 payload 都加载了一遍，manifest 里本来就有
`SaveTimeUtc`，可以只读 manifest。

---

## 5. 需要在编辑器里确认的（从资产二进制里读不到）

1. ~~`BP_Player_Sa.IsMedicated?` 到底有没有勾 SaveGame~~ —— **已经不重要了**：GameInstance 的
   `Leashed?` 是唯一来源，BPLM 的 BeginPlay 每次都会重新同步到 Pawn，所以 Pawn 上那个勾不勾都一样。
2. 各地图里 BPLM 实例在 Outliner 里的**实际名字**（存档的 key 就是它）。
   `L2_StreetOutsideBar` 看起来是 `BPLM_L2StreetBarFight1`；`BlackMarketStreet` 和
   `L3_CentralPoliceStation...` 两张图里都残留着旧类名字符串。
3. `BP_VTG_GameInstance` 的完整变量表（字符串里只看到 `Leashed?`、`CurrentMainQuest`、
   `FreeTime_TravelCount`、`SaveData`）。
4. 每个 BPLM 哪些变量其实是运行期缓存，应该进 `ExcludedProgressVariables`。
5. 上面审计表里那 12 个候选变量的**真实语义**（名字是从资产名表里读出来的，没打开过图）：
   哪些真的是「做完就该一直是做完」，哪些只是运行期缓存。

---

## 6. 相关代码位置

| 文件 | 负责 |
|---|---|
| `Source/Vertigo/Private/Save/VTGSaveCoordinator.cpp` | 槽位、`GatherWorldState`／`ApplyWorldState`、读档时序、Retry |
| `UVTGSaveCoordinator::CanSaveNow()` | 唯一的存档门禁：战斗／过场／对话／读档中都拒绝，并给回理由文案 |
| `Source/Vertigo/Public/Save/VTGSaveTypes.h` | `UVTGSaveGame`、`FVTGLevelProgressRecord`、`FVTGMapProgress`、GI 快照字段 |
| `Source/Vertigo/Public/Save/VTGSaveStatics.h` | **共用**的值变量筛选与文本导入导出（BPLM 和 GI 都用这一份） |
| `Source/VertigoEditor/Private/VTGGameInstanceSnapshot.inl` | GI 快照回归测试（`-run=VTGAlleyRepair -GameInstanceSnapshot`） |
| `Source/VertigoEditor/Private/VTGStoryAutoSave.inl` | stage 自动存档回归（`-run=VTGAlleyRepair -StoryAutoSave`） |
| `Source/VertigoEditor/Private/VTGSaveKeyWatcher.cpp` | 删除"名字就是存档 key"的 actor 时弹编辑器提示 |
| `Source/Vertigo/Private/UI/VTGSaveIndicator.cpp` | 右下角 SAVING 转圈；写盘前一个 tick 显示 |
| `Source/VertigoEditor/Private/VTGDialogueRecordAudit.inl` | 断言没有 Cond_PlayedNode 指向 NPC 节点（`-DialogueRecordAudit`） |
| `Source/VertigoEditor/Private/VTGLeashedDiag.inl` | 只读诊断：`-LevelProgressShape`／`-OrphanGates`／`-LeashedWiring`／`-LeashedDetail`／`-LeashedCallers` |
| `Source/VertigoEditor/Private/VTGLeashedDiag.inl` 的 `-DestroyTargets` | 列出每个 BPLM 销毁的 actor 和类 |
| `Source/Vertigo/Private/VTGLevelManagerBase.cpp` | BPLM 变量筛选（`IsProgressValue`／`IsProgressProperty`）、捕获／恢复、战斗恢复入口 |
| `Source/Vertigo/Public/VTGGameInstanceBase.h` | 只有 `CurrentStage`；BP 变量在 `BP_VTG_GameInstance` 上，由快照反射收集 |
| `Plugins/Narrative/Source/Narrative/Private/Dialogue.cpp:1469,1514` | 自动记录 `PlayDialogueNode_<NodeID>` |
| `Plugins/Narrative/Source/Narrative/Public/NarrativeSaveGame.h` | Narrative 存盘内容（任务 + `MasterTaskList`） |
| `Source/Vertigo/Private/UI/VTGTerminalSavePage.cpp` | SAVE/LOAD 页（C++ 自建 widget）；`OpenForTerminal` 终端模式、`OpenForMainMenu` 只读档模式 |
| `Source/Vertigo/Private/UI/VTGTerminalSaveLibrary.cpp` | 把页面当合成 tab（254）插进 `WB_TabSwitcherMenu` |
| `Source/Vertigo/Private/UI/VTGMainMenu.cpp` | 主菜单；Continue → 打开只读档页 |
| `Docs/SaveSystem.md` | 现有实现参考 |
| `Docs/CombatCheckpointGuide.zh-CN.md` | 你给的那份战斗检查点指南（仍然有效） |
## 7. 隐患评估（2026-11-20）

按严重度排。标了「推断」的是从代码读出来的，**没有实测**。


### 实际做的修法（2026-11-20）

**核心是一个门禁，三处共用：`UVTGSaveCoordinator::CanSaveNow(World, OutReason)`。**
它回答"现在写存档安全吗"，并给回一句玩家能看的理由。拒绝的情况：

| 情况 | 怎么判 | 理由文案 |
|---|---|---|
| 战斗中 | Pawn 自己的 `InCombat`（C++ 各处读的就是它） | "Not available within conflict." |
| 过场中 | 世界里有 `ALevelSequenceActor` 的 player 在 `IsPlaying()` | "Not available during a cutscene." |
| 对话／Note 中 | `UNarrativeUIInputComponent::GetActiveOwner()` 不是终端自己 | "Not available during a conversation." |
| 读档／重试中 | `PendingLoad` / `bRetrying` | "Cannot save during a level transition." |
| 没世界没 pawn | — | "Cannot save right now." |

接它的三处：`SaveTerminalSlot`、`SaveFromTerminal`、以及自动存档的等待条件。
以前这三处各写一套零散判断，现在不会再互相不一致。

**严重 1：** 自动存档不再用固定延时，改成**轮询等到安全**（每 0.25 秒问一次 `CanSaveNow`），
`StoryAutoSaveTimeout`（默认 180 秒）之后放弃并打日志。所以入场过场播完才存。
换图或读档接管时 `CancelPendingRestore()` 会把它取消掉。

**严重 2：** 槽位从 1 个变 2 个轮换（`StoryAutoSaveSlot = 10`、`StoryAutoSaveSlotCount = 2`，
`MaxSlots` 11 → 12）。`PickStoryAutoSaveSlot()` 选空的、否则选**较旧**的那个——
所以较新的那一拍永远不会被同一次坏写毁掉。槽位不是记在内存里而是**从 manifest 的时间推出来**，
所以下次开游戏也还是对的。写完还会 `DoesSlotExist` 回读一次确认。

关于你问的"能不能从一开始就没有坏存档"：**不能保证，但可以做到坏的那个永远不是唯一的那个。**
门禁消掉的是"语义上不该存的时刻"（过场、战斗、对话），轮换兜住的是"没预料到的状态 + 写一半"。
两个一起上，就是能做到的最好程度。

**严重 3：** 战斗中打开终端，SAVE/LOAD 页整页换成一行
"Not available within conflict."，样式对齐 `WB_TabWidget_Mail` 的 `NoMessage`：
`NotoSansSC-Medium`、字号 25、Overlay 居中、上内边距 20。页面每 tick 重新判一次，
所以战斗开始／结束时它会自己切换。回归里把字体和字号钉住了，Mail 的空态哪天改了会在那里报出来。

**中等 4：** `AVTGLevelManagerBase` 加了 `LevelManagerSaveId`（`EditAnywhere`）。
存档的 key 改成 `GetProgressKey()` = 填了就用它，没填就退回 Actor 名（所有现有 BPLM 的行为不变）。
另外两件事：
- `ClassPath` 从**精确相等**放宽成"相等 or 本类是它的子类"，所以给 BPLM 换父类不再清空旧存档。
  真正不匹配时会打 Warning 再跳过，不再静默。
- `ApplyWorldState` 现在会检查存档里**没人认领的 key**，逐条打 Warning 并提示该把
  `LevelManagerSaveId` 设成什么来救回旧存档。改名丢进度从"静默"变成"日志里有一行"。

**中等 6：** `SaveToSlot` 的写入顺序改成 主体 → narrative → ISX → **manifest 最后**。
前面任何一步失败就**不写 manifest**，所以槽位保持原来的样子（旧的好存档还能读，
从来没存过的槽继续显示为空），不会出现"看起来有效、内容是旧的"。

回归：`-run=VTGAlleyRepair -TerminalSave -VerifyOnly` 里新增 `SaveGate()`——
用真 pawn 真 PlayerController，验证安静时放行、战斗中拒绝且文案是终端那一句、
有序列在播时拒绝、保留槽两种情况都拒绝。`-StoryAutoSave` 补了双槽和门禁的断言。
四个回归全过。
### 严重 1 — 自动存档会存在进场过场里（已修，2026-11-20）

`StoryAutoSave` 在 BPLM `BeginPlay` 之后一个 tick 写。而**有 4 张图的入场序列是
`bPlayOnBeginPlay`**：`BPLM_SaApartment`、`BPLM_L2StreetBarFight`、`BPLM_L2TunnelCine`、
`BPLM_Tunnel_NearMarket`。另外 9 个 BPLM 身上有配好 `bHidePlayer` + `bDisableMovementInput`
的序列（不一定在 BeginPlay 播，但常常是入场就播）。

一个 tick 根本躲不开一段几秒的过场。所以自动存档很可能存在过场中间，后果有三层：

1. **`PlayerTransform` 存成过场摆位的位置。** 读档会把玩家丢在那儿——可能在镜头外、在空中、在墙里。
2. **story gate 已经标记"开始过了"。** 入场序列前面的事件如果已经走了 `EnterSavedStoryEvent`，
   存档会记下来。读档时入场被跳过，**但它的副作用从来没发生**。
3. **序列写进 mesh 的 relative transform 可能被烘进去**（见 [[sequence-restore-state-leak]]）。

改法：别用固定一个 tick，改成"等入场序列结束再存"。真正的解是下面那条
`CanSaveNow()`——它要问的第一个问题就是"有没有序列在播"。
在那之前，**自动存档这个功能最好先别开**，或者只在没有入场序列的那几张图上开。

### 严重 2 — 自动存档只有一个槽，没有轮换（已修，2026-11-20）

槽位 10 每次换图覆盖一次。所以**一旦它存进一个坏状态（比如上面那条），上一个好的就没了**。
而且现在它写得很频繁，坏状态被烘进去的窗口比以前大得多。

改法：开两个槽（10 / 11）交替写。约 15 行，是最便宜的保险。

### 严重 3 — 终端在战斗中照样能开，没有存档门禁（已修，2026-11-20）

`UVTGTerminalInputGate::TryAccept()` 只是个 250ms 冷却，**不看游戏状态**。
战斗里按 Tab 存档，存下来的东西里没有战斗状态——`EncounterResumeEvent` 只有
`SaveEncounterCheckpoint()` 会写。读回来会是"敌人满血重生、玩家打到一半的进度没了"，
或者更糟，一个半 dispatch 的 encounter。

改法：`CanSaveNow()`，三个问题，真相来源都现成：
`UNarrativeUIInputComponent::HasOwners()`（对话/Note 在跑）、BPLM 的 `IsCutscene` /
`CurrentSequencePlayer`（过场中）、战斗组件有没有 active encounter。
`SaveTerminalSlot` 和 `StoryAutoSave` 都接它，SAVE 按钮置灰并把理由写在状态行。

### 中等 4 — BPLM 改名 → 进度静默丢失（已修 2026-11-20；但"改名"这个说法在 §10 更正了）

进度的 key 是**实例的 Actor FName**，而且要求 `ClassPath` **精确相等**。所以：
在 Outliner 里重命名一个 BPLM 实例、重命名或移动 BPLM 资产、给它换父类——
三种操作都会让所有已有存档里这个 BPLM 的进度**悄悄失效，不报错**。

改法：加 `FName LevelManagerSaveId` 作为显式 key，并把 `ClassPath` 相等放宽成 `IsChildOf`。

### 中等 5 — 对话节点 ID 变了 → "已经选过的选项"会被忘掉（已知并接受）

Narrative 记的是 `PlayDialogueNode_<NodeID>`。**重新编排对话会重新生成 NodeID**，
旧存档里那些记录就对不上了，`Cond_PlayedNode` 这类条件会当成没选过。

这条没有便宜的解法。要知道的是：**改对话资产会让旧的测试存档在"选过什么"这件事上失效**，
所以测存档别留太久。

### 中等 6 — 一个槽是 4 个文件，但写入不是原子的（已修，2026-11-20）

`SaveToSlot` 的顺序是：主体 → **manifest** → narrative → ISX 广播。
而 manifest 就是"这个槽有效"的标记。所以如果 narrative 那一步失败，
你会得到一个**看起来有效、但任务数据是旧的**槽。

改法：把 manifest 挪到最后写。几行的事。

### 已知并接受

- **往返丢 BPLM 状态**：已审计，12 个事实，其中公寓那组 6 个该搬 GameInstance（见 §4 P0）。
  剩下的是一次性布尔，以后归 stage 管。
- **in-flight 状态不存**：timer、delay、DoOnce 内部、montage、正在播的序列都不存。
  这是设计选择，靠"只在自由活动时存"来回避——前提是严重 3 修掉。
- **freemove 对话中能存**：确认过这类对话强制 auto-advance、自动选项，不承载玩家选择，
  丢掉只是台词。


## 8. 第二轮隐患（2026-11-20）

§7 那批修完之后又过了一遍，这些是新查出来的。标「已验证」的是从代码或资产里读出来的事实。

### 严重 7 — 只有 2 个资产实现 `IVTGSaveable`，所以"销毁"对存档完全不可见（已修，2026-11-20）

全工程实现了 `IVTGSaveable` 的只有 **`BP_Player_Sa`** 和 **`BP_Hammer`**。
而 `HandleActorDestroyed` 只记录实现了这个接口的 actor：

```cpp
if (Actor && Actor->HasAnyFlags(RF_WasLoaded) && Actor->Implements<UVTGSaveable>())
```

同时，**14 个 BPLM 里有 8 个会调 `K2_DestroyActor`**：
L2StreetBarFight、PoliceStation、BlackMarket、BlackMarketEntrance、
Tunnel_NearMarket、Clinic、SaApartment、SewerUnderApt。

合起来的意思是：**这 8 张图里被销毁的东西，一个都没进 `DestroyedActors`。**
读档后它们全部回来。而 BPLM 自己"我已经做过了"的那个变量**是**被存下来的
（`RestoreLevelProgress` 会把它恢复成 true）。

所以 §4 里那条 P2「读档后值恢复了但世界没变」**不是边缘情况，是主线问题**：
至少 8 张图会出现"变量说做完了、世界说没做"。而且原因不是忘了调用什么，
是销毁这件事从一开始就在存档的视野之外。

两条路：
- **窄的**：给那些会被销毁的 actor 加上 `IVTGSaveable`（一个接口标记而已），
  销毁就会自动进 `DestroyedActors`，读档时重新移除。
- **宽的**：给每个 BPLM 一个幂等的 `VTG_RebuildScene` 事件，读档后调一次，
  让它按自己的变量把世界重新摆对（§4 P2 原本的建议）。

我偏向**窄的那条**：它不需要改 14 张图的蓝图，只要在被销毁的那些 actor 的类上加接口。


#### 实际改法：把销毁的"opt-in"去掉，而不是给 16 个类加接口

先查了那 8 个 BPLM 到底销毁什么（`-run=VTGAlleyRepair -DestroyTargets`）。结果是
**16 个互不相关的类**，没有一个共同基类可以一次搞定：

| 类 | 次数 |
|---|---|
| `BP_0ISXInteractiveCollision_C` | 9 |
| `BPA_0SmartTrigger_C` | 4 |
| `BPA_SmartRestrict_C` | 4 |
| `BPA_StageBlockingVolume_C`、`BP_FightingAreaCollision_C` | 各 1 |
| 敌人／NPC：`BP_Enm_BarFighter`、`BP_Enm_StreetGoon1/2/3`、`BP_NPC_StreetGoon1/2/3`、`BP_Bello_NPC`、`BP_Boss_SeaMonster_L3Sewer`、`BP_Drone_IPHA_NPC`、`BP_Philip_IPHA_NPC` | 各 1 |
| 车辆：`BPA_SaCar`、`BPA_PoliceVan` | 各 1 |
| `LevelSequenceActor`（运行时 Create 出来的）、以及 BPLM 销毁自己 | — |

所以逐个加接口是 16 处资产改动，而且**改完设计还是反的**。真正的问题是：

> **opt-in 对"捕获状态"是对的**（只序列化你标记过的），
> **对"销毁"是错的**——一个随地图加载进来的 actor 被游戏销毁了，它就该一直是销毁的。

所以改的是 `HandleActorDestroyed`：去掉 `Implements<UVTGSaveable>()` 这个条件，
`RF_WasLoaded` 的 actor 被销毁就记下来。读档侧对应地把"移除已销毁的"那一段
提到接口检查**之前**——状态恢复仍然是 opt-in，只有销毁不是。
`ResolveSaveId` 里的接口调用加了保护（没实现接口就用 Actor 名作 key）。

三道豁免，防止它伤到正常流程：
- **玩家 pawn**：由 game mode 重新 spawn，记录它会让读档把刚恢复的 pawn 删掉。
  判定不只看 `IsPlayerControlled()`（那个走 PlayerState，没有 game mode 时为假），
  还看 controller 和 player 0 的 pawn。
- **`AVTGLevelManagerBase`**：它是这张图的大脑，BeginPlay 才是恢复进度的地方，
  而且 `BPLM_L2StreetBarFight` 会销毁自己。
- **`DestructionUntrackedClasses`**（`EditDefaultsOnly`）：以后要放行别的类填这里，不用改 C++。

顺带确认了这不会改战斗重试的行为：检查点是在战斗分段**开始前**存的，那时还没有敌人死，
而 `DestroyedActorIds` 每次 world init 都 `Reset()`。所以重试仍然是"检查点之前销毁的保持销毁、
之后死的回来"——正是重试该有的样子。

回归：`-run=VTGAlleyRepair -TerminalSave -VerifyOnly` 里新增 `DestructionTracking()`。

**严重 8（敌人状态）顺带好了一半：** 被销毁的敌人现在会保持销毁。还没解决的是
"还活着但掉了血"的敌人——血量依然不存。那需要给敌人加 `IVTGSaveable` + 给血量勾 SaveGame，
是另一件事，而且只在"自动存档正好发生在战斗中途"时才有意义——而门禁现在不让那发生。


### 严重 8 — 敌人／NPC 状态完全不存（已验证；销毁那半已随严重 7 解决）

没有任何敌人实现 `IVTGSaveable`，所以血量、位置、死没死都不在存档里。
读一个剧情自动存档 → 杀掉的混混全部满血回来。

以前这不明显，因为自动存档只在战斗检查点发生。**现在它在战斗结束后的第一个安静时刻发生**——
"这场打完了"这个事实只存在于 BPLM 的变量里。如果某张图没有记这件事的变量，读档会重新开打。
建议逐个确认：8 张有战斗的图，"打完了"是不是都有一个会被存下来的变量。

### 中等 9 — 按名字反射的属性是一整类静默失效

存档系统有很多地方是靠属性名找东西的，改名**编译不会报错**：

| 名字 | 用处 | 有回归保护吗 |
|---|---|---|
| `InCombat` | 存档门禁判战斗 | ✅ `SaveGate()` |
| `Leashed?` / `Detect LeashedOrRaw?` | 需求 ④ | ✅ `-GameInstanceSnapshot` |
| `Health` | 终端／剧情存档存血量 | ❌ |
| `CanInteract?` | ISX 互动开关 | ❌ |
| `MailFields` 那 5 个邮件字段 | 邮件状态 | ❌ |
| `Player Pawn` | BPLM 缓存，`Detect LeashedOrRaw?` 要用 | ❌ |

最便宜的大保险：把这些名字集中成一张表，加一个回归逐个断言"这个属性在这个类上存在"。
几十行，能挡住一整类半夜才发现的问题。

### 中等 10 — 写入已经原子了，读取还不是

§7 中等 6 把 manifest 挪到最后写，所以**写**是原子的了。但**读**不是：
`HandlePostLoadMap` 里 `NC->Load(NarrativeSaveName(Slot), 0)` 的**返回值没有检查**。
narrative 读失败时，主体已经套上去了，你会得到一个"位置对、任务错"的世界，而且没有任何提示。

改法：检查返回值，失败就打 Error，并且考虑是不是该整体放弃这次读档。

### 中等 11 — 自动存档现在会在游玩中造成卡顿（没测过）

`SaveToSlot` 要遍历全世界的 actor、序列化、写 4 个文件。
以前只在战斗检查点发生（玩家本来就预期有个停顿），**现在发生在自由活动中**。
好的一面是它通常紧跟入场过场结束，是个天然的掩护点；坏的一面是**没人测过它要多久**。
建议测一次，大概率没事，但这是唯一一条玩家一定会直接感觉到的。

### 小 12 — 超时 180 秒会静默放弃

入场过场很长、紧接着进战斗或对话，这一拍的自动存档就没了，只有一条 Warning。
因为没盖章，下一张图会补存，所以不会连续丢。可接受，但要知道它会发生。

### 小 13 — 血量会进剧情自动存档（条件写法的副作用）

存血量的条件是 `Meta.SlotIndex != AutoSaveSlot`。槽 10/11 不等于 0，所以**会**存当前血量。
这大概是你想要的（剧情存档该保留当前血量），但它是条件写法的副作用，不是明确的决定。
如果想让剧情自动存档也满血，要把这个条件改成显式判断。

### 小 14 — 相机朝向不存

只存 pawn 的 `PlayerTransform`，不存 controller 的 control rotation。读档后相机朝向回默认。
小事，但玩家会注意到。


## 9. 第三轮隐患（2026-11-20）

### 严重 15 — 打死的敌人会活着回来（已修，2026-11-20，用上面的 (a)）

这条直接碰到你定的标准（"死了和活了、完成与否区分开就够了"）——**死活现在还没区分开。**

`UVTGCombatComponent::HandleDeath()` 做的是：

```cpp
CombatState = EVTGCombatState::Dead;
OnCombatStateChanged.Broadcast(OldState, CombatState);
OnDeath.Broadcast(Killer);   // ragdoll / 掉落 / BPI_OnEnmDeath 都在蓝图里
```

**它不销毁 actor。** 而 `CombatState` 是 `UPROPERTY(VisibleInstanceOnly, Category="Combat|Debug")`
——没有 SaveGame 标记，宿主也不实现 `IVTGSaveable`。

所以§8 严重 7 的修法只覆盖了"**BPLM 主动销毁**"的那批（相位清场时 destroy 掉的混混、Bello、Boss），
**不覆盖"玩家打死的"**——那些只是 `CombatState = Dead` 的 ragdoll，读档后满血站起来。

两个修法：

- **(a) 存档时把死掉的算作销毁。** `GatherWorldState` 里扫一遍世界，
  `FindComponentByClass<UVTGCombatComponent>()` 的 `CombatState == Dead` 就把它的 id 加进
  `DestroyedActors`。读档后尸体直接不在。**零资产改动**，用严重 7 刚建好的机械。
- **(b) 给敌人类加 `IVTGSaveable` + 一个勾了 SaveGame 的 `bDead`**，读档后重放死亡
  （ragdoll、掉落）。尸体还在，但要处理"重放一次死亡"的副作用。

按你"只要区分死活"的标准，**(a) 就够了**，而且便宜得多。唯一的可见差别是读档后看不到尸体。

### 严重 16 — New Game 不重置存档系统（已修，2026-11-20）

`ClearPersistentFlags()` **在游戏代码里没有任何调用者**（只有我的回归调它）。
而 Coordinator 是 GameInstance subsystem，活过换图。所以：

> 读一个存档 → 回主菜单 → New Game → `PersistentInts`、`PersistentNames`、
> `ProgressByMap`、以及自动存档的 beat 盖章 **全部留着**。

最具体的后果：`BPLM_L2StreetBarFight` 把 `LevelPhaseForL2StreetFight` 存在 `PersistentInts` 里
（它是 14 个 BPLM 里唯一这么做的，见 §4）。新游戏走到酒吧街，BPLM 读到**上一轮的"打完了"**，
于是跳过整场战斗。beat 盖章也还在，所以新游戏第一张图不会自动存档。

缓解：`HandlePreLoadMap` 在 `PendingLoad` 为空（普通换图，New Game 就是）时会清掉三个
checkpoint key，所以最糟的"新游戏直接跳进战斗恢复"不会发生。但上面那条会。

修法：加一个 `BeginNewGame()`，一次清掉 flags + 盖章 + `ProgressByMap` + stage。
**这一步需要在编辑器里连一下**：主菜单上 Continue 是 `Button`（C++ 绑的），
另外还有 `Button_1`、`Button_2`、`Button_86` 是蓝图接的，其中一个是 New Game。


#### 修法

**严重 15（死掉的敌人）：** `GatherWorldState` 在填完 `DestroyedActors` 之后再扫一遍世界，
`FindComponentByClass<UVTGCombatComponent>()` 的 `GetCombatState() == Dead` 就把它的 id
`AddUnique` 进去。玩家 pawn 和 BPLM 走的是和严重 7 同一道豁免（`ShouldTrackDestruction`）。
零资产改动。读档后尸体不在，这是刻意的取舍。

**严重 16（New Game）：** 加了 `BeginNewGame()`，清掉 `PersistentInts`／`PersistentNames`／
`ProgressByMap`／beat 盖章／`ResumedCheckpoint`／`RespawnOverride`／`PendingLoad`／
`bRetrying`／待写的自动存档，并把 stage 置空。

**不需要你连蓝图**：它在 `HandlePostLoadMap` 里自动调用——加载 `MainMenuLevelName`
（默认 `M_MainMenu`，`EditDefaultsOnly` 可改）且 `PendingLoad` 为空时。
也就是"玩家回到主菜单且不是在读档"= 这一轮结束了。
Continue 和读档菜单都在 OpenLevel **之前**就设好 `PendingLoad`，所以不会被误判成离开。
`BeginNewGame()` 本身是 BlueprintCallable，你想显式调也可以。

这也正好实现了你说的"让 New Game 绕过自动存档、下一次自动存档覆盖上一轮"：
盖章被清掉 → 新游戏第一张图会正常自动存档 → 双槽轮换会先盖掉较旧的那个，
两次自动存档之后上一轮的就没了。

**中等 17（改名）：** `ApplyWorldState` 里加了第二条孤儿警告：存档说某个 id 在这张图被销毁了，
但世界里没有同名的 actor、也没有刚被移除的，就打 Warning。不解决改名，但不再静默。

**小 19（轮询开销）：** `IsPlayerInCombat` 的 `FindFProperty` 按 pawn 类缓存了；
页面的 `RefreshAvailability` 从每帧改成每 0.2 秒；
自动存档的轮询间隔从 0.25 秒放宽到 0.5 秒（`StoryAutoSavePollInterval`，可配）。
最坏情况的全 actor 遍历次数从 720 次降到 360 次。

#### 关于 MasterTaskList（中等 18）的更正

**我把它说重了。`MasterTaskList` 不是无界的。** 它的类型是

```cpp
TMap<FString, int32> MasterTaskList;   // NarrativeComponent.h:334
```

key 是 `(TaskName + '_' + Argument).ToLower()`，写入逻辑是
"找到就把 int 加一，找不到才 `Add`"（`NarrativeComponent.cpp:679-685`）。
**所以它按 key 去重**，重复播同一个节点只是把计数 +1。

上限因此是**玩家播过的不同对话节点数**，而它又被**全游戏的对话节点总数**封顶。
工程里现在 53 个对话资产；按每个 30 个节点估，约 1600 条，每条 key 约 40–60 字符，
加上 int32 ≈ 64 字节 → **100% 通关大约 100 KB**，每个槽一份。不是问题。

**所以不建议改成"只在对话结束时记一条"**，两个原因：

1. **那会破坏需求 ①。** 逐节点的记录就是"这个选项我已经选过了"的实现方式——
   `Cond_PlayedNode` 和 `WBP_NarrativeButton_DialogueOption_ProfilePicture` 读的正是它。
   只记"这段对话播过了"，就拿不到"我当时选的是哪一句"。
2. **本来就没有每节点的磁盘写入。** Narrative 文件只在 `SaveToSlot` 里
   `NC->Save()` 时写一次。逐节点变化的只有内存里那个 TMap。

要确认的话，玩一段之后看一眼 `Saved/SaveGames/VTG_Narrative_*.sav` 的文件大小就知道了。


### 中等 17 — `DestroyedActors` 的 key 是 Actor 名，现在横跨 16 个类（已修 2026-11-20；"改名"说法见 §10 更正）

和 §8 中等 4（BPLM 改名）是同一个问题，但 BPLM 我给了 `LevelManagerSaveId`，这些没有。
在 Outliner 里重命名一个 ISX 碰撞／触发器 → 它的销毁记录不再匹配 → 读档后回来。
而且我加的孤儿警告只覆盖 `LevelProgress`，**不覆盖 `DestroyedActors`**。

便宜的修法：同样加一条警告——存档里有这个 id，世界里既没有同名的活 actor、
也没有刚被移除的，就打 Warning。不解决问题，但让它不再静默。

### 中等 18 — `MasterTaskList` 的体积（2026-11-20 我先说不用改；2026-11-21 按作者意见改了，见 §10）

每播一个对话节点就往 Narrative 存档里加一条 `PlayDialogueNode_<NodeID>`，永久保留
（这正是需求 ① 能成立的原因，见 §2①）。一轮完整流程可能是几千条字符串，**每个存档槽一份**。

这是整个系统里唯一会无界增长的东西。建议玩一段之后量一次存档文件大小，确认它不会失控。

### 小 19 — 两处我自己引入的轮询开销（已修 2026-11-20；存档提示见 §10）

- `UVTGTerminalSavePage::RefreshAvailability()` 每 tick 调 `IsPlayerInCombat`，
  里面是按名字 `FindFProperty`（对类的属性线性查找）。页面开着时每帧一次。该缓存那个 `FProperty*`。
- 自动存档的等待每 0.25 秒跑一次 `TActorIterator<ALevelSequenceActor>`，
  最坏 180 秒 = 720 次全 actor 遍历。能接受，但是个味道。


## 10. 第四轮：一处更正和三项收尾（2026-11-21）

### 更正 — 在 Outliner 里改名其实不会弄坏存档

我在 §8 中等 4 和 §9 中等 17 两次说过"在 Outliner 里重命名会让存档静默失效"。**这是错的。**

`AActor::SetActorLabel`（`ActorEditor.cpp:1194`）整个函数里**一次 `Rename` 都没有**——
它只写 `ActorLabel` 然后广播 `FCoreDelegates::OnActorLabelChanged`。
所以 Outliner 改名**不改 `GetFName()`**，存档的 key 不受影响。

真正会弄坏 key 的是**删掉再重新摆一个**：替换上去的那个 actor 会拿到一个新的自动名字，
而存档里记的还是旧名字。复制粘贴同理。所以这件事要防的是**删除**，不是改名。

`LevelManagerSaveId`（§8 中等 4）仍然值得填——它让这类事彻底免疫——只是它防的是
删除／重摆，不是改名。

### 收尾 1 — 删除保存键时的即时提示（已做）

`UVTGSaveKeyWatcher`（`UEditorSubsystem`，在 VertigoEditor 里）监听
`GEngine->OnLevelActorDeleted()`，删到下面两种 actor 时立刻弹编辑器通知 + 打 Warning：

- `AVTGLevelManagerBase` 且 `LevelManagerSaveId` 为空（它的进度就是按名字存的）
- 任何被同一张图里某个 BPLM 的蓝图对象变量引用着的 actor
  （BPLM 销毁的东西都是这么引用到的，而销毁记录按 actor 名存）

提示的内容就是上面那句话：现有存档指的是旧名字，重新摆一个会拿到新名字，对不上。
回归：`-TerminalSave -VerifyOnly` 里的 `SaveKeyWatcher()`。

### 收尾 2 — 对话记录从"逐节点"改成"玩家选项 + 对话结束"（已做）

§9 中等 18 我先说"不是无界、不用改"，作者指出那只是 demo 规模、还要算 DLC。改了：

- **NPC 台词默认不再记录。** 新增 `UDialogue::bRecordEveryNodePlayed`（默认 false），
  哪个对话真要对 NPC 台词做条件就在那个资产上勾回来。
- **玩家选的选项照旧记录** —— 需求 ① 完整保留。
- **对话跑完时记一条** `PlayedDialogue_<对话名>`，位置在
  `UNarrativeComponent::ClientExitDialogue_Implementation`（所有退出路径都汇到那里）。

**动手前先查实了前提**：全工程 `Cond_PlayedNode` 一共 45 个实例、分布在 5 个对话里，
**全部指向玩家选项节点，没有一个指向 NPC 节点**。所以去掉 NPC 记录对现有内容零影响。

新增回归 `-run=VTGAlleyRepair -DialogueRecordAudit`：扫全部对话资产，断言每个
`Cond_PlayedNode` 指向的都是玩家节点、或者该对话勾了 `bRecordEveryNodePlayed`。
现在是 34 个对话、45 个条件、全部合规。**这条回归是这次改动的安全带**——
没有它，以后谁加一个 NPC 节点条件，会在几个月后才发现它永远不成立。

注意：改的是 `Plugins/Narrative` 里的 `Dialogue.cpp` / `Dialogue.h` /
`NarrativeComponent.cpp` 三处。插件是 fork 过的（见 [[narrative-plugin-forked]]），
升级插件会冲掉它们。

### 收尾 3 — 右下角的存档提示（已做）

`UVTGSaveIndicator`（C++ 自建 widget，`UCircularThrobber` + "SAVING"，
NotoSansSC-Medium 17 号，右下角内缩 48px）。

**一个必须知道的限制：写盘那几帧它转不起来**，因为 `SaveToSlot` 是同步阻塞的。
所以做法是：轮询到"可以存了"的那一刻**先把它显示出来、`return true` 多等一个 tick**，
下一个 tick 才真正写，写完 `Finish()`。这样动着的转圈出现在停顿的**前后**，
而不是一个冻住的转圈卡在停顿**中间**。显示时间有 0.8 秒下限，写得太快也看得见。
自动存档被取消（换图／读档接管）时 `CancelPendingRestore()` 会收掉它。

手动存档没加——玩家自己点的，本来就知道在等什么。
回归：`-TerminalSave -VerifyOnly` 里的 `SaveIndicator()`。

---

## §11 编辑器开关：`VTG.AutoSaveInDevRuns`（2026-11-21）

### 为什么需要它

大部分 level sequence 要靠 **Simulate** 调。而 Simulate 和 Play 一样是 `EWorldType::PIE`
（`PlayLevel.cpp:1791` 建的 world context，`2362` 设的 world type），所以每次进编辑器试玩，
BPLM 的 `BeginPlay` 都会排一次故事自动存档——写进的是 `Saved/SaveGames` 下**和游戏同一批文件**。
一下午的动画调试会把槽 10/11 反复冲掉，主菜单的 Continue 指向一个测试点。

补一句实测：Simulate 里**没有被附身的 pawn**（`bStartedInSpectatorMode`，`PlayLevel.cpp:2824`），
而 `CanSaveNow` 第一条就是没有 pawn 就拒绝。所以在加这个开关之前，Simulate 其实也写不进去——
它只是**空转 180 秒然后打一条 Warning 放弃**。那是个意外，不是设计：从 Simulate 切到 Play、
或者 F8 附身，立刻就和普通 Play 一样会写。开关把这件事变成明确的规则。

### 它是什么

一个控制台变量，默认关：

```
VTG.AutoSaveInDevRuns 0   // 默认：从编辑器起的运行都不写故事自动存档
VTG.AutoSaveInDevRuns 1   // 要测存档系统本身时打开，行为和打包版完全一致
```

只管**自动**的故事存档（槽 10/11）。以下三样**永远照写**，在编辑器里也一样：

| 照写 | 为什么 |
|---|---|
| 终端里手动存 | 你自己点的，那正是做测试存档的正常办法 |
| 槽 0 战前检查点 | 死亡重试靠它，而打架恰恰是要在编辑器里反复试的 |
| 读档 | 只读，不碰盘 |

所以准确的说法是：它控制"编辑器要不要**自动**往我的存档文件里写"，不是"存档系统开不开"。

### 判断的是**构建**，不是世界类型

第一版写的是 `World->IsPlayInEditor()`，那是**漏的**：它等价于 `WorldType == EWorldType::PIE`，
覆盖 Play 和 Simulate，但 **Standalone 漏掉了**——Standalone 是另起的
`UnrealEditor.exe -game` 进程，世界类型是 `Game`。而 Standalone 恰恰是 PIE 视口显示不了的东西
（隧道加载视频就是）唯一的测试手段。那等于最该被挡住的那种测试完全没挡住。

改成编译期判断：

```cpp
#if WITH_EDITOR
 return World != nullptr && CVarAutoSaveInDevRuns.GetValueOnAnyThread() == 0;
#else
 return false;
#endif
```

`WITH_EDITOR` 对**编辑器启动的每一个二进制**都为真，Standalone 也在内；打包版为 0。
所以一个条件覆盖 Play / Simulate / Standalone 三种，而且发行版里**这段整个被编译掉**——
比"运行时判断一下发行版该不该存"是更强的保证：没有分支可走，也没有变量可设错。

### 关键性质：它没有状态

这是设计时唯一真正要守住的东西。被压制时**什么都不留下**：

- 不写 beat 戳（`VTG.StoryAutoSaveStage`）——所以系统不认为"这一拍已经存过了"
- 不建 ticker——所以没有一个排队中的写在等
- 不攒任何待办

早退点**故意放在戳和 ticker 之前**（`VTGSaveCoordinator.cpp`，`StoryAutoSave(UWorld*,FName)` 开头）。
因此：

> 把它关掉做自己的事，再打开——**不是一次状态迁移，不需要任何重置**。
> 那条被跳过的 beat 从来没被记成"已存"，所以它仍然欠着，下一次触发（换 stage、换图、
> 或者该 BPLM 再 BeginPlay）就正常写。

要注意的只有一点，而且是它应有的样子：**关着的时候做的事不会被追补**。打开开关不会把刚才那段
补存进去。同一拍里打开，要等到**下一拍**才写；想立刻留一个，从终端手动存。

### 屎山风险

低，而且是刻意压低的：

- **只有一个判断点。** `StoryAutoSave(UWorld*, FName)` 是所有自动故事存档的唯一漏斗
  （进图走 BPLM `BeginPlay`，换 stage 走 `OnStageBegin`，两条都汇到这里），所以一个 `if` 就够，
  不存在第二份会跑偏的规则。"disable 标志散到 N 处然后互相矛盾"正是这类开关变屎山的方式，
  这里从结构上没有这个机会。
- **不沉默。** 每压制一次打一条 Display 日志，写明是哪一拍、因为哪个变量、以及手动存档不受影响。
  "我玩了一遍怎么没存"必须能从 Output Log 里查出来，而不是靠猜。
- **不碰存档格式。** 没有新字段、新文件，`VTG_SAVE_VERSION` 不变。旧档新档互通。
- **性能是负的。** 多一次 CVar 读，每拍一次；省下的是一次同步写盘外加 180 秒 0.5 秒间隔的轮询。

### 回归

`-StoryAutoSave` 新增一段。注意**这个 commandlet 本身就是一次 dev run**，所以默认是被压制的——
测试在开头显式 `Set(1)` opt in（并断言默认值真的是 0），结尾还原，压制单独一段测。

断言的两个重点：一是**门禁按构建而不是世界类型**——`Game` 类型的世界也被压制，这条就是 Standalone
的覆盖证明；二是**无状态**——压制后戳没变、ticker 没建，把变量翻成 1 之后同一拍立刻就能写。
另外断言槽 0 不在管辖范围内。

### 顺带更正的一处注释

`VTGLevelManagerBase.h` 上 `LevelManagerSaveId` 的注释还写着"在 Outliner 里改名会把进度悄悄丢掉"，
那是 §10 已经更正掉的错话。改成了实情：改名安全（`SetActorLabel` 不调 `Rename`），
**删掉重摆**和**复制**才会换 key，而那一刻 `UVTGSaveKeyWatcher` 会弹提示。

### 在哪里调（2026-11-21 补）

控制台变量没有界面，所以额外做了一个菜单勾选项。

**顶部菜单 `Vertigo > Save System > Autosave in editor runs`**（Help 左边），
`VTGSaveDevMenu.cpp`，走的是工程里现成的 `VTGXxx::Startup()/Shutdown()` + `UToolMenus` 模式
（和 `VTGMailRowJump` 一样），从 `FVertigoEditorModule::StartupModule` 挂上。

设计上的要点是**勾选项自己不存状态**：读和写都走 `IConsoleManager` 找那个变量，
所以在控制台敲命令、勾选项的勾会跟着变，反之也一样——两者不可能对不上。
变量定义在 Vertigo 运行时模块里是个 file-static，编辑器模块只能通过控制台注册表拿到它，
这个耦合方向正好是对的。变量找不到时勾选项是灰的，而不是假装能点。

它仍然**重启编辑器就回到关**，这是故意的：默认挡住才是安全的那一侧。

三个地方的关系要记清楚：

| 在哪设 | 影响谁 |
|---|---|
| 顶部菜单勾选项 / Output Log 的 Cmd 框 | 编辑器进程 → Play 和 Simulate |
| Standalone 窗口里按 `~` | 只影响那个进程（它是另起的进程） |
| `Config/DefaultEngine.ini` 的 `[ConsoleVariables]` | 全部，但会进 git，且失去默认挡住的保护 |

注册时用的是**自己命名的 section**（`VTGProject`），不是主菜单的匿名 section——
`Shutdown` 里按名字 `RemoveSection`，如果用 `NAME_None` 会把引擎自己的菜单项一起删掉。

回归 `-SaveDevMenu`：断言菜单项在自己的 section 里、重复注册不会叠加（热重载会再跑一次 startup
回调）、勾选项读写的就是那个变量、能跟上控制台里敲的值、setter 幂等（点击处理是取反当前值，
所以 setter 必须幂等两者才同步）。
