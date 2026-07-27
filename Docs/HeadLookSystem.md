# StillNPC 转头看目标（Head Look System）

> 2026-07 添加。让任意 StillNPC 子类把头转向场景中指定 Actor（BPLM 传入），可随时开关，带平滑过渡。

---

## 1. 如何使用

在 Level Blueprint / BPLM / 任意剧情事件里，拿到 NPC 引用，调用：

```
TurnOnHeadLook (Target: Actor)   → 开始看向 Target（场景里任意 Actor / Target Point / Player）
TurnOffHeadLook                  → 平滑回到原动画
```

- Target 会动也没关系，ABP 每帧取它的位置。
- 开/关都有 FInterp 平滑过渡，不会瞬间弹头。

---

## 2. 实现结构（三层）

### A. BPI_CineAnmInterface（Content/Characters/）

新增接口函数：

| 函数 | 输入 |
|---|---|
| `SetHeadLookTarget` | `Target`（Actor Object Reference） |

约定：**传有效 Actor = 开，传空 = 关**。开关状态不需要单独的 bool。

### B. ABP_StillNPC_Basic_EnmAISkele（Content/Characters/）

新变量（分类 HeadLook）：

| 变量 | 类型 | 用途 |
|---|---|---|
| `HeadLookTarget` | Actor | 当前目标，空 = 关 |
| `LookAtLocation` | Vector | 每帧刷新的目标位置 |
| `LookAtAlpha` | Float | LookAt 节点权重，0~1 平滑过渡 |

**Event Graph**：
- 实现接口事件 `SetHeadLookTarget` → 把 Target 存进 `HeadLookTarget`。
- `Event Blueprint Update Animation` 里追加（三个都要用 **Set** 节点，不是 `==` 判断节点）：
  - `IsValid(HeadLookTarget)` 有效 →
    - 算目标世界坐标：`HeadLookTarget.GetActorLocation()` +Z 偏移到头高（人形 ≈ +160）
    - **转 component 空间**：`Inverse Transform Location`（T = `GetOwningComponent → GetWorldTransform`，Location = 上面的世界坐标）→ `Set LookAtLocation`
    - `Set LookAtAlpha = FInterpTo(Current=LookAtAlpha, Target=1.0, DeltaTime=DeltaTimeX, InterpSpeed=3.0)`
  - 无效 → `Set LookAtAlpha = FInterpTo(Current=LookAtAlpha, Target=0.0, DeltaTime=DeltaTimeX, InterpSpeed=3.0)`

> ⚠️ **LookAtLocation 必须是 component 空间**。Look At 节点的 Look At Location 吃的是相对 Mesh 组件的坐标，直接存世界坐标（GetActorLocation）会指向错误位置。必须经 Inverse Transform Location 转一次。
> ⚠️ FInterpTo 的 DeltaTime 接事件的 **Delta Time X** 输出 pin；InterpSpeed 填 3（填 0 = 不插值 = alpha 卡死）。

**Anim Graph**：插在整条链最末端，`NSlot → Output Pose` 之间：`NSlot Pose → Look At（Component Pose）→ Root`。

Look At 是骨骼控制节点，**不需要手动加 Local To Component / Component To Local**，编译器自动插空间转换。

Look At 节点设置（Details 面板）：
| 项 | 值 |
|---|---|
| Bone to Modify | head（先确认 SK 骨架头骨名） |
| Look At Location | 暴露 pin，接 `LookAtLocation` 变量 |
| Alpha Input Type | Float，然后接 `LookAtAlpha` |
| Look at Clamp | ≈ 70°（防止转成贞子） |
| Look at Axis | 先试 X，头朝向不对就换 Y / 勾 Invert |

> 放最末端 = 头会覆盖一切（含 DefaultSlot 的击飞/死亡 Montage）。平时 HeadLookTarget 为空 alpha=0 无影响；若和死亡 Montage 打架，把 Look At 往前挪到 gameplay pose 段，或按 `Is Cutscene` 把 alpha 压 0。

### C. BP_StillNPC（parent，Content/Characters/）

两个 Custom Event（分类 HeadLook）：

- `TurnOnHeadLook(Target: Actor)` → `Mesh → GetAnimInstance → SetHeadLookTarget (Message)`，传 Target
- `TurnOffHeadLook` → 同上，Target 留空

和跟随系统一样走 BPI_CineAnmInterface 的 Message 调用，Drone 这种 Anim Class = None 的角色调了也不会报错（Message 静默跳过）。

---

## 3. 设计要点（改动前先读）

- **零 Tick 开销**：不在 BP_StillNPC 开 Tick，位置刷新放在 ABP 的 Update Animation 里（ABP 本来每帧就在跑）。不要把这个逻辑搬进 Actor Tick——Tick 是跟随系统专用的，默认关。
- **过场冲突**：如果 Sequence 期间头被 LookAt 抢戏，在 ABP Update 里加一条：`IsCutscene? == true` 时 alpha 目标强制 0（ABP 已经通过 `SetIsCutscene` 接口拿得到这个状态）。默认没加，遇到再说。
- **不要给 BeginPlay 加任何东西**：和跟随系统同理，这是独立入口。
- 用了其他 ABP 的 StillNPC 子类：把 B 步骤在那个 ABP 里照抄一遍即可（接口已经共用）。

## 4. 待办 / 未实现

- [ ] 过场期间自动压 alpha 到 0（见上）
- [ ] 眼睛骨骼跟随（目前只转头）
- [ ] 转头速度做成变量（目前写死 FInterp speed 3）
