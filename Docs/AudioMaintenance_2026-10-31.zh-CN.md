# 音频整理与本次调整

## NarrativeSFX

Response 确认音效仍为 `Content/SFX/N/NSFX_Response1–3`，与 Blip 分开管理。
三条素材平均长度约 1.32 秒，当前过渡为：792 ms 保持确认外观 → 396 ms 淡出 → 132 ms 留空 → 提交选择。
尾音自然结束；推进时只释放确认音自己的 Blip 优先标记。维护方法见 `DialogueResponseAudio.zh-CN.md`。

## Sa 脚步位置

使用中的 43 个资源已集中到 `Content/SFX/SaFootstep`：

| 子目录 | 数量 | 原位置 |
|---|---:|---|
| Cues | 9 | `Content/SFX/Footsteps_Volume_02/Cues` |
| Waves | 9 | `Content/SFX/Footsteps_Volume_02/WAV` |
| Surfaces | 24 | `Content/SFX/FootstepAndFoleySounds/Footsteps/MonoVersions/Wavs` |
| Settings | 1 | `Content/SFX/SewerAudio/SA_Footsteps` |

DefaultSteps 是 Boots 01–06 的六个 Cue；BarefootSteps 是 Naked Feet Deep 01、05、06 的三个 Cue。
Concrete / Metal / Water / Wood 各六条素材也一起移动。未改换鞋/赤脚与材质的优先级，也未给材质或其子实例重新分类；这部分待讨论。

## 下水道 Boss 与蛋

- 11 个使用中的 Sound Cue：`Content/Characters/Monster/SeaMonsterBoss/Audio/used`。
- 29 个被这些 Cue 引用的源音频：`Content/SFX/MonsterNCreatures/BST09_BEASTIARIUM_VENOMBRUTE/BST09_BEASTIARIUM_VENOMBRUTE_DESIGNED_KIT/used`。
- 保留原来的随机音源池与轻微 pitch 变化，没有复制出第二套音效。

通过 Unreal AssetTools 移动并更新资源引用。旧位置保留 Redirector；不要把它们当作重复音频手工删除。逐文件旧/新路径见 `Scripts/AudioMaintenance/AssetMoves.tsv`；修改前资源及直接引用者备份见 `Scripts/AudioMaintenance/Backup`。

Boss Toxic Gas：`DT_SeaMonster_Phases → SeaMonster_P1 → Abilities → Toxic Gas → DamageMultiplier` 从 0.4 改为 0.8。在 AttackPower=10 时，首次 8、后续每次 2；原间隔、作用范围和安全消散时间保持原值。蛋的 Gas Burst 与其他攻击未调整伤害。

## 挥锤与蛋伤害的状态问题

完整关卡检查发现 `InCombat=true` 与 `StoryMode=true` 会同时存在。原来的 CheckEnemyTimer 只在 InCombat 改变时调用 ToggleCombatMode，无法恢复单独残留的 StoryMode。StoryMode 会阻止 IA_Fire，并使 Sa 的 PointDamage 分支免疫伤害。

修复位于 `BP_Player_Sa → EventGraph → K2Node_IfThenElse_86`：当下水道战斗策略要求战斗、但 StoryMode 仍开着时，也走现有 ToggleCombatMode。策略仍由 ResolveCombatState 决定，保留剧情锁、cinematic、死亡和已击败 Boss 的保护；其他地图的判断保持原逻辑。维护入口为 `VTGSewerCombat -RepairState`，只读检查加 `-VerifyOnly`，回归加 `-Test`。

验证结果：三种地图命名/策略场景的回归均为 0 failures；真实关卡拾取对白结束后，真实左键成功启动 `Hammer_Attack_2Combo_1_Root_Montage`。离屏渲染下，真实召唤蛋的毒雾伤害日志确认 Sa 为 100→96，后续毒伤继续扣血。当前蛋的表内倍率为 0.35，本次保留该值。

关卡探查入口：自动化测试 `Vertigo.Sewer.LiveInputProbe`，自动快照并还原 SaveGames。该探查会在无动画通知时主动调用原有通知，且主动模拟 Niagara，因此用于验证伤害接入，不用于统计完整单次技能总伤害或验收动画观感。NullRHI 下 Niagara 不启动，不能用它验收烟雾伤害；应使用真实/离屏渲染。最近离屏日志为 `Saved/Logs/SewerDamageBlockTrace.log`。

上述探查的挥锤与扣血断言均通过，但整项自动化仍因已有 `/InventorySystemX/InventorySystemX/Textures/Tabs/T_Note.T_Note` 加载错误标为 Fail；该贴图问题未纳入本次修改。详细伤害诊断可临时使用 `-LogCmds="LogTemp Verbose"`，普通日志级别不会输出每次毒伤记录。

## 检查

- `VTGAudioMaintenance -VerifyOnly`：83 个资源位置和已序列化引用。
- `VTGCombatPolish -AudioVariations -VerifyOnly`：11 个怪物 Cue 的三音源与 pitch。
- `VTGUIFeedback -ResponseAudio -VerifyOnly`：实际选项按钮接线与蓝图编译。
- `VTGUIFeedback -ResponseTransitionTests`：24 项过渡回归。
- `VTGAudioMaintenance -ToxicGas`：只读输出当前毒雾倍率。

不要为验证运行旧维护命令的默认 Apply 流程，它们还会重设其他历史参数。
