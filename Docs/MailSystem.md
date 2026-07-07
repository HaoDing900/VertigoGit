# Mail 系统 维护文档

> 主角在终端上阅读邮件的完整流程。核心逻辑在 `BP_Player_Sa`，终端表现在 `BP_PortableTerminal`，
> UI 走 ISX（InventorySystemX）的终端界面。本文按实际蓝图的 NodeName 记录，只写最终跑通的方案。

---

## 1. 一句话流程

有新邮件时按 TAB → 终端 UI 直接开在 Mail 页 + 主角掏出终端播开机 → 玩家点邮件触发 Narrative 对话，
主角保持"看终端"循环、相机切到 Mail 视角、锁移动 → 对话结束 → 主角+终端播关闭动画 → 终端收起消失 →
还控制、相机还原。

## 2. 涉及资产

| 资产 | 作用 |
|---|---|
| `BP_Player_Sa` | 主控逻辑（Mail graph + ISX Inventory graph） |
| `BP_PortableTerminal` | 手上那个终端 Actor（Child Actor 挂在 `hand_lSocket`，平时隐藏） |
| `ANM_Sa_LoopWatchingTerminal1`（montage） | 主角看终端的循环动画 |
| `ANM_Sa_CloseTerminal1`（montage） | 主角关终端动画 |
| `ANM_PortableTerminal_Open / InOpen / Close` | 终端自己的 开 / 保持打开循环 / 关 |
| ISX `AC_Inventory_HUD_Component` | 开关终端 UI、暂停、输入模式 |
| ISX `WB_TabSwitcher` | 终端内的分页切换 |
| Narrative `Begin Dialogue` | 邮件内容用 Narrative 对话实现 |

关键变量（`BP_Player_Sa`）：`HasNewMail` / `IsReadingMail` / `ISXHUDReady?` /
`Mail Prev View Target` / `Terminal Ref for Anim` / `Mail_CurrentDialogue` 等一组 Mail 变量。

---

## 3. 流程详解（按 NodeName）

### 3.1 ISX Inventory graph — 按 TAB 的入口 + 三道闸

```
EnhancedInputAction IA_Inventory (Triggered)
  → Branch (ISXHUDReady?)            // 闸1：HUD 初始化完成没有
      True → Branch (Is Reading Mail)  // 闸2：正在读邮件时不重复开
          False → Branch (Has New Mail) // 闸3：有没有新邮件
              True  → Open Inventory (Active Tab Index = 4, Show Inventory Tab = ☐)  // 4 = Mail 页
              False → Open Inventory (Active Tab Index = 3, Show Inventory Tab = ☐)  // 默认页
  两个 Open Inventory → Set Input Mode UI Only (In Widget to Focus = WB Inventory Ref)
```

- `ISXHUDReady?` 由 `Event HUD Component Initialized → SET Inventory HUD Component → SET ISXHUDReady?(true)` 置位。
- `Event On Inventory Component Initialized → SET Inventory Component` 同图另一条，缓存 inventory 组件引用。
- **Mail 页 = Active Tab Index 4**（WidgetSwitcher 内容序号，不是按钮序号）。

### 3.2 `Open Inventory` / `Open Tab Switcher`（在 AC_Inventory_HUD_Component 内）

- `Open Inventory` 函数加了输入 **`ShowInventoryTab`(bool, 默认 true)** 和 **`ActiveTabIndex`(int, 默认 0)**，
  内部透传给它调用的 `Open Tab Switcher`。默认值保持原行为，旧调用方不受影响。
- `Open Tab Switcher` 函数把 `Active Tab Index / Show Inventory` 转发给 `WB_TabSwitcher.OpenTabOnStart`。
- `OpenTabOnStart` 里 `Branch(Show Inventory)`：False 分支执行 `Set Active Tab (Index)` → 切到指定页。

> ⚠️ **不要在 Player 里单独再调一次 `Open Tab Switcher`**。`Open Inventory` 内部已经调它了，
> 单独再调会导致一次 TAB 触发两次、后一次把页面覆盖回默认。整条链只走 `Open Inventory` 一个入口。

### 3.3 `ReadMailSafe`（玩家点某封邮件时触发 = 开始阅读）

```
ReadMailSafe
 → Close Inventory (AC Inventory HUD Component)
 → Play Anim Montage (ANM_Sa_LoopWatchingTerminal1)          // 主角进入看终端循环
 → Set Visibility (Terminal, New Visibility = ☑)             // 终端显形
 → Terminal → Get Child Actor → Cast To BP_PortableTerminal
       → SET Terminal Ref for Anim                            // 缓存终端引用，后面关闭时用
       → Stay Opened Terminal                                 // 终端播开机并保持打开循环
 → Delay 0.04
 → Get Player Controller → Get View Target → SET Mail Prev View Target   // 缓存当前视角(还原用)
 → Set View Target with Blend (New View Target = Mail Camera, Blend 1.0)
 → Set Active (Player Follow Camera, New Active = ☐)
 → Set Active (Mail Camera,          New Active = ☑)
 → SET Is Reading Mail (☑)
 → Delay 0.2
 → Begin Dialogue (Narrative Component, Dialogue = Test 1,
                   Play Params Start from ID = test1_DialogueNode_NPC_6)
```

### 3.4 `On Finish Reading Mail`（Narrative 对话结束时触发 = 结束阅读）

```
On Finish Reading Mail
 相机/控制还原：
   → Set View Target with Blend (New View Target = Mail Prev View Target, Blend 1.0)  // 还原开场缓存的视角
   → Set Active (Mail Camera,          New Active = ☐)
   → Set Active (Player Follow Camera, New Active = ☑)
 清变量（Clean up all Mail Variables）：
   → SET Has New Mail (☐) / Is Reading Mail (☐) / Mail Current Dialogue / Mail Current Dialogue ID (None)
     / Mail Sender Name (None) / Mail Sender Avt
   → Close Inventory (AC Inventory HUD Component)
 主角+终端关闭动画：
   → Play Anim Montage (ANM_Sa_CloseTerminal1)   // 主角关终端
   → Delay 0.2
   → Close Terminal (Terminal Ref for Anim)      // 终端播关机动画
   → Delay 1.4                                    // 等关机动画播完
   → Set Visibility (Terminal, New Visibility = ☐)  // 终端隐藏
```

> 📌 图里若还留着 `Montage Jump to Section (Section Name = End, Montage = ANM_Sa_FullOpenNClose...)`，
> 那是早期 "FullOpenNClose 单 montage + 跳 End 段" 方案的残留。现在改用独立的
> `ANM_Sa_LoopWatching` / `ANM_Sa_Close` 两个 montage，那个 Jump 节点应已断开/可删。

### 3.5 `BP_PortableTerminal` 对外接口

- `Stay Opened Terminal`（自定义事件）：播 `ANM_PortableTerminal_Open` → 接 `InOpen` 循环。
- `Close Terminal`（自定义事件）：播 `ANM_PortableTerminal_Close`。
- 终端动画用 **`Play Animation`（raw sequence）直接播**，不用 Montage —— 终端 mesh 没有带 Slot 的
  Anim Blueprint，Montage 播不出来。

### 3.6 Tab 系统 与 对话窗口的交接

**Tab 系统结构（`WB_TabSwitcher`）**

终端 UI 里有两块东西叠着，别混为一谈：

1. **Inventory（ITEM 页）是独立 widget** —— `Inventory Widget Reference`，**不在** `WidgetSwitcher_132` 里，
   靠 `Show Inventory in Tab (Show)` 单独显示/隐藏。
2. **`WidgetSwitcher_132`** —— 装其余分页（Note/Document、Quest、Map、Mail 等），靠 `Set Active Tab (Index)`
   切内容，靠 `Set Visibility (Collapsed/Visible)` 整体开关。

`OpenTabOnStart` 的分流（见 3.2）：
- `Show Inventory = True` → 显示独立 Inventory 面板 + 折叠 WidgetSwitcher_132。
- `Show Inventory = False` → `Set Active Tab (Index)` 切到指定页 + 显示 WidgetSwitcher_132（Mail 走这条）。

> ⚠️ **两套序号别搞混**：顶部按钮栏的序号（含 ITEM 那颗）和 `WidgetSwitcher_132` 的内容序号是**不同的空间**
> （因为 Inventory 有按钮但不在 switcher 里）。实测 **`Active Tab Index = 4` 能正确开 Mail 页**，直接用这个值即可；
> 若日后加/挪分页，按钮高亮和内容切换可能要各对各的序号，以你当时的布局为准。

**对话窗口开始时的交接（关键）**

真正"读邮件"时，**ISX 的 Tab UI 是关掉的，画面上是 Narrative 的对话窗口**，不是终端分页。顺序在 `ReadMailSafe`（3.3）：

```
点邮件 → Close Inventory   // 关掉整个 ISX 终端 UI（分页、背景全收）
       → ...(动画/相机/锁输入)...
       → Begin Dialogue    // Narrative 对话窗口接管，邮件正文在这里显示
```

所以 Tab 系统的职责只有**对话开始之前那一小段"选哪封邮件"**；一旦 `Begin Dialogue`，分页 UI 就退场、交给 Narrative。
对话结束时 `On Finish Reading Mail`（3.4）里再 `Close Inventory` 是**兜底**（确保 UI 收干净），此时通常已经没有分页在显示。

> 因此调分页外观、按钮、切页手感，都在 `WB_TabSwitcher` / `AC_Inventory_HUD_Component` 里改；
> 调邮件正文的排版是 Narrative 对话那边，两者不要互相找。

### 3.7 强制读信（`ForceReadMail`）+ 输入锁

**用法**：任何地方（BPLM、关卡触发器、Narrative Event）想强制主角读信，只调这一个：
```
Get Player Character → Cast to BP_Player_Sa → ForceReadMail (Dialogue, DialogueID)
```
不要在外部自己拼动画序列——能力在主角，触发方只负责"喊"。

**`ForceReadMail` 内部**（`BP_Player_Sa`）：跳过 ISX 的 TAB/UI 那段，直接：
`Sa_LockInput → 播 Sa 开终端 montage + Terminal 显形开机 → Delay(开时长) → ReadMailSafe`。
从 `ReadMailSafe` 起和点邮件路径**同一条尾巴**（切相机、Begin Dialogue、对话结束 → `On Finish Reading Mail`）。

**输入锁**：两个事件，所有强制/过场复用：
- `Sa_LockInput`：Disable Input（Pawn + Controller 各一份）+ Stop Movement Immediately。开场调。
- `Sa_RestoreInput`：Enable Input（Pawn + Controller）+ **Set Input Mode Game Only**。在 `On Finish Reading Mail` 里调。
  > Narrative 结束时只 `EnableInput` 它自己那份、且**不管 Input Mode**，所以必须自己显式恢复，不能只靠它。

### 3.8 `Set Mail Data`（邮件数据 / DataTable 驱动）

所有邮件内容集中在 `DT_Mails` 一张表，蓝图不写死任何一封。

**资产**
- `S_MailRow`（Structure）：`SenderName`(Text) / `SenderAvt`(Texture2D) / `Dialogue`(同 Sa 的 `Mail_CurrentDialogue` 类型) / `DialogueID`(Name)。
- `DT_Mails`（DataTable，行结构 `S_MailRow`）：每封一行，Row Name = MailID。含兜底行 `Mail_Default`（`Unknown Sender` + `MailAvt_DefaultNPC`）。

**怎么用**
- 加邮件：`DT_Mails` 加一行，填 4 列。蓝图零改动。
- 触发：`Get Player Character → Cast BP_Player_Sa → Set Mail Data(MailID, HasNewMail=true)`。
- 匿名/占位：传 `Mail_Default`。

**`Set Mail Data(MailID, HasNewMail)` 内部**
```
Get Data Table Row(DT_Mails, MailID)
  ├─ Row Found     → Break S_MailRow → SET Mail Sender Name/Avt/Current Dialogue/Dialogue ID + Has New Mail
  └─ Row Not Found → Print("Mail row not found") → 改查 Mail_Default 行 → 同一串 SET
```
> `Row Not Found` 的 Print 别删——填错 MailID 时 log 能看到确切行名，兜底又保证画面不崩。
> Sa 变量的默认值只是兜底、不是数据源；数据永远来自 `DT_Mails`。

**`DialogueID` 怎么填（多封邮件合在一个 Dialogue 资产里）**
- `DialogueID` = 该邮件链**第一个 NPC 节点**的 **ID**（选中节点 → Details → ID 那一栏，
  形如 `L4_Entrance_BlackMarket_Still_DialogueNode_NPC_2`）。**节点上方的大标题只是显示名（label），不是 ID，别抄错。**
- ID 只在 **NPC 节点**里查（Narrative 的 `GetNPCReplyByID`）；填 Player 节点的 ID 会整个对话启动失败、什么都不播。
- ID 在一个 Dialogue 资产内必须唯一；链中间的节点不需要手工命名，靠连线依次播放。
- 留 `None` = 从对话根节点从头播。

**坑：`On Dialogue Finished` 提前触发（对话明明还在播）**
- 症状：只播了一个节点就触发 `On Finish Reading Mail`，但对话窗口还在继续。
- 原因：`Begin Dialogue` 被**调用了两次**（Sa 的 `ReadMailSafe` 一次 + `WB_TabWidget_Mail` 的 `StartReadingMail`
  里残留一次）。第二次因已有对话在播而立即失败/结束，广播了假的 Finished。
- 规矩：**`Begin Dialogue` 只允许 `ReadMailSafe` 一个调用点**。widget 只设状态、调 `Read Mail Safe`、藏 UI，不碰对话。
  （同款病史：`Open Tab Switcher` 双调用。流程动作只能有一个 owner。）

---

## 4. 关键排查结论（踩过的坑 → 真正原因）

按 TAB 一次却开两次 / 页面被覆盖回默认
: `Open Inventory` 内部已调 `Open Tab Switcher`，别再单独调第二次。参数透传即可，Mail = Tab 4。

**PIE 一开局立刻按 TAB，UI 出来了但没暂停、input 没锁**
: HUD 初始化竞速。用 `ISXHUDReady?` 闸门挡住 —— `Event HUD Component Initialized` 发出后才置 true，
  TAB 链最前面 `Branch(ISXHUDReady?)`。

**播终端动画时终端放大约 10 倍**
: 真凶是 **`ANM_PortableTerminal_Opened` 这一条动画的 root scale 与其它终端动画/reference pose 不一致**
  （它 1.0，其它和 reference 都是 ~10.978）。播它时尺寸跳变。
  **修法**：对它套 Animation Modifier —— `Remove Bone Animation (Bone = Root, **Include Children 取消勾选**)`，
  去掉 root 轨道让它回落到 reference pose，与全家统一。
  （`Include Children` 默认勾着，会连整条动画一起删光 —— 这是当初"动画全没了"的原因。）
  → 不是 Sa 手骨 scale、不是 socket、不是组件 scale、不是第二个 mesh，别再往那些方向查。
  → 新加终端动画时，先在动画编辑器里选 Root 骨骼确认 Bone Scale 与其它动画一致（~10.978）。

---

## 5. 已知遗留 / TODO

- **需求1（Notice 动画）未做**：触发事件 → 停下走路 → 播 `ANM_Sa_NoticeNewMail` → `HasNewMail = true`。
  这是"收到新邮件"的入口，目前 `HasNewMail` 靠什么置位需补。
- **边界情况**：玩家开了终端但不点邮件直接关掉（再按 TAB/ESC），主角可能卡在看终端姿势 +
  终端没收起。主流程稳定后再加保护。
- **魔法数字**：Tab index `3/4` 是写死的，日后调整分页顺序会错位，可考虑换成枚举。
- **终端 bind pose**：`BP_PortableTerminal` 的 reference pose root scale 是 ~10.978（非 1），
  只要可见时始终有动画驱动就没问题；若日后出现"隐藏切显示瞬间闪一下大"，回 Maya 把 root
  bind scale freeze 成 1 重导可根治。
```
