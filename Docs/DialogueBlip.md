# Dialogue Blip 配置与行为

`/Game/Narrative/Audio/DA_DialogueBlipLibrary` 当前包含 BlipNPC、Sa、Mono、IPHAP 池。BlipNPC 使用 `Content/SFX/Blip/BlipSFX_NPC1–3`；其余三个池各使用该目录下同名子文件夹的三条 SoundWave。角色映射与最新批量配置命令见 `DialogueBlip_Maintenance.zh-CN.md`。

## 编辑位置

1. 打开 `DA_DialogueBlipLibrary`，在 Details 的 **Blip Pools** Map 中新增键（Name）即可增加池；键就是 Blip Tag，不需要改图。
2. 每个池可设置 Sources、Pitch Min/Max、Volume Min/Max、Avoid Immediate Repeat。常用 1–3 个短素材，不硬限制数量；空项忽略，重复引用去重，循环素材不参与选择。单素材允许重复。
3. Dialogue Defaults → Speakers / Player Speaker Info → **Blip Tag** 指定音色池；与 Jaw 的 Speaking Actor Tag 无关。现有 VTG Player 角色配置为 Sa，NPC 根据既有身份迁移。新 Speaker 未填时使用 BlipNPC。
4. Dialogue Defaults → **Dialogue Blip → Enable Blips** 是对话总开关；选中节点 → **Blip → Enable Blip** 是节点开关，对主台词和备选台词都有效。都不会关闭已指定配音或 Jaw 动画。
5. 当前实际台词的 **Dialogue Sound** 就是配音 override，沿用原字段，包括 Alternative Lines 各自的音频。不新增第二个 Speaker Audio 字段。
6. Project Settings 的 Narrative Dialogue Settings → **Default Blip Library** 给新对话提供共享库。对话自身的 Blip Library 可覆盖它。现有对话已直接引用该库，供打包收集。

## 路由

### 角色 Voice Pitch

Dialogue 的 Class Defaults → Speakers → 对应角色 → Blip → **Voice Pitch**；玩家角色在 Player Speaker Info 的同名字段。

- **Default (Unchanged)**：不额外变调，保持音效池原有的随机 Pitch。旧资产未填写该字段时自动使用此值。
- **High (+3 semitones)**：随机 Pitch × 1.189207。
- **Low (-3 semitones)**：随机 Pitch × 0.840896。

这是每个 Dialogue 的 Speaker 设置，不是池的共享属性，也不根据姓名或性别自动推断。它只作用于 Blip；当前实际 Line 的 Dialogue Sound override 不受影响。普通播放倍率变调也会改变速度／时长；当前三条默认素材在 Low 与默认随机范围下仍能容纳于 3 秒保护上限内。

独立回归入口：`-run=VTGDialogueBlip -TestOnly`，不扫描迁移或保存资产，也不要求用户角色 Tag 保持最初分配。旧 `-VerifyOnly` 包含历史迁移断言，不适合已经自行调整角色 Tag 后的通用验证。

角色 Tag 批量配置此前由项目的 `VTGDialogueBlip` 编辑器 Commandlet 扫描 Dialogue 资产完成：更新 Speaker 元数据及其模板、备份并保存。这是一次性批量编辑，不是持续同步角色身份的注册表，也不是在 UI 中逐项手填。目前该迁移工具包含早期 Sa／默认池迁移规则；新增其他角色的批量映射应先明确角色身份、目标 Tag 与是否覆盖已有值，再扩展工具，避免直接重跑覆盖设计师的设置。

- 实际选中且完成文本变量替换的 Line 有 Dialogue Sound：只走原配音逻辑，Blip 不参与。
- 未填 Tag 或 Tag 不存在：查 BlipNPC；Tag 存在但没有有效素材：静音，不再回退。
- 空白、纯标点、富文本标签不触发；中英文及数字有效。共用 Jaw 的 CountVisibleCharacters，不依赖 Jaw 启用状态、Actor 绑定或模型。
- 内心独白等有文字但无需音效的节点使用 Enable Blip 关闭。
- 每次真实播放新句调用一次。UI 刷新、选项展示和历史记录不触发。同一节点或相同文本再次正常播放不会被去重。
- 选择器 `UNarrativeBlipLibrary::Choose` 可在 Blueprint 中调试，返回 Silence / VoiceOverride / Blip、解析 Tag、选中 Sound、Pitch、Volume。它只选取，不播放、不修改共享资产。

## 生命周期与优先级

Blip 使用对话实例持有的独立 2D 音频组件；不写入 DialogueSound，不绑定 EndCurrentLine，不参与阅读时长或音频结束推进。普通结束不截断短 Blip；下一句开始（包括静音句）、手动跳句、取消或退出对话会清理旧 Blip。暂停跟随现有对话音频暂停入口。

按住快速跳过会立即取消当前 Blip，并抑制后续 Blip；松开仅从后续新句恢复。现有开发控制台 `Narrative.FastForward` 也会抑制 Blip。自动播放 / 文字速度不等于快进。

以下声音已接入低优先级让位：

- Narrative 当前台词的显式配音。
- `VTGUIFeedbackLibrary` 的互动和 Quest 提示音（包括三个 Quest 事件）。
- `BP_VTG_AudioManager` 的 Audio_PlayOneShot / Audio_PlayPsych，覆盖相应 Narrative Event 调用。

这些声音开始时会停止当前 Blip；只要任一已登记组件仍播放或暂停，新句 Blip 就丢弃。结束后不补播、不排队。音乐、环境声、脚步声不自动压制。

**自定义剧情事件**如果直接用其它音频组件播放，需要在开始播放后调用 **Register Priority Audio(AudioComponent)**。原 `PlaySound2D` 没有返回组件，需使用 `SpawnSound2D` 并传入返回值。系统不会监听引擎所有声音，也不会自动修改任意自定义事件。

Max Blip Seconds 当前为 3 秒，仅是误填长音效的保护上限，不是台词时长。只有 Blip 最终输出音量额外乘以 0.3；配音 override、Quest、互动与剧情音效不受此系数影响。

## 实现与验证

- 插件：`NarrativeBlipLibrary.h/.cpp`（数据与无状态优先级桥接）、`DialogueBlip.cpp`（每对话播放器）。没有新增常驻 Actor、全局管理器或播放 Tick。
- 插件小范围扩展：FSpeakerInfo.BlipTag、节点 Enable Blip、UDialogue 生命周期钩子、共享库默认设置。
- 项目：现有 Skip UI、互动／Quest 音效、关卡 OneShot/Psych 接入优先级桥接。
- 初始化／迁移：`-run=VTGDialogueBlip`。只读复核：追加 `-VerifyOnly`。加 `-InstallNPC` 导入并配置三条 NPC 素材，配合 `-VerifyOnly` 验证引用与时长；不再要求音效库为空。
- 资产修改前备份：`Scripts/DialogueBlip/Backup/`，按原包路径保存。
- 自动检查使用内存中的无波形 SoundWave 元数据对象测试选择，不生成或保存音频素材。包含空池、回退、语音优先、文本过滤、空／循环／重复引用、单素材、不连续重复、范围倒置、快进、打断、暂停与时序隔离。

2026-10-30 验证：UE 5.3 Development Editor 编译通过；重新加载后 35 个 Dialogue 资产复核通过，均保留原生 PlayDialogueNode 集成入口；32 项路由与生命周期检查通过；所有池的 Sources 仍为空。日志为 `Saved/Logs/DialogueBlipVerify.log`。未填素材，因此未进行听感验证；本轮未在可视编辑器中逐项审阅 Details 布局。

已知范围：单人游戏的本地反馈；不新增网络同步。任意 Blueprint 若完全重写 PlayDialogueNode 且不调用 Parent，需要保留该集成入口。旧音频管理器重建工具若重建图，需要重新执行本迁移以恢复优先级接线。

## BlipNPC 素材接入更新

默认池及未填／未知 Tag 的回退标识现为 **BlipNPC**。现有 Speaker 的独立 BlipTag 若为 NPC，已迁移至 BlipNPC；没有修改场景 Actor Tags，也没有重命名 Sa 或其他专属池。

Blip 播放器在最后输出时将池内随机音量乘以 **0.3**，默认池最终为 0.27–0.30。源 WAV、SoundWave 音量和池的随机范围保持不变。

三条 WAV 长约 0.946 / 2.222 / 0.712 秒。3 秒保护上限为第二条及其降调播放留出余量；下一句或高优先级声音仍可按既定规则提前中断。
