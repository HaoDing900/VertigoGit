# Narrative 对话嘴部动画

日常配置、相关 BP、修改位置和排查步骤见 [中文使用指南](</G:/Epic/Unreal projects/VertigoGit/Docs/DialogueJawMovement_UserGuide.md>)。本文保留实现与维护说明。

已接入 `/Game/Characters/Sa/ABP_Sa` 和 `/Game/Characters/Sa/ABP_SaCine`。

此外已接入 FemaleClient、Bello、Corvos、Ulises 的四个指定 ABP。各自的 Tag、角色 BP、动画路径与接线位置见中文使用指南第 9 节；原动画资产保持不变，只采样 `c_jawbone_x`。NPC 接线及测试入口为 `VTGJawMovement -NPC`，只读复核增加 `-VerifyOnly -Test`，备份在 `Scripts/NPCJawMovement/Backup`。通用运行时兼容 `IsCutscene`、`Is Cutscene`、`IsCutScene?` 三种已有 Cutscene 变量名。

## 配置

1. 打开 Narrative Dialogue 的 Defaults，在角色的 `Speaker ID` 对应配置中填写 `Speaking Actor Tag`。例如 `SaP` 绑定已有 Player Actor，`SaC` 绑定已有 Cine Actor。玩家发言使用 `Player Speaker Info` 中同名字段。
2. Dialogue 的 `Jaw Movement` 分类：`Enable Jaw Movement` 默认开启；`Jaw Duration Multiplier` 默认 `0.25`。倍数越大，动画持续越长；越小越短。它不改变动画播放速度。0 表示不播放。
3. 选中 NPC 或 Player Dialogue Node，在 `Details - Dialogue Node > Jaw Movement` 中取消 `Enable Jaw Movement`，即可禁用这个节点的主台词及全部随机备选台词。默认开启。Node 不能覆盖全局禁用。

绑定支持 Actor Tags 和 Skeletal Mesh Component Tags。项目现有的 `SaP` 位于 Player 的 `CharacterMesh0` 组件上，也可以直接绑定。Tag 留空或场景中没有匹配时不播放；填写了 Tag 但找不到目标时输出警告。不生成角色，不改动 Narrative 原有 Avatar、镜头或 Montage 绑定。同一世界中多个有效 Actor 使用相同 Tag 时，本句不绑定并输出警告。同一 Actor 多处出现同名 Tag 只算一个目标。

## 播放规则

按变量替换后的实际台词文本估算阅读时间，过滤标点、空白和富文本标签，沿用项目的字符阅读速率、最短阅读时长、当前语言倍率和 Dialogue 阅读倍率，最后乘以 `Jaw Duration Multiplier`。纯标点台词（例如 `...`、`……`、`？！`、`——`）不播放嘴部动画，`你好……` 只按“你好”计数。中英文及数字保留。仅改变嘴部动画预算，不改变显示的台词文本或对话推进时间。

标点过滤对 Sa 和所有已接入 NPC 使用同一套逻辑。该版本通过 62 项运行检查，包括纯标点、文字混合标点、富文本、全角字母/数字，以及从正常台词切换到纯标点时取消嘴部。日志：`Scripts/JawMovement/punctuation-verify.log`。

`Jaw Min Play Rate` / `Jaw Max Play Rate` 默认 `0.7` / `1.2`：每轮开始时随机选择速率，一轮内不变。按动画播放相位累积，不会因为切换速度而跳到半张嘴的位置；掉帧跨过循环边界时也保留正确相位。速度变化不延长总播放预算。

`Jaw Min Amplitude` / `Jaw Max Amplitude` 默认 `0.5` / `1.0`：每轮抽取目标强度，使用平滑插值连续变化。1 为原动画张嘴幅度，0.5 为 jaw 相对于动画闭嘴姿势的旋转角度减半，0 为动画闭嘴姿势。不会缩放骨骼，不会改写 FBX，不会通过降低整层权重把 Montage 的 jaw 动作混回来。要关闭随机变化，把对应 Min 和 Max 设置成相同值。

每句重新抽取随机参数；同一帧内重复求值不会反复随机。上述范围保存在 Dialogue Defaults 中，可以分别调整。

随机变化版本已通过 51 项运行检查，包括循环内速度稳定、循环间速度变化、幅度连续、重复求值一致、时间预算不被延长、实际张嘴角度减半，以及半幅度时仍优先于 Montage。35 个项目 Dialogue 的默认配置复核通过：34 个继承 `0.25`，Base 自定义的 `0.2` 保留。日志：`Scripts/JawMovement/random-final-verify.log`。

新句开始时取消上一句；跳句、实际结束台词、结束对话时立即取消。正常耗尽预算时有短淡出；开始有短淡入。没有跨句残留的 Timer。

选项列表展示时不播放。**项目原本会立即推进默认阅读时长的 Player 选项，本实现保留该节奏，所以这种选项不会出现可见的持续开口。** 如需选中后让玩家实际开口停留，使用节点现有的固定时长、等待音频结束或手动推进模式。

ABP 的 `IsCutscene` 开启时取消当前说话。关闭后不会补播旧句。Sequencer 替换 AnimInstance 时，这个 ABP 模块也不参与该替代实例的输出。

## 动画与优先级

使用 `/Game/Characters/Sa/Anm/Expression_JawMovement`，对应 `Content/Characters/Sa/Anm/Expression_JawMovement.uasset`。

原 FBX 导入包含 192 个骨骼轨道、11 个有变化的轨道（jaw 和嘴唇）以及 1 条浮点曲线。原 FBX 未修改；项目内动画已清理为唯一 `c_jawbone_x` 轨道、无曲线，长约 0.333 秒，首尾姿势一致。清理前资产备份保存在 `Scripts/JawMovement/Expression_JawMovement.raw-import.uasset`。

现有 Sa 动画图有 Montage 位于眨眼之后，因此模块插入最终 Montage 之后、组件空间视线修正之前。运行时只替换 jaw 的局部变换，不写其他骨骼、曲线或属性，不使用 Montage Slot，不中断现有 Montage。眨眼链保持原样。Jaw 的权重淡入、淡出期间与原姿势混合，满权重时优先于 Montage 的 jaw。

两个 ABP 修改前的备份在 `Scripts/JawMovement/ABP_Sa.before.uasset` 和 `ABP_SaCine.before.uasset`。

## 验证

UE 5.3 `VertigoEditor Win64 Development` 编译。

应用/修复（幂等）：`UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGJawMovement -Test -unattended -nullrhi -nosound -nop4`

只读复核：增加 `-VerifyOnly`。`-Inspect` 导出两个 ABP 并报告导入轨道；动画缺失时会导入。

`-UpdateDefaults` 将旧 `0.5` 配置更新为 `0.25`，保留其他自定义倍率；继承原生默认值的 Dialogue 自动使用 `0.25`。实际需要修改的资产会先备份到 `Scripts/JawMovement/DefaultsBackup`。`-VerifyDefaults` 只读检查项目内 Dialogue 是否还有 `0.5`。

检查覆盖两个实际角色的最终骨骼输出、其他局部骨骼（含眼部）保持不变、与 FullBodySlot Montage 同时播放且 jaw 优先、全局/节点开关、空白文字、重复 Tag、取消、自然到期、Cutscene 切换和对话销毁。自动检查使用 NullRHI，不代替场景中的视觉审阅。

组件 Tag 修复后的复核通过 40 项检查。额外的 `-PlayBase` 模式通过 `UNarrativeComponent::BeginDialogue` 播放保存的 `/Game/Narrative/Dialogues/Levels/Base`，使用 Player 原有的 `CharacterMesh0` 上的 `SaP`，不添加测试 Actor Tag；已验证嘴部模块激活且最终 jaw 角度随时间变化。`-DiagnoseBase` 可只读导出 Base / BaseLevel 和配置诊断。复现与修复日志为 `Scripts/JawMovement/base-play.log`（修复前）和 `base-fixed.log`（修复后）。
