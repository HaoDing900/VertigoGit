# L3 Sewer 海怪蛋与 Eyed Broodnest 特效 / 酸液攻击

2026-10-14 已保存本地资产，并完成 C++ / Blueprint 编译与独立进程回归。针对既有 BossAI Toolkit 实现调整，没有修改示例 boss 或通用 Toolkit 资产。

## 特效与修改位置

| 用途 | 当前资产 | 在哪里修改 |
| --- | --- | --- |
| Sea Monster Egg 被打碎 | NS_BrainBurst_Cyan | BP_SeaMonster_Egg -> CombatFeedback 组件 -> Death Effect |
| 蛋 / Boss 受伤但未死亡 | NS_BulletHit_Cyan_Med1 | 各 Blueprint -> CombatFeedback -> Hit Effect |
| 蛋攻击毒云 | NS_PoisonCloud_Mid | Montages/AM_SeaMonsterEgg_ReleaseGas -> VTG_AttackEffect Notify -> Effect |
| Boss 喷酸起手 | NS_AcidSpit_Burst | Montages/AM_EggProducer_AcidSpit -> VTG_AttackEffect Notify -> Effect |
| 飞行酸液 | NS_AcidSpit_Burst | BP_SeaMonster_AcidProjectile -> AcidVisual Niagara 组件 -> Niagara System |

资产根目录：Content/Characters/Monster/SeaMonsterBoss。

项目实际血液资产拼写是 NS_BulletHit_Cyan_Med1（/Game/Resource/VFX/BloodPack/Hit/Niagara），不存在用户描述的 Mid1；使用现有 Med1。爆裂资产在 /Game/Resource/VFX/BloodPack/BrainBurst/Niagara。攻击 Niagara 在 /Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud 和 VFX_AcidSpit。

蛋毒云原本按体型选 Small/Mid/Large；本次固定 Effect=Mid，清空 MediumEffect/LargeEffect 覆盖，使任何体型的蛋都使用指定 Mid。毒云在 montage 0.8 秒触发，1.7 秒后停发射；清理仍由既有原生通知负责。受击、死亡沿用已有 CombatFeedback 的健康变化/销毁回调及一次性死亡保护。

## 远程投射攻击

沿用 BP_Ability_Projectile、BP_Projectile_Base 派生出的 BP_SeaMonster_AcidProjectile。Boss 的既有 AcidSpit 条目已存在，本次更新该条目，未重复增加同名能力。

DT_SeaMonster_Phases -> SeaMonster_P1 -> Abilities -> AcidSpit：

- AbilityClass：BossAI Toolkit BP_Ability_Projectile。
- SpawnClass：BP_SeaMonster_AcidProjectile。
- AnimMontage：AM_EggProducer_AcidSpit；发射由原 Toolkit damage notify 触发。
- 发射点：spine_006；数量 1。
- Targeting：450–1800 cm；Duration 3 秒；Cooldown 7 秒。
- DamageMultiplier：0.75，总伤害仍由 BossAI AttackPower / GetFinalDamage 计算。
- ProjectileMovement：Initial/Max Speed 850 cm/s，Gravity Scale 0.12，生命周期 5 秒。
- 隐藏了复制模板中的占位球网格；碰撞保留，Niagara 负责酸液外观。
- 命中由原框架 ApplyPointDamage 处理；DamagedActors 列表防止同一投射物重复扣血，命中后销毁。不增加 Niagara 粒子自身的第二套伤害。

## 临时音效选择与替换

Content/SFX/MonsterNCreatures 中是 SoundWave，所以本次将“攻击、受伤临时特效”按音效处理。选择同一 VENOMBRUTE 套装中对应攻击/呼吸/受伤用途的声音，不随机打乱类别。

SoundWave 根目录：Content/SFX/MonsterNCreatures/BST09_BEASTIARIUM_VENOMBRUTE/BST09_BEASTIARIUM_VENOMBRUTE_DESIGNED_KIT。以下文件名统一带 PS_DK_BST09_BEASTIARIUM__ 前缀。

| Sound Cue（SeaMonsterBoss/Audio） | 当前 SoundWave 后缀 |
| --- | --- |
| SC_EggMother_Roar | attack_long_03 |
| SC_EggMother_Swing | attack_medium_03 |
| SC_EggMother_Slam | attack_medium_04 |
| SC_EggMother_Gas | breath_long_02 |
| SC_EggMother_Hurt | hit_03 |
| 新建 SC_EggMother_AcidSpit | attack_long_04 |
| SC_SeaEgg_Gas | attack_short_04（Pitch 1.2） |
| SC_SeaEgg_Hurt | hit_05（Pitch 1.2） |

替换方法：打开上述 Sound Cue，修改 Wave Player 节点里的 Sound Wave。原 Cue 的空间衰减与图结构保留。喷酸 Montage 的 PlaySound 通知现指向独立 SC_EggMother_AcidSpit，以免以后更换毒云音效时连带改变喷酸声音。

## 验证与限制

- 构建 VertigoEditor Win64 Development 成功。
- 保存后从磁盘加载 Egg、Boss、Projectile，重新编译并检查 Niagara 引用、单一 Mid 通知、酸液能力、距离及正伤害倍率。
- 真实 BossAI BP_Ability_Projectile 的 OnBeginDamage 生成恰好一颗酸液投射物，存在实际前向速度。
- 实际碰撞回调对通用目标造成伤害，命中后投射物销毁。
- 对实际 BP_Player_Sa 的碰撞会使 Health 下降，重复 overlap 不再次扣血。
- 既有出拳缓冲 / montage 窗口回归也通过。
- 最初真实 Pawn 扣血检查失败的原因是隔离世界缺少 authority GameMode，Unreal APawn::ShouldTakeDamage 会拒绝所有伤害；补全测试环境后通过，没有为此绕过真实玩家的伤害处理。
- 验证为 null RHI 隔离回归；未完成完整 L3 PIE 战斗、画面大小/位置、实际 AI 选招频率及音量试听。因此不把数据接线验证当作视觉/音频成品验收。

日志：build.log、apply.log、verify.log。修改前资产备份：Backup/。
