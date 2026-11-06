# Dialogue Blip 简洁维护指南

## 1. 需要哪些 BP、资产和代码？

| 文件／资产 | 用途 |
|---|---|
| `Content/Narrative/Dialogues/` 下的 Narrative Dialogue 资产 | 配置角色 Tag、音高、节点开关及逐句配音 |
| `Content/Narrative/Audio/DA_DialogueBlipLibrary` | 集中管理各 Tag 的音效池、随机 Pitch／Volume、避免连续重复和播放保护上限 |
| `Content/SFX/Blip/BlipSFX_NPC1–3` | 默认 BlipNPC 池的三个声音；同目录保留原始 WAV |
| `Content/LevelNSequence/LevelAudio/BP_VTG_AudioManager` | OneShot／Psych 剧情音效让 Blip 停止、让位；不负责选择 Blip |
| `Config/DefaultEngine.ini` | Narrative 设置中的共享默认 Blip Library 引用 |

**不需要在关卡放置新 Blip Actor，也不需要给角色添加音频组件或专用 BP。**

维护代码主要位于 `Plugins/Narrative/Source/Narrative/`：

- `Public/Dialogue.h`：Speaker 的 Blip Tag、Voice Pitch，以及对话级设置。
- `Public/DialogueSM.h`：节点的 Enable Blip。
- `Public/NarrativeBlipLibrary.h`、`Private/NarrativeBlipLibrary.cpp`：池结构、随机选择、音高预设和优先级桥接。
- `Private/DialogueBlip.cpp`：实际播放、最终音量、停止与最长时长保护。
- `Private/Dialogue.cpp`：真实台词播放与退出／跳句／暂停的接入点。

项目侧的 `VTGDialogueHistorySubsystem.cpp` 接入快进；`VTGUIFeedbackLibrary.cpp` 接入互动／Quest 音效让位。升级 Narrative 插件时须保留上述扩展。

## 2. 怎么用？

### 配置一个角色

1. 打开对应 Narrative Dialogue，点击 **Class Defaults**。
2. NPC：展开 **Speakers → 对应角色 → Blip**。玩家：展开 **Player Speaker Info → Blip**。
3. 填 **Blip Tag**：例如 `BlipNPC` 或 `Sa`。它与场景 Actor Tags、Jaw 的 Speaking Actor Tag 无关。
4. 选 **Voice Pitch**：Default 不额外变调；High 升 3 半音；Low 降 3 半音。
5. 编译并保存 Dialogue。

同一角色在不同 Dialogue 中各自保存配置，修改一处不会自动同步所有对话。

### 配置一个池

打开 **DA_DialogueBlipLibrary → Blip Pools**，新增或展开同名 Key，在 **Sources** 中填入短、非循环的 SoundWave 或 SoundCue。多个角色可以共享一个池；常用 1–3 条素材。

- **Pitch Min／Max、Volume Min／Max**：每次播放的随机范围。
- **Avoid Immediate Repeat**：多条有效素材时避免连续选中同一条。
- **Max Blip Seconds**：播放保护上限，当前为 3 秒；不是对话时长。

新增专属池只需添加 Key 和素材，再填写角色 Tag，不需要修改蓝图执行图。空池允许存在，会静音。

### 开关与配音

- Dialogue Defaults → **Dialogue Blip → Enable Blips**：整段对话开关。
- 节点 Details → **Blip → Enable Blip**：单节点开关，适用于内心独白等。
- 实际台词的 **Dialogue Sound**：填写后使用该配音，跳过 Blip。Alternative Lines 每条使用自己的字段。

### 调整整体音量

目前最终倍数写在 `Private/DialogueBlip.cpp` 的播放调用中：

```cpp
Choice.Volume * 0.4f
```

当前为 `0.4f`，修改此系数后需要重新编译；尚未提供该倍数的 Details 输入框。它只降低 Blip，不影响配音 override、互动或 Quest 音效。

## 3. 什么原理？

每次实际播放新台词时，使用已选中的主／备选 Line 和变量替换后的文字：

1. 清理上一句 Blip；本句有 Dialogue Sound 时交给原配音系统。
2. 若 Blip 关闭、正在快进、文字只有空白／标点，或有优先音效播放，则静音，不排队补播。
3. 按角色 Blip Tag 找池：未填或找不到 → `BlipNPC`；同名池存在但为空 → 静音。
4. 从有效素材中随机选择，并随机生成 Pitch 与 Volume。
5. 最终 Pitch = 随机 Pitch × 角色倍率（Default=1、High≈1.1892、Low≈0.8409）；最终 Volume = 随机 Volume × 0.4。
6. 使用独立 2D 音频组件播放，**不修改台词时长、不用音效结束来推进对话**。

普通句尾允许短音自然播完；下一句开始、手动跳句、快进或退出会停止旧 Blip。暂停跟随对话暂停。音高倍率也会改变播放速度和声音长度。

已登记的配音、互动／Quest 和 OneShot／Psych 音效会让 Blip 让位；音乐、环境、脚步不自动压制。自定义剧情音效可在开始播放后调用 **Register Priority Audio**，传入其 AudioComponent。

### 排查与验证

听不到时依次检查：**实际 Line 是否有配音 → 开关／快进 → Tag → 同名池 Sources → 优先音效是否仍在播放**。例如 Sa 池存在但为空，就不会回退到 BlipNPC。

独立回归：`-run=VTGDialogueBlip -TestOnly`，不修改资产。此前批量 Tag 写入使用的是自定义编辑器 Commandlet，不是运行时自动绑定；不要把带历史迁移规则的完整安装命令当成日常刷新按钮。

更多技术细节见同目录 `DialogueBlip.md`。

## 2026-10-31 角色配置

共享库的 Mono、IPHAP、Sa 池分别使用 `Content/SFX/Blip/` 下同名文件夹中的三条音频。

| Speaker ID | Blip Tag／Voice Pitch |
|---|---|
| ID_Thought、ID_Mail、ID_Monologue | Tag = Mono |
| IPHA-Drone、IPHA-Drone???、IPHA-Projection | Tag = IPHAP |
| Bello、May | Voice Pitch = High，保留原 Tag |
| Fell | Voice Pitch = Low，保留原 Tag |

本次批处理仅覆盖 `Content/Narrative/Dialogues/Levels`，包括 Speakers、Player Speaker Info 和 Party Speaker Info；未列出的角色保持原配置。专用应用命令：`-run=VTGDialogueBlip -SpeakerProfiles`；追加 `-Inspect` 只检查，追加 `-VerifyOnly` 只读核对。备份位于 `Scripts/BlipSpeakerProfiles/Backup/`。
