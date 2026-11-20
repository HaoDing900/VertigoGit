# Mail 相机、提示音与过场入口

## Mail 相机

BP_Player_Sa 的 Mail 相机往返现在遵循以下规则：

| 切换 | Blend Time |
|---|---|
| 实际第三人称 → Mail | 1 秒，保留原值 |
| Mail → 原第三人称 | 1 秒，保留原值 |
| Fixed / Lock / 其他相机 → Mail | 0，直接切换 |
| Mail → Fixed / Lock / 其他相机 | 0，直接切换 |

判断参考控制器实际 View Target、待切换目标、Player_FollowCamera 状态及第三人称模式标志。不会只因为 Sa 的 Follow Camera 还处于 Active 就误判成第三人称。

既有 Mail → Note → Mail 的 generation 保护、输入锁和相机归属检查保留；这条特殊归还路径也使用相同的 Blend Time 规则。未改相机 Transform、焦距、位置或朝向。

## 新邮件提示音

SetMailData 的 NewMail? 为 true 时，播放一次 `/Game/SFX/Character/Ting_Coins`。同一封未读邮件的重复更新不重复响；不同邮件到达会再次响，读取后新的未读通知也会响。NewMail? 为 false 时不响。

声音由玩家蓝图中的 VTG_NotifyNewMail 节点引用，走独立 2D 提示，不占用 Audio Manager 的 Ambient / Music。

## L2 酒吧外街战斗围栏

BPLM_L2StreetBarFight 原本在创建并播放战斗开场 Sequence 的链中立即 ActivateCollision。现在移动到现有 OnFinished_CombatIntro 回调，再继续原来的过场结束流程。

不使用写死 Delay；沿用实际 Sequence 的完成通知。战斗结束清理和读档恢复战斗时的 VTG_CheckpointActivateArena 保留。

## Sequence 开始时结束对话

在 **Sequence 快进检查 → 进入时结束 Narrative 对话（Sequence 默认）** 设置资产默认值；也可在 Details → 过场入口设置。修改资产默认值后保存 Sequence。地图实例的 **本实例：进入时结束 Narrative 对话** 可单独覆盖；点击 **跟随 Sequence** 清除实例覆盖，修改实例后保存关卡。

- **资产默认关闭**。未设置实例覆盖时跟随 Sequence 资产；启用的默认值也适用于动态创建的播放器。
- 开始播放时强制结束玩家当前 Narrative 对话，忽略 Can Be Exited / Allow Exit。
- 只执行一次。暂停后继续不重复退出，也不会在整个 Sequence 期间禁止新对话。
- 在第一帧事件前退出旧对话；第一帧事件主动开启的新对话会保留。
- 停止后重新播放属于新的一次播放，会再执行入口规则。
- 资产默认与实例覆盖分别保存到 Sequence 和关卡；实例明确关闭优先于资产默认开启。
- 蓝图 Create Level Sequence Player 动态实例会读取 Sequence 资产默认值；Actor 的 VTG.EndNarrativeDialogueOnStart / VTG.KeepNarrativeDialogueOnStart 标签可分别强制开启/关闭。
- Narrative 自己创建的对白相机镜头自动排除，避免对白镜头中断自己的对白。
- `1PlayLevelSequence` / `PlayDialogueLevelSequence` 创建的 Narrative 演出同样按归属排除；修正了 Corvos 演出被自身入口规则提前结束的问题，详见 [回归说明](CorvosSequenceRegression.md)。

其他蓝图可以从 **Narrative Component 引用**拉出 **Force Exit Dialogue**。普通 Try Exit Dialogue 继续遵守 Can Be Exited。Force Exit 沿用 Narrative 的结束事件与 Deinitialize，会正常触发 OnDialogueFinished；它不是暂停或保存对话进度。

## 维护与验证

运行实现：VTGMailReadingComponent、VTGSequenceDialogueSubsystem / Gate / EntrySystem 和 NarrativeComponent::ForceExitDialogue。EntrySystem 使用 Sequencer 的 Spawn 阶段扩展，早于最早的事件与相机生成；未修改引擎源码。

资产更新工具：`VTGMailSequencePolish`；`-Verify` 在新进程读取已保存蓝图并检查接线；`-RuntimeTests` 验证相机、提示去重和真实 Sequence / Narrative 播放行为。

改动前两份蓝图备份：`Scripts/MailSequencePolish/Backup`。编译通过，两份蓝图编译 0 错误、0 警告，新行为 20 项运行检查通过。原有 NoteMailCameraReturn / NoteMailCameraSession 两项自动化回归通过。日志在 `Scripts/MailSequencePolish`。

测试使用 NullRHI、关闭音频设备，未代替完整游戏内画面与听感验收。

2026-11-19：补充资产级开关，修复只勾选关卡实例、运行时另建播放器未继承设置的问题。LS_L2BarStreet_PreDrunkFight 已启用资产默认；普通 Allow Exit=false 不阻止该强制入口退出。Narrative 自身对白镜头保护保持不变。
