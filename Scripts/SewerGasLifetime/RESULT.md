# Sewer：烟雾伤害、召唤蛋和酸液可见性

## 烟雾时序与伤害

改动位置：`/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_SeaMonsterEgg_ReleaseGas` 与 `AM_EggProducer_ToxicGas`。

两条 Montage 的旧 `Notify_Damage` 已移除，避免旧的一次范围伤害/中毒与新烟雾重复扣血。现在由 `VTG_AttackEffect` 的 **Smoke Damage** 参数创建烟雾实体；烟雾在空间范围内周期调用原 BossAI Toolkit 的伤害接口。离开范围、被墙隔开、烟雾进入安全消散阶段或攻击者死亡后不再造成伤害。普通命中反馈和玩家伤害处理仍沿用原实现。

| 动画内时间 | 海怪蛋 | Boss 毒气 |
|---|---:|---:|
| 特效开始 | 0.80 秒 | 1.00 秒 |
| 首次伤害 | 1.10 秒 | 1.50 秒 |
| 停止伤害 | 3.20 秒 | 3.40 秒 |
| 粒子生命周期结束 | 3.80 秒 | 4.00 秒 |

- 蛋最初的基准判定为 0.85 秒；现在延迟 0.25 秒至 1.10 秒。
- 烟雾存在 3 秒，最后 0.6 秒安全消散。粒子的寿命与伤害共用这套配置；若特效意外提前结束，伤害也立即停止。
- 首次伤害保留原技能伤害倍率；之后每 0.5 秒检查一次玩家位置。蛋的后续每跳是首次倍率的 15%，Boss 是 25%，避免每跳重复全额爆发伤害。
- `DT_SeaMonster_Phases` 中这两个技能的 Damage Duration/PerTickMultiplier 清零、DamageType 改为框架 Default，移除脱离烟雾后仍持续 4–5 秒的旧中毒状态。
- Boss 的 `ForceShowTelegraph` 开启，从技能开始警示，到第 1 秒喷出毒气，第 1.5 秒开始伤害；原伤害半径 645.368112 不变。Cooldown 由 10 秒改成 10/0.9 ≈ 11.111 秒，冷却频率降低 10%；技能 Duration=4 秒，覆盖整团烟雾的生命周期。

### 维护烟雾

- Montage → `VTG_AttackEffect` → Smoke Damage：`SmokeLifetime`、`SmokeDamageDelay`、`SmokeSafeFadeSeconds`、`SmokeTickInterval`、`SmokeTickMultiplier`。
- 独立 Niagara：`/Game/Characters/Monster/SeaMonsterBoss/FX/NS_Egg_ToxicGas_Lifetime`、`NS_Boss_ToxicGas_Lifetime`。原共享 Niagara 未修改。
- 粒子 Initialize Particle → Lifetime Min/Max 均设置成相同值，去掉额外 Loop Delay。蛋是 3 秒；Boss 因 `EffectPlayRate=2.5`，模拟寿命为 7.5 秒，对应现实游戏时间 3 秒。以后改变时长时，应同时维护 **Niagara 寿命 = SmokeLifetime × EffectPlayRate**。更换 Niagara 时也要重新校准 Boss 的 EffectReferenceRadius。
- Runtime：`VTGSmokeHazard.h/.cpp`。独立的 Toolkit 伤害代理复制本次攻击配置，保证原动画/技能结束后，剩余烟雾仍能持续工作；结束时一并清理。

## Spawn Eggs 不出现的原因与修复

`BP_Ability_SpawnEggs` 原先 `SpawnCount=0`，BeginPlay/父初始化未启用，也没有把技能表 `Visuals.SpawnClass` 赋给实际生成类。

- 启用父 BeginPlay，增加 `InitializeEggSpawner`：读取 Modifier.X（当前为 3）、设置 SpawnRadius=600、复制 SpawnClass。
- 保留原 OnExecute 的生成流程和 Montage 中现有 `Notify_Execute`。
- 原流程完全依赖 NavMesh；现在 Nav 查询失败时，在附近地面射线命中点尝试生成，避免没有 NavMesh 就完全不生成。没有地面也不会盲目生成。
- 修改数量：`DT_SeaMonster_Phases` → `SeaMonster_P1` → Spawn Eggs → `Modifier.X`。

## 酸液弹丸

位置：`BP_SeaMonster_AcidProjectile`。

- `AcidVisual`：相对缩放 0.3 → 1.2。
- 新增 `VisibleAcidCore`：24 cm 发光绿色球体，材质 `/Game/Characters/Monster/SeaMonsterBoss/FX/M_AcidProjectile_Visible`，无碰撞。
- `AcidFlight`：`ImpactEffect=NS_AcidSpit_Burst`、`ImpactScale=1.5`。命中销毁时在碰撞位置播放独立酸液飞溅；射程耗尽时不播放假命中特效。
- 未扩大命中碰撞、未改变速度和射程。维护入口：组件 AcidVisual、VisibleAcidCore、AcidFlight。

备份位于本目录 `Backup/`。工具：`VTGSewerGasLifetimeCommandlet`（`-Apply` 安装、默认回读、`-Runtime` 运行逻辑验证）；不要重跑旧的范围/特效 Apply 工具覆盖此次时序。

## 验证

- `build.log`：编译通过。
- `runtime-rhi.log`：真实 Niagara 粒子模拟与框架伤害调用通过。蛋与 Boss 的首次延迟、离开/重入、原 Ability 销毁后继续工作、最后 0.6 秒安全期均通过；安全期仍存在 200 个烟雾粒子，未再造成伤害。
- 同一运行验证中，调用实际 `OnExecute` 在没有 NavMesh 的测试地面生成 **3 只蛋**。
- 弹丸验证通过：发光核心跟随弹丸位置，命中销毁会生成酸液飞溅，射程耗尽不会产生假命中飞溅。
- `verify.log`：独立进程回读资产配置。
- 尚未完整试玩 L3 Sewer 整场战斗及最终视觉观感。
