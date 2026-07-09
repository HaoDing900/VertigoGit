# StillNPC 跟随系统（Follow System）

> 2026-07 添加。让任意 StillNPC 子类（Drone、Bolt、Sa、FemaleClient、Philip 等）可以被剧情事件开启"跟随主角 / 跟随指定 Actor"。

---

## 1. 如何使用

### 让 NPC 跟随主角

在 Level Blueprint / 任意剧情事件里，拿到 NPC 的引用，调用：

```
Start Following Player(Connect Pawn)
```

- **New Target Character 留空** → 默认跟随 Player Pawn（主角）。
- **New Target Character 接任意 Actor** → 跟随那个 Actor。想让 NPC 走去一个固定地点，就在关卡里放一个 Target Point（游戏内不可见），把它接进来。

NPC 会以 `FollowSpeed` 的速度走向目标，距离小于 `FollowDistance` 时自动停下，目标再走远会继续追。过场期间（`IsCutscene?(ForABP)` = true）自动暂停跟随，不会和 Sequence 抢控制权。

### 每个角色可调的参数（变量默认值，子类各自覆盖）

| 变量 | 含义 | 参考值 |
|---|---|---|
| `FollowSpeed` | 跟随移动速度 | 200 |
| `FollowDistance` | 贴近到多少距离停下 | 250 |
| `FacingYawOffsetForFollow` | 朝向补偿角。模型没按 UE 惯例朝 X+ 的角色（如 Drone）填 90/-90/180 | 0 |

### 停止跟随

把 `FollowCharacterTarget` 清空（Set 节点输入留空）。
⚠️ 目前没有封装好的 `StopMoving` 事件（见第 2 部分待办）。

---

## 2. 维护文档

### 动过的蓝图

**BP_StillNPC（parent，Content/Characters/）— 全部核心逻辑在这里**

| 项目 | 说明 |
|---|---|
| 新变量 | `FollowCharacterTarget`（Actor）、`FollowSpeed`、`FollowDistance`、`FacingYawOffsetForFollow`（均在 Follow 分类下） |
| 新事件 `Start Following Player(Connect Pawn)` | 入参 IsValid 判断：有效存入 FollowCharacterTarget，无效则取 Player Pawn。然后设 CharMoveComp 的 MaxWalkSpeed = FollowSpeed，最后 `Set Actor Tick Enabled(true)` 打开 Tick |
| 新函数 `MoveTowardTarget(MoveTarget: Actor)` | "腿"：算方向（目标位置 − 自身位置，Z 清零，Normalize）→ AddMovementInput；再用 FindLookAtRotation 的 Yaw + FacingYawOffsetForFollow → MakeRotator → RInterpTo(速度5) → SetActorRotation 做平滑转身。**子类可 override**（Drone 飞行版计划走这里） |
| Event Tick | "大脑"：IsValid(FollowCharacterTarget) → 不在过场 → 距离 > FollowDistance → 调 MoveTowardTarget。**Class Defaults 里 Start with Tick Enabled = false**，只有跟随开启时 Tick 才跑，平时零开销 |
| Capsule 碰撞 | Pawn 通道 Block → **Overlap**（防止主角把跟随中的 NPC 顶飞；NPC 对场景碰撞不变） |

**BP_Drone_IPHA_StillNPC（Content/Characters/Drone_IPHA/）**

- `FacingYawOffsetForFollow` 设为非 0（Drone 模型未按惯例转 -90°，不能改 Mesh 组件旋转——会破坏已绑定的 Level Sequence，所以用这个变量在运行时补偿）。

### 设计要点（改动前先读）

- **移动与动画解耦**：parent 只负责位移，各角色 ABP 自己根据速度切动画。Drone 无 ABP（Anim Class = None），所以零动画工作量。
- **转向没有用 Orient Rotation to Movement**，而是在 MoveTowardTarget 里手动 SetActorRotation——就是为了能加 FacingYawOffset 补偿。不要顺手把 Orient Rotation 勾回来，会和手动旋转打架导致抖动。
- **不要给 BeginPlay 加跟随逻辑**：BeginPlay 已有 DestroyIfNotOnStage → Outline → Cutscene → 交互 Timer 一整串，跟随是独立入口。
- MoveTowardTarget 内部自带 IsValid 保险，空目标调用会静默跳过，不报 Accessed None。

### 待办 / 未实现

- [ ] `StopMoving` 封装事件（清目标 + `Set Actor Tick Enabled(false)`，把 Tick 关回去省性能）
- [ ] `GoToLocationTarget` 第二目标变量（"主动前往某地点，到达后自动停"模式，与跟随共存、优先级更高）——当前用 Target Point 接进跟随事件可以凑合，但到达后不会自动停
- [ ] Drone 的飞行版 `MoveTowardTarget` override（Movement Mode = Flying，方向不清 Z，目标点加悬浮高度偏移）——当前 Drone 是贴地走的
- [ ] 推广到其他地面角色时：检查各自 ABP 是否有"速度 > 0 播走路动画"的状态切换
