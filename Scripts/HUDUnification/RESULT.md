# HUD 与剧情战斗衔接修改

## 已实现

1. L2 街头战斗 AfterFight 结束时，Female Client 最终 Yaw 为 **-112°**。位置不变；读档恢复也使用该角度。在 BPLM_L2StreetBarFight 的 L2FightState 组件内可修改 Female Client End Transform。
2. WBP_Action 的 Dodge / Punch 使用 Inventory 图片。键位仍从 Enhanced Input 查询；切换输入设备时跟随 CommonInput。无法映射的键保留文字回退。
3. Narrative CommonBoundAction 继续使用原来的绑定机制，改用项目内的 CommonInput ControllerData。资源目录实际为 `/Game/Inventory/InventoryXBased/UMG/Textures/Controls`（Controls 有 s）。项目副本在 `/Game/UI/ControllerData`，DefaultGame.ini 指向副本，未修改插件内 Widget。
4. WBP_Action 新增 BloodOverlay，复制 WBP_LegacyActionHUD 中 BloodOverlay 的 Brush 和颜色。条件：战斗 HUD 显示、玩家存活、0 < Health / MaxHealth_Normal <= 15%。回血、死亡、脱战或隐藏角色/相机时折叠。使用 Collapsed，复用已有 HUD，不反复创建销毁。Action HUD 的 viewport ZOrder 仍为 -10，低于 Narrative。
5. 下水道存在强制自由战斗规则：`FoundTarget OR 当前地图为 L_SewerUnderApartment`，因此空场仍能攻击海怪蛋。现在由 CombatPresentation.ResolveCombatState 统一处理：保留 Boss 战前自由攻击，死亡/剧情输入锁时不战斗；Eyed Broodnest 真实血量从正数降至零时调用 FinishSewerCombat，保存 L3SewerBossDefeated=1 并切到 StoryMode，后续敌人检测计时器不能再次拉回战斗。该标记随已有存档系统保存和恢复；读取 Boss 战前的存档可重新战斗。Boss 的 CombatFeedback 组件中 `Ends Sewer Combat On Death` 控制此行为。
6. BPLM 继承新增 **Fade Configured** 节点。Get BPLM 后拖线搜索此名称即可调用；原 Fade2s_AddDelayAfter 保留。
   - Time Before Kill：Widget 从出现到移除的总秒数，<=0 使用 WBP_Fade 自身自动收尾。
   - Hold Duration Until Fade：全黑停留秒数。
   - Fade Duration：黑色变透明秒数。
   - Disable Fade：维持黑色，不执行淡出。
   - Reverse Fade On Appear：先从透明渐黑，再停留和淡出。
   - Z Order：默认 100。
   - 返回 Fade Widget，可按需 Remove From Parent。所有参数在 Construct 前赋值。此节点不会阻塞调用它的执行线；后续剧情等待可接 Delay / 自己的 Sequence 完成事件。
   - 示例：Time Before Kill=0、Hold=1、Fade Duration=2、Reverse=true，得到 1 秒渐黑、停 1 秒、2 秒淡出。
7. CombatPresentation 自动管理 Mesh 下的 Weapon_BrassKnuckle_L/R。仅在 InCombat、存活、未隐藏角色、CurrentMeleeWeapon=None、HasWeaponInHand=false 且 WeaponBase_ISX 没有 EquippedItem 时显示。玩家手部 socket、左右手附件位置不改。初始 Hidden in Game 可以继续勾选，运行时由组件切换。

## 验证

构建、资产读回、功能回归记录见本目录 build.log / verify.log。回归涵盖 15% 边界、回血/死亡/脱战隐藏、拳套空手/近战/远程条件、剧情锁、Boss 死亡后持久脱战、Fade 参数，以及 Narrative 实际 CommonActionWidget 取得的图片路径。

修改前的资产备份在 Backup。未进行 Git push。
