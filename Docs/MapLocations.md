# 地图地点名称与悬浮介绍

重新打开 Unreal Editor，进入：

`/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/MapWBP/WBP_Map_1`

在 Designer 的 Hierarchy 中逐个选择 `WBP_MapNode`、`WBP_MapNode_1`、`WBP_MapNode_2`、`WBP_MapNode_3`。在 Details 的 **Map Location** 类别里填写：

- **地点名称 / Location Name**：显示在该按钮内部，普通、hover 和选中状态均显示。
- **地点介绍 / Location Description**：支持多行。鼠标停在按钮上时，在旁边的黑色悬浮框里显示白字，长文本自动换行。

四个按钮的初始名称分别为 Slickwater Docks、Bureau Module、Commerce Module、North Tunnel，来自现有图标名称，可以任意修改。介绍初始为空；未填写介绍时，悬浮框只显示地点名称。预览图片中的介绍是测试时的临时示例，没有写入地图资产。

## 本地化

两个字段都是具有独立文本身份的 FText，与项目其他显示文字使用同一套手工翻译表和语言设置。

- 上方 **Origin** 修改原文。
- 下方 **多语言译文** 折叠区填写 English（en-US）、简体中文（zh-Hans）以及项目配置的其他语言。
- 先填写非空原文，再填写译文，使用现有 **保存并应用译文** 操作。
- 未翻译时沿用 Origin。切换游戏语言时，按钮名称和已经出现的悬浮文本都会通过 FText 更新。

也可以在 Content Browser 选中 `WBP_Map_1`，通过 Window → 手工多语言编辑收集该资产的文本。不要为了翻译改动 MyNodeData 中的 MapNode、LocationTag 等内部标识。

## 实现与验证

WBP_MapNode 继承新的 VTGMapLocationNode，原有 hover、点击、贴图切换和子区域列表保留。仅将名称改为独立显示文本，并修复原标签 8 像素换行宽度及可见性。悬浮框使用 UMG 的标准 tooltip 放置机制，不接收点击或键盘焦点。

实际蓝图实例已验证：4 个独立名称、名称和介绍的手工译文保存/读取、文本语言即时更新、hover/unhover/click 回调、名称布局与黑底白字渲染。C++ 与蓝图编译通过。

预览：`Scripts/MapLabels/map-preview.png`、`Scripts/MapLabels/tooltip-preview.png`。

原资产备份：`Saved/QA/MapLabelsBefore/`。重复迁移/验证入口：`-run=VTGAlleyRepair -MapLabels`；只验证时增加 `-VerifyOnly`。
