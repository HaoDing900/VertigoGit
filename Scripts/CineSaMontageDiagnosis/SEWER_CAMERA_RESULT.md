# L_SewerUnderApartment Smart Trigger 相机检查（2026-10-16）

关卡中的 `BPA_0SmartTrigger`：TRIGGER ID=`L3_FirstTimeSeeEgg`，Stage=`L3Sewer`，Lock Cam on Trigger=True。Lock Cam Point 正确指向 `BP_1CameraLockAutoTrigger_L3MonsterEgg`（实例 `BP_1CameraLockAutoTrigger_C_3`），相机 ID=`L3_MonsterEgg`，Active Stage?=`L3Sewer`。

图表已接通：Trigger overlap → BPLM.ReceiveSmartTriggerCall → 开始 L3Sewer_Still（NPC_12）→ 返回后检查相机 IsValid → BPLM.LockCameraPoint。因此不是遗漏锁相机调用。

发现可复现的启动 Stage 时序问题：相机父蓝图 `/Game/Blueprints/BP_0CameraLockPoint` 在 BeginPlay 后 Delay 0.2，再执行 DestroyIfNotOnStage。Stage 不匹配会 Destroy Actor。`BPLM_SewerUnderApt` 则在 BeginPlay 中调用父类后 Delay 0.1，才设置 GameInstance.CurrentStage=L3Sewer。Stage 并非在加载地图前已确定；固定 Delay 也不能保证长帧中不同对象 latent action 的处理顺序。Smart Trigger 自身等 0.5 秒再检查 Stage，因此可以存活到正确 Stage，但相机已在较早的检查中被销毁。

验证：独立隐藏编辑器，加载磁盘关卡，Simulate。`Saved/QA/camera-runtime-detail.log` 中触发前相机 IsValid=False，已不在 GetAllActorsOfClass 结果中。对照运行仅在内存中将本关卡 BPLM 的 bOverrideStageForTesting=True、TestStage=L3Sewer 提前设置：`Saved/QA/camera-runtime-control.log` 中相机 IsValid=True、存在于 Actor 列表，触发后 bCameraLocked=True、ActiveCameraPoint 为正确实例。

边界：Simulate 使用 Spectator Controller，BPLM 的游戏 Controller 缓存未正常初始化，出现对应 Accessed None；此实验用于验证相机存活和锁定分支，不能当作完整 Play 模式视角切换验证。测试未保存关卡或蓝图，现有资产改动均保留。

维护建议：直接 PIE 测本关卡时，在关卡 BPLM 上使用现有测试 Stage 覆盖（L3Sewer）；正式关卡入口使用 OpenLevelWithStage，使 Stage 在 OpenLevel 前设置。若要统一消除这一类启动问题，Stage 检查应等待明确的初始化完成条件，避免用固定 Delay 判断角色/相机是否属于当前 Stage。
