# 连续 Q/E 切换页签的焦点修复

## 原因

WB_TabSwitcher 的 SetActiveTab 先转换新页面到 WB_TabWidget_Mail。转换失败的执行引脚原先未连接；因此非邮件页面没有执行后面的 L_ActiveWidget 更新、OnActiveTabChanged 广播和 SetFocus，键盘焦点留在失活的旧页面。

SAVE / LOAD 是 HandleTerminalSave 的独立分支，也没有恢复焦点。根 Widget 的 OnKeyDown 还把未初始化的 Tab Switcher Widget 属性传入输入库，有多个实例时会退回全局查找。

## 修改

- 将邮件转换失败路径连接到邮件专用刷新之后的公共页面激活/焦点流程。
- 根 Widget 的 OnKeyDown 使用 Self 作为 TabSwitcherInputs 的接收实例。
- HandleTerminalSave 在新页面成为活动页面后，调用 Terminal->SetFocus，保证连续 Q/E 可以离开 SAVE / LOAD。
- 添加可重复执行的命令行修复/检查入口：`-run=VTGAlleyRepair -TabFocus`；追加 `-VerifyOnly` 时不保存蓝图。

蓝图原件已备份到 Saved/QA/TabFocusBefore/WB_TabSwitcher.uasset。

## 验证

BaseLevel 正常 PIE，实际 BP_Player_Sa_C 和 PC_Game_VTG_C。以 Tab 打开 Terminal，初始焦点只设置一次，随后每个 Q/E 都通过原生 Slate 键盘事件单独发送，间隔跨越多个帧，中途没有鼠标点击或再次设置焦点。

- 修复前：第一次 Q 从邮件页切到地图页，后续输入失效，28 次输入中 27 次失败。
- 修复后：连续 12 次 Q、12 次 E、4 次交替 Q/E；28/28 通过，覆盖全部 5 个可见页签、两端循环及 SAVE / LOAD。
- 每次检查预期活动页签和焦点归属，修复后无失败。
- 蓝图编译通过；项目 C++ 编辑器目标编译通过。

证据：continuous-baseline.json/log、continuous-fixed.json/log、focus-apply.log。

测试没有执行保存或载入游戏操作。临时键盘探针已移出编辑器源目录并归档，最终编辑器模块重新编译。
