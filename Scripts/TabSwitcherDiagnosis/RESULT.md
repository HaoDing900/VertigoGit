# TabSwitcher Q/E 后台运行诊断

场景：`/Game/ThirdPerson/Maps/BaseLevel`，正常 PIE，实际 `BP_Player_Sa_C` 和 `PC_Game_VTG_C`。

## 结论

打开 Terminal 时，输入模式的焦点目标仍然是背包 Widget，显示中的 TabSwitcher 没有键盘焦点。因此 Q/E 不会到达 Terminal 页面的 OnKeyDown；鼠标点击仍然有效。

`AC_Inventory_HUD_Component.OpenTabSwitcher` 调用 `SetInputMode`（NewEnumerator3，UI Only）。该函数的 `SetInputMode_UIOnlyEx.InWidgetToFocus` 连接到 `GetInventoryWidget`，没有连接到正在显示的 TabSwitcher。

## 运行证据

- 独立 PIE Widget 测试：菜单可见，5 个导航按钮。真实 Slate E/Q 事件使页签 `0 → 1 → 0`。
- BaseLevel/Sa：通过 PlayerController 输入 Tab 打开 Terminal。显示中的实例 `WB_TabSwitcher_C_1` 可见且在 viewport 中，但它及子页面都没有键盘焦点。Q/E 前后当前页签均为 3。
- 仅在运行时调用显示实例的 `SetKeyboardFocus`，焦点转移到当前邮件页 `WB_TabWidget_3`。
- 随后相同的 Slate E/Q 事件使显示实例页签 `3 → 4 → 3`。另一个隐藏实例保持页签 0。
- 再直接调用显示实例的 TabSwitcherInputs(E)，页签变成 4。

日志：`gameplay-runtime.log`（基线及临时焦点验证）、`gameplay-baseline.log`（未干预基线）、`slate-runtime.log`（独立 UI 键盘分发测试）。JSON 状态分别保存在同目录。

## 建议修复位置

在 `AC_Inventory_HUD_Component.OpenTabSwitcher` 的打开流程中，输入模式设置完成且页面初始化完成后，针对本次打开的 TabSwitcher 调用 `SetKeyboardFocus` / `SetUserFocus`。现有 OnFocusReceived 会将焦点继续转给当前页面。

这比直接修改通用 SetInputMode 中所有场景的焦点目标更局部，也不会影响背包自身的输入模式。

本次只进行了诊断和临时运行时焦点实验，没有保存或修改游戏蓝图。临时 C++ 探针源码归档为 `VTGTabSwitcherProbe.cpp.txt`，从编辑器模块移除后重新编译。
