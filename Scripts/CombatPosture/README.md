# Sa 战斗姿态统一修正（2026-10-19）

此前 `spine_02_x` 的骨骼本地 X 轴 -15° 修正仅在 Hammer 分支生效，现在统一为：`InCombat=true` 且不在 StoryMode / 死亡 / 隐藏玩家 / 过场覆盖时生效。拳头、Hammer、StunGun、SMG 和后续武器均使用同一判定，不依赖武器枚举。

继续使用 ABP_Sa 中原有的一个 Transform (Modify) Bone 节点；在完成 locomotion / montage 混合后施加一次修正，不叠加第二个 -15°。保持骨骼平移、缩放以及原始动画资产不变。脱战后即使仍拿着 Hammer 也不再施加修正。

维护位置：

- `Source/Vertigo/Private/Animation/VTGCameraLookAnimInstance.cpp`：NativeUpdateAnimation 中的战斗姿态条件。
- `Source/Vertigo/Public/Animation/VTGCameraLookAnimInstance.h`：参数与默认角度。
- `/Game/Characters/Sa/ABP_Sa` 的 Class Defaults → **Combat Posture** → **Combat Spine X Degrees**（默认 -15），或 **Combat Spine Correction Enabled**。

为了保留已有 AnimGraph 连线及蓝图参数值，内部序列化成员仍名为 HammerSpine*，编辑器显示名和分类已改为 Combat Posture。此次无需重存玩家蓝图或 ABP_Sa。

验证工具沿用 `VTGHammerPosture -VerifyOnly`：检查现有节点仅一个且为 Bone Space / Additive / spine_02_x，再比较真实 Sa 网格最终求值姿态的骨骼相对旋转。覆盖拳头、Hammer、远程武器标志、攻击、切换武器、脱战、死亡和剧情排除条件。编译与回归日志在本目录。

编译和上述回归全部通过。实际骨骼相对旋转差值为 X/Roll=-15°、Pitch/Yaw=0°，骨骼平移和缩放不变。尚未进行关卡内的人工观感验收。
