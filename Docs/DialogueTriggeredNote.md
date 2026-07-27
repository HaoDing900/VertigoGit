# 对话触发 Note（Dialogue-Triggered Note）

> 2026-07 添加。让 Narrative 对话中的某个节点自动弹出一张 ISX Note，玩家读完关闭后自动触发后续事件。
> 正常情况下 ISX Note 只能通过 PDA 打开，这套流程绕开 PDA 直接开全屏 Note。

首个实装：`BPLM_PoliceStation` 的 `L3_OpenContract`。

---

## 1. 整体链路

```
对话节点 (Events 里挂 NE_OpenNote)
  → NE_OpenNote 找到本关 BPLM
  → 调用 BPLM 的事件 A（如 L3_OpenContract，参数 NoteAsset）
  → 绑定 AC_NotesComponent 的 OnStateChanged
  → Client Open Full Screen Widget → Note 弹出
  → 玩家关闭 → OnStateChanged 广播 Closed
  → HandleNoteClosed → 解绑 → 修输入模式 → 调用事件 B
```

## 2. 零件在哪

| 零件 | 位置 |
|---|---|
| `AC_NotesComponent` | 挂在 `BP_Player_Sa` 上 |
| `Narrative` 组件 | 也在 `BP_Player_Sa` 上 |
| Note 内容资产 | `PDA_NoteText` 的 Data Asset，放 `Content/Inventory/InventoryXBased/Notes/` |
| 现成参考 | `A_Terminal_Lore`（项目里已有的开 Note 终端，但它不监听关闭） |
| 输入模式设置 | `DA_NotesOptions` 的 `InputModeOnClose` 属性（全局） |

---

## 3. BPLM 侧实现

### 变量

- `NoteComp` : `AC_NotesComponent` Object Reference

### 事件 A：`XX_OpenNote`（输入 `NoteAsset` : PDA_NoteText）

```
自定义事件 (NoteAsset)
  → Set NoteComp  ← GetPlayerCharacter → GetComponentByClass(AC_NotesComponent)
  → IsValid (NoteComp)
      ├ Is Valid → [可选] AddNoteToList(Note = NoteAsset)     // 想让 Note 之后能在 PDA 里回看才要
      │          → Add Delegate: OnStateChanged (Event = HandleNoteClosed)
      │          → Client Open Full Screen Widget (Note Data Asset = NoteAsset)
      └ Is Not Valid → Print String "NoteComp not found"
```

⚠️ 顺序：**先绑定，后打开**。
⚠️ `AddNoteToList` 如果不用就整个删掉，别留悬空节点（exec 没连的话它不会执行，容易误以为生效了）。

### 事件：`HandleNoteClosed`（输入 `NewState` : E_NoteWidgetState）

由 Add Delegate 的 Create Event → "Create a matching function" 自动生成。

```
HandleNoteClosed (NewState)
  → Switch on E_NoteWidgetState
      └ Closed →  NoteComp → Remove Delegate: OnStateChanged (同一个 HandleNoteClosed)
                → Delay (0.0)
                → Get Player Controller → Set Input Mode Game and UI
                → 调用事件 B
```

**为什么要 Delay 0.0 + Set Input Mode**：
`AC_NotesComponent` 有个函数 `ChangeInputModeOnClose`，它读 `DA_NotesOptions` 里的 `InputModeOnClose`（全局，默认 Game Only）并在关闭时应用。不覆盖的话对话 UI 会点不动。Delay 0.0 排到下一帧，保证我们最后一个设。
对话 UI 还是没反应就把 `Game and UI` 换成 `UI Only`。

> 备选方案：直接把 `DA_NotesOptions` 的 `InputModeOnClose` 改成 `Game And UI`，就不用 Delay + Set Input Mode 了。但这是全局的，会影响玩家从 PDA 正常看 Note 的关闭行为。

### 事件 B：`XX_NoteClosed`

写后续逻辑。如果对话要继续播（见第 5 节），在这里加：

```
GetPlayerCharacter → GetComponentByClass(Narrative Component)
  → Get Current Dialogue → Skip Current Line
```

---

## 4. Narrative Event 侧：`NE_OpenNote`

位置：`Content/Narrative/Events/Note/`
父类：`NarrativeEvent`
参考同套路的现成资产：`NE_Cam_LockCameraByID`

**变量**：`NoteAsset` : `PDA_NoteText` Object Reference，勾 **Instance Editable**

**Override `Execute Event`**：

```
Execute Event (Pawn, Controller, NarrativeComponent)
  → Find Actor of Class In World Of
        World Source = NarrativeComponent    ← 不要用 Pawn，非玩家对话时可能是 null
        Actor Class  = 你的 BPLM 类
  → 调用 BPLM 的事件 A，Note Asset = NoteAsset
```

---

## 5. 对话节点设置

选中要触发的 dialogue node：

- **Events → +** → 选 `NE_OpenNote`
  - `Note Asset` = 你的 Note 资产
  - `Event Runtime` = **End**（台词播完才弹；选 Start 就是一开口就弹）
- 想让对话停下来等玩家读完：
  - **Line → Duration** = `Never`
  - **Is Skippable** = 勾上
  - 然后靠事件 B 里的 `Skip Current Line` 把对话推到下一个节点，第二段剧情事件正常挂在下一个节点上即可

如果不需要对话等待（Note 弹出时对话继续播），就跳过 Duration/Skippable 这两项，事件 B 里也不要 `Skip Current Line`。

---

## 6. 已知坑

| 症状 | 原因 / 解法 |
|---|---|
| Note 完全不弹 | 检查有没有 `Client Open Full Screen Widget` 节点，以及 Add Delegate 的 `then` 有没有接下去 |
| Note 弹了但看不见 | 被对话 UI 盖住（NarrativeCommonUI 层级高）。开之前把对话 widget 设 Hidden，关闭时恢复 |
| 关掉 Note 后对话点不动 | 第 3 节的 Delay + Set Input Mode 没做，或该用 `UI Only` |
| 事件 B 触发多次 | `Remove Delegate` 漏了，绑定累积了 |
| 事件 B 完全不触发 | Switch 接错引脚（要接 `Closed`）。在 HandleNoteClosed 上挂 Print String 打印 NewState 排查 |

## 7. 不要动的东西

- `AC_NotesComponent` 的 `ChangeInputModeOnClose` 函数在插件里，所有 Note 共用，改了插件更新会冲突。
- `Content/Inventory/InventoryXBased/` 下 `Notes/L0_Computer` 和 `Notes/L0/L0_Computer` 是两个重名文件，选资产时注意路径。
