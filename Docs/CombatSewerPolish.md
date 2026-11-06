# Sa 出拳、闪避衣料声与 Sewer Boss 调整

## 出拳向下修正

ABP_Sa 新增最终脊椎修正 `VTG_PunchDown15`，在实际空手出拳 Montage 播放期间，将上半身向玩家朝向的下方旋转 15°。停止攻击后平滑归零，退出 combat 后立即清除。不会修改相机、Actor Transform、原始动画、其他武器或基础战斗站姿。

参数：ABP_Sa → Class Defaults → Combat Posture → **Punch Down Degrees**，默认 15。其余已有 Combat Spine X Degrees 与 Aim Offset Clamp 保留。

实际骨骼求值验证：开启前后脊椎世界旋转差为 Pitch -15°，Yaw / Roll 为 0，误差 0°。

## Dodge 随机衣料声

新 Cue：`/Game/SFX/Character/SaCombat/SC_Sa_DodgeCloth`。

随机池为用户指定的三份 SoundWave：Stereo 的 light_fast_movement_13 / 15，加 Mono 的 light_fast_movement_13。等权重随机，启用 Without Replacement；每次闪避播放其中一份，不会三份同时播放。原 SoundWave 不修改。

挂载在 Sa 现用的两个 Montage 的 0.01 秒，通知名 `VTG_DodgeCloth`：

- ANM_Sa_Dodge_F2_Montage
- ANM_Sa_Dodge_B_Montage

Cue 音量倍率 0.55，使用现有玩家脚步衰减，通知跟随玩家。该声音独立于 Ambient / Music。

## Freemove NPC_2 卡住

已复现的失败路径：全局 Auto 关闭 → 文本计时结束 → NPC_2 等待手动 Continue。NPC_2 实际连着 NPC_3，并不是断线；Action 短评没有普通对话的手动继续入口，因而看起来卡住。

新增 Dialogue 配置 **Always Auto Advance**，默认关闭。只为 `L3Sewer_Freemove` 开启，确保游走短评不被全局 Auto OFF 挂住。普通剧情对话仍保留手动模式。验证了 NPC_2 → NPC_3 → 对话正常退出。

## Boss 血条等待入场对白

`L3BossEncounter` 的 StartBossEncounter 调用启用 Wait For Intro Dialogue，在进入战斗前隐藏 Boss HUD。

KillCount >= 4 的既有两分支及台词不变：

- True：L3Sewer_Still_DialogueNode_NPC_20
- False：L3Sewer_Still_DialogueNode_NPC_19

两处 Begin Dialogue 后均登记当前对话实例。只有该实例的 OnDialogueFinished 才解除隐藏；其他对话的结束通知不会误解除。自然结束或强制退出都走正常清理。启动失败且没有有效对话时解除隐藏，避免永久丢失血条。

这里只增加 HUD 显示条件，保留已有战斗时机。读档恢复不重播入场对白，C++ StartBossEncounter 默认不等待对白。原有死亡隐藏与脱战处理保留。

## 酸液远程攻击

| 参数 | 调整前 | 调整后 |
|---|---:|---:|
| AcidSpit 发动最大距离 | 1800 cm | 3600 cm |
| 弹丸最大飞行距离 | 1800 cm | 3600 cm |
| 碰撞球半径 | 24 cm | 48 cm |
| 可见核心 / 原静态网格缩放 | 0.24 | 0.48 |
| 原 AcidVisual 组件缩放 | 1.2 | 2.4 |

这里的放大按三个方向的尺寸各 ×2 处理。当前使用的可见核心材质、原 Niagara 启用状态、伤害、速度、冷却与命中特效保留。弹丸依旧在到达最大距离后销毁；现有 5 秒寿命足以覆盖 850 cm/s 下的 36 米距离。

## 验证与维护

Editor Development 编译通过。ABP_Sa、L3Sewer_Freemove、BPLM_SewerUnderApt 和 BP_SeaMonster_AcidProjectile 编译均为 0 错误、0 警告。独立进程重新加载后检查 Montage 通知、Cue、图接线和保存值，并运行骨骼、真实对话推进、HUD 绑定与弹丸清理检查。

原有 Sewer Encounter 回归、Dialogue History / 手动与自动推进回归通过。未开启音频设备或进行完整游戏内观感验收。

更新工具：`-run=VTGCombatSewerPolish`。`-Verify` 只验证资产，`-Tests` 加运行检查，`-Inspect` 输出当前参数。改动前资产备份与日志位于 `Scripts/CombatSewerPolish`。
