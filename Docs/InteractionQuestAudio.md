# 互动与 Quest 音效

Dialogue 选项的确认与 Hover 音效见 [对话选项音效维护](DialogueResponseAudio.zh-CN.md)。它们使用独立的 NSFX_Response 素材池。

`BP_0ISXInteractiveCollision` 实例 Details → **Interaction Audio**：

- **Interaction Sound Enabled**：默认开启；关闭后不播放互动反馈，包括 override。
- **Interaction Sound Override**：可指定 SoundWave、SoundCue 等 SoundBase。留空时，从 `SFX/Ovani/UI4/Mouse/Hover/Hover_Magic_001` 至 `005` 五条素材等概率随机播放一条。

声音在实际 `OnInteract_Server` 互动事件到达时播放一次，随后继续原有逻辑；不是鼠标 hover 或靠近时播放。使用独立 2D one-shot，即使互动逻辑随即销毁物体也不会截断音效。

以下 Quest 事件每次 ExecuteEvent 触发时，从 `SFX/Ovani/UI4/Signals/Update/` 的 `UsedInQuest/Signals_Update_Magic_003_quest`、`UsedInQuest/Signals_Update_Glitch_004_quest`、`UsedInQuest/Signals_Update_Magic_005` 等概率随机播放一条：

- `/Narrative/DefaultEvents/NE_BeginQuest`
- `/Narrative/DefaultEvents/NE_CompleteNarrativeTask`
- `VTG Ensure Quest State`（原生 `UVTGEnsureQuestStateEvent`）

这是事件触发反馈，因此重复触发但未实际改变 Quest 状态时也会播放。播放调用单独位于 `UVTGUIFeedbackLibrary`，不接入或修改 Narrative Dialogue 的语音、字幕或自带音频组件。互动与 Quest 同时触发时，各自播放自己的反馈。

默认素材通过原生类的 UPROPERTY 强引用保存，供打包收集。当前按项目单人模式接入现有互动事件，没有增加网络 RPC。

安装：`UnrealEditor-Cmd Vertigo.uproject -run=VTGUIFeedback -unattended -nullrhi -nosound`。
验证：上述命令追加 `-VerifyOnly`，检查默认素材数量、静音与 override 优先级、实例可编辑属性以及事件接线和蓝图编译。
原蓝图备份：`Scripts/UIFeedback/Backup/`。
