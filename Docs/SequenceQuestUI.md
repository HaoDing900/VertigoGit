# Sequence 期间隐藏 Quest UI

Sequence 快进检查与 Sequence/关卡 Actor 的 Details → 过场入口均提供 **播放期间隐藏 Quest UI**，默认开启，修改后保存 Sequence。未添加设置的既有 Sequence 也使用开启默认值。

仅影响 BP_Narrative3Overlay 的 TaskBox：任务图标、标题、通知和目标列表。对话正文、头像及其他 UI 沿用自身逻辑。

播放与中途暂停期间遮挡；自然结束、停止、销毁或完成后 Pause At End 停留最后画面时解除遮挡。若多个 Sequence 重叠，只要仍有一个活动 Sequence 要求隐藏就继续遮挡。

实现使用 UVTGQuestSequenceBox 的独立内层 Slate 显示门控。外层 UMG Visibility、RenderOpacity、任务事件、动画和文本继续由 Quest 自己控制。结束时取消门控，不强制显示已经被任务逻辑隐藏或结束的通知，也不补播已过期通知。

TaskBox 保留原名称、Canvas Slot、三组子项布局及 Blueprint 事件/动画；Sequence 开关保存在 UVTGSequenceSettings.bHideQuestUI。默认关闭/开启入口退出对话的设置与本开关相互独立。

验证：Vertigo.Sequence.QuestUI 覆盖独立门控、实时任务可见性修改、文本/头像独立性、暂停、开关、重叠、停止、自然结束、销毁和 Pause At End；Vertigo.Dialogue.History、SideTextFit 回归覆盖实际 Overlay 及既有对话行为。NullRHI 自动化不代替游戏内画面验收。
