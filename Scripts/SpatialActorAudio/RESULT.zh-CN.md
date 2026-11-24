# Bello 与 IPHA Drone 空间循环音效

## Bello
- 从 ANM_Bello_FixCar1 移除指定修车声音的 4 个 PlaySound notify，保留其他 notify。
- BPLM_PoliceStation 的 L3_SetBelloIdle 先调用 SetBelloRepairLoop，读取实际 Bello 引用与 L3_Bello_Helped；false 播放，true 停止。
- L3_Bello_CarRepairTalk 和 L3_BackgroundConv_BelloCar 的 BeginDialogue 后暂停该 Bello 的修车声。仅对应的对话结束才解除暂停；未帮助则恢复，已帮助仍保持停止。
- 声音附着在 Bello 身上，不依赖动画循环。重复调用不会叠加组件。
- SA_BelloRepair：1 米内保持音量，随后线性衰减，约 10 米处静音；音量倍率 0.35。

## Drone
- BP_Drone_IPHA_NPC 新增 ElectricHum 组件，使用 Electric_Hum_Loop_E。
- SA_DroneHum：1 米内保持音量，随后线性衰减，约 8 米处静音。
- IdleVolume=0.08，MovingVolume=0.13；IdlePitch=0.95，MovingPitch=1.30。
- 每 0.05 秒按 Actor 实际位置变化计算速度，覆盖 Follow、GoTo、MoveTo 和时间轴位移。低于 10 cm/s 忽略微小漂移，300 cm/s 达到最大变化；突发传送不触发高音调。
- 使用平滑插值，停止后恢复低音量与低音调。Pitch 同时影响播放速率。
- 参数在 Blueprint 的 ElectricHum 组件 Details → Spatial Loop 中调整；距离在两个 SoundAttenuation 资产中调整。

## 实现与验证
- 原资产备份在 Scripts/SpatialActorAudio/Backup。
- 原 SoundWave 的 looping 属性不修改；运行时使用私有循环 SoundCue，跟随 Actor，支持远距离虚拟化和销毁时清理。
- 编辑器资产接入命令：-run=VTGLevelAudio -SpatialSetup。
- 验证命令：-run=VTGLevelAudio -SpatialVerify。覆盖资产引用与编译、动画拆除、待机和对话接线、重复调用、对话隔离、已帮助后不恢复、空间音频配置、实际位移驱动、音量与音调插值、销毁清理。
- 验证使用无音频输出的测试世界；尚未在游戏里试听，距离与音量是可调整的初始值。

最终结果：Development 编译成功；verify-final.log 完整验证退出码为 0，包括 Actor teardown 循环组件清理。
