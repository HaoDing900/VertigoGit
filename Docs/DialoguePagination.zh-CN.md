# 英文对话同节点分页

2026-11-15。普通阅读型 NPC 台词默认启用，Origin/中文保留完整原文，不自动分页。Project Settings → Game → Manual Localization → Dialogue → English Dialogue Pagination 可关闭。

## 作者操作

在现有 Narrative 节点中保留完整 Origin 中文。英文仍填写到同一文本字段的 en-US 译文，保存并应用即可。英文放不下实际 Line_Main 文本框时自动分页：优先完整句末，避开常见称呼缩写与小数，然后尝试逗号/分号及单词边界。不以字符数作为页面容量。

需要精确节奏时只在英文译文插入 `[page]`，例如 `First sentence.[page]Second sentence.`。标记不需要出现在中文，显示和历史面板中均隐藏。每个手工分段如果仍过长会继续自动分页。跨页富文本标签自动关闭并在下一页重开，不切开标签或 Unicode 字素。无法安全容纳一个单词/损坏标签时保留完整文本，避免丢字；特别长的不可断单词需作者处理。

## 行为

手动 Continue 和既有跳过输入先翻页，最后一页才执行原节点完成流程。Auto 每页用现有阅读时间函数计时；Auto OFF 的自然计时结束只等待继续。翻页不重播节点开始/结束事件、动画、镜头、配音或 Blip。CurrentLine.Text 始终完整，历史仍只记一条。

仅 standalone、普通可跳过 NPC 阅读型台词启用。玩家选项、自由移动短评、路由、不可跳过、固定时长、配音、明确 Shot 台词不分页，以保护原演出时序。不扩展到 Sequence 字幕。页码在现有对话提示层显示。切换回中文/Origin恢复整段；切回英文或改变框大小，从当前节点第一页重新排版，不重播节点事件。

布局使用当前实际挂载 BP_Narrative3Overlay.Line_Main 的几何与富文本样式，等待实际布局就绪后建立页面；没有可见文本框时不猜测页面容量。特殊 UI 类/自定义装饰器仍应在真实画面检查。

## 验证

UE5.3 Development Editor 编译通过。Vertigo.Dialogue.Pagination.Flow、Split 通过，覆盖手工分页、句末/缩写、富文本、实际 RichText 测量、手动/自动页内推进、完整文本不变及最后一页才触发结束事件。扩展的 Vertigo.Dialogue.History 在真实 PIE 挂载的 Narrative widget 上通过，覆盖英文自动分页、实际正文/页码、Origin/中文不分页及语言切换。运行使用 NullRHI，无可见画面验收；尚需实际长译文、特殊字体/装饰器的观感检查。

日志：Saved/Logs/DialoguePaginationTests.log、DialoguePaginationUI.log。

## 退出崩溃

修复 VTGManualLocalization::Shutdown 在 Slate 已销毁后仍访问 FSlateApplication::Get 的断言。输入预处理器与窗口管理器清理增加 IsInitialized 检查。此修正与分页一起编译，正常编辑器退出烟测日志 LocalizationShutdownSmoke.log。
退出验证补充：使用 QUIT_EDITOR 完整启动/正常退出，进程退出码 0，无 CurrentApplication.IsValid 断言。日志 Saved/Logs/LocalizationShutdownVerified.log。

## Side Textbox 自适应与兜底分页（2026-11-19）

BP_Narrative3Overlay 的 Line_Side 在非 origin 语言下按实际框体与富文本样式逐号缩小。可读下限为原字号的 75%，且通常不低于 14；原设计字号本身更小时保留原字号作为下限。短文本、切换回 origin 或结束对话时恢复原样式。共享样式表不会被修改。

普通阅读型 NPC 的侧框台词缩小后仍溢出时，在同一节点内分页。动作/自由移动模式使用每页阅读时间自动接续；最后一页才执行节点结束流程，不重播节点事件，CurrentLine.Text 和历史保留完整台词。侧框适用于所有非 origin 语言，不受主框 English Dialogue Pagination 开关限制；主框仍沿用上文的英文分页规则。

固定时长、配音、Shot、不可跳过和玩家选项保留既有演出时序，使用字体适配，不插入额外的节点阅读页。可用 [page] 显式控制支持分页台词的断点。
