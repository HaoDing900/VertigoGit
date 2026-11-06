# Sa 视角跟随旋转：修改位置

使用的动画蓝图：`/Game/Characters/Sa/ABP_Sa`。

## 头部与脖子

- **驱动**：实际玩家相机的朝向，相对 Sa 身体朝向计算偏移，再平滑应用到 `neck_x` 和 `head_x`。普通与战斗模式共用。
- **修改位置**：打开 `ABP_Sa` → **Class Defaults → Camera Look**。
  - `Max Look Yaw`：左右合计跟随上限，当前每侧 **40°**。
  - `Max Look Pitch`：头颈上下跟随上限，当前每侧 **30°**。
  - `Look Interp Speed`：跟随速度，当前 **7**。
- 脖子分担 35%，头部分担 65%；这是合计角度，不是每根骨骼各转 40°。攻击、闪避、终端或固定镜头等状态会暂停跟随并平滑回正。
- 驱动代码：[VTGCameraLookAnimInstance.cpp](<G:/Epic/Unreal projects/VertigoGit/Source/Vertigo/Private/Animation/VTGCameraLookAnimInstance.cpp>)；分摊比例也在这里。

## 脊椎随视角上下转动

- **驱动**：`ABP_Sa` 的 Event Graph 使用玩家 `Get Base Aim Rotation` 与身体旋转的差值，取 Pitch 并乘以 **−1**，经 `Clamp` 后写入 **Aim Offset**。AnimGraph 用它叠加旋转 `spine_02_x`。拳头与锤子共用。
- **修改位置**：`ABP_Sa` → **Event Graph**，搜索变量 **Aim Offset**，找到写入它之前的 **Clamp**。
  - `Min = -20`：向上看最多偏移 **20°**。
  - `Max = 30`：向下看最多偏移 **30°**。
- 此处限制的是相对原有姿态的视线偏移；不要把正负方向反过来。

## 基础战斗姿态

`ABP_Sa` → **Class Defaults → Combat Posture → Combat Spine X Degrees**，当前 **−15°**。这是额外的骨骼局部 X 轴姿态修正，不是视角跟随上限。仅需调整抬头／低头幅度时，修改上面的 Aim Offset Clamp。

修改后 Compile、Save。以上都是动画表现设置，不限制玩家相机本身的转动范围。

出拳期间额外向下 15° 的参数是 Combat Posture → **Punch Down Degrees**。它只在空手拳击 Montage 播放时生效，独立于上述基础战斗姿态与视角 Clamp，详见 [战斗与 Sewer 调整](CombatSewerPolish.md)。
