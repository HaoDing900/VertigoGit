# BlackMarketStreet 巷战修复

修改资产：BPLM_BlackMarket；BP_CollisionComponent。地图、敌人 Mesh 和主角蓝图未保存修改。
原资产备份：本目录 Backup/。BPLM 备份包含此前已完成的 Drone 移动朝向修正。

## 原因

L4_GoonInCombat 只执行 L4_SetEnmAtCombatPosition 和 L4_HideGoonNPC。
死亡委托绑定和 StoryMode?=false 原来只存在于另一个 StartAlleyConflict 入口。
因此从当前触发路线进入战斗不会绑定死亡回调，而且主角可能仍处于 StoryMode? 免伤。

BP_CollisionComponent/EventGraph/K2Node_CallFunction_12 的 ApplyPointDamage 未指定 DamageTypeClass。
主角 ReceivePointDamage 用 BI_DamageType.ProcessDamage 的 NewDamage 扣血；引擎原生 DamageType
不实现该接口，接口消息输出默认为 0。这与旧测试目标直接 TakeDamage 扣血的情况不同。
已指定项目的 BP_DamageType_Default，使现有主角伤害处理正常工作。

## 死亡通知与统计入口

- BP_AI_Base（/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base）的事件分发器：On EnemyDeath。
- BPI_OnEnmDeath（同目录）的 Dead?(For Cutsence) 是另一个剧情死亡接口；黑市计数实际监听上面的分发器。
- BPLM_BlackMarket/EventGraph 新增 InitializeAlleyCombat，两个战斗入口共用。
- 只绑定地图管理器的 Enm_Goon1、Enm_Goon2、Enm_Goon3（已核对 BlackMarketStreet 实例引用）。
- 分别进入 AlleyEnemy1Defeated、AlleyEnemy2Defeated、AlleyEnemy3Defeated。
- 每路 Do Once → 原 OnDefeated → L4_LeftEnemies 减一 → <=0 → AfterAlleyFightComplete → 原战斗区域停用逻辑。
- 初始化也有 Do Once，初始计数设为 3，重复触发不会重置进度。
- 原 GetAllActorsOfClass(BP_AI_Base) 的全场绑定执行入口已断开，节点保留作参考。
- 初始化设置 StoryMode?=false、InCombat=true，并调用原 ToggleCombatMode。

## 验证

C++ Development Editor 构建通过；BPLM、碰撞组件、主角蓝图零编译错误。
实际运行绑定的死亡分发器，每人重复广播两次：剩余 2→1→0；前两人不结算，第三人只结算一次。
中途重复初始化不会恢复计数。
在带权威 GameMode 的独立游戏世界中，真实 BP_Player_Sa 经 ProcessDamage 扣血；
Misto/Nara 拳部碰撞和 Brosky 武器碰撞均通过 EnableCollision → Trace → ApplyPointDamage → Health 测试。
测试没有运行完整 BeginPlay/PIE 剧情；测试副本在完成分支用计数器替代后续对话与屏障操作，
也跳过仅用于表现的 ToggleCombatMode 调用，但保留 StoryMode/InCombat 赋值。
无视口测试中的 HUD、玩家查找警告源于没有运行完整 UI/本地玩家初始化。

复验：UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGBlackMarketCombat -VerifyOnly -unattended -nullrhi -nosound
