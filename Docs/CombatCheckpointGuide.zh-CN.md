# 战斗临时存档：使用与维护

适用项目：Vertigo · 核对日期：2026-10-24

这套机制在战斗开始前保存剧情进度，让玩家死亡后从这场战斗重新开始，避免重做此前完成的对话和调查。它复用项目已有的 `VTGSaveCoordinator`，没有建立另一套独立存档格式。

“临时”指它使用自动存档槽，后续自动存档会替换它；它不是退出游戏就消失的内存存档，也不是暂停并精确续接战斗的快照。

## 1. 日常使用

### 玩家会经历什么

1. 完成战前剧情，进入战斗开始点。
2. 系统保存当前剧情值、玩家重生位置，以及这场战斗的恢复入口。
3. 玩家死亡并选择 Retry，系统重新加载当前关卡的自动存档。
4. 恢复玩家、背包、任务和 BPLM 进度，再重建战斗场景。
5. 敌人重新开始战斗，不重播该恢复入口已经跳过的片头和对话。

保留的是**存档发生时**的进度。存档之后发生、尚未再次保存的变化不会被保留。战斗中已经造成的敌人伤害、临时召唤物等，不应当依赖这套机制续接。

### 存档槽与胜利后的行为

| 操作 | 当前行为 |
| --- | --- |
| 战前 `Save Encounter Checkpoint` | 写入自动存档槽 0，并记录检查点 ID、地图和顺序 |
| 死亡 Retry | 优先读取当前地图的槽 0；没有当前地图的自动存档时，从关卡开头重启 |
| 手动存档 | 当前配置使用槽 1–9，也包含 BPLM 进度及已选择的恢复入口 |
| 胜利时 `Set Encounter Resume Event` | 修改内存中的恢复入口，供下一次保存使用；**不会立即写盘** |
| 后续自动存档 | 可以替换槽 0 中原来的战前快照 |

因此，刚打赢后如果还没有再次保存，槽 0 仍可能是战前存档，Retry 仍会重新打这场战斗。这是当前设计。若要增加“胜利后自动存档”，应另选一个稳定的剧情结束点，不要直接在死亡回调或尚在播放的对话中保存。

## 2. 已接入的战斗

三个战斗的检查点顺序目前都是 `100`。顺序比较以地图为范围，不会因为不同地图同为 100 而互相阻止。

| 地图 / BPLM | 战前检查点 ID | 战斗恢复事件 | 胜利后恢复事件 |
| --- | --- | --- | --- |
| `BlackMarketStreet` / `BPLM_BlackMarket` | `L4_AlleyFightStart` | `VTG_ResumeAlleyFight` | `VTG_ResumeAfterAlleyFight` |
| `L2_StreetOutsideBar` / `BPLM_L2StreetBarFight` | `L2StreetFightStart` | `VTG_ResumeL2StreetFight` | `VTG_ResumeAfterL2StreetFight` |
| `L_SewerUnderApartment` / `BPLM_SewerUnderApt` | `SewerBossFightStart` | `VTG_ResumeSewerBossFight` | `VTG_ResumeAfterSewerBossFight` |

### 黑市巷战

- 保存边界位于 `InitializeAlleyCombat` 的共享一次性入口，在敌人初始化前保存。
- 保存前将首次进入、战斗前阶段等标记推进到正确状态。
- 恢复时重建巷子、Drone 和战斗区域，重新初始化三名敌人及死亡绑定。
- `AfterAlleyFightComplete` 切换后续存档的恢复事件。
- `L4_AlleyConflictActive` 控制战斗期间禁用巷口触发器。
- `L4_AlleyExitReady` 决定完成战斗后仍保留巷口互动，还是执行最终离开清理。**打赢不等于已经准备离开**，不要把这两个状态合并。

### L2 酒吧街战斗

- 用 `Save Encounter Checkpoint` 替换原片头结束处的 `AutoSave`。
- 保存发生在 `L2StreetFightPhase = 1` 写入后、正式战斗启用前。
- 恢复时重建夜景、NPC 和战斗区域碰撞，进入原有战斗设置，跳过片头及开打时的首句对白。
- `VTGL2FightStateComponent::CompleteAfterFight` 将后续恢复事件切换为胜利后入口。
- 保留旧的 `L2StreetFightPhase`：0 为片头，1 为战斗，2 为战后。
- `L2StreetFightAfterDialoguePending` 表示战后对话是否仍需进行，不能只凭“敌人已死”就清除它。
- 胜利后读档保留存下来的玩家位置，不再把玩家传送到战后动画结束点。

### 下水道 Boss 战

- `VTGSewerEncounterComponent::StartBossEncounter` 在 Boss 启用伤害和 AI 前保存。
- 重复开始请求不会反复覆盖战前存档；读档尚未完成时 AI 保持等待。
- 恢复时重建围栏和房间互动状态，再开始 Boss 战。
- BPLM 中的 `L3_MonsterEggKillCount`、`L3_CountedEggIds` 随快照保存，死亡重试回到战前保存的数值。
- Boss 死亡后切换到胜利后恢复事件，后续存档读回来会隐藏并禁用新加载的 Boss。
- `L3_CheckpointDoorOpened`、`L3_CheckpointPassedDoor` 用来重建圆门状态；恢复时不调用整段开门剧情。
- 旧 `SewerStart` / `SewerLairDrop` 检查点保留；新的 Boss 战检查点顺序 100 高于旧落点检查点 10。

## 3. 保存范围

底层保存的是符合规则的 **BPLM 值变量**，不是整个关卡对象图。

| 数据 | 是否由 BPLM 进度快照保存 | 使用要求 |
| --- | --- | --- |
| Bool、整数、浮点、Enum、Name、String、Text | 是 | 适合剧情标记、阶段、计数、稳定 ID |
| 反射字段全部为受支持值类型的 Struct | 是 | 如位置、Transform 等；含对象引用的结构不适用 |
| 支持值类型组成的 Array、Set、Map | 是 | 元素及键值必须递归满足保存规则 |
| Actor、Component、UObject 引用 | 否 | 读档后重新查找、生成并写入缓存 |
| 包含对象引用的 Struct 或容器 | 不属于支持范围 | 改存 ID / 值数据，再重建引用 |
| Event Dispatcher 的绑定 | 否 | 恢复时重新绑定，并防止重复绑定 |
| Delay、Timeline、正在运行的 Sequence、活动对话 | 否 | 用明确的稳定状态重建流程 |
| DoOnce 的内部执行状态 | 否 | 新加载后会重新开始；不要用它单独表达持久剧情状态 |
| 敌人运行时状态、临时召唤物 | 不由本机制完整续接 | 按战斗恢复事件重建；其他 Saveable Actor 仍走原存档管线 |

玩家、物品和 Narrative 任务由现有存档管线分别保存；不要为了使用战斗检查点，把它们全部复制进 BPLM。

### 变量筛选规则

- 关卡 BPLM 的受支持 Blueprint 值变量默认参加保存，通常不需要逐项勾选 `SaveGame`。
- `Transient` / `DuplicateTransient` 变量不参加保存。
- 在 BPLM 的 **Class Defaults → Excluded Progress Variables** 中，可以按变量名排除临时值。
- 公共父蓝图 `BPLM_C` 的相机、输入、序列等缓存默认不保存；`GeneralLevelPhase` 是例外，明确标记 `SaveGame` 且类型安全的父级值属性也可以保存。
- `SaveGame` 标记不能让不支持的对象引用变成可保存的进度数据。

例子：保存 `DoorOpened = true` 并不自动打开门。恢复事件仍需根据这个 Bool 将门设置到打开姿势、同步碰撞和互动条件。

## 4. 为一场新战斗接入

### 第一步：确认 BPLM 继承关系

关卡管理器必须直接或间接继承 `AVTGLevelManagerBase`。通常继续使用现有公共 `BPLM` 父蓝图即可，不要为了接入而丢掉原有父蓝图逻辑。

确认地图放置的确实是预期 BPLM 实例，相关敌人、场地和互动引用也都指向当前地图中的正确对象。

### 第二步：先建立两个恢复事件

在该关卡 BPLM 中新建两个**没有任何参数**的 Custom Event，例如：

```text
VTG_ResumeWarehouseFight
VTG_ResumeAfterWarehouseFight
```

事件名将以 `Name` 值存入存档。节点中的 `Resume Event` 必须与实际事件名一致。

战斗恢复事件负责：

1. 重建灯光、场地、必要 NPC、同伴和对象缓存。
2. 按当前实现重建敌人及死亡通知绑定，重设本轮战斗所需计数。
3. 同步场地碰撞、互动触发器、玩家战斗模式和镜头。
4. 最后允许敌人攻击。

胜利后恢复事件负责：

1. 清除、隐藏或禁用战斗敌人。
2. 按保存的剧情阶段重建 NPC、门、触发器、同伴和离开路线。
3. 保留存下来的玩家位置，除非该恢复点明确需要固定位置。
4. 只继续确实未完成的后续剧情。

不要让恢复事件调用包含整段片头的“首次进入关卡”事件。建议拆出可复用的“设置场景”和“启用战斗”事件，由正常流程和读档恢复共同调用。

### 第三步：选择战前保存边界

正确的边界是：重要战前对话已经结束，剧情标记已经写入，玩家位置稳定，而敌人还不能造成伤害。

Blueprint 连接示例：

```text
战前对话 / Sequence 结束
  → 写入“战前剧情已完成”“现在进入战斗”等进度变量
  → 本次战斗的一次性保护
  → Save Encounter Checkpoint
      Target            = Self（当前 BPLM）
      Checkpoint Id     = WarehouseFightStart
      Resume Event      = VTG_ResumeWarehouseFight
      Respawn Transform = 已确认安全的重生 Transform
      Order             = 100
  → 重建 / 启用战斗
```

保存节点的 Target 是 BPLM。底层会取得 Coordinator，不需要把 Target 接到 Save Coordinator 上。

`Respawn Transform` 可以取玩家当前 Transform，也可以用专门的安全点。应避开墙体、落点和会立即触发其他剧情的位置；通常使用单位缩放、仅水平朝向。此 API 会使用传入的 Transform，不会自动修正不合理的缩放或位置。

`Order` 用于阻止同地图内较早的检查点覆盖较晚的检查点。多场顺序战斗可用 100、200、300。**相同 Order 仍可能再次保存**，因此重复触发保护仍然需要设计。

正常保存时应检查返回值。返回 False 表示这次保存没有成功，不应显示“保存成功”。保存失败时 API 会回退本次修改的恢复入口和检查点 ID，但不会自动回滚你在 Blueprint 中已经改过的剧情变量。

### 第四步：在胜利时切换入口

```text
确认本场敌人全部被击败（只处理一次）
  → 写入战斗完成状态
  → Set Encounter Resume Event
      Target       = Self
      Resume Event = VTG_ResumeAfterWarehouseFight
  → 正常战后剧情
```

这一步不写盘。不要在此后不加区分地调用原片头或战前保存节点，否则可能覆盖想保留的快照。

### 第五步：处理 BeginPlay 与场景副作用

读档流程为：

```text
加载地图
  → 按 BPLM 实例匹配进度，恢复值变量
  → BeginPlay 初始化公共引用
  → 有有效的战斗恢复记录：跳过 OnStageBegin
  → 恢复玩家位置、背包、Actor 状态和 Narrative 任务
  → OnSlotLoaded
  → 执行保存的恢复事件一次
```

有几个时序细节必须保留：

- 有战斗恢复记录时，跳过的是 `OnStageBegin`，**Blueprint BeginPlay 仍会执行**。不要在 BeginPlay 中无条件重播剧情或重置剧情变量。
- 恢复事件应承担被跳过的场景初始化，包括原先在 OnStageBegin 里设置的围栏、灯光和互动条件。
- 恢复入口已经在 `OnSlotLoaded` 中执行，不要再等待下一次同名广播；L2 的 `ResumeAfterFight` 已为此做专门处理。
- 原生层会刷新常用玩家、Controller、GameInstance 和 Narrative 缓存，并恢复基本输入、镜头和行走模式；本场战斗特有的状态仍要自行设置。

## 5. 重复执行与异步流程

`Is Resuming Encounter` 仅在**同步执行恢复事件期间**为 True。在此期间再次到达 `Save Encounter Checkpoint` 会返回 True，但不写盘，用来保护原来的重试快照。

这个 True 不能被解释为“刚刚生成了一个新的磁盘存档”。

如果恢复事件经过 Delay、Timer、异步回调后才调用保存节点，回调执行时同步保护已经结束，可能重新覆盖存档。对于这种流程，应使用明确的恢复状态保护，或让恢复路线避开保存节点，而不能只依赖 `Is Resuming Encounter`。

场景重建应尽量能够安全重复调用：避免重复生成敌人、累加计数或重复绑定死亡事件。底层的一次性分发可以防止重复广播重跑入口，但不替代战斗自身的防重设计。

## 6. 日常维护

### 添加或修改变量

新变量先判断它是“剧情事实”还是“运行缓存”。剧情事实用 Bool、阶段、ID、计数等表达；引用和临时结果读档后重建。增加门、互动器或同伴状态时，检查正常流程与两个恢复事件是否同步更新。

改变变量名称或类型后，旧存档不一定能恢复它：当前匹配使用变量名和 C++ 类型，新字段没有旧值时保留加载对象已有的值。不要假设 Blueprint 重命名会自动迁移存档中的字符串键。

### 重命名事件、移动资产或替换地图中的 BPLM

当前记录按 BPLM Actor 的 `FName` 查找，并核对 BPLM 的类路径；变量还会核对名称和类型。恢复事件也按存下来的名字查找。

以下变更都需要专门验证旧存档：

- 重命名或删除恢复 Custom Event。
- 给恢复事件添加参数。
- 移动 / 替换 BPLM 类资产。
- 删除地图中的 BPLM 后重新放置，使实例内部名称改变。
- 改变量名、类型或排除列表。

优先保留旧事件名作为无参数转发入口。若需要迁移 Actor 键、类路径或变量键，应在存档代码中明确实现迁移，不能只依赖编辑器显示名称或资源重定向器。

找不到有效恢复事件时，系统会退回普通阶段入口；旧存档仍然可读不等于一定能保持相同的恢复效果。

### 调整检查点位置或战斗流程

先确认新的保存边界位于关键剧情完成之后、伤害启用之前。移动起战点时，同时检查重生点、自动重叠触发器、相机及场地碰撞。修改击败人数、敌人种类、Boss 启动条件或离开路线时，也要修改恢复路径和测试。

下水道恢复围栏位置目前在 `RestoreEncounterCheckpointScene` 中使用原关卡入口设置的坐标。若修改围栏位置，应同时核对正常入口与这里的恢复位置。

### 旧存档与测试清理

旧存档不能补回当时未保存的变量。验证新接入时，先正常走到新保存边界，生成一份新检查点。

`Clear Checkpoint` 会删除自动存档槽 0 并清理检查点标记，不会清空全部手动存档。它是实际删除操作，只应在明确的新游戏流程或可丢弃的测试数据上使用。

不要通过反复运行修复工具的 Apply 模式来进行日常测试；日常检查使用下文的 Verify 参数。

## 7. 验证步骤

### 手动游戏验证

建议使用专门的测试存档环境，依次检查：

1. **首次进入**：片头、战前对话和调查照常发生，战前生成检查点。
2. **死亡重试**：已完成的战前剧情不重播，玩家在安全位置，敌人、场地和操作状态正常。
3. **值变量恢复**：选取至少一个剧情 Bool、一个阶段、一个计数，对照战前与重试后的值。
4. **重复触发**：重复走过起战 Trigger，不生成第二组敌人、不重复绑定、不反复保存。
5. **胜利后保存再读档**：敌人不再参战，NPC、门和离开路线正确，已完成对话不重播。
6. **仍有后续互动**：打赢但尚未准备离开时，未完成的互动仍可进行；不要提前销毁对象。
7. **旧存档与打包版本**：检查旧入口兼容性，以及目标地图是否实际被打包。

若直接从胜利后读档测试，请先确保胜利后确实产生了新存档；否则读到战前槽 0 并重新开战是预期行为。

### 当前自动验证命令

在项目根目录运行 PowerShell。将引擎路径替换为当前使用的 UE 版本；下面的命令均为验证模式。

```powershell
# 黑市：共享检查点与三名敌人的相关回归
& 'G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject' `
  -run=VTGBlackMarketCombat -Checkpoints -VerifyOnly -unattended -nullrhi -nosound

# 黑市后续：巷口触发器生命周期、受伤混合设置和 Story UI
& 'G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject' `
  -run=VTGBlackMarketFollowup -VerifyOnly -unattended -nullrhi -nosound

# L2 / 下水道：保存值往返、OnSlotLoaded 和实际场景恢复
& 'G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject' `
  -run=VTGBlackMarketFollowup -EncounterVerify -unattended -nullrhi -nosound

# 下水道原有的 Boss 启动、死亡、蛋击杀归属与去重
& 'G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject' `
  -run=VTGSewerEncounter -unattended -nullrhi -nosound
```

依次运行，确认退出码为 0，并查看日志中的 PASS / SUCCESS。不要仅凭最后出现一个成功标记忽略随后的崩溃或失败退出码。

这些测试使用内存存档和隔离世界，不写玩家存档槽。L2 隔离测试会避开库存模式切换与教程 Widget 等渲染相关路径，因此仍需手动验证完整游戏画面、HUD、教程和实际死亡重试。

## 8. 常见问题排查

| 现象 | 优先检查 |
| --- | --- |
| Retry 回到关卡开头 | 是否成功保存了当前地图的槽 0；保存返回值；是否后来被其他地图自动存档替换 |
| 读档后重复战前对话 | 实际读到的是否新存档；恢复事件名是否正确；BeginPlay 是否无条件播放剧情；恢复路线是否调用了完整首次进入流程 |
| 值恢复了，但门 / 碰撞没有变化 | 变量恢复不会重放副作用；检查恢复事件是否应用了门姿势、碰撞和互动状态 |
| Boss 在加载中就开始攻击 | 检查 AI 启动是否等待 Pending World Restore 结束，以及是否存在另一条直接启动路线 |
| 敌人不出现、出现两组或死亡计数不对 | 检查地图预放与运行时生成是否冲突；死亡绑定是否重复；本轮剩余人数是否正确初始化 |
| 手动存档里打赢了，读档又开始战斗 | 胜利后是否切换恢复事件；该存档是否真的在切换之后生成 |
| 打赢后立刻 Retry 又要重打 | 检查是否尚未产生战后存档；仅 Set Encounter Resume Event 不会覆盖战前槽 0 |
| BPLM 某变量没保存 | 检查变量类型、Transient、排除列表、是否为公共父级运行缓存；是否在保存之后才修改 |
| 读档后变量又变回去 | 检查 BeginPlay、组件 BeginPlay、初始化事件和恢复事件是否重新赋值 |
| 读档后永久等待、操作锁住 | 恢复入口中是否又等待下一次 OnSlotLoaded；相机 / 输入锁、Movement Mode 与战斗模式是否同步 |
| 旧存档在改名后失效 | 核对实例 FName、类路径、恢复事件名、变量名与类型；需要显式迁移或保留兼容入口 |
| 重试后原检查点被覆盖 | 检查重复起战入口、普通 AutoSave，以及 Delay / Timer 后脱离同步恢复保护的保存调用 |

## 9. 代码与资产定位

以下路径相对项目根目录。

| 文件 / 目录 | 维护职责 |
| --- | --- |
| `Source/Vertigo/Public/VTGLevelManagerBase.h` | BPLM 保存 / 恢复 Blueprint API 与排除配置 |
| `Source/Vertigo/Private/VTGLevelManagerBase.cpp` | 变量筛选、捕获 / 还原、恢复时序和一次性分发 |
| `Source/Vertigo/Private/Save/VTGSaveCoordinator.cpp` | 槽位写入、地图加载、玩家 / 物品 / 任务恢复和 Retry |
| `Source/Vertigo/Public/Save/VTGSaveTypes.h` | BPLM 进度记录及存档数据结构 |
| `Source/Vertigo/Private/Save/VTGL2FightStateComponent.cpp` | L2 胜利交接、战后对话标记和场景恢复 |
| `Source/Vertigo/Private/Combat/VTGSewerEncounterComponent.cpp` | Boss 保存边界、AI 等待、胜利入口及圆门 / 围栏恢复 |
| `Source/VertigoEditor/Private/VTGCombatCheckpointRepair.inl` | 黑市检查点接入和回归工具 |
| `Source/VertigoEditor/Private/VTGBlackMarketFollowupRepair.inl` | 黑市触发器等后续修复和验证 |
| `Source/VertigoEditor/Private/VTGStreetSewerCheckpoints.inl` | L2 / 下水道 Blueprint 接入工具 |
| `Source/VertigoEditor/Private/VTGStreetSewerCheckpointTests.inl` | L2 / 下水道内存保存及场景恢复回归 |
| `Scripts/CombatCheckpoints/Backup` | 黑市检查点接入前的资产备份 |
| `Scripts/BlackMarketFollowup/Backup` | 黑市后续修复前的资产备份 |
| `Scripts/EncounterCheckpoints/Backup` | L2 / 下水道本次接入前的资产备份 |

备份是某次修改前的历史版本，可能不包含之后的其他修复。回退前应比较版本并备份当前资产，不要直接覆盖整个关卡蓝图。

相关实施记录：

- [原存档系统参考](SaveSystem.md)
- [L2 / 下水道接入与测试记录](../Scripts/EncounterCheckpoints/RESULT.md)
- [黑市后续修复记录](../Scripts/BlackMarketFollowup/RESULT.md)
