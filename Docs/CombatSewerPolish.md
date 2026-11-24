# Sa 出拳、闪避衣料声与 Sewer Boss 调整

## 出拳向下修正

ABP_Sa 新增最终脊椎修正 `VTG_PunchDown15`，在实际空手出拳 Montage 播放期间，将上半身向玩家朝向的下方旋转 15°。停止攻击后平滑归零，退出 combat 后立即清除。不会修改相机、Actor Transform、原始动画、其他武器或基础战斗站姿。

参数：ABP_Sa → Class Defaults → Combat Posture → **Punch Down Degrees**，默认 15。其余已有 Combat Spine X Degrees 与 Aim Offset Clamp 保留。

实际骨骼求值验证：开启前后脊椎世界旋转差为 Pitch -15°，Yaw / Roll 为 0，误差 0°。

## Dodge 随机衣料声

新 Cue：`/Game/SFX/Character/SaCombat/SC_Sa_DodgeCloth`。

随机池为用户指定的三份 SoundWave：Stereo 的 light_fast_movement_13 / 15，加 Mono 的 light_fast_movement_13。等权重随机，启用 Without Replacement；每次闪避播放其中一份，不会三份同时播放。原 SoundWave 不修改。

挂载在 Sa 现用的两个 Montage 的 0.01 秒，通知名 `VTG_DodgeCloth`：

- ANM_Sa_Dodge_F2_Montage
- ANM_Sa_Dodge_B_Montage

Cue 音量倍率 0.55，使用现有玩家脚步衰减，通知跟随玩家。该声音独立于 Ambient / Music。

## Freemove NPC_2 卡住

已复现的失败路径：全局 Auto 关闭 → 文本计时结束 → NPC_2 等待手动 Continue。NPC_2 实际连着 NPC_3，并不是断线；Action 短评没有普通对话的手动继续入口，因而看起来卡住。

新增 Dialogue 配置 **Always Auto Advance**，默认关闭。只为 `L3Sewer_Freemove` 开启，确保游走短评不被全局 Auto OFF 挂住。普通剧情对话仍保留手动模式。验证了 NPC_2 → NPC_3 → 对话正常退出。

## Boss 血条等待入场对白

`L3BossEncounter` 的 StartBossEncounter 调用启用 Wait For Intro Dialogue，在进入战斗前隐藏 Boss HUD。

KillCount >= 4 的既有两分支及台词不变：

- True：L3Sewer_Still_DialogueNode_NPC_20
- False：L3Sewer_Still_DialogueNode_NPC_19

两处 Begin Dialogue 后均登记当前对话实例。只有该实例的 OnDialogueFinished 才解除隐藏；其他对话的结束通知不会误解除。自然结束或强制退出都走正常清理。启动失败且没有有效对话时解除隐藏，避免永久丢失血条。

这里只增加 HUD 显示条件，保留已有战斗时机。读档恢复不重播入场对白，C++ StartBossEncounter 默认不等待对白。原有死亡隐藏与脱战处理保留。

## 酸液远程攻击

| 参数 | 调整前 | 调整后 |
|---|---:|---:|
| AcidSpit 发动最大距离 | 1800 cm | 3600 cm |
| 弹丸最大飞行距离 | 1800 cm | 3600 cm |
| 碰撞球半径 | 24 cm | 48 cm |
| 可见核心 / 原静态网格缩放 | 0.24 | 0.48 |
| 原 AcidVisual 组件缩放 | 1.2 | 2.4 |

这里的放大按三个方向的尺寸各 ×2 处理。当前使用的可见核心材质、原 Niagara 启用状态、伤害、速度、冷却与命中特效保留。弹丸依旧在到达最大距离后销毁；现有 5 秒寿命足以覆盖 850 cm/s 下的 36 米距离。

## 验证与维护

Editor Development 编译通过。ABP_Sa、L3Sewer_Freemove、BPLM_SewerUnderApt 和 BP_SeaMonster_AcidProjectile 编译均为 0 错误、0 警告。独立进程重新加载后检查 Montage 通知、Cue、图接线和保存值，并运行骨骼、真实对话推进、HUD 绑定与弹丸清理检查。

原有 Sewer Encounter 回归、Dialogue History / 手动与自动推进回归通过。未开启音频设备或进行完整游戏内观感验收。

更新工具：`-run=VTGCombatSewerPolish`。`-Verify` 只验证资产，`-Tests` 加运行检查，`-Inspect` 输出当前参数。改动前资产备份与日志位于 `Scripts/CombatSewerPolish`。

## 2026-11-21 Boss 入场对白与尸体收尾

### 入场对白不再被抢

原来 `L3BossEncounter` 直接调 `Begin Dialogue`。如果此刻还有别的对白占着场面，`Begin Dialogue` 返回 false，`WaitForBossIntroDialogue` 拿到 `bDialogueStarted=false` 就立刻放出血条，NPC_19 / NPC_20 这句永久丢失。

现在分两层兜住，两条路都保留原有的"说完才放血条"行为：

- `StartBossEncounter(bWaitForIntroDialogue=true)` 在隐藏血条之后，按名字取 BPLM 上的 `Narrative`，若有对白在播就先 `TryExitDialogue`。可退出的对白（`L3Sewer_Freemove`，`bCanBeExited=1`）就此让位，入场对白照原时机播。
- 不能退出的对白（`L3Sewer_Still`，`bCanBeExited=0`，第一次看到蛋那句走的就是它）退不掉，于是登记一次延后重试：等它自然结束，隔一帧重新 `ProcessEvent` 原有的 `L3BossEncounter`，整条链（含杀蛋数分叉）照常跑一遍。只重试一次，失败就按原逻辑放血条，不会循环。

延后一帧是必要的：在 `OnDialogueFinished` 里直接 `Begin Dialogue` 会递归进正在收尾的对白组件；回调里也挡掉 `bStartingNewDialogue=true` 的同帧通知。蓝图接线、对白资产和血条时长都没有改动。

### 打死之后的尸体

`HandleBossDefeated` 增加一个计时器，`Boss Body Hide Delay`（`SewerEncounter` 组件的 Class Defaults，默认 5 秒）之后把 Boss 隐藏、关碰撞、停行为组件 tick —— 和读档恢复（`RestoreEncounterCheckpointScene` 与 `ResolveBoss` 的 defeated 分支）本来就会做的一样。

改之前：当场打死会把尸体连碰撞一起留在 Boss 房中央（可能挡住闸门开关），而死一次读档回来 Boss 直接凭空消失，两种玩法看到的房间不一样。死亡动画和死亡音效照播，只是 5 秒后清场。组件销毁时清掉计时器。

### 栅栏坐标只留一份

`RestoreEncounterCheckpointScene` 里硬编码的 `RoundFence` 坐标删掉了。关卡里 `BP_ConstructionFence_Round` 的摆放位置和 `L3_SaPosition_Entrance` 写的值一致（实测 delta 0.00），所以这条恢复路径不需要再写第三份。以后要挪入口栅栏，只改关卡摆放和 BPLM 里那个 `Set Actor Location`。

### 验证

VertigoEditor Win64 Development 编译 0 错误。只读检查 `-run=VTGSewerBossIntro`：BPLM 的 `Narrative` 属性与 `L3BossEncounter` 事件都还在（反射按名字取，改名会静默失效，所以这里断言）、两个对白各走哪条兜底路径、栅栏摆放与 beat 期望值的差值。4 项全过。

尚未在真实关卡里试玩：抢对白的重试时机和 5 秒清场的观感需要进游戏确认。

## 2026-11-21 开门台词挪到门开启的那一帧

`L3Sewer_Still_DialogueNode_NPC_22`（圆门开启的短评）原来接在 `L3 Open Round Door` 调用节点的 `then` 上。那条链里有个 `Delay 1.0`，蓝图调到 latent 节点就把控制权交回来，所以台词实际在**拉下闸的瞬间**响，比门真正开启早一秒。

现在改接到金库门声 `Play Sound at Location`（`445099__lezaarth__vault-2`）的 `then`，和 `RoundDoor_anim_open` 同一帧触发。闸门机关那条链本身、`Delay 1.0`、音效和动画都没有改动。

**副作用：相机锁会晚约 1 秒解除。** 这个节点挂的注释是「finish the interaction through Narrative so the trigger camera lock is released」—— ISX `L3_OpenRoundDoor` 勾了 Lock Cam on Interact（锁到 `BP_1CameraLockAutoTrigger_C_2`），靠这句对白结束来释放。挪动之后顺序变成：拉闸 → 1 秒 → 门开 + 台词 → 台词结束解锁，也就是相机会锁着看完开门过程再放开。节点位置挪到了门声节点的右下方，免得拉一条横穿全图的长线。

工具：`-run=VTGSewerDoorLine` 应用，`-VerifyOnly` 只验证（断言这句由门声节点驱动且只有一条入边）。顺手把施工牌那句（NPC_28）误抄成 `RoundDoorOpen` 的注释改成了 `Construction Sign Comment`，NPC_22 自己的注释保留不动。改前资产备份在 `Scripts/SewerDoorLine/Backup/`。

蓝图编译 0 错误 0 警告，独立进程重新加载后验证通过。相机多锁这 1 秒的观感需要进游戏确认。

## 2026-11-21 下水道战斗手感

从资产里量出来的问题，不是靠感觉列的。只读审计工具：`-run=VTGSewerFeelAudit`（montage 时长、rate、每个 notify 的触发时间）和 `-run=VTGSewerCombatFeel -Audit`（Boss 阶段表、骨骼名、组件 tag）。应用与验证：`-run=VTGSewerCombatFeel -Apply` / 不带参数即从磁盘重新加载后验证。改前资产备份在 `Scripts/SewerCombatFeel/Backup/`。

### Boss 现在会对被打做出反应

`DT_SeaMonster_Phases` 的 `SeaMonster_P1` 行，`HitReacts` 四个方向原来全是 `None`（蛋那一行是填好的，所以打蛋有反馈、打 Boss 没有）。现在四个方向都填回 `AM_EggProducer_GotHit`。

`AM_EggProducer_GotHit` 的 `RateScale` 从 1.0 提到 1.6，实际时长 1.125s → 0.703s。原因是玩家最快的连段间隔约 0.70s，1.125s 的受击动画会在连打时不断打断自己。

不会出现「Boss 被打到动不了」：`BPC_Boss_Behavior::PlayHitReact` 开头先判 `PlayingAbility`，技能期间只有 `InterruptedBy` 里列出的 HitType 才能打断（CDO 是 `NewEnumerator2`，玩家的伤害走的是默认 HitType）。所以受击动画只在 Boss 技能间隙播。

### Headbutt 放回能力表

原来被拿掉的近身招。恢复成 Oct 17 的那份，只改了一处：`Targeting.MaxDistance` 420cm（原 420，保持），`Cooldown` 3.0s，`Damage.DamageMultiplier` 1.5（攻击力 10 → 15 点）。`ComponentTag="head"` 是对的：`BP_Boss_SeaMonster_L3Sewer` 上有 `HeadHitbox`（挂 `spine_006`）和 `NeckHitbox`（挂 `spine_004`），两个 SphereComponent 的 tag 都是 `head`。

拿掉它之前，贴脸打 Boss 时它只剩每 10～11 秒放一次毒气或下蛋 —— AcidSpit 的 `MinDistance` 是 450cm，近身用不了。换句话说近身没有任何威胁，不需要走位。

**攻击范围的可读性**（当初拿掉的原因）：`AM_EggProducer_Headbutt` 上加了一个 `VTGAttackEffectNotify`，在 0.850s（也就是 `Notify_Damage` 伤害窗口开启的同一帧）从 `spine_006` 喷出 `NS_AcidSplash_Med1`。注意是在**命中判定的那一帧**，不是提前预警。

喷溅朝向：`VTGAttackEffectNotify` 新增 `bOrientToTarget`。打开之后旋转取「从喷发点指向玩家」而不是攻击者的 actor rotation。Boss 是锚定的，根骨朝向不一定跟着打击方向走；不过实测 `BPC_Boss_Behavior::ActivateAbility` 本来就会先 `Find Look at Rotation` 转向目标，所以两者通常一致，这个开关是保险。

可调项都在 montage 的 notify 上：`EffectScale`（现 1.5）、`ActiveSeconds`（现 1.0）、触发时间。如果玩家绕到背后 Boss 还是会挥空，那就把能力表里的 `Targeting.DirectionCondition` 从 `Any` 改成正面。

### 一次命中 = 顿帧 + 双方 + 震屏

原来 `Hit Stop` 的时长按 `ComboIndex` 从 Select 节点取：`0.0 / 0.08 / 0.10 / 0.40`。ComboIndex 从 0 开始，所以**每套连段的第一击顿帧是 0**；而蛋只有 60 血、锤子第一击 40 伤害，大量战斗就是一两下。Option 0 改成 0.07。（Option 3 的 0.40 取不到，ComboIndex 最多到 2，没动它。）

原来的 `Hit Stop` 自定义事件只把 `CustomTimeDilation` 设在玩家自己身上，怪照常动 —— 读起来是「我卡了一下」不是「我打中了」。现在 `DealPunchDmg` 里那一个调用节点换成 `UVTGCombatStatics::PlayMeleeHitFeedback`，一个节点做三件事：冻结攻击者、冻结 `FindMeleeTargets` 返回的每个目标（走已有的 `UVTGCombatComponent::ApplyHitStop`）、在玩家相机上播一次 `Sudden_Cam_Shake_Slight`（Scale 0.05，比受击时的 0.1 轻）。只在真的打中东西时震（目标数组为空就不震）。

`HitStop` 自定义事件本体留在图里没删，现在没有调用方。

### 锤子出手更快

`Hammer_Attack_2Combo_1_Root_Montage` 的 `RateScale` 1.0 → 1.45，伤害判定 0.428s → 0.295s 实际时间。`Hammer_Attack_2Combo_2_Root_Montage` 1.0 → 1.30，0.306s → 0.235s。市面上近战轻攻击从按下到判定普遍在 0.10～0.20s。第三段 `Hammer_Attack_Infinite_1_Montage` 是收招，本来就是重的，没动。

`ANS_ComboWindow` 是按百分比的，跟着一起加速。拳头那三段本来就在 0.15～0.26s，没动。

### 闪避：只认 notify 窗口

**原来闪避无敌根本没生效。** `Notify_Sa_DodgeInvincible` 的 NotifyBegin 里 `Get All Actors Of Class` 取的是 `BP_Player_May_C`（BackUp 目录下的旧角色），NotifyEnd 取的才是 `BP_Player_Sa_C` —— 从 May 那边抄过来只改了一半。所以在 Sa 身上：Begin 什么都不做，End 把玩家 mesh 的碰撞设成 `PhysicsOnly`（而且再也没人改回去）。两个 Print String 也会在非 Shipping 构建里刷屏。

实际在起作用的是 `Event PointDamage` 里那条 `IsDodging?` 分支，而 `IsDodging?` 在**整段 montage**（0.79～1.17s）都是 true，还只挡点伤害 —— 毒气（radial damage）完全躲不掉。也就是说宽容度和招式类型挂钩，玩家学不到统一的节奏。

现在：
- 新的 C++ notify state `UVTGDodgeInvincibleNotify` 换掉四段闪避 montage 上的旧 notify，窗口时间原样保留（F 0.083–0.520，B 0.128–0.558，L/R 0.083–0.502）。它只在 `UVTGPlayerPresentationComponent::bDodgeInvincible` 上打标记，不动 `bCanBeDamaged`、不动 mesh 碰撞。
- 不用 `bCanBeDamaged` 是有原因的：清掉它 `UGameplayStatics::ApplyPointDamage` 会直接早退，伤害事件根本不触发，那就没法在同一个窗口里既免伤又给「完美闪避」奖励。
- `Event PointDamage` 那条分支的条件从 `IsDodging?` 换成 `UVTGCombatStatics::IsDodgeInvincible`。
- `Event RadialDamage` 补了一条同样的分支（原来是直接进 Process Damage）：命中时在窗口内 → 播 `PerfectDodge_SlowMo` 并跳过伤害，否则照常受伤。

结果：点伤害和范围伤害用同一个 0.44s 窗口，慢动作奖励也落在这个窗口里而不是闪避动作的收尾。玩家 mesh 的碰撞设置从此保持资产里写的那份。

### 验证

VertigoEditor Win64 Development 编译 0 错误。`BP_Player_Sa` 蓝图编译 0 错误 0 警告。独立进程重新加载后 `-run=VTGSewerCombatFeel` 全部断言通过：能力表里有 Headbutt、HitReacts 指向 GotHit、GotHit 实际 0.703s、splash 在 0.850s 的 spine_006、两段锤子的判定时间、四段闪避的 i-frame 窗口、第一击顿帧 0.07、`PlayMeleeHitFeedback` 的 Targets/CameraShake 接线、两条闪避分支都读 `IsDodgeInvincible`。

三个新节点的 self 引脚是显式接 `Self` 节点的，没有依赖 `DefaultToSelf` 推断。

**尚未进游戏试玩。** 需要确认的：受击动画 1.6 倍速会不会太抖、Headbutt 3 秒 CD 对剧情向战斗是不是太频繁、splash 的 `EffectScale` 1.5 够不够看清范围、0.07 的第一击顿帧和 0.05 的震屏强度、以及闪避窗口收紧之后躲毒气的难度。

## 2026-11-21 暂停菜单退出页、下水道数值、锤子模型、PIE 误报

### 暂停菜单的 EXIT GAME 现在问你去哪

新建 `UVTGExitConfirmPage`（`Source/Vertigo/{Public,Private}/UI/VTGExitConfirmPage.*`），完全照 `UVTGLanguageSettingsPage` 的做法用 C++ 搭：同一张 `SettingsFrame` 背景图、同一套 Noto Sans SC 字体（Regular/Medium/SemiBold）、同样的 940×800 ScaleBox + SizeBox + CanvasPanel 栅格、同样的面包屑（`PAUSE / EXIT`）/标题/分隔线/返回按钮 + `Esc / B` 提示位置。所以两张卡片看起来是同一个界面的两页。

两个选项：**返回主菜单**（走 `UVTGMediaLoadingPageSystem::OpenLevelWithLoadingScreen`，和死亡界面同一条路，共用加载页）和 **退出到桌面**（`QuitGame`）。两者都先 `SetGamePaused(false)` 并把输入模式和光标恢复成游戏态，否则从暂停状态直接切场景会把暂停和鼠标带过去。键盘/手柄导航起点是「返回」而不是任何一个结束游戏的选项。

主菜单地图是 `UVTGPauseMenu::MainMenuLevel`（`EditDefaultsOnly`，默认 `/Game/ThirdPerson/Maps/MainMenu/M_MainMenu`），在 WBP_PauseMenu 的 Class Defaults 里可改。

**本地化**照已有做法：`UVTGLocalizationSubsystem::OnLanguageChanged` → `RefreshLanguage()` 里按语言逐条 `FText::AsCultureInvariant`，和语言页一字不差的模式。中文：退出游戏 / 要去哪里？/ 返回主菜单 / 退出到桌面 / 返回 / 上次存档之后的进度将会丢失。

蓝图那边：`WBP_PauseMenu` 里 `On Clicked (B_Exit) → Quit Game` 的连线和 `Quit Game` 节点删掉了（否则会和 C++ 的 `OnClicked` 同时触发，直接退出）。工具 `-run=VTGPauseExitPage -Apply`，不带参数即验证。备份在 `Scripts/PauseExitPage/Backup/`。

### EXIT GAME 文字改不动、本地化锁死

原因是两件事叠在一起：

1. 这句的 FText 是 `NSLOCTEXT("", "0C8F908E49A77F8406E906A6C53553C1", "EXIT GAME")` —— **空命名空间 + 自动生成的 GUID 键**。`VTGManualLocalization` 的 gather 会把包里每个对象都走一遍（widget tree 的副本和编译出的类副本都算），同一个 key 配上两份不同原文就判键冲突并写 `lockReason`，面板从此只读。参见 [[manual-localization-conflict-lock]]。
2. 翻译表里那条的 zh-Hans 是「退出到主菜单」。在编辑器里改英文原文，运行时仍然显示旧译文，看起来就是「改了没用」。

现在这句改成 `NSLOCTEXT("VTG.WidgetLabels", "WBP_PauseMenu.ExitEntryText", "EXIT GAME")`，和 `LanguageEntryText` 同一套命名约定（`VTGLoadingPauseLocalizationCommandlet` 当初跳过了它，因为那个工具只处理「还没有 key」的文本）。`Localization/ManualTranslations.json` 里对应改了四行：namespace、key、`approvedDisplayText` 改 true、`lockReason` 清空，译文从「退出到主菜单」改成「退出游戏」（按钮现在是开子页，不是直接回主菜单）。

widget 本身还叫 `TextBlock`，没有改名——UMG 改控件名要动变量引用，风险大于收益；key 里已经写清楚它是什么了。

### 下水道数值

- **蛋的血量 ×1.5**：9 个摆在关卡里的蛋每个都在实例上覆盖了血量，所以改的是关卡不是蓝图。60 → 90，`Health` 和 `MaxHealth` 一起改，`L_SewerUnderApartment.umap` 已保存，备份在 `Scripts/SewerCombatFeel/Backup/`。
  **顺带发现**：`BP_SeaMonster_Egg` 的类默认血量是 2500（`BPC_Boss_Behavior` 的 CDO 默认值，没人改过），而 Spawn Eggs 生成的蛋用的就是类默认值。没动它，但值得确认一下生成的蛋是不是真的有那么厚。
- **Boss 伤害 ×1.5**（不含 Spawn Eggs，蛋自己那一行 `SeaMonsterEgg_P1` 完全没动，所以生成的蛋和地图上的蛋伤害一致）：Toxic Gas 0.8 → 1.2、AcidSpit 0.75 → 1.125、Headbutt 1.5 → 2.25。攻击力 10 的话分别是 12 / 11.25 / 22.5 点。

这些在工具里写的是**绝对值不是倍率**，所以重跑不会叠加。

### 读档重试后锤子变回旧模型

`BP_Hammer` 捡起来时执行 `BP_Player_Sa.MeleeWeapon.SetStaticMesh(/Game/Weapon/Hammer/Hammer)`，是对的。但 `EquipMeleeWeapon` 自定义事件会从玩家身上的 `WeaponDataMap` 重新读 `WeaponModel` 再 `SetStaticMesh` 一次，而那里存的是 `/Game/AnimationAsset/MC_Sample/Demo/Objects/Hammer/SM_Prop_Hammer`（示例素材里的旧锤子）。任何重新装备的路径都会把模型换回去，读档重试最明显。

`WeaponDataMap` 里锤子那条的 `WeaponModel` 改成 `/Game/Weapon/Hammer/Hammer`，和蓝图一致。拳头那条本来就没有模型，没动。

### PIE 退出时的假报错

`UVTGSaveKeyWatcher` 挂在 `GEngine->OnLevelActorDeleted()` 上，本意是作者在 Outliner 里删掉摆放好的 actor 时提醒存档键会失配。但这个委托在**游戏世界里也会触发**，PIE 结束时每个 actor 都会被销毁，于是每次退出 PIE 都报一次「Deleted "PC_Game_VTG_C_0": BPLM_SewerUnderApt_C_1 refers to it as "Player Controller"」，还顺带触发「Errors/warnings reported while playing in editor」弹窗。

加了一条世界类型判断：只有 `EWorldType::Editor` 才会走后面的检查。PIE / 独立运行销毁 actor 不碰磁盘上的存档，本来就没什么可提醒的。

**相机和操作卡住这部分没有复现，也没有定位。** 假报错消失之后如果还卡，需要一份新的 log。从贴过来的 log 里有两条值得怀疑：`LogUObjectGlobals: Warning: Failed to find object '/Game/ThirdPerson/Input/IMC_Default.IMC_Default'`（资产在磁盘上存在却加载失败，输入映射没了就什么都按不动），以及 StealthComponent 那两条 `Attempted to access index 0 ... of length 0`。两条在第一次运行时也会出现，所以不能直接解释「第二次才卡」。

### 验证

C++ 编译 0 错误。`BP_Player_Sa` 与 `WBP_PauseMenu` 蓝图编译均 0 错误 0 警告。独立进程重新加载后 `-run=VTGSewerCombatFeel` 与 `-run=VTGPauseExitPage` 全部断言通过：9 个蛋都是 90、三条 Boss 伤害倍率、锤子模型指向 `/Game/Weapon/Hammer/Hammer`、EXIT GAME 的 key 是 `VTG.WidgetLabels/WBP_PauseMenu.ExitEntryText`、B_Exit 上已没有 Quit Game 调用。

**退出页没有进游戏看过。** 需要确认：卡片在暂停界面里的位置和语言页是否一致、返回主菜单是否干净地离开了暂停状态、以及翻译面板是否已经可以编辑这一条（可能要先在面板里按一次「保存并应用译文」重新生成 locres）。

## 2026-11-22 血条遮挡、蛋血量统一、锤子节奏回调

### Boss 血条不再盖在暂停菜单上

没有去动 ZOrder（那要去猜暂停菜单落在哪一层，而且两边都可能再变）。改成血条自己让路：`UVTGBossHealthBar::NativeTick` 里先判 `World->IsPaused()`，暂停时把 `RenderOpacity` 压到 0 并提前返回。

能这么写是因为**游戏暂停时 Slate 仍然在 tick UMG**，所以血条不需要暂停菜单找到它、也不需要任何一方知道对方的层级。提前返回顺带冻住了伤害拖尾和伤害数字的计时（暂停时它们本来会偷偷走完），恢复游戏后按原来的 `FadeTime` 淡回来。

### 蛋的血量统一成 100

- **摆在关卡里的 9 个**：每个实例上的覆盖值从 90 → 100（`L_SewerUnderApartment.umap`）。
- **Spawn Eggs 生成的**：原来用的是 2500。

2500 这个值有个坑：`BPC_Boss_Behavior` 组件是**声明在 `BP_Boss_Base` 上**的，海怪 Boss 自己也继承同一个声明 —— 那 2500 就是 Boss 本体的血量，直接改父级会把 Boss 一起改成 100。

所以给 `BP_SeaMonster_Egg` 加的是**子类覆盖**（`UInheritableComponentHandler::CreateOverridenComponentTemplate`），也就是在子蓝图里选中继承来的组件、改一个值时编辑器生成的那种记录。`BP_Boss_Base` 原封不动，工具里有一条断言专门守这件事：

```
check(!bInherited || !FMath::IsNearlyEqual(Number(Node->ComponentTemplate, TEXT("MaxHealth")), EggHealth));
```

验证输出：`egg class default now health=100.000 max=100.000 (BP_Boss_Base still 2500.000)`。

工具里写的是绝对值 100，不是倍率，重跑是空操作。

### 锤子节奏从 1.45/1.30 回调到 1.2

1.45 实际玩下来过头了。两段都改成 **1.2**：

| | rate 1.0（原始） | 1.45 / 1.30 | 现在 1.2 |
|---|---|---|---|
| `Hammer_Attack_2Combo_1` 判定 | 0.428s | 0.295s | **0.356s** |
| `Hammer_Attack_2Combo_2` 判定 | 0.306s | 0.235s | **0.255s** |

### 关于「Save/Load 丢失地图初始化」的调查（未改动任何东西）

**结论：Save/Load 没有跳过 BeginPlay，也没有跳过 OnStageBegin。** `UVTGSaveCoordinator::LoadSlot` 的顺序是：

```
RestoreGameInstanceValues(SaveObj);        // 注释：before the map opens
PersistentInts / PersistentNames = ...;    // 注释：so actors can read them in BeginPlay
GI->SetCurrentStage(SaveObj->Meta.Stage);  // 注释：exactly like OpenLevelWithStage does
OpenLevelWithLoadingScreenByName(...);     // 地图这时候才打开
```

之后 `AVTGLevelManagerBase::BeginPlay` → `RestoreLevelManager(this)` → `OnStageBegin(GetActiveStage())`。`ApplyWorldState` 里也有一行印证：「By now every level manager in this map has had its BeginPlay and claimed its record.」

顺着 `BPLM_L2StreetBarFight` 的图走了一遍：`Event On Stage Begin` → `Switch on Name(Stage_BarStreet_L2)` → `L2 Activate Level Manager` → `Lighting Night`（那一串 MPC `Set Scalar Parameter Value`）→ 两条分支都会到 `L2 Initialize NPC`。这条链在读档时**应该**是通的，所以具体哪一步没跑还没定位到。

两个待验证方向：
1. **时序覆盖** —— `ApplyWorldState`（含 `RestoreInteractions` 和存档里的 actor 状态）跑在 `HandlePostLoadMap` 的 ticker 里，也就是 BeginPlay **之后一拍**。BPLM 在 `OnStageBegin` 摆好的 NPC 位置可能被存档记录盖回去。最符合「NPC 位置丢了」。
2. **MPC 是另一回事** —— MPC 是全局渲染状态，存档根本不碰。如果亮度也丢，说明 `Lighting Night` 真的没执行，和 NPC 位置是两个不同的原因。

最便宜的验证：在 `Lighting Night` 和 `L2 Initialize NPC` 开头各插一个 Print String，正常进图和读档进图各跑一次对比。

**假跳过系统**是 `UVTGSequenceSkipSubsystem`（`Docs/SequenceSkipSettings.md`、`Docs/SequenceSkipAudit.md`）：状态机 `Idle → Holding → FadeToBlack → FastForward → Reveal`，`AdvanceFrame()` 一帧一帧推进 Sequencer 所以 Trigger/Repeater 和 Finish 委托照常触发，每个 Sequence 在面板里单独勾 `Allow Skip`（存在资产的 `VTGSequenceSettings` user data 里）。

**不建议用它解决这个问题**：它是给「玩家正在看过场、想快进」用的，读档进图时根本没有 sequence 在播；丢失的 MPC 和 NPC 位置是 BPLM 蓝图直接调的，不在 Sequencer 事件轨上；而且它自己的文档写明「快进仍不会推进世界 Delay/Timer 或独立 Montage 的时间」，BPLM 初始化链里只要有一个 Delay 就照样断。

框架里已有的分离机制是 `AVTGLevelManagerBase::EnterSavedStoryEvent(EventId)`（存档里记录过的返回 false），配合 `GetResumeCheckpoint()`。正确分法是：初始化放在 `OnStageBegin` 的无条件路径上，一次性剧情包在 `if (EnterSavedStoryEvent("xxx"))` 里。**`BPLM_L2StreetBarFight` 目前一次都没用过这个 API**，它靠自己的 `L2StreetFightPhase` persistent int 在分支。

### 验证

C++ 编译 0 错误。独立进程重新加载后 `-run=VTGSewerCombatFeel` 全部断言通过：9 个摆放蛋 100、生成蛋 100 且 `BP_Boss_Base` 仍为 2500、两段锤子 rate 1.2、以及之前那批（Headbutt、HitReacts、splash、顿帧、闪避分支、锤子模型）全部保持。

**血条遮挡没有进游戏验证过**，需要在 Boss 战里按一次暂停确认。
