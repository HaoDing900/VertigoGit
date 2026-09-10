# 攻击中断卡住：2026-09-10 复查与修复

项目：`E:/HaoGameHubb/VertigoGit`。本次只处理攻击，不搭建或修改关卡。

## 原因

当前 `BP_Player_Sa.uasset` 的 SHA256 为 `5FD9B7277336092218281AA41125F9C97F74306A3D98C1712D81A172434AEA2D`，与 2026-09-09 首次修复前的备份完全相同。当前文件不含上次的中断恢复连线；仅凭文件比较不能确定恢复旧版本的具体操作来源。

`ExcuteCombo` 在播放攻击前禁用移动，但 `K2Node_PlayMontage_2.OnInterrupted` 只接 `PrintString("Interrupted")`，后面没有执行流。动画中断后，`IsAttacking` 和移动锁没有清理，空格闪避执行重置才恢复。旧 `EndAttackSafely` 又使用 `IsAnyMontagePlaying`，会把非攻击动画也当作阻止清理的理由。

这解释了中断后为何卡住；本次没有录制用户遇敌瞬间的动画调用栈，因此不把具体抢占攻击的动画归因于某一个未经确认的来源。

## 修改

- 保留原 Interrupted 打印，追加 `Delay(0)` → `EndAttackSafely`。
- 下一次 latent 更新时检查 `IsAttacking`，以及当前连击对应的攻击 Montage 是否有效且仍在播放。其他动画不再阻止攻击清理；已开始的下一段连招继续保留。
- `ResetCombo` 额外关闭 `IsInComboWindow`，沿用清理攻击标志、缓存输入和连击索引的原逻辑。
- 死亡中断时清理标志，但不重新启用行走；已被闪避重置的状态也不再被旧回调覆盖。

仅保存一个游戏资产：`Content/ThirdPerson/Blueprints/BP_Player_Sa.uasset`。原 EventGraph 的 1114 个节点全部保留，新增 10 个节点。

同时修正了现有编辑器验证工具 `Source/VertigoEditor/Private/VTGRepairMeleeCommandlet.cpp` 中重复初始化临时世界的问题，并准确定位攻击重置所用的 Walking 节点。蓝图修复仅使用引擎节点，游戏运行不依赖命令行工具。

## 验证

修复前在引擎临时世界中复现中断锁死：外部停止、非攻击动画替换等回归用例失败。修复后六组状态用例通过：外部停止解锁、连招衔接、非攻击 Montage 替换、已重置动作保护、死亡保护、正常完成。角色蓝图编译 0 错误、0 警告。

测试执行真实角色事件和 Montage 回调，但使用原生 AnimInstance 隔离移动动画蓝图及渲染；不是完整关卡手动通关。

本次备份、修复前复现和修复后日志位于 `Saved/MeleeInterruptFix/20260910-recurrence/`。`BP_Player_Sa.before.uasset` 是本次修复前的角色备份。Content 文件清单对比仅此角色资产改变；目标关卡文件 SHA256 保持一致。

仅复验已保存蓝图：

```powershell
& 'D:/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/HaoGameHubb/VertigoGit/Vertigo.uproject' -run=VTGRepairMelee -VerifyOnly -Test -unattended -nop4 -nullrhi -nosound
```
