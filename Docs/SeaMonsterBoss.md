# 下水道海怪 Boss（L_SewerUnderApartment）

基于 BossAIToolkit 搭建，没有改动任何 Toolkit 自带资产。

## 资产（`Content/Characters/Monster/SeaMonsterBoss/`）

| 资产 | 作用 |
|---|---|
| `BP_Boss_SeaMonster` | Boss 本体，父类 `BP_Boss_Base`，模型 `EggProducer` |
| `BP_SeaMonster_Egg` | 海怪蛋（被召唤的小怪），父类 `BP_Boss_Base`，模型 `Sea_Monster_Egg1` |
| `DT_SeaMonster_Phases` | 两行阶段数据：`SeaMonster_P1`（Boss）、`SeaMonsterEgg_P1`（蛋）。**招式数值都在这里调** |
| `BT_BossAI_Stationary` | 原地不动的行为树：沿用 Toolkit 的 `Try Activate Ability` 服务和 InCombat 判断，只转身朝向目标，没有 MoveTo / 绕圈 / EQS |
| `BP_Ability_SpawnEggs` | `BP_Ability_Summon` 的子类，只重写 `On Execute`（见下文"为什么"） |
| `ABP_EggProducer` / `ABP_SeaMonsterEgg` | 最简动画蓝图：idle 循环 → DefaultSlot → 输出，Toolkit 的 Montage 才能播 |
| `Montages/AM_*` | 招式 Montage，带 Toolkit 的 `Notify_Damage` / `Notify_Execute` 窗口 |
| `FX/P_SeaMonster_ToxicGas` / `P_SeaMonster_EggGas` | StarterContent `P_Smoke` 的绿色一次性版本 |

## 招式

| 招式 | Toolkit 能力 | 触发距离 | 说明 |
|---|---|---|---|
| 顶人 Headbutt | `BP_Ability_Hitbox`（tag `head`） | 0–4.2m | 头部两个碰撞球（`spine_006`/`spine_004`），伤害窗口 0.85–1.5s（采样头骨运动得出），击退 |
| 释放毒气 Toxic Gas | `BP_Ability_Area` + `BP_DamageType_Poison` | 0–9m | 以 Boss 为中心半径 7m，地面有预警圈，中毒持续 5 秒 |
| 召唤海怪蛋 Spawn Eggs | `BP_Ability_SpawnEggs` | 0–11m | 一次 3 颗，冷却 15 秒，生成在 Boss 前方可到达的地面上 |
| 蛋：毒气爆发 Gas Burst | `BP_Ability_Area` + Poison | 0–2.8m | 蛋只在玩家靠近时喷毒，60 血 |

- 原地不动：行为树没有移动任务，移动速度全 0；胶囊体在导航网格上挖空（NavArea_Null），蛋不会生成在 Boss 身下。
- 进战：`On Sight`（视野 30m，70°）。
- Boss 死亡时（`OnBossDeath`）场上所有蛋一起死。
- 血量 1000、攻击力 10（伤害 = 攻击力 × 表里的 DamageMultiplier）。

## 关卡摆放

`L_SewerUnderApartment` 隧道下方的深坑其实被一堵整面墙（`SM_Cube80`，X≈-3750）分成两间。玩家从东侧的竖井（≈ -2450, 2600）掉下来，所以 Boss 放在**东侧房间**靠西墙的位置，面朝竖井，相距约 10m。
新增 `NavMeshBounds_SeaMonsterLair`，只覆盖东侧房间（原有 NavMesh 只到 Z -438，召唤蛋需要导航网格）。

## 主角攻击打不到 Boss 的原因与修改

`BP_Player_Sa` 的拳头（`DealPunchDmg`）原来：
1. `Sphere Overlap Actors` 的 Class Filter 写死 `BP_AI_Base` —— 只有 AI Behavior System 的敌人会被找到；
2. 用 `Apply Damage`（普通 `DamageType`）—— 只触发 AnyDamage。Toolkit 的 Boss 只处理 Point/Radial Damage，并且要求伤害类型实现 `BI_DamageType`。

修改（不针对 Boss 特判）：
- 新增 C++ `UVTGCombatStatics::FindMeleeTargets`（`Source/Vertigo/.../Combat/VTGCombatStatics.*`）：范围内**会对伤害做出反应**的 Pawn（实现 VTGDamageable，或蓝图里处理了 Point/Any/Radial Damage），永远不返回调用者自己。两套 AI 都满足，无人机这类不处理伤害的 Pawn 不会被选中。
- `DealPunchDmg` 和软锁定 `FindNearestEnemy` 都换成 `FindMeleeTargets`；`FindNearestEnemy` 去掉了 Cast To BP_AI_Base，`CurrentTarget` 改为 Actor 类型（只用于 IsValid / GetActorLocation）。
- 伤害节点换成 `Apply Point Damage` + `BP_DamageType_Default`，和枪走同一条通道。`BP_AI_Base` 用 `UsePointDamage?` 二选一处理 Point/Any，不会重复扣血。

## 为什么要 `BP_Ability_SpawnEggs`

Toolkit 的 `BP_Ability_Summon` 用 `GetRandomLocationInNavigableRadius`：在 2D 半径内取**任意**导航网格点（实测把蛋刷到了 16m 高的上层走道），查询失败时直接刷在 Boss 身上。子类改用 `GetRandomReachablePointInRadius`（从 Boss 前方 3m 出发、与地面连通的点），失败就跳过。

另：召唤用 `Priority Enemy` 而不是 `Self Only` —— Toolkit 给 "Self" 打分是按缺失血量（75% 血以上为 0），`Self Only` 的召唤在满血时永远不会放。

## 验证

在真实关卡里用 `-game` 跑过一遍（玩家传送进房间、保持不死、记录 45 秒）：三个招式都放出、蛋喷毒、Boss 位移 0、`DealPunchDmg` 打 Boss 1000→990、Boss 死后蛋全部死亡。6 个改动/新增蓝图用 `CompileAllBlueprints` 编译 0 错误 0 警告。修改前的 `BP_Player_Sa` 和关卡备份在 `Saved/SeaMonsterBoss/`。

## HUD Boss 血条

屏幕底部居中、黑魂/如龙式：名字（Egg Mother）在上，细长血条在下。
- 被打时红条立刻掉，后面的浅黄"伤害残影"停 0.7 秒再追上来；一套连击的总伤害显示在右侧，停手 1.5 秒后消失。
- 进战淡入；Boss 死后空血条停 2.5 秒再淡出；脱战（Toolkit 重置 Boss）直接淡出。

| 部分 | 位置 |
|---|---|
| 逻辑（C++） | `Source/Vertigo/.../UI/VTGBossHealthBar.*`：残影、伤害数字、淡入淡出；时长参数可在 WBP 的 Class Defaults 里改 |
| 外观 | `Content/Widget/BossHealthBar/WBP_BossHealthBar`：按控件名绑定（`BossNameText` / `HealthBar` / `DamageTrailBar` / `DamageText`），颜色字体随便改 |
| 挂载 | `UVTGBossHealthBarComponent`（Boss 上的 `BossHealthBar` 组件）：填 Display Name 和 Bar Class。从同一 Actor 上带 `Health` / `MaxHealth` / `InCombat` 属性的组件读数据（即 Toolkit 的 `BPC_Boss_Behavior`），所以**其它 Toolkit Boss 加上这个组件就有同款血条** |

Toolkit 自带的 `WB_BossHealth` 只在它的 Demo 玩家里创建，`BP_Player_Sa` 不会用到，所以不会出现两条血条。

## 可以继续做的

- 毒气目前复用 `choking` 动画（和召唤同一个），有专门的喷气动画可以直接换 `AM_EggProducer_ToxicGas` 的片段。
- 二阶段：在 `DT_SeaMonster_Phases` 加一行，再把 Boss 的 `Phases` 数组加上切换条件即可。
