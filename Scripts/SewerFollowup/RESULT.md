# Sewer 召蛋、伤害、血条与开门检查（2026-10-17）

## 改动及维护位置

1. **召蛋靠近主角**：`BP_Ability_SpawnEggs` 的 On Execute 保留原有 SpawnCount 循环，改为调用 `Spawn Egg Near Target`。每次在主角周围约 200–350 cm 的环带随机取点，优先投射到导航，再检测地面；不要求精准围住玩家。无 NavMesh 时也可生成。数量仍为 3，冷却仍为 10 秒。
   - 实现：`Source/Vertigo/Private/Combat/VTGSmokeHazard.cpp` 的 `InitializeEggSpawner` / `SpawnEggNearTarget`。
   - 生成半径默认 350 cm、最小距离 200 cm；这两处在上述函数维护。
2. **召唤蛋按死亡来源处理**：生成时增加 `VTGBossSummonedEgg` Tag 和 `VTGBossSummonedEggComponent`。玩家或玩家拥有的武器造成致命伤时，保留原死亡动画与 ragdoll；自动死亡/清理时 Destroy Actor；召唤它的 Boss 死亡时直接 Destroy 仍活着的召唤蛋。玩家已击杀留下的尸体不会因为 Boss 后来死亡而被额外清掉。原本摆在关卡里的蛋没有该组件，继续原有死亡表现。
   - Toolkit 构造脚本可能覆盖 Owner，生成完成后重新明确 Owner / Instigator 为 Boss，避免依赖短命的技能 Actor。
   - 维护：`VTGSmokeHazard.cpp` 中 `UVTGBossSummonedEggComponent`。通过 OnTakeAnyDamage 确认致命伤的玩家 Controller / 武器 Owner 链；蛋的 OnBossDeath 在伤害处理内提前广播，因此销毁判定延到下一 Tick，等待该次伤害来源到齐。非致命的玩家攻击不会影响之后的自动清理。
   - 自动到期或剧情主动清理也可调用组件节点 `Despawn Summoned Egg`，直接 Destroy，无需走死亡演出。已有 Actor Destroy / LifeSpan 到期仍直接销毁，不修改它们的时间设置。
3. **烟雾命中**：原来的 Visibility 检查会把其他怪物的碰撞也当成遮挡；改为检查 WorldStatic / WorldDynamic 场景遮挡，忽略技能自身，生物的 Pawn 碰撞不再挡毒雾。墙和门等场景障碍仍然挡伤害。
   - 原来首次检测即使主角在范围外也会消耗首次伤害，晚进入只拿到较低持续伤害；现在通过空间检查后才消费首次伤害。
   - 实现：同文件 `AVTGSmokeHazard::Pulse`，Boss 和蛋的毒雾共用。
4. **八颗心与重复扣血**：确认是连续百分比，保留正确的 Filled / Hurt / Empty 图片。把现有等价计算集中为 `UVTGPlayerHearts::GetHeartFill`，绘制和回归使用同一入口。
   - `Source/Vertigo/Private/Combat/VTGCombatPresentation.cpp`。
   - 总血量 100 时，一颗心=12.5。100→96 时右侧最后一颗心应剩 68%；50/100 或 400/800 都是四颗满心。
   - 本次读取关卡 Sa 为 Health=100、MaxHealth_Normal=100，没有发现该实例血量超上限。
   - 实际回归发现 Sa 的 PointDamage 链先经过 DamageType 扣血，然后继续进入旧的 Set Health 再扣一次（传入 4，实际 100→92）。修复 `BP_Player_Sa` EventGraph 中 `K2Node_Knot_103` 的执行连线，跳过第二次 Set Health，直接进入其原有受击反馈 / 死亡判断；其他玩家逻辑不调整。修复后传入 4 应为 100→96。
5. **开门卡住**：ISX Collision 的 TRIGGER ID 是 `L3_OpenRoundDoor`，BPLM Switch on Name 只有 `L3_RoundDoorSwitch` 对应开门流程，ID 不匹配。因此 ISX 自己的锁镜头仍执行，BPLM 后面的开关动画、开门动画、对话没有进入。
   - 修改 `/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt` 的 Receive Smart Trigger Call 分支，增加 `L3_OpenRoundDoor`，接入原来的 `L3_RoundDoorSwitch → L3_OpenRoundDoor` 流程；保留旧 ID。
   - 原流程仍为：剧情脱战锁 true → 播开关动画 → Delay 1 秒 → 播开门动画 → Begin Dialogue（`L3Sewer_Still_DialogueNode_NPC_22`）。

## 当前攻击参数（未重新平衡伤害）

关卡 Boss 的 `BPC_Boss_Behavior`：AttackPower **10**，Health / MaxHealth **1000**。

| 招式 | 伤害 | 范围 / 距离（cm） | 持续与冷却 |
|---|---|---|---|
| Toxic Gas | 首次 4，之后每 0.5 秒 1 | 实际伤害半径 645.37；AI 可在目标距离 0–900 时选招 | 技能持续 4 秒；冷却 11.11 秒 |
| Spawn Eggs | 本招本身 0；生成 3 个蛋 | 可选招距离 0–1100；现在在主角周围约 200–350 生成 | 技能持续 3.5 秒；冷却 10 秒 |
| AcidSpit | 7.5（Sa 目前整数血量路径在满血时会显示扣 8） | 可选招距离 450–1800；弹速 850 cm/s，命中需碰撞到主角 | 技能持续 3 秒；冷却 7 秒 |

Headbutt 已不在当前出招表。

蛋：基础攻击力 10，Gas Burst 首次 2，后续倍率 0.15（计算伤害 0.3；Sa 整数血量会产生取整），半径 250，冷却 4 秒。

Boss 毒雾时间：招式开始后 1 秒出现特效，1.5 秒开始伤害，3.4 秒停止伤害，4 秒结束粒子。一直站在烟雾内且没有免疫时，通常是首次 4 + 三次持续 1，总计 7。实际命中取决于是否仍在范围内和计时帧。

以下情况不掉血是既有规则：在 645.37 实际半径之外（虽然 AI 选招范围到 900）、躲在墙后、离开烟雾、烟雾进入安全消散段、主角正在 Dodge 或 StoryMode。开关开启后的剧情脱战也会设置 StoryMode。

伤害和出招维护：`Content/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases` 的 `SeaMonster_P1` / `SeaMonsterEgg_P1`，以及对应 Boss / Egg 的 `BPC_Boss_Behavior → AttackPower`。毒雾时间在 `AM_EggProducer_ToxicGas` / `AM_SeaMonsterEgg_ReleaseGas` 的 VTGAttackEffectNotify 上。

## 验证与备份

资产修改前备份：`Scripts/SewerFollowup/Backup`。

编译：`build.log`；资产应用与 Blueprint 编译：`apply.log`；实际运行回归：`runtime.log`。

此前运行回归已通过：无导航生成三只蛋、生物不挡毒雾而墙体仍阻挡、首次与持续伤害为 4 / 1、晚进入仍获得首次伤害、消散停止伤害、真实 Sa 的 100→96、八颗心均分，以及实际 BPLM 收到 `L3_OpenRoundDoor` 后开始开关动画。

召唤蛋分类清理的编译和回归分别记录于 `summon-cleanup-build.log`、`summon-cleanup-runtime.log`。
分类回归已通过：玩家致命伤、玩家武器击杀保留正常死亡；自动死亡、先被玩家打伤再自动死亡、主动 Despawn 都销毁；Boss 死亡清理活召唤蛋并保留已完成玩家击杀的尸体；场景放置蛋不受影响。

检查导出：`abilities.json`、`map.txt`、相关蓝图 `.txt`。未完成整关人工 PIE 演出验收，仍需在正常击败 Boss 后检查开门对白与相机释放的完整体验。
