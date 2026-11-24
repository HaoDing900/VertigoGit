# L3 Sewer：Boss 遭遇与 Monster Egg 击杀计数

## 维护位置

- `/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt`：`ReceiveSmartTriggerCall` 的 Name Switch 现在明确匹配 `L3BossEncounter`，不再使用 Default 分支触发 Boss。
- `L3BossEncounter` Event → `SewerEncounter` 组件的 `Start Boss Encounter`。如果以后增加战前对白，可把这个调用移到对白完成后。
- Boss 原框架的 `OnBossDeath` 信号 → `L3_AfterBossIsDefeated` Event，仅处理一次。后续对白、Sequence 等继续接在这个 Event 后。
- 地图 `/Game/ThirdPerson/Maps/L_SewerUnderApartment`：BPLM 的 `L3_Boss` 保留原 Boss 引用，补齐 `L3_BossTrigger` 为 ID 是 `L3BossEncounter` 的 `BPA_0SmartTrigger2`。
- Boss 蓝图 `/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer`：Boss Behavior 的 Combat Trigger 设为框架现有的 `Take Damage`，初始 Can Be Damaged 关闭；因此不会按距离/看到玩家自动开战，也不能在触发前被打死。触发后组件开启伤害，并调用框架 `SetCombatState(true)`，保留原攻击列表。

## BPLM 中读取进度

- `L3_MonsterEggKillCount`：主角击杀 Monster Egg 的累计数量。预放置的蛋及 Boss 新生成的蛋都会报告；非致死受击、重复伤害、Boss 清场均不增加数量。
- `L3_BossEncounterStarted`：已经启动 Boss 遭遇。
- `L3_BossDefeated`：Boss 已死亡且已通知后续 Event。
- `SewerEncounter` 组件的 `On Monster Egg Kill Count Changed(KillCount)`：需要在数量达到条件时触发剧情，可绑定这个事件。
- `L3_CountedEggIds`：内部去重记录，不需要手工维护。以上变量使用现有 BPLM 进度存档；预放置蛋的稳定 ID 可避免读档后重复计数。计数不负责额外改变蛋自身的存档/重生行为。
- 读档恢复已开战状态时继续战斗；恢复已击败状态时隐藏并关闭 Boss 碰撞，不重复播放击败 Event。

## 实现

- `Source/Vertigo/Public/Combat/VTGSewerEncounterComponent.h`
- `Source/Vertigo/Private/Combat/VTGSewerEncounterComponent.cpp`
- Egg 蓝图 `/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg` 仅增加 `PlayerEggKillReporter` 组件，沿用原有伤害、死亡动画和 ragdoll。
- 计数监听引擎 `OnTakeAnyDamage`（在现有 Point/Radial Damage 蓝图处理之后广播），只把致死一击归属主角或其武器/投射物的死亡计入。不修改主角蓝图。
- 修改前资产备份在 `Scripts/SewerEncounter/Backup`。

## 验证入口

`-run=VTGSewerEncounter`：只读资产检查及独立测试场景中的逻辑验证。`-Apply` 是资产安装工具，日常维护无需运行。

本次编译和验证通过（`build.log`、`verify.log`）：检查了保存后的地图引用及分支、触发前待机/触发后框架黑板 InCombat 开启、死亡通知、非致死伤害、主角/武器击杀、清场排除、重复通知、计数存档恢复和稳定 ID 去重。

完整关卡中的移动、战斗与后续演出仍需在编辑器 PIE 中走一遍；逻辑测试不能代替完整关卡试玩。
