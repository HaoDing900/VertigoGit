# 场景音频面板与 Sa 脚步调整（2026-11-05）

- `VTGCutsceneAudioDetails` 继续保留 Actor 和 AudioComponent 两种检查入口，以及它们原有的编辑器定制。根据 Details Builder 的基础类决定由哪一方添加自定义行，避免 AmbientSound 单组件 Actor 同时触发两个入口造成重复。实际 Details 控件树与渲染验证：AmbientSound 1 行、AudioComponent 1 行。
- `VTGPlayerAudioComponent` 在 `NoRunning!SlowDownWalk=true` 且已评估动画帧包含有效权重的 `/Game/Characters/Sa/Anm/BS_Sa_NoRunning` 时，额外乘以 0.8。原 FootstepVolume、随速度变化的强度、随机 Pitch、衰减和音效池不变。其他动画来源及单独一个条件成立时均保持系数 1。
- `/Game/Characters/Sa/Anm/Loco/Sa_am_Loco_Walk_Fwd_NoRM` 增加两个 VTGFootstepNotify：0.35 秒 foot_r、0.95 秒 foot_l。依据指定时间点两脚的组件空间高度选择落地脚；不是固定 PlaySound，因此沿用材质分类与 Sa 脚步状态检查。保留其他 Notify，不改变动画姿势、时长或 BlendSpace 配置。

原动画备份：`Scripts/SaFootstepPolish/Backup/Sa_am_Loco_Walk_Fwd_NoRM.uasset`。

验证工具：`-run=VTGMailSequencePolish -FootstepPolish`；`-Verify` 在新进程只读检查保存动画并运行真实 Sa 动画蓝图的 Normal / NoRunning BlendSpace 测试。验证了两个条件各自不满足的情况、返回普通移动，以及基础 FootstepVolume 不变。

`-DetailsPreview=绝对路径前缀 -AllowCommandletRendering` 在临时 Actor 和组件上构造实际 Details 面板，计数标签并导出预览。日志及预览在 `Scripts/SaFootstepPolish`。资产修改只涉及上述一份动画，其他两项为 C++ 修改，需要新启动的 Editor 加载已编译 DLL。
