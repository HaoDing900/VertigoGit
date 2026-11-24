# 巷战自动检查点与通用 BPLM 进度保存

2026-10-24。沿用 VTGSaveCoordinator 与原有自动存档槽 0，没有另建存档系统。

## 已接入 BlackMarketStreet

- BPLM_BlackMarket 的 InitializeAlleyCombat 共用 DoOnce 后新增 Save Encounter Checkpoint：
  L4_AlleyFightStart，Order=100，复活位置取战斗开始时玩家的 Transform。
- 正常两条战斗入口都会保存一次；恢复入口不会重写原检查点。
- 存档保存 BPLM 剧情变量及 VTG_ResumeAlleyFight 恢复事件。
- 读档时跳过 OnStageBegin 开场，在玩家、Quest、物品恢复完毕后执行一次恢复事件。
- 恢复事件重建巷子屏障、Drone 战斗位置、三名敌人、各自死亡委托与战斗区域；计数重置为 3。
- AfterAlleyFightComplete 设置 L4_FightHappened?=true，并将后续存档恢复入口改为
  VTG_ResumeAfterAlleyFight。该入口清理敌人/屏障、隐藏剧情 NPC 并恢复 Drone 跟随。
- 本次只新增战前自动存档；正常胜利后仍继续原有对话，不在对话中途覆盖战前检查点。

原 BPLM 资产备份：Backup/BPLM_BlackMarket.uasset。地图资产未修改。

## 共用存档改动

原来就会保存子 BPLM 的 Bool、数值、Enum、Name、String 及 GeneralLevelPhase。
现在还支持 Text、只含值的结构体、以及只含可保存值的 Array/Set/Map。
通过 BPLM Class Defaults 的 Excluded Progress Variables 可排除临时变量。
运行时对象引用、委托、Delay/DoOnce 执行现场和活动对话不作为变量快照恢复。
公共 BPLM 父类的相机/输入锁等仍默认排除；安全类型可用 SaveGame 标记明确加入。

通用接口位于 VTGLevelManagerBase：

1. 战前对话结束且战斗尚未启用时，调用 Save Encounter Checkpoint。
2. 参数指定本关唯一 Checkpoint Id、无参数恢复 Custom Event、安全复活 Transform 和检查点 Order。
3. 恢复事件只建立当前玩法现场，不播放已经完成的开场/对话。
4. 胜利时调用 Set Encounter Resume Event，指定单独的战后场景恢复事件，供后续存档使用。
5. 如需战后自动存档，应等战后对话完成、场景稳定后再保存。

L2 与下水道的原检查点设计保留，未批量改写其他关卡的剧情图。
旧存档仍可读；缺少恢复入口的旧记录走原开场逻辑，需要新到达巷战触发点生成检查点。

## 验证

- Development Editor C++ 编译通过，实际 BPLM 蓝图零编译错误。
- verify.log：COMBAT_CHECKPOINT_SUCCESS；最终复验记录见 verify-final.log。
- 真实 BPLM 的对话 Bool、数组、Transform 经 SaveGame 内存序列化往返保持一致。
- 实际 SaveCoordinator 在 BeginPlay 前恢复；已保存战斗跳过 Stage 开场，在 OnSlotLoaded 后仅恢复一次。
- 恢复中 Save Encounter Checkpoint 返回成功且不覆盖记录；旧记录仍执行正常 Stage 入口。
- 实际 BlackMarketStreet 的三名敌人、剧情 NPC、Drone、屏障及唯一战斗区域引用核对通过。
- 原三名敌人重复死亡通知不重复计数、第三人死亡只完成一次、玩家受到各敌人攻击扣血的测试通过。
- 所有测试使用临时世界与内存 SaveGame，没有写入或加载玩家的真实存档槽。

额外运行旧 `VTGAlleyRepair -LevelProgress -VerifyOnly` 时，BlackMarketEntrance 的事件连线
未满足该旧检查脚本的固定图结构前置条件，检查提前退出（legacy-progress.log）。
未修改 Entrance 资产，也未把该项记作通过；本次独立测试覆盖了新旧记录及正常 Stage 入口。

复验命令：UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGBlackMarketCombat -Checkpoints -VerifyOnly -unattended -nullrhi -nosound

尚需在完整 PIE 中体验：正常进行到巷战 → 故意死亡 → Retry；确认不重播战前对话、三名敌人均重新参战、Drone 与屏障位置正确。
