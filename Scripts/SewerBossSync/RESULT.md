# L3 Sewer Boss 血条与范围攻击同步（2026-10-16）

## 修改与维护入口

### 血条

- Boss：`/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer` → `BossHealthBar` 组件 → `Caustics`。
- `CausticOverlayMaterial` 改为 `/Game/Characters/Monster/SeaMonsterBoss/FX/M_BossHealth_CausticHUD`；`CausticOverlayOpacity=0.65`。
- 新材质使用 `/Engine/EditorMaterials/WidgetMaterial_Caustic` 当前引用的同一张蓝色水纹，在项目内重建为 User Interface / Translucent 材质，添加缓慢 UV 流动和可调 `CausticTint`（默认白色，保留纹理原色）。原 Engine 材质不修改，也不作为运行时父材质引用。
- 材质参数：`CausticTexture` 纹理、`CausticTint` 颜色；Panner 的 Speed X/Y 调流动速度，TextureCoordinate 的 U/V Tiling 调纹理密度。组件的 Overlay Opacity 调整叠层整体透明度；材质 `Opacity` 由现有 Widget 初始化逻辑设为 0.65。
- 沿用 `UVTGBossHealthBar::NativePaint` 的剩余血量裁剪及原有名称、血量显示逻辑。
- 编辑器游戏测试时，可在控制台执行 `VTG.PreviewCombatHUD` 显示 68% 血量的预览条；此命令仅存在于编辑器构建，不修改关卡或存档。

### 范围毒气

- 动画：`/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_ToxicGas`。
- `VTG_AttackEffect`：0.60 秒开始，`Effect Play Rate=2.5`，`Effect Scale=1`，`Effect Reference Radius=223`，`Active Seconds=2.5`。
- `Notify_Damage`：由原 0.65–0.95 秒后移为 **1.10–1.40 秒**，仍使用 BossAI Toolkit 原伤害通知和区域技能，伤害倍率、中毒效果、持续时间及冷却不变。
- Boss 专用 Niagara：`/Game/Characters/Monster/SeaMonsterBoss/FX/NS_Boss_ToxicGas`，复制自原 `NS_PoisonCloud_Large`。去掉额外的 Emitter State → Loop Delay=1 秒；使用 Local Space，让实例缩放可以同时缩放粒子位置与尺寸。原始毒气和小怪效果不改。
- 攻击范围继续在 `DT_SeaMonster_Phases` → `SeaMonster_P1` → `Toxic Gas` → `Modifier.X` 调整（当前 700）。运行时读取当前区域技能 `Radius`，以 `Radius / EffectReferenceRadius` 缩放特效；以后只改这个半径，视觉尺寸会跟随。
- `EffectReferenceRadius=223` 是专用特效在 1.25 秒模拟年龄时的校准值。因为播放速度 2.5，伤害前 0.5 秒对应特效模拟 1.25 秒。离屏粒子测量得到 200 粒子，90% 粒子外缘半径约 223.4，因此将主要可见云团扩散到约 700 后才启用伤害。烟雾为随机软边，并非严格几何圆边。
- 若更换 Niagara 或改其发射器参数，需要重新校准 Reference Radius；若只改攻击半径，则不需要。

## 源码与验证

- `Source/Vertigo/Public/Combat/VTGCombatPresentation.h`、对应 cpp：通用攻击特效通知新增独立特效速度、尺寸及可选的区域半径匹配。默认值 1/1/0 保持其他特效行为。
- 编辑器工具：`VTGSewerBossSyncCommandlet.cpp/.h`；新增 MaterialEditor 编辑器模块依赖，用于项目 UI 材质构建。
- `build.log`：编译通过；`apply-tint.log`：蓝图编译、四项资产保存及通知检查通过；`material-complete.log`：最终材质保存；`verify-final.log`：独立进程重新读取同步配置通过。
- `sample5.log`：原毒气存在 1 秒额外发射延迟；`sample-boss.log`：专用毒气 0.25 秒已产生 200 粒子，并记录校准半径。
- 修改前蓝图和 Montage 备份在本目录 `Backup/`。维护时使用本次工具，避免重跑历史修复脚本将设置恢复为旧值。
- 实际游戏视口验证：`live-hud-ui.log`、`live-hud.png`，已确认水纹叠层可见且只覆盖剩余 68% 血量，空血部分没有水纹。单独控件的离屏预览会跳过材质，不能用 `hud-preview.png` 判断游戏中的最终显示。
- 此次完成编译、资产独立回读、粒子模拟测量和游戏视口血条验证；尚未完成 L3 Sewer 整场实战观感回归。
