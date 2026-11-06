# SmartRestrict 实例互动设置

图标稳定性修复：手动模式通过 ISX 的 IsInteractionEnabled 控制可互动状态，代理检测碰撞始终保持 QueryOnly，不再随提示启停切换碰撞。ISX 的 IsVisibleActor 对带 VTG_SmartTriggerInteractionProxy 标签的目标使用专用检测：视线忽略自身代理和所属 SmartRestrict 的阻挡墙；相机在碰撞内部时无命中也视为可见；其他场景墙体仍会遮挡。普通 ISX 目标沿用原路径。SmartRestrict 销毁时同步清理代理。

在关卡中选中 BPA_SmartRestrict，Details 搜索以下名称：

- Require Interaction：启用按键互动；默认关闭，进入范围自动触发。
- Interaction Prompt：提示文字。
- Interaction Detection Distance：ISX 检测距离，单位 cm。
- Interaction Confirm Distance：ISX 允许确认互动的距离，单位 cm，对应原组件 OverridePickUpDistance。
- Require Exit Before Retrigger：本次触发后，必须离开实际范围才可再次触发。

两个距离默认 -1，表示沿用 ISX 默认值。例如 Detection Distance=300 表示检测距离为 3 米；如需在同样距离确认，可把 Confirm Distance 也设为 300。

距离配置仅用于按键互动模式，并在开始游戏生成该实例的互动代理后传入其 AC_ActorInteractionComponent。每个实例独立。触发范围仍由 Box(ChangeSizeHere) 决定，扩大检测距离不会扩大此范围。

维护：VTGSmartTriggerComponent.cpp 负责传递配置；VTGUIFeedback -SmartInteractionDetails -VerifyOnly 可验证实例可编辑属性及两个实例的实际 ISX 数值。
