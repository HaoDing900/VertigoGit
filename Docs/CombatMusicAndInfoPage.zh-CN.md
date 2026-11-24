# 战斗音乐 + 游戏内提示页（2026-11-22）

## 一、改音源和音量：只有一张表

`/Game/SFX/BGM/DT_CombatMusic`

行名就是 track id，三列能改的东西：**音源 / 音量 / 淡入 / 淡出**。

| 行名 | 哪场战斗 | 当前音源 | 音量 |
|---|---|---|---|
| `StreetBarFight` | L2_StreetOutsideBar 酒吧门外群架 | `881457__cat-fox_alex__drum-loop_random_200bpm` | 0.30 |
| `SewerBoss` | L_SewerUnderApartment 下水道 Boss | `881454__cat-fox_alex__drum-loop_deep-4_95bpm` | 0.60 |
| `BlackMarketAlley` | BlackMarketStreet（AfterAdult 阶段）小巷战斗 | `881507__cat-fox_alex__drumloop_underground-5_90bpm` | 0.30 |

**改音乐 = 开这张表改 Music；改大小声 = 改 Volume。蓝图一行都不用动。**
表的位置记在 Project Settings → Vertigo → VTG Combat Music（写在 `Config/DefaultGame.ini`）。

### 循环和停止

循环**不靠音源资产的 Looping 勾选**。`UVTGCombatMusicSubsystem`（`Source/Vertigo/.../Audio/VTGCombatMusic.*`）
拿着 audio component 不让它销毁，播完就 `Play()` 一次，所以普通 SoundWave 也会一直循环到战斗结束。
换音源时不用管它是 Wave 还是 Cue、有没有勾 Looping。

停止的接法：

- 街头：`BPLM_L2StreetBarFight` → `PlayAfterFightSequence` 事件开头 `VTG Stop Combat Music`。
- 小巷：`BPLM_BlackMarket` → `AfterAlleyFightComplete` 事件开头 `VTG Stop Combat Music`。
- Boss：`BP_Boss_SeaMonster_L3Sewer` 的 `EnemyAudio` 组件自己管（打死后再响 1.5 秒才淡出）。
- 中途切地图：subsystem 随 world 销毁，自己停。

### 开始的接法

- 街头：进战那条链上的 `VTG Start Combat Music (StreetBarFight)`（原来是 `Spawn Sound 2D` + `Set BGM`，
  那套既不循环也永远不停，已经删掉）。
- 小巷：`InitializeAlleyCombat` → `DeferEncounterUntilDialogueEnds` → Branch 的 **False** 分支上。
  **一定要挂在 gate 后面**：这个入口是在 Narrative 节点事件里调的，对白没结束时整段会被延后，
  挂在事件本身上音乐会压着最后一句台词响。
- Boss：`EnemyAudio` 组件的 **Music Track Id** 填 `SewerBoss`，`Combat Music` 留空。
  组件 BeginPlay 时去表里查音源和音量。**`Combat Music` 填了资产会盖掉 Track Id**，所以必须留空。

### 加一场新战斗的音乐

1. `DT_CombatMusic` 加一行，行名自己起。
2. 战斗开始处放 `VTG Start Combat Music`，Track 填行名；结束处放 `VTG Stop Combat Music`。
   （敌人走 `UVTGEnemyAudioComponent` 的话，改填 Music Track Id 就行，不用接节点。）

---

## 二、提示页 `WBP_InfoInGame`

原 `/Game/Widget/Tutorials/WBP_Tutorial_Battle` 已改名并移到
`/Game/Widget/InfoPage/WBP_InfoInGame`，父类改成 C++ 的 `UVTGInfoInGamePage`。
**这一个资产服务所有提示页**，页面内容靠 `Info Page Id` 区分，不用再复制控件。

### 改文字 / 加一页

两张表，行名就是 key：

- `/Game/Widget/InfoPage/DT_InfoPages` —— 一行一页：面包屑 / 标题 / 正文 / 关闭按钮文字。
  正文留空时那一行自动折叠，地方让给按键提示。
- `/Game/Widget/InfoPage/DT_InfoPageLines` —— 一行一条按键提示：
  `Page`（挂到哪一页）/ `Order`（排序）/ `Action`（Input Action）/ `Key Override`（没有 IA 时填死按键）/ `Label`（说明）。

当前只有 `Combat` 一页，三条提示：`Combat.Attack`（IA_Fire）、`Combat.Block`（右键，没有 IA）、
`Combat.Dodge`（IA_Dodge）。**优先填 `Action`**：它读玩家当前实际绑定和当前输入设备，
玩家改键或插手柄时提示自己会跟着变；`Key Override` 是没有 Input Action 时的退路。

加新一页：`DT_InfoPages` 加行 + `DT_InfoPageLines` 加几行（Page 填那个行名），
然后在蓝图里调 `Show Info Page`（Page Id 填行名），或者做个 `WBP_InfoInGame` 的子蓝图把 `Info Page Id` 改掉。

### 版式和本地化

版式照暂停菜单子页面那套母本（见 [PauseMenuPolish.md](PauseMenuPolish.md) 和 `VTGExitConfirmPage.cpp`）：
940×800 的 `ScaleBox(ScaleToFit, DownOnly)`，面包屑 y=94 / 标题 y=142 / 分隔线 y=235 / 关闭按钮 y=626，
字体统一 `Noto Sans SC`（标题 SemiBold 36、正文 Regular 20、提示 Medium 18、按钮 Medium 22）。
**背景图是 `/Game/2DArt/UI/GameOption/InfoBox`**（暂停页用的是同尺寸的 `ExtendedUIBox`，所以不会被拉变形）。
按键提示照 `WBP_Action` 右下角那套：先用 CommonInput 的平台按键 PNG（45×45），
没有图才退回短名字（LMB / RMB / Shift…），字体字号与 `WBP_Action` 一致。

文字走项目现有**手工多语言面板**，命名空间固定 `VTG.InfoPage`，key 是 `行名.列名`
（`Combat.Title`、`Combat.Attack.Label`…）。en-US 和 zh-Hans 都已填并编译进
`Content/Localization/Vertigo`。改文案照 [ManualLocalization.md](ManualLocalization.md) 的日常流程走。

原来那三段文字是硬编码中文、带 UMG 自动生成的「空命名空间 + GUID」key，
会触发翻译面板的键冲突锁；控件里的 Text 已清空，文案全部搬进表里。

### 暂停和输入没动

开页暂停游戏、出光标、关页恢复，仍然是 `WBP_InfoInGame` 自己蓝图图里的 Construct / Destruct /
`Button_151` 的 On Clicked。C++ 只负责填文字和按键图标，**不碰暂停和输入**。
所以重建控件树时 `CanvasPanel_25`（`fade` 动画的目标）和 `Button_151`（On Clicked 的绑定）
是复用的原控件，没有重建 —— 这两个绑定都是按名字找的，重建会静默失效。

---

## 三、维护命令

```
-run=VTGCombatMusicInfo                  只验证（三条 track 解析、两张页表、三处接线、控件树、父类、背景图）
-run=VTGCombatMusicInfo -Apply           建表 + 写 settings + 改名重建控件 + 接线 + 收集译文 + 编译 locres
-run=VTGCombatMusicInfo -Apply -Reseed   额外：把表里的行改回 C++ 基线（会盖掉编辑器里调过的音量和文案）
```

`-Apply` 可重跑：已经接好的线会被认出来跳过，编辑器里改过的音量和文案在不加 `-Reseed` 时保留。
改前的资产备份在 `Scripts/CombatMusicInfoPage/Backup/`（含改名前的 `WBP_Tutorial_Battle.uasset`）。

为改名加的一个小工具：`VTGManualLocalization::ForgetAsset()`。
翻译表里按 context 记着来源资产，资产改名后旧 context 读不出来，
`Compile` 会整体停在「无法复核原资产」。`-Apply` 会先把旧路径那 4 条条目清掉。
**以后再改名带文字的资产，记得清一次，否则整个 locres 编译不过。**

## 验证

`-run=VTGCombatMusicInfo -Apply` 与随后独立进程的只验证运行都 `VTG_CMI PASS (3 tracks, 3 combat lines)`，
四个蓝图编译 0 error 0 warning，locres 按字节核对：`zh-Hans` 里有「战斗模式/攻击/防御/闪避/继续游戏」，
`en-US` 里有对应英文。

**还没在 PIE 里实际听过三段音乐、也没看过提示页的实际观感。** 这一步要进游戏复测：
街头进战、下水道 Boss 触发、小巷 AfterAdult 阶段开打，各听一遍循环和结束淡出，
并看一眼提示页在 16:9 和超宽下的排版。
