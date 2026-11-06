# 锤子 Combo Recovery Bar

使用现有 WBP_Action 的 Recovery 条，无需添加第二个 Widget。空手仍读取 RecoveryMontage；装备 SmolHammer 时从玩家 WeaponDataMap 对应条目的 ComboMontages 中读取最后一段蒙太奇，因此跟随实际使用的锤子收尾动作。

锤子当前收尾为 Hammer_Attack_Infinite_1_Montage，总长约 3.033 秒，最后一个 Notify_Damage 窗口结束于约 1.314 秒。Recovery 从此处开始，以蒙太奇实际位置填充；只有现有攻击和移动锁都解除后才显示完成。HUD 不改变攻击速度、伤害或锁定时间。

闪避、死亡、隐藏角色／过场、攻击打断和装备切换会清理显示。回调按当前跟踪的蒙太奇识别，防止上一种武器的结束回调污染新的进度。

维护入口：UVTGActionHUD::ResolveRecoveryMontage／RefreshRecovery。验证命令：VTGRecoveryHUD。已验证原拳击行为以及锤子的伤害窗口、实际进度、播放速率变化、闪避、打断和装备切换。
