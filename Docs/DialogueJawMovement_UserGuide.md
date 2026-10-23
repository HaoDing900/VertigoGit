# Narrative Jaw Movement 使用指南

这套功能让已接入的角色在 Narrative 台词播放时循环张嘴，并加入随机速度和张嘴幅度。当前支持 Sa Player / Cine、FemaleClient、Bello、Corvos 和 Ulises。它是说话的视觉示意，不做音素口型同步。日常使用只需要配置 Dialogue 和角色 Tag，不需要给每个节点添加 Narrative Event。

## 1. 需要哪些 BP 和资产

| 用途 | Unreal Content Browser 路径 | 需要检查什么 |
| --- | --- | --- |
| 可操作的 Sa | `/Game/ThirdPerson/Blueprints/BP_Player_Sa` | 角色 Mesh 使用 `ABP_Sa`，场景内有对应实例 |
| Cine 版 Sa | `/Game/Characters/Sa/BP_Cine_Sa` | 角色 Mesh 使用 `ABP_SaCine`，场景内有对应实例 |
| Player 动画蓝图 | `/Game/Characters/Sa/ABP_Sa` | 已接入 Jaw Movement 动画节点 |
| Cine 动画蓝图 | `/Game/Characters/Sa/ABP_SaCine` | 已接入同一套 Jaw Movement 节点 |
| 张嘴动画 | `/Game/Characters/Sa/Anm/Expression_JawMovement` | 当前只含 `c_jawbone_x` 轨道，约 0.333 秒一个循环 |
| 要播放的对话 | 你当前使用的 Narrative Dialogue 资产，例如 `/Game/Narrative/Dialogues/Levels/Base` | 配置角色 ID、Tag、全局开关和参数 |
| 对话触发位置 | 当前关卡的 Level Blueprint，或原有交互 BP | 沿用现有 `Begin Dialogue` 播放流程 |

Content Browser 中 `/Game` 就是项目的 `Content`。例如动画文件在磁盘上的位置是 `Content/Characters/Sa/Anm/Expression_JawMovement.uasset`。

**不用手动给角色添加 Jaw Component，也不用新建 Jaw Montage。** 系统会在成功绑定角色、开始说话时创建运行时组件。只添加 Tag 并不能让任意角色自动获得这个功能：角色当前使用的 ABP 里也必须有 Jaw Movement 节点。

## 2. 最快的配置方法：让 Sa Player 开口

1. 打开你要播放的 Narrative Dialogue，例如 `Base`，进入其 Defaults / Class Defaults。
2. 在 `Speakers` 中找到实际发言的角色 ID，例如 `Sa`。
3. 展开该角色的 `Jaw Movement` 配置，把 `Speaking Actor Tag` 填成 `SaP`。
4. 在 Dialogue 的 `Jaw Movement` 分类确认 `Enable Jaw Movement` 开启。
5. 选中一条有文字的 NPC Dialogue Node，确认它的 Speaker ID 是 `Sa`，并且节点自己的 `Enable Jaw Movement` 也开启。
6. 保存 Dialogue，在 Level BP 或现有交互逻辑中照常调用 `Begin Dialogue`，选择这个 Dialogue 资产，然后进入 PIE 测试。

这里的对应关系是：

```text
Dialogue Node 的 Speaker ID：Sa
        ↓ 查找 Dialogue Defaults 中的角色配置
Sa 的 Speaking Actor Tag：SaP
        ↓ 在当前世界寻找匹配 Tag 的角色或骨骼网格组件
BP_Player_Sa 的 CharacterMesh0（Component Tags 包含 SaP）
        ↓
ABP_Sa 的 Jaw Movement 节点输出嘴部动画
```

`Sa` 是 Narrative 的角色 ID，`SaP` 是场景绑定用的 Tag，两者不需要同名。

如果要让 Cine 版 Sa 说话，将这个 Speaker 的 `Speaking Actor Tag` 填成 `SaC`，并确认对应 Cine 实例的 Actor Tags 或骨骼网格 Component Tags 中有 `SaC`。同一世界应只有一个角色匹配该绑定 Tag。

### Actor Tags 和 Component Tags 去哪里看

打开角色 BP 或选中关卡中的角色实例：

- **Actor Tags**：在角色自身的 Details / Class Defaults 中搜索 `Tags`。
- **Component Tags**：在 Components 面板选中角色的 Skeletal Mesh，再在 Details 中搜索 `Component Tags`。

系统同时支持这两种 Tag。项目里已经确认 Player 的 `SaP` 位于 **CharacterMesh0 的 Component Tags**，不需要再复制到 Actor Tags。

修改场景实例的 Tag 只影响这个实例；修改角色 BP 的默认 Tag 会影响使用其默认值的实例。不要让 Player 和 Cine 同时使用同一个绑定 Tag，否则系统无法确定谁应该开口，本句会跳过绑定并输出警告。

## 3. 全局开关、Node 开关与特殊台词

| 想实现的效果 | 设置位置 | 操作 |
| --- | --- | --- |
| 整个 Dialogue 都不驱动嘴部 | Dialogue Defaults → `Jaw Movement` | 关闭 `Enable Jaw Movement` |
| 只有一个 Node 不张嘴 | 选中 Node → `Details - Dialogue Node` → `Jaw Movement` | 关闭该 Node 的 `Enable Jaw Movement` |
| 某个 Speaker 从来不驱动场景角色 | Dialogue Defaults → 对应 Speaker | `Speaking Actor Tag` 留空 |
| 内心独白、旁白不张嘴 | 对应 Speaker 或对应 Node | Tag 留空，或关闭 Node 开关 |
| 台词来自 Player Dialogue Node | Dialogue Defaults → `Player Speaker Info` | 配置这里的 `Speaking Actor Tag`，例如 `SaP` |

两个开关默认都开启。Node 只能进一步禁用，不能覆盖全局关闭。Node 开关作用于该节点的主台词和所有随机备选台词。

**“画面里是玩家 Sa”不代表一定要用 Player Speaker Info。** 如果写的是普通 NPC Dialogue Node，并且 Speaker ID 选的是 `Sa`，就配置 `Speakers` 中的 `Sa`；只有真正的 Player Dialogue Node 才使用 `Player Speaker Info`。

空白台词、只有空格或换行的台词，以及只有标点的台词都不会张嘴。`...`、`……`、`？！`、`——` 表示沉默时可直接使用；`你好……` 只按“你好”计算嘴部时长，`“真的吗？”` 只按“真的吗”计算。中英文、数字照常计入；富文本包裹的纯标点同样保持静默。这个过滤只影响嘴部，不改变台词显示或对话推进时间。

选项列表展示时也不会张嘴。项目原有的默认阅读时长 Player 选项会在选中后立即推进，因此没有可见的持续开口。若希望选中后让 Sa 留下来实际说一句话，使用已有的固定时长、等待音频结束或手动推进模式。

## 4. 参数怎么调

以下参数都在 **Dialogue Defaults → Jaw Movement** 中，按整个 Dialogue 配置。

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `Enable Jaw Movement` | 开启 | 控制整个 Dialogue 是否启用 |
| `Jaw Duration Multiplier` | `0.25` | 控制张嘴持续多久；越大越长，越小越短，0 不播放 |
| `Jaw Min Play Rate` | `0.7` | 每轮随机播放速度的下限 |
| `Jaw Max Play Rate` | `1.2` | 每轮随机播放速度的上限；1 为动画原速 |
| `Jaw Min Amplitude` | `0.5` | 随机张嘴强度的下限 |
| `Jaw Max Amplitude` | `1.0` | 随机张嘴强度的上限；1 为动画原有张嘴幅度 |

**持续时间、播放速度和幅度是三件独立的事：**

- 持续时间按实际台词的字符阅读时间估算，再乘以 `Jaw Duration Multiplier`。估算会过滤标点、空白和富文本标签，并沿用项目语言、阅读速率及 Dialogue 阅读倍率。它不会强制延长台词停留时间。
- 速度在每个 loop 开始时抽取一次，一轮内保持不变。下一轮可能变快或变慢，但总持续时间不变。
- 幅度在每轮随机目标之间平滑变化。它缩减的是 jaw 相对于动画闭嘴姿势的旋转角度，不是缩小骨骼，也不是降低整个动画层的权重。

例如一条台词的估算阅读时间是 8 秒，倍率 `0.25` 时，嘴部动画预算约为 2 秒。如果台词提前结束或被跳过，会立刻取消；不会为了播满 2 秒阻止对话推进。

### 几种常用调整

| 需求 | 调法 |
| --- | --- |
| 开口持续时间更长 | 增大 `Jaw Duration Multiplier` |
| 嘴动得慢一些 | 同时降低 Min / Max Play Rate |
| 嘴张得小一些 | 同时降低 Min / Max Amplitude |
| 关闭速度随机，固定原速 | Min / Max Play Rate 都设为 `1.0` |
| 关闭幅度随机，固定七成幅度 | Min / Max Amplitude 都设为 `0.7` |
| 完全关闭该 Dialogue 的系统 | 关闭全局开关 |

调整幅度不会削弱嘴部相对于 Montage 的优先级。短句可能只有一两个循环，随机速度的差异会比长句不明显。

默认值已统一为 `0.25`。上次复核时 Base 有自定义倍率 `0.2`，该值按约定保留；若希望 Base 也使用 `0.25`，直接在它的 Defaults 中修改。

## 5. 去哪里看修改过的部分

### 在动画蓝图里查看

分别打开 `ABP_Sa` 和 `ABP_SaCine`，进入 **AnimGraph**，查找：

```text
节点标题：Narrative Jaw Movement (Jaw Only)
节点注释：VTG Narrative Jaw Only
```

可以使用蓝图编辑器的查找功能搜索 `Narrative Jaw`，或在输出端附近查看。节点位置在最终 Montage 之后、组件空间视线修正之前，其输入输出是一条 Pose 链。节点的 `Sequence` 指向 `Expression_JawMovement`，`Jaw Bone` 为 `c_jawbone_x`。

原 Sa 图中有 Montage 位于眨眼之后，所以 Jaw 节点没有简单放到最早的眨眼输入前。现在的接法保证最终输出时 jaw 高于 Montage，且节点只改 jaw，原有眨眼链保留。

这个节点是 C++ 提供的原生动画节点，因此在 BP 的 Event Graph 中看不到完整的随机和计时逻辑；它们在下面的代码文件中。

### 代码位置与职责

| 文件 | 修改内容 / 主要入口 |
| --- | --- |
| [Dialogue.h](</G:/Epic/Unreal projects/VertigoGit/Plugins/Narrative/Source/Narrative/Public/Dialogue.h>) | `SpeakingActorTag`、Dialogue 全局开关、持续倍率、速度及幅度范围 |
| [DialogueSM.h](</G:/Epic/Unreal projects/VertigoGit/Plugins/Narrative/Source/Narrative/Public/DialogueSM.h>) | Dialogue Node 的 `bEnableJawMovement` |
| [Dialogue.cpp](</G:/Epic/Unreal projects/VertigoGit/Plugins/Narrative/Source/Narrative/Private/Dialogue.cpp>) | 台词开始、结束、跳句和对话销毁时调用 `StartJawMovement` / `StopJawMovement` |
| [DialogueJaw.cpp](</G:/Epic/Unreal projects/VertigoGit/Plugins/Narrative/Source/Narrative/Private/DialogueJaw.cpp>) | 用 Speaker 配置查找 Actor / Mesh Tag，估算持续时间并启动组件 |
| [NarrativeJawComponent.cpp](</G:/Epic/Unreal projects/VertigoGit/Plugins/Narrative/Source/Narrative/Private/NarrativeJawComponent.cpp>) | `SamplePlayback`：循环相位、每轮随机速度、平滑幅度、到期及取消 |
| [AnimNode_NarrativeJaw.cpp](</G:/Epic/Unreal projects/VertigoGit/Plugins/Narrative/Source/Narrative/Private/AnimNode_NarrativeJaw.cpp>) | `PreUpdate` 检查 Cutscene 并采样；`Evaluate_AnyThread` 只更新 jaw 的最终姿势 |
| [AnimGraphNode_NarrativeJaw.h](</G:/Epic/Unreal projects/VertigoGit/Plugins/Narrative/Source/NarrativeDialogueEditor/Public/AnimGraphNode_NarrativeJaw.h>) | 蓝图编辑器中显示的 Jaw 动画节点标题和节点定义 |
| [VTGJawMovementCommandlet.cpp](</G:/Epic/Unreal projects/VertigoGit/Source/VertigoEditor/Private/VTGJawMovementCommandlet.cpp>) | 动画导入与清理、两个 ABP 的接线、默认值迁移和自动验证 |

NPC 的接线与验证在 [VTGNPCJawMovement.cpp](</G:/Epic/Unreal projects/VertigoGit/Source/VertigoEditor/Private/VTGNPCJawMovement.cpp>)，由同一个 `VTGJawMovement` Commandlet 的 `-NPC` 模式调用。

运行时组件是 `UNarrativeJawComponent`。一般无需在 BP 中手动操作；暂停 PIE 并查看对应角色的运行时组件时，可以用于确认是否已经成功启动过绑定。

### 动画资产里查看

打开 `Expression_JawMovement`，检查骨骼轨道 `c_jawbone_x`。原 FBX 没有修改；项目内动画清理掉了其他骨骼轨道和曲线。随机速度与幅度是运行时计算的，所以不会显示成这个 AnimSequence 中新增的动画曲线。

## 6. 与 Montage、眨眼和 Cutscene 的关系

- 系统不占用 Montage Slot，也不会停止当前 Montage。只在嘴部层有权重时接管 jaw。
- 嘴部层只修改 `c_jawbone_x` 的局部变换，不复制动画中的其他骨骼、表情曲线或属性。身体动作和眨眼继续由原图负责。
- ABP 的 Cutscene 标志为 true 时取消当前说话；切回 false 不会补播旧句。下一条有效台词可以重新启动。兼容项目已有的 `IsCutscene`、`Is Cutscene` 和 `IsCutScene?` 三种变量名。
- 正常启动和耗尽预算时有短淡入、淡出；跳句、结束台词或关闭对话时立即取消。

不需要为普通台词额外发送“开始张嘴 / 停止张嘴”的 Narrative Event。

## 7. 没有张嘴时，按这个顺序检查

1. **Speaker 配对**：Node 的 Speaker ID 是否对应你填写 Tag 的那个配置项？是否把普通 `Sa` 台词误填到了 Player Speaker Info？
2. **两个开关**：Dialogue 全局开关和 Node 开关是否都开启？
3. **实际场景目标**：PIE 时角色是否存在？匹配的是当前角色实例的 Actor Tags 或 Skeletal Mesh Component Tags；不是 Actor Label、BP 名称或 Gameplay Tags。
4. **Tag 是否唯一**：是否同时有两个角色匹配 `SaP` / `SaC`？重复会跳过绑定。
5. **当前动画实例**：选中角色 Mesh，检查 Animation Mode / Anim Class 是否正在使用已接线的 `ABP_Sa` 或 `ABP_SaCine`。Sequencer 控制或其他动画实例可能接管输出。
6. **Cutscene 状态**：运行时 ABP 的 `IsCutscene` 是否为 true？
7. **文字和时间**：台词是否为空，或是不是立即推进的 Player 选项？持续倍率是否为 0，幅度是否被设为 0？
8. **最新代码**：修改过 C++ 后重新编译并重新打开编辑器，确保加载了新 DLL。

在 Output Log 搜索 `Narrative jaw`：

```text
no actor or skeletal mesh component with tag ...
```

表示填写了 Tag，但当前世界没有匹配角色或 Mesh。

```text
matches multiple actors; no jaw binding ...
```

表示匹配到多个角色。需要给它们使用不同的绑定 Tag。

这些警告不是所有禁用情况都会输出：例如全局关闭、Node 关闭、Tag 留空、空白台词本来就会直接跳过。

## 8. 测试与维护入口

日常测试可以在 Base Level 用 `Begin Dialogue` 播放 Base：给 `Sa` 配置 `SaP`，写一条有足够文字的台词，观察嘴部；然后分别测试跳句、关闭某个 Node、播放身体 Montage 和切换 Cutscene。

如需代码层验证，使用 UE 5.3 的 `UnrealEditor-Cmd.exe`，在项目目录运行以下参数组合：

```text
Vertigo.uproject -run=VTGJawMovement -VerifyDefaults -VerifyOnly -Test -unattended -nullrhi -nosound -nop4
```

这是默认值与运行逻辑的只读复核，不重新接线、不重导动画。随机版本最近一次通过 51 项运行检查，日志为 [random-final-verify.log](</G:/Epic/Unreal projects/VertigoGit/Scripts/JawMovement/random-final-verify.log>)。NullRHI 检查覆盖最终骨骼输出，但仍需要在场景里观察视觉效果。

完整的导入、备份、维护命令说明见 [DialogueJawMovement.md](</G:/Epic/Unreal projects/VertigoGit/Docs/DialogueJawMovement.md>)。一般使用时无需运行应用 / 修复命令，也无需恢复旧 ABP 备份。

## 9. FemaleClient、Bello、Corvos、Ulises 的配置

四个 NPC 沿用本指南前面的 Dialogue 开关、Node 开关和随机参数。打开 Dialogue Defaults，在实际发言的 Speaker 配置中填写下表的 `Speaking Actor Tag`，即可驱动对应场景角色。NPC 必须已存在，且运行时使用下表的 ABP；系统不会自动生成角色或自动改写所有 Dialogue 的 Speaker 配置。

| Speaking Actor Tag | 角色 BP | 已接入 ABP | 使用的嘴部动画 |
| --- | --- | --- | --- |
| `FemaleClient` | `/Game/Characters/NPC/FemaleClient/BP_FemaleClient_StillNPC` | `/Game/Characters/NPC/FemaleClient/ABP_FemaleClientLS` | `/Game/Characters/NPC/FemaleClient/ANM/ANM_FemaleClient_Mouth` |
| `Bello` | `/Game/Characters/NPC/Bello/BP_Bello_NPC` | `/Game/Characters/NPC/Bello/ABP_Bello_NPC` | `/Game/Characters/NPC/Bello/ANM_Bello_Mouth` |
| `Corvos` | `/Game/Characters/NPC/Corvos/BP_Corvos_NPC` | `/Game/Characters/NPC/Corvos/ABP_NPC_Corvos` | `/Game/Characters/NPC/Corvos/ANM_Corvos_Mouth` |
| `Ulises` | `/Game/Characters/Ulises/BP_Ulises_NPC` | `/Game/Characters/Ulises/ABP_Ulises` | `/Game/Characters/Ulises/ANM/Mouth_Ulises` |

已验证原有 Tag：FemaleClient、Ulises 的角色和 Mesh 上都有对应 Tag；Bello、Corvos 使用 Actor Tag。无需额外添加 Jaw Component 或复制 Tag。

例如一条 Node 的 Speaker ID 是 `NPC_1`，但实际要让 Bello 发言，就把 **Speakers → NPC_1 → Speaking Actor Tag** 设为 `Bello`。Speaker ID 不必改名成 Bello。

在四个 NPC 的 AnimGraph 输出端，紧邻最终 `Output Pose` 的输入前，可以找到同名节点 `Narrative Jaw Movement (Jaw Only)`，注释仍为 `VTG Narrative Jaw Only`。该位置位于已有 Montage、Look At 和面部处理之后，只接管 `c_jawbone_x` 的局部变换，其他骨骼与曲线原样通过。

四个动画各自使用自己的骨架，循环时长约为 FemaleClient 0.292 秒、Bello 0.280 秒、Corvos 0.333 秒、Ulises 0.320 秒，首尾 jaw 姿势匹配。原动画轨道未清理或重写；模块只采样 jaw，其余嘴唇、眼部、身体轨道和曲线不进入覆盖层。

FemaleClient、Bello、Ulises 的 Cutscene 变量名为 `Is Cutscene`，Corvos 为 `IsCutScene?`。系统已兼容这些命名，不需要重命名原变量。

NPC 只读复核参数：

```text
Vertigo.uproject -run=VTGJawMovement -NPC -VerifyOnly -Test -unattended -nullrhi -nosound -nop4
```

这会检查四个已保存的 ABP、动画兼容性，并用实际角色 BP 的原有 Tag 测试嘴部输出、眼部及其他骨骼保持不变、Montage 同播、全局/Node 开关和 Cutscene 切换。四个 ABP 修改前的备份位于 `Scripts/NPCJawMovement/Backup`。不带 `-VerifyOnly` 会应用接线（幂等），日常使用无需运行此命令。

四个 NPC 的保存后复核通过 52 项检查，结果见 [NPC 验证日志](</G:/Epic/Unreal projects/VertigoGit/Scripts/NPCJawMovement/verify.log>)。
