# VTG Audio — 使用说明

更新：2026-11-04。

Audio Manager 只管理 **Ambient / Music**，对应 AC_Ambience、AC_Music。Psych 通道、Psych NE、管理器和 BPLM 的旧共享 OneShot 接口已移除。独立 OneShot NE 保留，不依赖管理器；Tag、循环、对话结束停止与外部组件注册的用法见 [NE 使用笔记](NE_Usage_Notes.md)。

场景 Sound Cue 实例的过场开关与 Sequence 预设见 [过场声音使用说明](../CutsceneAudio/RESULT.md)。开关默认关闭，不修改共享 Cue。新旧 Sequence 未配置时均保留现场声音。

## 关卡接入

资产目录：/Game/LevelNSequence/LevelAudio。

1. 创建 BP_VTG_AudioManager 子类，配置继承的两个 Audio Component 的 Sound、Volume Multiplier、Auto Activate。
2. 将子类放入关卡，在关卡 BPLM 的 AudioManagerRef 中指定它。
3. BPLM 子类覆写 Audio_ApplyStage，按 Stage 调用下列接口。覆写 BeginPlay 时保留项目现有父类调用。

未指定引用时，BPLM 会查找一个 Audio Manager 并缓存。多个管理器或流式关卡同时存在时，请显式设置引用。

## 接口

Audio Manager 与 BPLM 均提供同名接口；从 BPLM 内调用时 Target 为 Self。

| 函数 | 输入 | 行为 |
|---|---|---|
| Audio_PlayAmbience | NewSound: Sound Base, FadeTime: float | 替换环境音并渐入 |
| Audio_StopAmbience | FadeTime: float | 环境音渐出并停止 |
| Audio_PlayMusic | NewSound: Sound Base, FadeTime: float | 替换音乐并渐入 |
| Audio_StopMusic | FadeTime: float | 音乐渐出并停止 |

NewSound 为空时保持原声。FadeTime 小于零按零处理；零表示立即执行。每条音轨只保留一段声音，替换会停止原声音，不是 Crossfade。循环由声音资产决定。重复 Play 会重启该音轨，请勿每帧调用。

BPLM 在 BeginPlay 初始化后，使用 GetActiveStage() 调用一次 Audio_ApplyStage。父类实现留空；运行中切换阶段时，请在现有阶段切换流程再次调用它。

## Narrative Event

对话或任务节点的 Events 可使用：

- NE_VTG_Audio_PlayAmbience / NE_VTG_Audio_StopAmbience
- NE_VTG_Audio_PlayMusic / NE_VTG_Audio_StopMusic

路径为 NE → BPLM → AudioManagerRef。事件通过玩家 Pawn 所在世界查找 BPLM，适用于一个活动 BPLM 的关卡。多个 BPLM 同时加载时，需要明确目标路由。

NE_VTG_Audio_PlayOneShot / NE_VTG_Audio_StopOneShot 独立运行，详见 [NE 使用笔记](NE_Usage_Notes.md)。

## 示例与验证

BPLM_AudioExample、BP_VTG_AudioManager_Example、L_VTG_Audio_IntegrationTest 为演示资产。演示 BPLM 的 BeginPlay 不运行正式剧情初始化链，请勿直接复制到正式地图。

VTGLevelAudioCommandlet -Verify 验证两个管理器通道与 BPLM 转发；-IndependentVerify 验证独立 OneShot；-CutsceneVerify 验证过场混音。

管理器接线为 Blueprint；独立 OneShot 与过场混音有 C++ 运行依赖。自动化测试未开启音频设备，渐变听感与循环接缝需在游戏中试听。
