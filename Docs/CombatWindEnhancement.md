# 更明显的拳风与锤头风效

## 已调整

- `NS_fist_Wind`：粒子颜色 Alpha 从 0.03–0.13 提高为 **0.18–0.45**，颜色亮度提高约 15%，更快显现并延长中段可见时间，仍在寿命末段淡出。玩家拳风缩放从 1 改为 **1.25**。
- 新增独立 `NS_hammer_Wind`，Alpha 为 **0.22–0.50**，锤头风效世界缩放 **1.4**。原始环境风 `NS_Wind_Flows_02_Fast` 和共用材质保持原样。
- 两种战斗风仍使用单个本地空间发射器、原有 30 生成倍率、6 个 Burst、0.10–0.18 秒寿命和零预热。增强可见度不靠提高粒子生成密度。

## 锤子挂点和触发

风效附着在 Sa 的实际 `MeleeWeapon` Static Mesh Component 上，跟随锤子的位置和朝向。

当前锤子 `SM_Prop_Hammer` 的较窄锤头端面位于模型局部 Y≈-20.37、Z≈55.62；风效挂点设为 **(0, -20.4, 55.62)**，局部方向朝 **-Y**。没有改动锤子模型、材质或共享 Socket。

只接受 WeaponDataMap 中目前三个锤子攻击 Montage：

| Montage | 原有 Notify_Damage 窗口 |
|---|---|
| Hammer_Attack_2Combo_1_Root_Montage | 0.428–0.683 秒 |
| Hammer_Attack_2Combo_2_Root_Montage | 0.306–0.650 秒 |
| Hammer_Attack_Infinite_1_Montage | 0.966–1.314 秒 |

视觉比窗口提前 0.08 秒出现，延后 0.05 秒结束。收招等待阶段不会持续显示。还要求 combat、存活、IsAttacking、实际装备并显示对应锤子，及该 Montage 正在播放。

每帧复用当前风效组件。停止挥击、中断 Montage、切换武器、死亡或退出 combat 后立即停用并销毁；退出 combat 后没有保留的 Niagara 风效组件。锤子使用一份风效，拳头使用两份。

## 调整入口

`BP_Player_Sa → Components → FistWindAndRecovery`：

- **Fist Wind → Wind Scale**：拳风大小。
- **Hammer Wind**：锤头风效资源、模型、Montage 列表、挂点位置、方向和大小。

透明度在两个 Niagara 资产的 User Parameters → **Color Minimum / Color Maximum 的 Alpha** 调整。

## 验证

Editor Development 编译通过，玩家蓝图编译为 0 错误、0 警告。独立进程重新加载后，18 项增强风效检查通过，覆盖三招挥击窗口、锤头附着与方向、组件复用、长收招不显示、中断、切换拳头和退出 combat 的完整清理。原拳风／30 秒回血回归另有日志。

未完成游戏内观感或 GPU 耗时验收；运行检查使用 NullRHI。模型挂点通过原始顶点位置核对。

更新命令：`-run=VTGFistWind -WindEnhance`；重新加载验证加 `-VerifyOnly`，运行检查用 `-WindEnhanceTest`，原拳风及回血回归用 `-Verify`。旧的全量 `-Apply` 会重设基础参数，运行后需重新应用增强配置。

备份与日志：`Scripts/CombatWindEnhancement`。
