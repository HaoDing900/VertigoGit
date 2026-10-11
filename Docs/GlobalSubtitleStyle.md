# 全局字幕样式

重启 Unreal Editor 后，字幕轨道右键 **全局字幕样式（独立窗口）**。不需要先打开字幕预览。

独立面板可停靠、浮动、关闭；点击“全局字体、颜色与描边”标题箭头可折叠设置区。预览开关仍在原预览面板中，只影响编辑器预览，不关闭游戏字幕。

**Use Global Subtitle Style** 默认开启。修改 Subtitle Font / Speaker Font、字号、Text Color、描边开关/宽度/颜色，会实时影响所有使用本系统且未设置轨道覆盖的 Sequence 字幕。点击 **保存全局字幕设置**，写入项目 DefaultGame.ini，重启和游戏运行均可使用。也可在 Project Settings 搜索 Global Subtitle Style。

特殊过场可在字幕轨道勾选 **Override Global Style**，使用该轨道的原样式。关闭全局 Use Global Subtitle Style 后，所有轨道恢复各自的样式；既有样式数据没有被批量修改或删除。

全局设置不改变台词、时间、本地化标识或显示说话者规则，也不影响 Narrative 系统原有对话 UI。

验证：编译通过，Vertigo.Sequence. 下三组测试通过；覆盖不同轨道共享全局字号、实时修改、局部覆盖、关闭总开关恢复本地样式及原预览/字幕时间行为。独立窗口的交互式视觉布局未在用户编辑器中复查。
