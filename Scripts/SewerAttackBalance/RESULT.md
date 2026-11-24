# Sewer 攻击微调（2026-10-17）

## 本次修改

- `Content/Characters/Monster/SeaMonsterBoss/Montages/AM_SeaMonsterEgg_ReleaseGas.uasset`：`Notify_Damage` 窗口从 0.85–1.15 秒改为 **1.35–1.65 秒**，实际伤害延后 0.5 秒。音效与攻击 VFX 仍在 0.8 秒开始；保留伤害窗口长度 0.3 秒、原伤害/中毒参数和 Rate Scale=1。
- `Content/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases.uasset` → `SeaMonster_P1` → `Abilities` → `Toxic Gas` → `Modifier.X`：700 → **645.368112**。按水平圆形攻击面积缩小 15% 计算，半径乘 sqrt(0.85)，面积保留 85%。
- 毒气 VFX 沿用现有按区域技能 Radius 自动缩放的逻辑，因此跟随伤害范围缩小。原 Boss 毒气伤害窗口 1.10–1.40 秒保持不变。

## 在 Unreal 中查看 Boss 攻击

打开 `/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases`，选择 **SeaMonster_P1** 行，展开 **Abilities**。

| Index | AbilityName | 用途 | Cooldown |
|---|---|---|---|
| 0 | Headbutt | 近距离头槌，命中造成击退 | 3 秒 |
| 1 | Toxic Gas | 周围范围毒气，命中附带中毒 | 10 秒 |
| 2 | Spawn Eggs | 召唤海怪蛋 | 15 秒 |
| 3 | AcidSpit | 远程发射酸液投射物 | 7 秒 |

- `Damage`：伤害倍率、伤害类型、中毒与击退等。
- `Cooldown`：该技能的冷却时间；不等于 Boss 每次攻击之间的固定间隔。
- `Targeting.MinDistance/MaxDistance`：选择技能时对目标距离的条件。**Toxic Gas 的实际伤害半径是 Modifier.X**，不要把 MaxDistance 当成毒气半径。
- `Visuals.AnimMontage`：对应动画，打开 Montage 后查看 Notify 轨道即可检查伤害和特效时机。
- `SeaMonsterEgg_P1` 行是海怪蛋的 Gas Burst 技能，不是 Boss 的第二阶段。
- Boss 使用的阶段/技能表入口：`BP_Boss_SeaMonster_L3Sewer` → `Boss Behavior` 组件 → `Phases Table` 与 `Phases`。

## 验证与备份

编译通过；独立命令进程回读检查：`-run=VTGSewerAttackBalance -Verify`。表格前后对比仅改变 Toxic Gas 的 Modifier.X。完整战斗观感尚未在 PIE 中试玩。

备份在本目录 `Backup/`；工具源码为 `Source/VertigoEditor/Private/VTGSewerAttackBalanceCommandlet.cpp/.h`。日常调参直接使用上述资产；无需重跑历史 Apply 工具。
