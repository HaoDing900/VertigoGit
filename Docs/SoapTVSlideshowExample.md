# BaseLevel / SoapTV 对话幻灯片示例

已配置资产：
- `/Game/ThirdPerson/Maps/BaseLevel`
- `/Game/2DArt/SlideShow/testSlideShow`
- 场景里的 `SlideShow_L4BlackMarket_SoapTV`，ID 保持原来的 `Soap_ep1`。

## 运行

BaseLevel 的 Level Blueprint：BeginPlay 新增 Sequence 分支（Then 0 保留原逻辑），Then 1 等待 1 秒后调用 `StartSoapTVSlideshow`。

`StartSoapTVSlideshow` → Get Player Character → Get Component By Class (NarrativeComponent) → Cast to NarrativeComponent → Begin Dialogue。

Begin Dialogue 已选择 testSlideShow，Play Params > Start From ID 为 `testSlideShow_DialogueNode_NPC_1`。如果不希望进入关卡就自动播放，断开新 Sequence 的 Then 1；在你希望的时机调用 `StartSoapTVSlideshow` 即可。

## 对话里的换图蓝图

打开 testSlideShow，双击以下台词节点，会跳到已接好的 Event Graph：

| 节点 ID 后缀 | 对应数组项 | Show Slide 编号 |
|---|---|---|
| NPC_1 | index 0 | 1 |
| NPC_3 | index 1 | 2 |
| NPC_4 | index 2 | 3 |

每一组都是：
`该对话节点开始事件 → Branch（Is Valid）→ Show Slide`。
`Find Slideshow By ID(Soap_ep1)` 的返回值同时接到 Is Valid 和 Show Slide.Target。

以后修改这一组蓝图中的 ID 和 Slide Number 即可客制化。Slideshow 场景实例必须有同样的 ID；Show Slide 编号始终是数组 index + 1。

NPC_2、NPC_3、NPC_4 的可见测试台词保留原来的 0、1、2，并设为 Never + Is Skippable；点击推进。NPC_1、NPC_5 保留原来的空控制节点。

## 结束

NPC_5 的 Details > Events 挂了 `VTG Close Slideshow`：ID=Soap_ep1，Runtime=End，Fade Seconds=0.5。

这里使用节点的 End 事件，因为当前插件双击生成的蓝图事件只在节点开始时调用，无法用于真正的结束时刻。

默认结束时幻灯片淡出并自动移除，返回游戏；On Slideshow Closed 随后发出通知。若是结局，希望保留黑屏，勾选场景 Director 的 Keep Black Screen On Close。此示例不自动切其他关卡；需要接 credits 可绑定 On Slideshow Closed。

## 验证与备份

Dialogue 与 Level Blueprint 编译无错误；保存后通过单独 UE commandlet 重新加载检查。尚未执行实际 PIE 画面与输入测试。

原始关卡和对话备份：`Saved/SlideshowExampleBackup/Content/` 下对应路径。

