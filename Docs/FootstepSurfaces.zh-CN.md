# Sa 材质脚步试用

2026-11-05：核对 `M_Wall_Concrete1` 家族。以下五个现有资源及其动态子实例均通过 Concrete 路由检查；规则沿父实例链继承，后续派生实例同样生效。现有分类已满足要求，不需要改材质外观或 shader。

- `/Game/EnvArt/Material/Hard/M_Wall_Concrete1`
- `/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1`
- `/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1_Inst`
- `/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1_Inst1`
- `/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1_Inst2`

资产注册表递归检查涵盖了上述父级的全部已注册 Material Instance 子资源。验证工具为 `VTGMailSequencePolish -PoliceFootsteps`，报告在 `Scripts/PoliceFootsteps/diagnosis.log`。该工具只读，不保存地图、材质或对白资产。分类优先级仍遵循下方规则（赤脚及显式实例表面标签优先）。

警局对话后偶发无脚步声尚未在隔离测试复现：七个 Narrative Sequence 事件结束后，动画模式、脚步播放条件、音量、池内容与测试地板射线均正常。此结果不等同于完整警局地图的游戏内试听，也未验证实际游玩中的动画通知是否到达；按用户要求暂不修改相关运行时行为。

补充：SM_FireEscape_Landing2 有明确 Metal 网格规则，其默认材质 MI_FireEscape_a 也在 Metal 材质表中。赤脚判断仍先于网格和材质判断。

2026-11-01：已接入原来的动画脚步 Notify，无须再给动画添加 Play Sound。

- 实际身体 Mesh 为 BarefootMesh 时始终使用 BarefootSteps，地面不覆盖赤脚。赤脚池为空时保持静音。
- 穿鞋：组件／Actor 的 AudioSurface.Water、Metal、Concrete、Wood 标签优先；其次查询命中面的材质及其父实例链；最后尝试材质／物理材质名称和已有 Wood、Concrete 物理表面类型。
- 用户指定的 17 个材质名称对应项目中的 22 个完整资源路径，已在 VTGPlayerAudioComponent.cpp 的 MaterialIsMetal 表中列出。同名副本均按指定类型分类。材质子实例继承最近的已分类父级。
- MI_Ground_Tiles_01 按要求归 Metal。消防梯按 MI_FireEscape_a、MI_B4_Trim_b 材质归 Metal；没有增加整目录或 mesh 名称规则。
- 未知地面或穿鞋表面池为空时回退原来的 DefaultSteps（Boots）。音频继续使用 Content/SFX/SaFootstep 下的已有池。
- 多材质网格优先用碰撞面索引选择材质；如果碰撞没有面信息且各材质槽不同，不猜测整块网格的材质，使用物理材质／显式标签或默认池。

维护入口：Source/Vertigo/Private/Audio/VTGPlayerAudioComponent.cpp 的 ResolveFootstepPool。音量及各个音效池仍在 Sa 的 VTGPlayerAudioComponent 上配置。当前路径表在 C++ 中，移动这些材质资源时需要同步路径。

试听建议：穿鞋走过 Concrete 和 MI_Grid2／消防梯，再换赤脚走同一路线。赤脚应始终为原来的三条赤脚音效。自动化测试验证分类、子实例、换装与回退；听感和各关卡实际碰撞仍需在游戏中试听。
