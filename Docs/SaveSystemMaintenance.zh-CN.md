# 存档系统 · 维护文档

> 这是**用的时候查的文档**：怎么用、怎么开关、搭场景会不会被影响、以后改东西要注意什么。

存档系统一共四份文档，各管一摊：

| 文档 | 什么时候看 |
|---|---|
| **这份** | 用、开关、搭场景注意、以后改东西 |
| [`SaveSystem.md`](SaveSystem.md) | **接蓝图的时候。** 哪个节点、哪些引脚、放在哪个图里。**那份不是这份的英文版** |
| [`FreeSaveSystemReview.zh-CN.md`](FreeSaveSystemReview.zh-CN.md) | 想知道**当初为什么这么做**。那是过程记录和评审轮次 |
| [`CombatCheckpointGuide.zh-CN.md`](CombatCheckpointGuide.zh-CN.md) | 只关心战斗检查点（槽 0） |

---

## 速查

**槽位分配**（共 12 个，`MaxSlots = 12`）

| 槽 | 谁在用 | 玩家能存进去吗 | 玩家能读出来吗 |
|---|---|---|---|
| 0 | 战前检查点（死亡重试靠它） | ✗ | ✓ |
| 1–9 | 玩家手动存档 | ✓ | ✓ |
| 10–11 | 故事自动存档，两槽轮换 | ✗ | ✓ |

**一个槽 = 三个文件**，都在 `Saved/SaveGames/`：

```
VTG_Slot_<N>.sav        主存档
VTG_Manifest_<N>.sav    槽信息（时间、地图、stage）—— 这个文件在不在，决定槽算不算有效
VTG_Narrative_<N>.sav   Narrative 的 quest + 已完成任务表
```

**最常用的三件事**

| 我想…… | 怎么做 |
|---|---|
| 调动画时不让它动我的存档 | 默认就不动。见 [1.3](#13-开关) |
| 测存档系统本身 | 顶部菜单 `Vertigo > Save System > Autosave in editor runs` 打勾 |
| 清掉所有存档重新开始 | 删 `Saved/SaveGames/` 下的 `VTG_*.sav`。主菜单进新游戏会自动重置内存状态 |

---

# 第一部分 · 使用

## 1.1 玩家怎么存和读

**两个入口，同一个页面**（`UVTGTerminalSavePage`）：

1. **终端里的 SAVE/LOAD 页** —— 完整功能，能存也能读
2. **主菜单的 Continue** —— 同一个页面，但是**只读模式**（`LoadOnly`）。主菜单里没有世界也没有 pawn，存档没有意义

页面的行为：

- **Save 按钮**只对槽 1–9 亮。槽 0 和 10/11 是游戏的，不是玩家的
- **Load 按钮**对任何有 manifest 的槽都亮，**包括自动存档槽**。自动存档是能读的，只是不能覆写
- **没有删除功能**。要删只能去 `Saved/SaveGames/` 手动删

## 1.2 自动存档什么时候发生

三层，互不干扰：

| 层 | 触发时机 | 写哪 |
|---|---|---|
| **战前检查点** | BPLM 在开打前主动调 `SaveEncounterCheckpoint` | 槽 0 |
| **故事自动存档** | 进入一个新的 **beat** = 换地图 **或** 换 stage | 槽 10/11 轮换 |
| **手动** | 玩家在终端点 Save | 槽 1–9 |

**故事自动存档不会立刻写。** 它排一个轮询（每 0.5 秒），等到"现在可以存了"的那一刻才写，最多等 180 秒就放弃并打一条 Warning。因为进图往往直接接一段自动播的过场，任何固定延迟都会落在过场中间。

**同一个 beat 只存一次。** 一张图里好几个 BPLM 都会报 stage begin，`BeginPlay` 也可能重复跑。但只记**最后一个** beat，不记历史——所以 A→B→A 会存三次，每次覆盖上一次。这是安全网该有的样子。

**写盘的那几帧会卡。** `SaveToSlot` 是同步的。右下角有个转圈提示（`UVTGSaveIndicator`），但它是"确定要写了之后、真写之前"显示的，所以**动着的转圈出现在停顿的前后，而不是一个冻住的转圈卡在停顿中间**。

## 1.3 开关

控制台变量 `VTG.AutoSaveInDevRuns`，**默认 0（关）**。

关的时候：**从编辑器起的任何运行都不写故事自动存档**。Play、Simulate、Standalone 都算。

**三个地方能调，而且它们不通：**

| 在哪设 | 影响谁 |
|---|---|
| 顶部菜单 `Vertigo > Save System > Autosave in editor runs` | 编辑器进程 → Play 和 Simulate |
| Output Log 底部的 Cmd 框敲 `VTG.AutoSaveInDevRuns 1` | 同上（和菜单勾选项是同一个东西） |
| Standalone 窗口里按 `~` 敲同一条 | **只影响那个进程**（Standalone 是另起的进程，菜单管不到它） |
| `Config/DefaultEngine.ini` 的 `[ConsoleVariables]` | 全部，但会进 git，而且失去"默认挡住"的保护 |

**关着的时候这三样照常写**，在编辑器里也一样：

- 终端里手动存 —— 你自己点的，那就是做测试存档的正常办法
- 槽 0 战前检查点 —— 死亡重试靠它，而打架恰恰要反复试
- 读档 —— 只读，不碰盘

**重启编辑器回到关。** 故意的：默认挡住才是安全的那一侧。

**它没有状态。** 关掉做自己的事、再打开，**不是一次状态迁移，不需要任何重置**。被跳过的 beat 从来没被记成"已存"，所以仍然欠着，下次触发就正常写。唯一的注意点：**关着的时候不会追补**。打开开关不会把刚才那段补存进去；同一拍里打开要等下一拍，想立刻留档就从终端手动存。

## 1.4 搭场景时要注意什么

**绝大部分事情不受影响。** 纯编辑器状态下 `UVTGSaveCoordinator` 根本不存在（它是 `UGameInstanceSubsystem`，没有 GameInstance 就没有它）。不读、不写、不记。

唯一在编辑器里活着的是 `UVTGSaveKeyWatcher`，它只在**你删 actor 的那一刻**跑一下，而且只在两种情况下说话。删普通道具、删灯、删一百个石头——一声都不出。

### 放心做的

| 操作 | 结果 |
|---|---|
| 加 actor | 完全自由。新 actor 不在任何旧档里，加载时不会被碰 |
| 删普通 actor | 旧档如果记着"它已被销毁"，加载时发现没人叫这名字，只打一条 Warning。它本来就该不在 |
| **在 Outliner 里改名** | **安全。** `SetActorLabel` 不调 `Rename`，`GetFName()` 不变，存档 key 不受影响 |
| 给 BPLM 加新变量 | 旧档里没这个键，新变量保持默认值 |
| 删 BPLM 变量 | 存档里多余的键找不到属性，跳过 |
| 移动 / 重命名 BPLM 资产，或换父类 | 祖先类也接受；真不匹配会**大声**丢弃并打 Warning |
| 改 BPLM 变量的类型 | 有类型校验挡着，不会把 int 塞进 bool，那个值回默认 |

### 真会咬人的

| 操作 | 后果 | 怎么办 |
|---|---|---|
| **改地图名**（`.umap` 文件名） | 旧档指向一张不存在的图，**没有补救** | 要改趁早 |
| **改 BPLM 变量名** | 旧档里那个值按旧名字存着，找不到新名字 → **静默回默认值**，不报错 | 改之前想一下它是不是"进度"。临时中间状态填进 BPLM 的 `ExcludedProgressVariables` 就彻底不参与存档 |
| **删掉 BPLM 再重新摆一个** | 替换的 actor 拿到新的自动名字，key 对不上 | `UVTGSaveKeyWatcher` 会在删的那一刻弹提示。填 `LevelManagerSaveId`（任意唯一字符串，如 `"L5.Clinic"`）之后连这个也免疫 |
| **删掉某个 BPLM 会销毁的 actor** | 销毁记录按名字存，换名字就对不上，那东西会复活 | 同上，watcher 会提示 |
| **复制粘贴 BPLM** | 同"删掉再重摆" | 填 `LevelManagerSaveId` |

## 1.5 存档文件在哪

```
G:\Epic\Unreal projects\VertigoGit\Saved\SaveGames\
```

想从干净状态开始：删掉这个目录下所有 `VTG_*.sav`。

**PIE 和 Standalone 共用上面这个目录**——这正是需要 1.3 那个开关的原因：在编辑器里试玩，写的就是你自己平时玩的那批文件。只有打包版不一样，它在玩家的 `%LOCALAPPDATA%\Vertigo\Saved\SaveGames\` 下。

---

# 第二部分 · 隐患

编号是为了以后能引用。状态列会随版本更新。

## 还没解决的

| 编号 | 问题 | 严重度 | 现状 |
|---|---|---|---|
| **S1** | **读档不是原子的。** `NC->Load()` 的返回值没检查（`VTGSaveCoordinator.cpp:563`）。Narrative 文件坏了或丢了，读档会继续往下走，玩家拿到一个世界状态对、quest 状态错的存档 | 中 | 未修。写入侧已经是原子的（manifest 最后写），读取侧还不是 |
| **S2** | **反射按名字取属性，但没有一张表。** `InCombat`、`Health`、`SaveData`、邮件那几个字段都是按字符串找的。蓝图里改个名，这边静默失效 | 中 | 未修。应该集中成一张表 + 一条回归断言每个名字都还存在 |
| **S3** | 改 BPLM 变量名 → 旧档那个值静默回默认 | 中 | 未修。有 `ExcludedProgressVariables` 可以主动排除，但没有"改名了"的检测 |
| **S4** | 改地图名 → 旧档失效，无补救 | 中 | 不打算修。存档按地图名索引是合理设计，代价是地图名要早定 |
| **S5** | 改对话 → "已经选过的选项"记录失配 | 低 | 用户判断暂时无影响。记录按节点 ID，改图会让旧记录指向不存在的节点 |
| **S6** | `SaveToSlot` 同步阻塞，**实际卡多久没量过** | 低 | 未测。已有转圈提示掩盖，但没有数字 |
| **S7** | `MasterTaskList` 随内容线性增长 | 低 | 已缓解：NPC 台词默认不记，只记玩家选项 + 对话结束。但仍然随 DLC 增长 |
| **S8** | **Narrative 插件是 fork 的。** `Dialogue.cpp`、`Dialogue.h`、`NarrativeComponent.cpp` 三处改动，升级插件会冲掉 | 中 | 结构性的。升级前必须先 diff 这三个文件 |
| **S9** | 公寓那组日状态（`IsDay?`/`IsDim?`/`IsSaNaked?`/`IsMedicated?`/`HasNewMail`/`CanLeaveRoom?`）还挂在 BPLM 上，应该搬到 `BP_VTG_GameInstance` | 低 | 未做，纯编辑器工作。现在靠 GI 快照兜住了，能跑，但位置不对 |
| **S10** | Standalone 的开关要在它自己的窗口里单独设，菜单管不到 | 低 | 进程隔离导致的，没法用菜单解决 |
| **S11** | 评审文档 §5 里四项只能在编辑器里人工验证的项 | 低 | 未验。自动化不了 |

## 已经解决的（留着免得重复踩）

| 问题 | 怎么解的 |
|---|---|
| 打死的敌人读档后活着回来 | 存档时把 `EVTGCombatState::Dead` 的算作已销毁 |
| 销毁记录要 opt-in，导致 8 个 BPLM 销毁的东西全部复活 | 去掉 `IVTGSaveable` 要求，`RF_WasLoaded` 的 actor 被销毁就记 |
| New Game 继承上一周目的 `PersistentInts` | `BeginNewGame()`，主菜单地图加载时自动调 |
| 自动存档存在进场过场里 | 轮询 `CanSaveNow` 等到安全时刻 |
| 自动存档只有一个槽，坏一次就没了 | 两槽轮换 |
| 一个槽多个文件但写入不原子 | manifest 最后写；前面任何一步失败就不动旧 manifest |
| 终端在战斗中照样能开 | `CanSaveNow` 拒绝 + 页面显示 "Not available within conflict." |
| 调动画时自动存档churn存档文件 | `VTG.AutoSaveInDevRuns`，见 1.3 |

---

# 第三部分 · 原理

## 3.1 一个槽 = 三个文件，manifest 最后写

```
主存档 → Narrative 文件 → ISX 等蓝图子系统广播 → （全都成功才）manifest
```

**manifest 是"这个槽有效"的凭证。** 以前它第一个写，结果后面任何一步失败都会留下一个"自称完好、实际装着上一局 quest"的槽。现在反过来：前面任何一步失败就不碰旧 manifest——这个槽要么还是上一次那个好存档，要么干脆不出现在列表里，不会半生不熟。

## 3.2 存的时候采集什么

| 内容 | 来源 |
|---|---|
| BPLM 进度 | 每个 `AVTGLevelManagerBase` 的蓝图值变量，按 `GetProgressKey()` 存 |
| 已销毁的 actor | `RF_WasLoaded` 且被销毁过的，**加上**战斗状态为 Dead 的 |
| 每个 actor 的状态 | 只有实现了 `IVTGSaveable` 的（opt-in） |
| 玩家 | transform、inventory、`UVTGPlayerProgressComponent`、邮件字段、Health |
| GameInstance 变量 | 蓝图值变量快照，`Leashed?` 就在这里 |
| 检查点标志 | `PersistentInts` / `PersistentNames` |
| 走过的地图 | `ProgressByMap`，按地图名分键 |
| Narrative | 单独文件，quest + 已完成任务表 |

**采集和恢复是两套 opt-in 规则，别搞混：**

- **状态**是 opt-in 的——只有标了的才存。因为序列化一个 actor 的全部字段既贵又容易出错
- **销毁**不是 opt-in 的——地图里摆着的东西被游戏销毁了，就该一直是销毁的。这条当初做成 opt-in，结果 8 个 BPLM 销毁的 ISX 碰撞、触发器、被移走的 NPC 和敌人全部复活，而 BPLM 里那个"已经做完了"的变量还是 true。那就是"值恢复了但世界没变"这个 bug 的根源

## 3.3 读的时候的顺序

顺序是这套东西最容易踩的地方。**三个阶段：**

```
① 开图之前
   GameInstance 变量回填
   PersistentInts / PersistentNames 回填
   stage 设好
   ProgressByMap 回填
   → 然后才 OpenLevel

② 每个 BPLM 的 BeginPlay
   RestoreLevelManager(this) —— 在蓝图 ReceiveBeginPlay 和 OnStageBegin 之前

③ 开图完成后晚一 tick
   ApplyWorldState（玩家、actor、销毁、交互）
   NC->Load()
   OnLoadSubsystems 广播（ISX 等）
```

**为什么 ① 必须在开图之前：** actor 在 `BeginPlay` 里就会读这些值。晚一步它们读到的是默认值。

**为什么 ③ 要晚一 tick：** 玩家 pawn、inventory 这些要等世界真的建好。

## 3.4 门禁 `CanSaveNow`

**只有一个地方回答"现在能不能存"**，所以终端和自动存档不可能给出矛盾的答案。拒绝的情况：

| 情况 | 玩家看到的理由 |
|---|---|
| 没有世界或没有 pawn | "Cannot save right now." |
| 正在读档 / 正在战斗重试 | "Cannot save during a level transition." |
| 战斗中 | "Not available within conflict." |
| 有 level sequence 正在播 | "Not available during a cutscene." |
| 对话或 Note 占着 UI 输入 | "Not available during a conversation." |

战斗那条读的是**玩家 pawn 自己的 `InCombat` 标志**，和战斗表现代码读的是同一个——所以这里的答案和玩家屏幕上看到的不会矛盾。

对话那条有个细节：**终端自己也占着 UI 输入**，但那不是拒绝的理由——它就是那个在问的人。

## 3.5 beat = 地图 + stage

```
"<地图短名>|<stage，没有就是 ->"
```

两者任一变化就是新 beat，别的都不是。

**为什么要带地图：** 14 张图里目前只有 4 张真的设 stage，只按 stage 判断几乎永远不触发。
**为什么要带 stage：** 覆盖"同一张图里 stage 推进"。

stage 为 `None` 是合法的——一张没有 stage 的图照样值得存一次。

## 3.6 key 是什么

| 对象 | key |
|---|---|
| BPLM 进度 | `LevelManagerSaveId`，空的话用 actor 的 `GetFName()` |
| 销毁记录 / actor 状态 | `IVTGSaveable::GetSaveId()`，空的话用 `GetFName()` |
| 玩家 | 固定 `"VTG.Player"`（运行时生成的名字不稳定） |

`GetFName()` 是**关卡里稳定的**——Outliner 改名不动它，所以改名安全。会换的是删除重摆和复制。

## 3.7 开关为什么按 `WITH_EDITOR` 而不是世界类型

第一版写的是 `World->IsPlayInEditor()`，**那是漏的**。它等价于 `WorldType == EWorldType::PIE`，覆盖 Play 和 Simulate（这两个共用 PIE 世界类型），但 **Standalone 漏掉了**——Standalone 是另起的 `UnrealEditor.exe -game` 进程，世界类型是 `Game`。

而 Standalone 恰恰是 PIE 视口显示不了的东西（隧道加载视频就是）唯一的测试手段。等于最该挡住的那种测试完全没挡住。

`WITH_EDITOR` 对编辑器启动的**每一个**二进制都为真，Standalone 也在内；打包版为 0，**这段整个被编译掉**——比"运行时判断一下发行版该不该存"是更强的保证：没有分支可走，也没有变量可设错。

---

# 第四部分 · 后续维护

## 4.1 按任务查

### 加一张新地图

1. 地图名**早定**，之后改名会废掉所有旧档（S4）
2. 图里的 BPLM 填上 `LevelManagerSaveId`
3. 不需要注册任何东西。自动存档会在进图时自动触发（beat 变了）

### 加一个 BPLM

1. **填 `LevelManagerSaveId`**，任意唯一字符串，如 `"L7.Rooftop"`。这一步省了，以后删掉重摆就会丢进度
2. 它的蓝图**值变量**会自动进存档。对象引用永远不进
3. 临时的中间状态放进 `ExcludedProgressVariables`，别让它参与存档。注意这个是**类默认值**（`EditDefaultsOnly`），改的是整个 BPLM 蓝图；而 `LevelManagerSaveId` 是**每个实例**各自填的（`EditAnywhere`）
4. **注意父类 `BPLM` 上的变量默认不存。** 只有 `GeneralLevelPhase` 和标了 `SaveGame` 的例外——因为父类上那些是摄像机、输入、sequence 的临时记账，读档后必须是干净的。所以变量加在父类上还想存，**就得标 `SaveGame`**

### 让 BPLM 销毁一个 actor

不用做任何事——销毁会被自动记录。但是：

- 被销毁的那个 actor**别删掉重摆**，key 会变（watcher 会提示）
- 如果它很重要，给它实现 `IVTGSaveable::GetSaveId()` 返回一个固定名字

### 加一个需要保存自身状态的 actor

1. 实现 `IVTGSaveable`
2. 要存的字段标 `SaveGame`
3. `GetSaveId()` 返回一个稳定的名字
4. 需要存位置的话 `ShouldSaveTransform()` 返回 true

### 改对话

- **玩家选项**和**对话结束**会被记录，NPC 台词默认不记
- 哪个对话真要对 NPC 台词做条件判断，在**那个对话资产**上勾 `bRecordEveryNodePlayed`
- 勾完**跑一次 `-DialogueRecordAudit`**。它会扫全工程，断言每个 `Cond_PlayedNode` 要么指向玩家节点、要么该对话已经勾回全记录。没有这条回归，一个永远不成立的条件可能几个月后才被发现

### 升级 Narrative 插件

**先 diff 这三个文件**（S8）：

```
Plugins/Narrative/Source/Narrative/Private/Dialogue.cpp
Plugins/Narrative/Source/Narrative/Public/Dialogue.h
Plugins/Narrative/Source/Narrative/Private/NarrativeComponent.cpp
```

改动是：`bRecordEveryNodePlayed` 这个开关、NPC 节点记录被它 gate 住、以及 `ClientExitDialogue_Implementation` 里补的那条 `PlayedDialogue_<对话名>`。

### 给存档加字段

1. 字段加到 `FVTGSaveGame` / `FVTGMapProgress` 等结构里
2. **`VTG_SAVE_VERSION` 加 1**
3. 在 `LoadFromSlot` 里加一段迁移，把旧版本折叠成新形状（照 v2→v3 那段写）
4. 跑 `-TerminalSave -VerifyOnly`

### 改了 C++

按工程惯例：**重编 + 把 editor DLL 一起提交**，pre-commit hook 会拦。这次涉及的 DLL 包括 `Plugins/Narrative/Binaries/` 下的三个。

### 改了行为之后，哪份文档要动

三份文档分工不同，所以要改的不一样。**别只改一份**：

| 改了什么 | 要动 |
|---|---|
| 加/改了蓝图能调的节点 | `SaveSystem.md` 的节点表 |
| 改了槽位分配、门禁条件、存了什么 | 这份（第一、三部分）**和** `SaveSystem.md` 的 §7 gotchas |
| 解决了一条隐患 | 这份第二部分，把它从"还没解决"挪到"已经解决" |
| 加了新的回归 | 这份 4.2 |
| 只是讨论过程 | `FreeSaveSystemReview.zh-CN.md` 追加一节，别往这两份塞

## 4.2 回归清单

改了存档相关的东西，**六条全跑**：

```powershell
& "G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject" -run=VTGAlleyRepair -TerminalSave -VerifyOnly -unattended -nopause -nosplash
```

```powershell
& "G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject" -run=VTGAlleyRepair -StoryAutoSave -unattended -nopause -nosplash
```

```powershell
& "G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject" -run=VTGAlleyRepair -GameInstanceSnapshot -unattended -nopause -nosplash
```

```powershell
& "G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject" -run=VTGAlleyRepair -LevelProgress -VerifyOnly -unattended -nopause -nosplash
```

```powershell
& "G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject" -run=VTGAlleyRepair -SaveDevMenu -unattended -nopause -nosplash
```

```powershell
& "G:\Epic\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "G:\Epic\Unreal projects\VertigoGit\Vertigo.uproject" -run=VTGAlleyRepair -DialogueRecordAudit -unattended -nopause -nosplash
```

| 回归 | 覆盖什么 | 期望 |
|---|---|---|
| `-TerminalSave -VerifyOnly` | 终端页面、门禁、销毁追踪、key watcher、转圈提示、主菜单入口 | 12 条 PASS |
| `-StoryAutoSave` | beat 判定、轮换、拒绝条件、New Game 清理、dev run 开关 | 1 条 PASS |
| `-GameInstanceSnapshot` | GI 变量快照往返 | 1 条 PASS |
| `-LevelProgress -VerifyOnly` | BPLM 进度形状 | 2 条 PASS |
| `-SaveDevMenu` | 菜单勾选项和控制台变量不会对不上 | 1 条 PASS |
| `-DialogueRecordAudit` | 每个 `Cond_PlayedNode` 都还成立 | 1 条 PASS |

**这些回归不碰玩家的存档文件。** 自动存档的写入被推到一个测试里永远不会 pump 的 ticker 上；而且 commandlet 本身也是一次 dev run，默认就被开关挡着（测试里显式 opt in、结尾还原）。

## 4.3 只读诊断

查问题用的，不改东西：

| 命令 | 看什么 |
|---|---|
| `-LevelProgressShape` | BPLM 进度字段的形状 |
| `-OrphanGates` | 没人认领的 stage gate |
| `-DestroyTargets` | 哪些 BPLM 会销毁哪些 actor |
| `-PlayedNodeConditions` | 全部 `Cond_PlayedNode` 指向谁 |
| `-LeashedWiring` / `-LeashedDetail` / `-LeashedCallers` | `Leashed?` 那条链 |

## 4.4 出问题先看哪

| 症状 | 先查 |
|---|---|
| 读档后世界没变，但 BPLM 的"做完了"是 true | 销毁记录对不上。Output Log 搜 `was destroyed in` |
| 读档后某个 BPLM 进度全丢 | Output Log 搜 `no level manager in`，它会直接告诉你该把 `LevelManagerSaveId` 填成什么 |
| 玩了一遍什么都没存 | Output Log 搜 `skipped:`（开关挡的）或 `gave up after`（180 秒没等到安全时刻） |
| 某个值读档后回了默认 | 变量改过名，或者在 `ExcludedProgressVariables` 里 |
| 死掉的敌人活着回来 | 战斗状态没到 `Dead`，或者那个 actor 没有 `RF_WasLoaded`（运行时生成的不记） |

## 4.5 关键文件

| 文件 | 作用 |
|---|---|
| `Source/Vertigo/Private/Save/VTGSaveCoordinator.cpp` | 全部存读逻辑 |
| `Source/Vertigo/Public/Save/VTGSaveTypes.h` | 存档格式 + 版本号 |
| `Source/Vertigo/Private/VTGLevelManagerBase.cpp` | BPLM 进度采集/恢复 |
| `Source/Vertigo/Private/UI/VTGTerminalSavePage.cpp` | SAVE/LOAD 页面 |
| `Source/Vertigo/Private/UI/VTGSaveIndicator.cpp` | 右下角转圈 |
| `Source/VertigoEditor/Private/VTGSaveKeyWatcher.cpp` | 删 actor 时的提示 |
| `Source/VertigoEditor/Private/VTGSaveDevMenu.cpp` | 顶部菜单开关 |
