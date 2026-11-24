# Sa injured 左手形变：只读诊断

用户确认：手掌/手指本身，受伤后原地站立也会反复变大/变小。

已确认：
- ABP_Sa 的战斗姿态 Modify Bone 只改 spine_02_x 旋转，ScaleMode=Ignore。
- ANM_Sa_InjuredForBlend、ANM_Sa_InjuredWalking、正常 Idle 的源动画手掌及手臂 Scale 约为 1；61 个时间点无循环缩放。
- 受伤表情动画中可变缩放只见于脸部，未找到手掌/手指循环缩放关键帧。数据见 inspection.json。
- 实际 Sa_Rig3 使用 PPABP_Sa，物理资产 Sa_PhysicsAsset_WithHairNCape 的 hand_l、arm_stretch_l、forearm_stretch_l 均为 Kinematic。导出见 physics.t3d。
- Standing 中另有 May 的 Injured 动画节点，但它的 State 没有连接，不能据此认定当前问题来自该动画。
- 当前低血量实际入口是 Low Health → Injured cached pose；上半身混合权重 0.7。
- 独立 Editor 预览可切换 Low Health 姿势，但采样期间动画姿态被冻结，不能当作循环动画的有效运行时复现。

尚未确认最终根因，未修改任何骨骼、Mesh、动画或蓝图资产。尝试编译的临时 C++ 诊断扩展已移除；编辑器打开导致 DLL 链接被占用，没有关闭用户编辑器。

下一步：在用户实际出现问题的 PIE 中运行 watch_pie.py。它只读取左手骨骼 Scale、旋转、与父骨骼间距、Health、Low Health 及当前 Montage，采样 8 秒并写入 pie-trace.json，不改游戏、资产或存档。

Output Log 底部切换到 Python，执行：

```python
exec(open(r'G:/Epic/Unreal projects/VertigoGit/Scripts/InjuredScale/watch_pie.py', encoding='utf-8').read())
```

脚本完成后可直接读取本目录 pie-trace.json，确认是真实骨骼缩放、骨骼长度变化，还是旋转/蒙皮带来的视觉形变，再进行针对性修复。
