# 第一拳被冲刺收步动画打断

项目：`E:/HaoGameHubb/VertigoGit`。2026-09-10。

## 已捕获的实际原因

在 `L_CombatTest` 中记录到两次相同调用链：

1. 攻击输入调用 `Punch` → `ExcuteCombo`，播放 `ANM_Sa_FightFist09_Inplace_Montage`。
2. 约 0.325 秒或 0.350 秒后，`IA_Sprint` 的 Completed 分支调用 `PlayAnimMontage`，播放 `ANM_Sa_Walk_F_End_Inplace_Montage`。
3. 第一拳立即触发 `interrupted=1`。两段 Montage 同属 `DefaultGroup`。

实际调用栈、时间与动画名见 `Saved/MeleeInterruptFix/20260910-source-trace/live-before.log`，搜索 `MELEE TRACE`。不是仅由屏幕上的 Interrupted 字样推测。

此前的修复解决了中断后未解除移动锁的问题；本次修复处理导致意外中断的收步动画调用源。

## 源头修改

在角色蓝图中，冲刺结束播放收步动画之前增加判断：`IsAttacking OR IsAnyMontagePlaying`。

- 忙碌时跳过收步动画，继续执行原来的冲刺状态和速度恢复。
- 空闲时继续播放原收步动画。
- 不修改攻击、闪避和受击 Montage 的正常调用；真正的受击或闪避仍可按原设计中断攻击。
- 保留上次中断后清理状态的保护，以及全部原有节点。新增节点注释为 `VTG sprint stop priority v1`。

仅修改角色游戏资产 `Content/ThirdPerson/Blueprints/BP_Player_Sa.uasset`；未保存任何关卡或怪物资产。

## 验证方式

测试从编译后的 Enhanced Input 绑定中查找实际 `IA_Sprint.Completed` 事件，调用真实事件链。在第一拳开始后 0、0.10、0.33、0.60 秒分别释放冲刺；修复前四种情况全部被收步动画替换。

同时检查正常冲刺结束仍播放收步、普通与受限步行速度仍恢复、不重启已经播放中的其他 Montage，以及此前的中断恢复、连招、死亡保护等回归。

修复后上述四种释放时机全部保留第一拳，全部回归通过；角色编译 0 错误、0 警告。保留原有节点，在 `K2Node_CallFunction_8` 前新增 6 个节点。`L_CombatTest` 与 `L_SewerUnderApartment` 文件哈希均未改变。

只读复验：

```powershell
& 'D:/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/HaoGameHubb/VertigoGit/Vertigo.uproject' -run=VTGRepairMelee -VerifyOnly -Test -Sprint -unattended -nop4 -nullrhi -nosound
```

测试在临时世界中执行，不保存测试状态。日志和修改前角色备份位于 `Saved/MeleeInterruptFix/20260910-source-trace/`。

## 可选追踪工具

以 `-VTGMeleeTrace` 启动编辑器时，编辑器模块会给 PIE 玩家附加临时 `VTGMeleeMontageTrace`，记录 Montage 开始、中断、组名和蓝图调用栈。该组件不会写入角色或地图；普通启动不启用，不进入打包运行时模块。
