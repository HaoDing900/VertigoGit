# VTG Audio — Narrative Event 使用笔记

更新：2026-11-04（管理器简化为 Ambient / Music；独立 OneShot 不变）

这些 **NE（Narrative Event）** 放在对话或任务节点的 **Events** 列表中。环境音、音乐使用 **BPLM → Audio Manager**；PlayOneShot / StopOneShot 已改为独立 2D 音效，不需要地图放置 Audio Manager。

资产目录：`Content/LevelNSequence/LevelAudio/NE`。

| NE | 作用 | 需要设置 |
|---|---|---|
| `NE_VTG_Audio_PlayAmbience` | 播放或替换环境音，支持渐入 | `NewSound`、`FadeTime` |
| `NE_VTG_Audio_StopAmbience` | 环境音渐出并停止 | `FadeTime` |
| `NE_VTG_Audio_PlayMusic` | 播放或替换背景音乐，支持渐入 | `NewSound`、`FadeTime` |
| `NE_VTG_Audio_StopMusic` | 背景音乐渐出并停止 | `FadeTime` |
| `NE_VTG_Audio_PlayOneShot` | 独立播放音效，可循环、淡入、随对话结束停止 | `NewSound`、`PlaybackID`、`Loop`、`StopWhenDialogueFinished`、`FadeInTime`、`VolumeMultiplier`、`StopFadeOutTime` |
| `NE_VTG_Audio_StopOneShot` | 停止匹配的音效，可淡出 | `SoundToStop`、`PlaybackID`、`FadeOutTime` |

**参数：**

- **NewSound**：要播放的 Sound Wave 或 Sound Cue。
- **FadeTime**：渐入／渐出时长，单位秒；`0` 表示立即执行。
- **FadeInTime**：OneShot 渐入时长，默认 `0`；不决定播放时长，也不提供片尾自动渐出。
- **VolumeMultiplier**：独立音效音量倍率，默认 `1`。
- **PlaybackID（Tag）**：优先匹配。同一 Narrative Component 内，一个非空 Tag 对应一个播放实例。Stop 填 Tag 即可，忽略 SoundToStop；Tag 找不到时也不会改用 Audio 停别的音效。同一 Tag 的重复 Play 会替换旧声音，即使音效资源不同。
- **SoundToStop**：折叠到 Advanced，仅当 PlaybackID 为 `None` 时使用。此时停止该 Narrative Component 注册的同资源实例（可能多份）。想只停其中一份请使用 Tag。两个都空不执行任何停止。
- **Loop**：默认关闭。SoundWave 支持无缝循环；SoundCue 会复制运行时图、移除内部 Looping / Wave Player 循环，再按开关决定是否整图重复（整图重复不保证无缝）。均不修改源资源。其他程序化声音类型不支持强制循环覆盖。
- **StopWhenDialogueFinished**：默认开启。整段播放所属对话结束、取消或被替换时停止，不是单句结束。关闭后可跨对话继续播放，通过对应 Stop 停止；切换世界仍会清理。
- **StopFadeOutTime**：对话结束时的淡出时长，默认 `0`；显式 Stop 使用该 Stop 的 **FadeOutTime**。

**使用时注意：**不同 Tag 可同时播放。没有 Tag 的 Play 只替换同资源的无 Tag 实例，不影响带 Tag 的实例。自然结束会释放本系统创建的组件。任务事件没有当前对话时，自动停止开关没有对话可绑定，循环音效需要显式 Stop。新 OneShot NE 默认不在读档时重新触发。

**管理器清理：**Psych 通道、`NE_VTG_Audio_PlayPsych` / `NE_VTG_Audio_StopPsych` 和 BPLM / Audio Manager 上的旧共享 `Audio_PlayOneShot` / `Audio_StopOneShot` 函数已移除。独立 PlayOneShot / StopOneShot NE 路径和参数保持，已有对话继续使用独立播放行为。场景声音的过场开关见 [过场声音使用说明](../CutsceneAudio/RESULT.md)。

**例子：**Play 设置 `NewSound=SFX_Pill`、`PlaybackID=Pill`；Stop 只填 `PlaybackID=Pill`。同一所属组件不要把这个 Tag 用于需要独立控制的其他声音。

## 停止其他蓝图播放的音效

其他蓝图可使用 `Spawn Sound 2D` / `Spawn Sound Attached` / `Spawn Sound at Location`，然后把返回的 Audio Component 交给 **Register Audio For Narrative Stop**：

1. Audio Component：那次播放返回的组件，或 Actor 自带的 Audio Component。
2. Narrative Component：与将来执行 Stop NE 的玩家 Narrative Component 相同。
3. Playback ID：非空 Tag，例如 `MachineHum`。
4. Stop When Dialogue Finished：注册入口默认关闭；开启时仅绑定注册当时的当前对话。

之后 Stop NE 只填 `MachineHum` 即可停止或淡出该组件。注册不会开始播放、调整音量或改变衰减；停止不会销毁外部拥有的持久组件。调用 **Unregister Audio For Narrative Stop** 可解除注册而不停止音效。组件自然播放完毕也会解除注册。

这里的 Tag 是本系统的播放 ID，不是 Actor Tags 或 Component Tags，不会搜索全世界同名对象。未注册的声音不能被此 NE 停止。`Play Sound` 的一次性调用不返回 Audio Component，需要改用 `Spawn Sound` 才能接入。

外部代码复用组件播放另一段声音前，应先 Unregister，再在新一次播放后重新 Register。检测到组件已换成不同 Sound 时，旧注册会失效，避免误停新声音；同组件同资源的重新播放必须由外部代码重新注册来明确生命周期。注册函数返回 bool，表示是否接入成功。

“恢复原环境音”不属于此功能：独立音效不修改环境音通道。验证与备份见 `Scripts/IndependentNarrativeAudio/`。

## Sewer 环境音来源调整

已移除玩家 `PlayerAudio` 中自动启动 Sewer 水声、管道底噪、随机管道声及自动混响的运行逻辑，并清除 `BP_Player_Sa` 对应的环境声资源配置。保留脚步、衣物／护甲音效。

原资源 `Content/SFX/SewerAudio/SC_SewerWaterLoop` 保留供关卡音频使用。此次是移除角色上的环境音职责，未自动把这些声音重新放入地图；之后应在关卡 Audio Manager 子类或独立空间声源中配置。

