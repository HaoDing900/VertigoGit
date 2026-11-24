# 剧情期间收起武器并保持脱战

所有 BPLM（继承 VTGLevelManagerBase）新增节点 **Set Player Story Combat Lock**，分类 `Story | Combat`。Target 接自己的 BPLM / Self，返回 bool 表示是否找到主角和 CombatPresentation 组件。

## 接线

在 `BPLM_SewerUnderApt` 的 `L3_AfterBossIsDefeated` 或自己的剧情 Event 中：

`Event → Set Player Story Combat Lock (Enabled=true) → 原有过场播放逻辑`

过场正常结束，以及停止/中断的收尾路径：

`原有过场收尾 → Set Player Story Combat Lock (Enabled=false)`

开启节点在同一执行链中立即收起武器并退出战斗，无需 Delay。可重复开启，不会覆盖最初的权限快照。查询节点 `Is Story Combat Locked` 位于玩家的 CombatPresentation 组件上，其他蓝图也可直接调用该组件的 `Set Story Combat Lock`。

如果演出结束后还要空手自由走动，可暂时保留 true，在离开房间的 Trigger 中设 false。该开关不接管移动、视角或 Sequencer 的输入锁；仍由现有过场框架处理。

## 行为

- 通过原有 `EquipMeleeWeapon(无武器/枚举0)` 收起锤子和 WeaponDataMap 中的近战武器。
- 通过 InventorySystemX 的 `HideWeapon` 隐藏远程装备，保留物品；不删除或从背包移除。
- 停止 WeaponDataMap 中登记的攻击 Montage 和连招缓存，清理自动射击计时器，隐藏拳套和战斗 HUD。
- 持续阻止附近敌人检测将 InCombat 重新打开；锁期间再次装备的武器在组件 Tick 被收起。
- 新 CombatComponent 的攻击/射击等战斗动作也尊重此开关；移动仍遵循其原有状态规则。
- false 只解除本开关并还原其临时修改的攻击权限；不自动拔武器、不强制进入战斗、不清除既有过场输入锁、不复活角色。
- 不写入存档，不更改 Sewer Boss 已死亡的持久标记。Boss 死后的 Sewer 脱战规则依旧有效。

## 维护

- 公共 BPLM 节点：`Source/Vertigo/Public/VTGLevelManagerBase.h` / `Private/VTGLevelManagerBase.cpp`。
- 开关与收武器实现：`Source/Vertigo/Private/Combat/VTGStoryCombatLock.cpp`；声明在 `Public/Combat/VTGCombatPresentation.h`。
- 防重新入战：`VTGCombatPresentation.cpp` 的 ResolveCombatState 和组件 Tick。
- 新战斗框架动作门禁：`VTGCombatComponent.cpp` 的 CanDoAction。
- 后续近战武器继续登记在现有 WeaponDataMap；远程武器走 InventorySystemX，无需为每种武器加分支。完全独立的新装备系统需要接入本开关的收起/动作门禁流程。

此次不修改 BP_Player_Sa 或 BPLM 资产，不替用户接入尚未指定的剧情执行链。重启编辑器后在 BPLM 中搜索节点即可。

## 验证

编译：`Scripts/StoryCombatLock/build.log`。

真实 BP_Player_Sa 运行回归：`Scripts/StoryCombatLock/runtime.log`；验证收锤子、ISX 装备保留、重复开关、重新装备抑制、附近敌人判定、原生动作门禁、保留过场输入锁和死亡状态。

未进行整个 Sewer 关卡的手动 PIE 演出验收；接入自己的事件后需检查正常结束和中断均能到达收尾节点。
