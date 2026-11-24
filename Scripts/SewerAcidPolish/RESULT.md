# Sewer 酸液、受击混合与第三段锤击

## 酸液喷吐与命中

维护入口：`/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_AcidProjectile`。

- `AcidFlight.MuzzleEffect` → `/Game/Characters/Monster/SeaMonsterBoss/FX/NS_AcidSpit_Muzzle`：从原 `NS_AcidSpit_Burst` 复制，保留喷吐形状和运动；设置 Local Space、粒子寿命 0.45 秒、无额外发射延迟。原始共享 Niagara 不修改。
- 发射特效在实际弹丸发射时播放一次，使用 ProjectileMovement 的实际 Velocity 把特效的 **+Z 喷射轴**转向射击方向。它停留在发射位置，不随弹丸悬浮移动；最长 0.6 秒清理。
- 旧 `AcidVisual` 组件取消自动激活并清空 Asset，避免把喷吐爆发当作持续飞行特效。
- `VisibleAcidCore` 材质改为用户指定的 `/Game/Resource/VFX/BloodPack/_Commons/Materials/MI_BloodDirection_07_Green`。碰撞体、射速和射程不变。
- `AcidFlight.ImpactEffect` 改为 `/Game/Resource/VFX/BloodPack/Hit/Niagara/NS_BulletHit_GreenErosion`。只在主角的伤害通知中 DamageCauser 是这颗酸液弹时播放一次；其他攻击不会触发。单纯销毁、击墙或射程耗尽不再向主角播放绿色侵蚀。
- 源码：`VTGSewerBossPresentation.h/.cpp` 的 `UVTGAcidFlightComponent`。不改玩家蓝图的通用受伤效果。

## Boss 蓝血和受击动作

- `BP_Boss_SeaMonster_L3Sewer` → `CombatFeedback`：`HitEffect=NS_BulletHit_Cyan_Med1`，`HitEffectSocketName=spine_003`，`HitReactionMontage=AM_EggProducer_GotHit`。
- `AM_EggProducer_GotHit`：独立 `BossHitReactGroup / BossHitReact` Slot，Blend In=0.08、Blend Out=0.12 秒。
- `ABP_EggProducer_L3Anchored`：在原有 Local To Component / 根骨骼固定和瞄准修正之前，增加缓存基础姿势 + Hit Slot + Layered Blend per Bone，从 `spine_001` 开始混合上半身。
- `egg_producer_Skeleton` 仅增加上述 Slot 与 Group；不改变骨骼结构或参考姿势。
- `CombatFeedback` 在非致死掉血时播放受击 Montage（最短重触发间隔 0.2 秒），`Montage_Play(..., StopAllMontages=false)`，避免本次受击表现强行停掉其他 Montage。
- Boss 的阶段 HitReacts 和组件 HitReacts 旧引用清空，避免旧框架的整段受击播放与新混合层重复。其他敌人的默认受击流程不变。
- 源码：`VTGCombatPresentation.h/.cpp` 中 `UVTGEnemyFeedbackComponent`。

## 出招表

`DT_SeaMonster_Phases` → `SeaMonster_P1` → `Abilities` 移除 Headbutt 条目，保留其动画资产。

当前顺序：Toxic Gas、Spawn Eggs、AcidSpit。

## Sa 锤子第三段

- Player Sa 的现有锤子 ComboMontages 顺序：`Hammer_Attack_2Combo_1_Root_Montage`、`Hammer_Attack_2Combo_2_Root_Montage`、`Hammer_Attack_Infinite_1_Montage`。
- 仅修改 `/Game/Characters/Sa/Anm/Sa_Hammer/Combo/Hammer_Attack_Infinite_1_Montage` 的 **Rate Scale=1.3**，前两段仍为 1。
- 保留原动画、段落及 Notify 的动画时间位置，它们随 Montage 一起加速。没有修改或保存 BP_Player_Sa。

## 验证

- `build.log`：编译通过。
- `runtime.log`：实际方向向量与喷吐 +Z 对齐、只播放一次并留在发射点、限时清理、绿色侵蚀伤害来源隔离通过。
- 同一运行验证：Boss 非致死受击后，受击与攻击 Montage 同时处于活动状态，混合姿势正常求值。
- Sa 第三段 Montage 实际在 0.1 秒内推进 0.130 秒，确认运行时 1.3 倍速。
- `verify.log`：独立回读保存配置。
- 尚未完整试玩 L3 Sewer 整场战斗及最终画面观感。

修改前资产在本目录 `Backup/`。工具源码：`VTGSewerAcidPolishCommandlet.cpp/.h`；默认回读，`-Apply` 安装，`-Runtime` 运行验证。请勿重跑旧酸液/受击 Apply 工具覆盖本次分工与 Slot 配置。
