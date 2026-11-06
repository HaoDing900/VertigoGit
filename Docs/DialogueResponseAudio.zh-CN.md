# 对话选项 NarrativeSFX 与确认过渡维护

## 使用位置

实际使用的按钮：`/Game/Narrative/NarrativeUI/Widgets/WBP_NarrativeButton_DialogueOption_ProfilePicture`。

- 确认选项：随机播放 `/Game/SFX/N/NSFX_Response1`、`2`、`3`。
- Hover 选项：随机播放 `/Game/SFX/N/NSFX_ResponseHover1`、`2`、`3`。
- 两组分别避免连续重复。素材通过原生类强引用参与打包。
- 鼠标和已有 Simulate Clicked / SimulateHovered 路径共用接入点。自动播放的 Player 台词不会触发 UI 确认音效。

## 行为与优先级

确认函数先检查按钮可用、未锁定、对话正在等待选择、选项仍然有效。点击时播放 NarrativeSFX，隐藏未选选项和旧台词显示；保留已确认选项的外观，忽略过渡期间的 Hover / Unhover 外观更新。

总时长取三条确认素材的平均长度，四舍五入到 10 ms。当前素材长 1.301 / 1.414 / 1.233 秒，因此总时长约 **1.32 秒**。前 60%（792 ms）保持选中，接下来 30%（396 ms）淡出，最后 10%（132 ms）留空，再调用原有 `TrySelectDialogueOption` 一次。使用 Timer 与 UMG 动画，没有新增 Tick。计时随游戏暂停，低帧率时在截止时间后的首个可执行帧提交。

Narrative 自身的选择/跳过入口共享 pending guard，重复点击和直接调用选择都不能绕过。取消对话、销毁选中按钮或更换对话会清理计时器并还原原始 UI 可见性/透明度；只释放本次过渡拥有的锁。无台词路由仍交给 Narrative 原逻辑处理。不会改变实际台词的阅读时间。

确认声停止旧 Hover，并注册为 Narrative 的优先音效：立即停止旧 Blip，过渡期间抑制新 Blip。提交或取消时只撤销该确认声的优先标记，让下一句正常触发 Blip；确认声和旧确认声的尾音继续自然播放。其他 Quest 音效自己的优先标记保持不变，不会停止正式配音或 Quest 音效。

Hover 同时最多一条，新的 Hover 替换旧声音，离开或销毁按钮时停止。触发间隔至少 60 毫秒；确认、Quest 或正式配音等优先音效播放期间忽略 Hover。

当前播放音量和 Pitch 倍数均为 1.0，不使用 Blip 的 0.4 音量倍数。未改动音频资产自身音量或按钮样式；现有按钮样式的 Pressed / Hovered Sound 均为空。

## 维护文件

- `Source/Vertigo/Private/Audio/VTGUIFeedbackLibrary.cpp`：两组素材列表。
- `Source/Vertigo/Private/Audio/VTGResponseAudioSubsystem.cpp`：播放、去重、Hover 间隔、确认优先级和确认入口。
- `Source/Vertigo/Public/Audio/VTGResponseAudioSubsystem.h`：每个 World 独立的声音状态，无 Tick。
- `Source/Vertigo/Private/Audio/VTGResponseTransition.cpp`：`GetTransitionDuration()` 计算平均时长；`BeginTransition()` 的 0.6 / 0.9 设置保持与淡出截止点。
- `Plugins/Narrative/Source/Narrative/Private/Dialogue.cpp`：原生 pending guard，与 Narrative 选择/跳过/退出流程联动。
- `Source/VertigoEditor/Private/VTGResponseFeedback.inl`：蓝图接线安装与验证。

安装命令：`-run=VTGUIFeedback -ResponseAudio`；只检查现有接线与素材则加 `-VerifyOnly`。原按钮备份在 `Scripts/UIFeedback/Backup/WBP_NarrativeButton_DialogueOption_ProfilePicture.uasset`。

本次按项目现有单人模式接入；未新增多人网络音效同步。

回归：`-run=VTGUIFeedback -ResponseTransitionTests`。无声命令行测试验证时序、单次提交、暂停、取消、返回菜单和尾音组件保留；最终音画观感仍应在 PIE 试听。
# 2026-11-01 视觉过渡补充

最新调整：确认音效通过 SpawnSound2D 的 PitchMultiplier=1.1 加速播放（音高也会略升，不修改源素材）。UI 在此前 920 ms 的基础上连续乘以 0.9 和 0.8，当前总时长约 660 ms。按下外观保持约 252 ms，再用 151 ms 渐变回 hover；整按钮在 429–594 ms 淡出，淡出速度在压缩后的基础上提高 20%，594–660 ms 留空。尾音继续保留对 Blip 的优先级。

ResponseChoiceLocked 由临时 UVTGResponseLockedBackdrop 显示，平时隐藏且不接受输入。点击时以全屏锚点、零边距铺满整个游戏 viewport（不跟随按钮尺寸），在独立 viewport 图层 ZOrder=-1000（低于常规对话 UI）显示；0–50 ms 淡入，保持到 429 ms，429–594 ms 与按钮一起淡出。完成、取消和销毁时隐藏并移除，避免残留或闪现。使用原 Texture2D 强引用参与打包。

最新节奏：UI 总时长改为确认素材平均长度的 70%，当前约 920 ms；552 ms 保持、276 ms 整体淡出、92 ms 留空。按下反馈层同步压缩为保持 350 ms、渐变 210 ms。确认音效仍完整原速播放，UI 提交后尾音保留 Blip 优先级，播放结束后自然释放；取消过渡也不截断尾音。

暂停隐藏已补齐 CharacterName_Main／Side、MonologueBox、ThoughtBox、MailBox，涵盖普通对话、独白、思考与邮件。Mail 主／侧名字的输出经 FilterSpeakerLabel 过滤：SpeakerID 为 Mail、IDMail、ID_Mail，或显示名为 Mail（忽略大小写）时输出空名字；其他角色保留原格式。规则仅影响界面名字，不修改 Speaker 数据或历史记录。

后续修复：HUD 的 BP_Narrative3Overlay 通过同 World、同本地玩家定位，兼容其 NarrativeComponent 字段尚未赋值的情况。框、头像、文字同时设置 Hidden 与透明度零，取消／提交时恢复。按钮淡出结束即设 Hidden，阻止 UMG 稍后恢复透明度造成闪现。回归检查使用真实 Overlay 资源中的四个框／头像控件，并模拟动画结束后的透明度恢复。

确认后 Line、DialogueBox／DialogueBox_Side、AvtProfile_Main／AvtProfile_Side 同步隐藏。按钮使用本实例的原生 NormalPressed 画刷作为临时反馈层：0–500 ms 保持，500–800 ms 渐变回原有 hover 背景。文字与图标继续显示在反馈层上方。整按钮仍在 792–1188 ms 淡出，1188–1320 ms 留空，1320 ms 提交。音频尾音独立自然结束。

成功提交时旧选项保持隐藏，避免恢复透明度造成闪现；取消时恢复原始 UI 状态。临时层在清理时移除并恢复原内容与按钮槽布局。该改动没有修改共享 Button Style 资源。

