# WBP_Action 与敌人姓名

2026-10-14 已实施；本地资产和 C++ 已保存、编译。

## 玩家战斗 HUD

新资产：/Game/Blueprints/Widget/WBP_Action，父类 UVTGActionHUD（继承现有 UVTGPlayerHearts）。沿用原来的五颗血量图标、部分填充和受伤闪光；不再单独创建原生血条。

BP_Player_Sa 的 CombatPresentation 组件：Action HUD Class = WBP_Action；Action HUD Z Order = -10，低于默认 viewport Z=0 的 Narrative HUD。一个玩家只创建一个实例，InCombat=true 且 HidePlayerNCam=false 时显示；否则 Collapsed，不销毁，避免反复创建。EndPlay 移除。

右下角 Dodge 和 Punch 两行提示。布局、文字和按键框在 WBP_Action Designer 可编辑。DodgeAction=IA_Dodge；PunchAction=IA_Fire（当前玩家出拳沿用 Fire 输入路径）。按键取自 Enhanced Input 当前映射，并依据 CommonInput 的键鼠/手柄状态筛选；鼠标、Ctrl、Shift、肩键/扳机缩写。无有效映射显示 —，不编造按键。

L2 BPLM 原来单独创建的 WBP_BattleModeHUD 仅显示静态敌人名字，已移除该 CreateWidget/AddToViewport 链，避免重复战斗 HUD。战斗教程仍保留。当前 BPLM 的前一次战后存档修复保持存在。

## 敌人姓名

WBP_EnemyHealthBar 加入 EnemyName 文本框，位于原即时/追赶血条上方。原有血量更新、追赶效果和血条可见性逻辑保持使用。

BP_AI_Base 和 BP_SeaMonster_Egg 添加 EnemyDisplayName（FText，Enemy UI 分类，可编辑关卡实例），并挂上 EnemyIdentity 组件。姓名从所属 Actor 字段读取，填写空值时使用组件 Fallback Name。组件为带姓名血条提供至少 180×40 的绘制区域。

使用：选中关卡中的敌人，Details -> Enemy UI -> Enemy Display Name 填写姓名；或在敌人 Blueprint 的 Class Defaults 中修改默认姓名。子类继承该槽。

已填写默认姓名：Misto、Nara、Brosky、Bar Fighter、Sea Monster Egg。姓名与原血条一起出现/隐藏；没有额外强制全血敌人的血条显示。

## 验证

- VertigoEditor Development C++ 编译成功。
- 所有修改 Blueprint 编译通过；新进程从磁盘重载 WBP_Action、玩家和血条验证成功。
- 实际 BP_Player_Sa 实例三次进出战斗，始终复用一个 WBP_Action，非战斗时折叠；层级值低于 Narrative 默认层级。
- 实际 Misto 实例填写 CUSTOM NAME，原头顶血条同步显示。
- Nara、Brosky、Bar Fighter、Sea Monster Egg 实际 Actor 实例的原头顶血条均显示其姓名。
- Python 独立重载确认所有五个默认姓名已写入资产。
- 实际 UMG 渲染预览：preview.png；图片中的 Space/LMB 是示意标签，运行时使用当前实际映射。
- 最终回归进程 exit=0，final-verify.log：Success - 0 error(s)。项目原有资产警告仍存在。

尚未进行完整关卡 PIE 战斗、实际手柄切换或改键试玩。回归环境没有游戏 viewport；其层级检查验证配置值和实例显示/隐藏，UI 实际遮挡仍应在真实 Narrative HUD 场景确认。

日志：build.log、final-verify.log、defaults.log。备份：Backup/。新增原生类后请重新打开编辑器加载最新模块。
