# Sa 头痛 Narrative Event（新版 PP + 音效）

继续在原有对话或任务节点使用：

`/Game/Narrative/Events/CharacterStatus/NE_PP_SaHeadache`

事件资产路径和类名未改动，旧节点中的该事件引用可继续使用。实际播放的旧 Actor 也保留原路径：

`/Game/Narrative/Events/CharacterStatus/PP_Sa_Headache`

## 事件参数

| Details → Headache | 默认 | 作用 |
|---|---:|---|
| Blend Weight Intensity | 1 | 最大视觉强度，0～1；保留旧变量及 GUID，兼容原事件实例中的强度值 |
| Headache Duration | 5 秒 | 从触发到完全消失的总时长 |
| Headache Audio Volume | 0.65 | 仅本次头痛音效的音量，0 静音 |
| Headache Envelope Influence | 0.12 | 画面跟随音频包络的幅度，0 关闭；最大 0.25 |

Event Runtime（Start/End/Both）和 Refire On Load 没有更改。若选 Both，会在开始和结束分别触发；重复触发会复用并重启同一个播放实例。

旧 Cam Shake Type 和 Spawn Transform 保留在 Legacy (unused) 类别以兼容序列化，但不再使用。旧 Camera Shake、Delay 和销毁再生成的播放链已移除。

## 播放行为

事件只调用一次 `PlaySaHeadache`：查找已有 `PP_Sa_Headache`，没有时生成一个；随后调用 `PlayHeadacheVFX(Intensity, Duration)`。

- 使用 `M_PP_Headache`，包含增强颗粒、血红错位重影和周边压迫。
- 一个可复用 MID、一个 PostProcessComponent、一个 AudioComponent。只操作本效果的材质和音量，不替换相机后处理或其他声音。
- Actor 无 Tick；强度通过仅在播放期间激活的 TimelineComponent 更新，结束后停用。
- 归一化强度曲线关键点：`(0,0), (0.066,0.35), (0.176,1), (0.286,0.92), (0.44,0.32), (0.84,0.18), (1,0)`，三次插值。Timeline Play Rate = `1 / Duration`。默认保留原先 0.33 秒上升、0.88 秒达峰、1.43 秒开始进一步减弱的节奏，2.2 秒仍有约 32% 的余效，4.2 秒约 18%，5 秒归零（再乘峰值强度与轻微音频调制）。
- 重触发覆盖上一轮：清除两段音量计时器、重启同一 Timeline 和同一音频组件，从新触发点重新计算完整时长。视觉从当前值平滑过渡，不叠加强度。
- 自然结束时清除两段音量计时器、停止声音和 Timeline、将材质强度和组件 Blend Weight 归零。地图退出/Actor 销毁也会清理。
- Intensity 或 Duration ≤ 0 表示停止。

`PP_Sa_Headache` 现在继承项目内 `AVTGSaHeadacheEffect`，提供 Timeline、音频包络绑定与安全复用支持。Narrative Event 仍是可编辑的蓝图，参数在对话节点中可调；未新增 Manager 或改动 Level Manager。

## 音效与轻微同步

原 SoundWave：`/Game/SFX/Character/SFX_SaHeadache1`，未剪辑、重导入或修改声音资产。

导出分析显示音频长 **23.254 秒**，前约 **0.8 秒**逐渐增强，100ms RMS 峰值约 **0.56**。播放从音频开头开始，使用 0.06 秒淡入。

默认总时长 **5 秒**，分两段处理：

- **1.43 秒**开始用 S 曲线降低音量，经过 **0.77 秒**降至原音量的 **32%**，继续播放，不停止声音。
- 保留低音量尾声至 **4.2 秒**，再用 **0.8 秒**完全淡出，到 **5 秒**停止。

时长参数改变时，以上时间点同比缩放；音效不拉伸、不变调，也不循环。新的触发会取消上一轮两个音量计时器，避免旧计时器截断新一轮声音。

实时同步使用 AudioComponent 的 OnAudioSingleEnvelopeValue（Audio Mixer），Attack=60ms、Release=180ms。包络乘 2 后钳制到 0～1；默认将视觉强度在基础曲线的 88%～100% 之间轻微调制。不会让整幅画面按音频高频闪烁。没有可用包络时仍可按基础曲线播放。

## 测试

1. 在对话/任务节点添加或继续使用 `NE_PP_SaHeadache`，默认参数触发一次。
2. 在暗部和有明亮门框/灯具的场景观察：颗粒与血红边缘双像逐渐增强，随后和音效一起收回。
3. 默认 Duration=5，检查 1.43 秒之后只是减弱，2.2 秒仍有余效，5 秒完全消失；Intensity 可尝试 0.5 / 1，音量建议先用默认 0.65。
4. 在尾段再次触发：应只存在一个播放 Actor，并从最新触发开始重新计时 5 秒，不会被旧一轮的结束时间截断。
5. 测试结束、停止或切图后，确认画面恢复且声音停止。
6. 如关卡中手动放置了常亮的 `BP_HeadachePostProcess` 预览蓝图，请先将它的强度设为 0，避免与 Narrative 播放叠加。该独立预览资产未改变。

自动验证：`narrative-verify.log` 覆盖真实 Blueprint Event 调用、同实例复用、合成包络响应、重复触发衔接和结束归零。测试使用无音频设备的命令行世界；实际音色、响度与音画观感仍需 PIE 试听确认。

```powershell
& 'G:/Epic/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/Epic/Unreal projects/VertigoGit/Vertigo.uproject' -run=VTGHeadacheVolume -IntegrateNarrative -VerifyOnly -NullRHI -unattended -nosplash -NoSound -nop4
```

两份修改前备份位于本目录 `Backup/`，未覆盖或删除原音频。
