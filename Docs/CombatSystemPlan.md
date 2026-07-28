# VTG 战斗系统重构计划（Yakuza 0 风格）

> 状态：**① + ③ C++ 骨架已写**（`Source/Vertigo/{Public,Private}/Combat/`），②做了最小版，④已内含，⑤待做
> 目标：给 VTG 加 Yakuza 0 风格的 fight level

## 现状（已审阅的资产）

- `Content/ThirdPerson/Blueprints/BP_Player_Sa.uasset` — 玩家，EventGraph 从 y≈5000 铺到 12000
- `Content/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter.uasset` — 敌人
- 父类 `Content/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base.uasset`（第三方 AI Behavior System 插件）
- 相关：`BP_CollisionComponent`、`BP_AIBehaviorsComponent`、`BPC_StatusEffects`（BossAIToolkit）、`BI_DamageType`、`BPI_OnEnmDeath`

### 玩家现有战斗流程
```
Punch(CustomEvent)
  → 检查 IsInCineCutscene_LockInput → 检查 DuringPunch?
  → AnimPunchRandom: RandomInt(0,2) → SwitchInt → 三个 Set Anim_Punch（硬编码 Montage）
  → DuringPunch=true → DisableMovement → PlayMontage
  → NotifyBegin → DealPunchDmgWithFistRorL?
      → SphereTraceSingleForObjects(forearm_socket → hand_socket, R=40) ×2（左右手各一套复制图）
      → BreakHitResult → ApplyDamage(Punch Damage) → Niagara + Sound → HitStop(CustomTimeDilation)
  → montage 结束 → Delay 0.1 → SetMovementMode Walking → Delay 0.3 → DuringPunch=false
```

### 敌人现有战斗流程
```
MeleeCollisionComponent.OnCollisionEnabled → SwitchEnum(E_CollisionPart) → SetCollisions(socket 数组)
MeleeCollisionComponent.OnHit → Server_SpawnHitVFXandSound → ApplyDamageToActor(MeleeAttackDamage)

ReceivePointDamage / ReceiveAnyDamage
  → CurrentHealth -= Damage
  → <=0 ? OnEnemyDeath 派发 → DoOnce → Delay → DeadRagdoll → Deactivate 组件
          → BPI_OnEnmDeath.Dead?(ForCutscene) → 找 BPLM_L2StreetBarFight → PlayAfterFightSequence
  → 否则 GetHitDirection → Hitted() → UniHitMontage → HitStop+CamShake → LaunchCharacter 后退
```

---

## 核心缺陷

### 1. 玩家和敌人是两条互不认识的管线
| | 玩家 | 敌人 |
|---|---|---|
| 命中检测 | SphereTraceSingleForObjects | CollisionComponent + OnHit 委托 |
| 发伤害 | 原生 `ApplyDamage`（float） | `ApplyDamageToActor` → `BI_DamageType.ProcessDamage` |
| 顿帧 | CustomTimeDilation 0.1s | CustomTimeDilation 0.15s + CamShake |

同一件事两种写法。**顿帧双方不同步**，攻击信息量不一致。

### 2. 招式硬编码，不是数据
三个 `Set Anim_Punch` 节点 + 一个 `Punch Damage` 变量。加第 4 招要改蓝图；每招无法有独立的伤害/击退/硬直/取消窗口。

### 3. 没有状态机，只有散落的 bool
`DuringPunch?` `CanAttack?` `CanPunch?` `IsAiming` `IsDead?` `IsInCineCutscene_LockInput(NotWorkForMont)` `FightingBoss?` `Crowd Control State`
每个输入入口都在串 Branch（IA_Fire 已串 6 层）。死亡瞬间还能出拳、cutscene 里 timer 仍在跑。

### 4. Combo 手感缺件
- **输入缓冲缺失**：靠 `Delay 0.3` 解锁 `DuringPunch?`，玩家早按 0.05s 就被吞
- **取消窗口缺失**：应该是 AnimNotifyState，不是 Delay
- **攻击吸附 / 软锁定缺失**：敌人会 RInterp 转向玩家，玩家不会转向敌人
- **命中去重 + 多段 trace 缺失**：单帧 SphereTraceSingle 会穿透，且只能命中一人
- **受击分级缺失**：敌人只有一个 `Uni Hit Montage`，无轻/重/浮空/倒地区分

### 5. 零散坑
- 武器判定用**字符串比较** `EqualEqual_StrStr "SMG1_May"` → 改名即崩，应换 enum/GameplayTag
- 玩家 `Health` 是 **int**、敌人 `CurrentHealth` 是 **double**，玩家伤害走 `FTrunc` 截断
- 多个节点 `ErrorType=1` / orphaned pin：`OnChangeStates`、`ResetPhysics`、`Item.GetAmmo`、`Multicast_RotateTo`、`DealPunchDmgWithFistRorL?`、`PC_Game_VTG.SaLevel?` 的 cast
- `BP_Player_Sa` 单张 EventGraph 塞了 Health / Weapon / Fist / Aim / CrowdControl 全部逻辑

---

## 完全没有的系统（Yakuza 0 必需）

| 系统 | 说明 |
|---|---|
| Heat 槽 + Heat Action | 上下文终结技（按位置/敌人状态/场景物件判定），全游戏爽点 |
| 抓取 / 投掷 | 基础动词 |
| 场景武器（椅子、自行车） | 标志性玩法 |
| 击倒状态 + 踩踏追击 | 战斗节奏收尾 |
| 格挡 / 闪避 / 破防 | `OnHitReact` 有 Blocking 参数，但玩家**没有主动格挡输入** |
| 战斗风格切换 | 数据驱动架构的天然产物，现结构做不了 |
| 敌人围攻 token 系统 | 否则多敌同时扑上必死 |
| Encounter 管理 | 圈定区域、BGM、相机拉远、结束判定 |
| 战斗相机 | 自动拉远、锁定构图 |
| Combo counter / 伤害数字 UI | |

---

## 重构方案（按顺序，每步可单独跑通）

### ① `UVTGCombatComponent`（C++）
把 Punch 整坨从 Player EventGraph 挪出来。玩家和敌人**共用同一个组件**，攻击流程只留一份。

### ② `UVTGAttackDataAsset`
一招一个资产：
```
Montage / Damage / HitReactType / LaunchVector / HitStopDuration
ComboWindowStart-End / NextAttacks[] / bCanCancelInto / HeatCost
```
Combo = 数组。加招 = 加资产，不碰蓝图。

### ③ 统一伤害结构 + 接口
```cpp
USTRUCT(BlueprintType)
struct FVTGHitEvent {
    float Damage;
    EVTGHitReact ReactType;
    FVector Launch;
    float HitStop;
    bool bBlockable;
    AActor* Instigator;
};

// IVTGDamageable
// 注意：叫 ReceiveCombatHit，不能叫 ReceiveHit —— 那个名字被 AActor::ReceiveHit（Event Hit 节点）占了，
// 蓝图实现接口时会报 "Cannot override ... declared in a parent with a different signature"
virtual void ReceiveCombatHit(const FVTGHitEvent& Hit);
```
玩家、敌人、场景物件全走这一条。**顿帧由攻击方统一驱动双方**。

### ④ `EVTGCombatState` + `CanDoAction(EVTGAction)` 单一 gate 函数
所有 bool 收进去（Idle / Attacking / HitStun / Blocking / Dodging / Grabbing / Downed）。
Punch / Fire / Aim / Dodge 入口只调一次这个函数，不再串 Branch。

### ⑤ ~~`ANS_Melee_Trace`（AnimNotifyState）~~ → 复用已有的 Montage Notify Window
**不用写新 notify。** 三个 Sa 拳头 montage 上已经挂了 BossAIToolkit 的 `Notify_Damage`，它的父类是
`UAnimNotify_PlayMontageNotifyWindow` —— 这种 notify 会广播 `UAnimInstance::OnPlayMontageNotifyBegin/End`，
C++ 直接绑就行（玩家蓝图里 Play Montage 节点那两个引脚就是这个委托的皮肤）。

组件在 `BindMontageNotifies()` 里订阅，Begin → `BeginMeleeTrace`，End → `EndMeleeTrace`；
连续 sweep 由组件**自己的 tick** 驱动（只在 hitbox 开着的那几帧开 tick），不需要 NotifyTick。
socket 和半径从攻击资产读，不放 notify 上。

> ①+③ 做完后，Heat / 抓取 / 场景武器都变成"往数据资产加东西"，不再动核心逻辑。

---

## 已写的 C++（骨架）

| 文件 | 内容 |
|---|---|
| `Public/Combat/VTGCombatTypes.h` | `EVTGHitReact` / `EVTGCombatState` / `EVTGCombatAction` / `FVTGHitEvent`（③） |
| `Public/Combat/VTGDamageable.h` + `Private/.../VTGDamageable.cpp` | `IVTGDamageable`：`ReceiveCombatHit` / `IsAlive` / `CanBeHitBy` / `GetCombatFocusLocation`（③）。默认实现自动转发给 actor 上的 CombatComponent —— 加接口 + 加组件，零节点就能挨打 |
| `Public/Combat/VTGAttackData.h` | `UVTGAttackDataAsset`（②的最小版，①没它没法跑） |
| `Public/Combat/VTGCombatComponent.h` + `Private/.../VTGCombatComponent.cpp` | ①：状态机 + `CanDoAction`（④）、输入缓冲、combo 链、连续 sweep + 去重的近战 trace、受击/格挡/死亡、双方同步顿帧、软锁定转向 |

设计要点：
- **顿帧由攻击方一次算好，同时打在双方身上**（`ApplyHitStop` 是 static），不再两边各写一套。
- **VFX/SFX 全留在蓝图**，挂 `OnHitLanded` / `OnHitReceived` / `OnDeath` 委托 —— C++ 模块不引 Niagara 依赖，特效仍归美术调。
- **`HandleLegacyDamage(float, Causer)`** 是迁移桥：老的 `ReceiveAnyDamage` / AI 插件的 float 伤害包成 `FVTGHitEvent`，新老可以并存着改。
- **`bUseTimedComboWindow`** 是 combo 窗口的临时 timer 版，等 ⑤ 的 AnimNotifyState 做好就把它关掉。

## 项目里已有的 notify 机制（查过了，别重复造）

| 资产 | 父类 | 谁在用 |
|---|---|---|
| `Notify_Damage`（BossAIToolkit） | `AnimNotify_PlayMontageNotifyWindow` | **挂在三个 Sa 拳头 montage 上**，玩家伤害窗口就是它 |
| `Notify_Sa_DodgeInvincible` | 同上 | 玩家闪避无敌帧 |
| `ANS_HitCollision`（AI 插件） | `AnimNotifyState` | 敌人，带 `E_CollisionPart` 参数开关 `BP_CollisionComponent` |

**坑**：那三个 montage 上 `Notify_Damage` 的 **Name 字段是空的**，广播出来是 `None`。所以组件的
`DamageNotifyName` 默认就是 `None` = "攻击中任何 notify 窗口都算伤害窗口"。等哪天真填了名字再改这个默认值。

## 下一步

1. **建 3 个 `UVTGAttackDataAsset`**，对上现有那三个 montage：
   `ANM_Sa_FightFist02_2b_Inplace1_Montage` / `ANM_Sa_FightFist04_1_Inplace1_Montage` / `ANM_Sa_FightFist09_Inplace_Montage`。
   填 `TraceSockets`（原来是 forearm→hand socket）、`TraceRadius=40`、伤害，然后用 `NextAttacks` 串成 1→2→3。
2. **接蓝图**：`BP_Player_Sa` 和 `BP_Enm_BarFighter` 加 `VTGCombatComponent` + `IVTGDamageable` 接口，
   输入入口改成 `TryAction(Attack)`，VFX/SFX 挪到 `OnHitLanded` / `OnHitReceived` 委托上，
   然后删掉旧的 Punch / 伤害那坨图。
3. 手感调完再考虑：往 montage 上加一个名叫 `Combo` 的 notify window，把 `bUseTimedComboWindow` 关掉。

构建方式见 memory `build-vertigo-cpp.md`（引擎在 `G:\Epic\UE_5.3`，工程 `Vertigo.uproject`）。
