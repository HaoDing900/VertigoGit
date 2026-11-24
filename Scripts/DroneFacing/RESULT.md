# Drone 移动朝向修正

父蓝图：/Game/Characters/BP_StillNPC
新增纯函数：GetMovementFacingRotation

输入 DesiredWorldRotation：模型视觉上希望面向的世界旋转（例如 Find Look at Rotation 的输出）。
输出 TargetRelativeRotation：已补偿且已转换到 Capsule 父组件空间的旋转，接 Move Component To 的 Target Relative Rotation。
不要对已经补偿过的旋转再次调用此函数。

复用 FacingYawOffsetForFollow。Drone 原值为 -90 度；没有修改该值、Mesh Transform、Follow 图或 ABP。
已将 BPLM_BlackMarket 的 K2Node_CallFunction_154 接到此父类函数。
原生 Move Component To 不会自动调用父类函数；其他调用需要显式接入输出。
只影响目标朝向，保留原节点的转身插值、目标位置和 2.5 秒时长。

实现：Source/Vertigo/Public/Characters/VTGMovementFacing.h 和对应 cpp。
迁移与校验：VTGDroneFacing commandlet；-VerifyOnly 为只读资产校验。
原资产备份位于 Backup/。BPLM 备份包含本次修改前已有的用户改动。

验证：C++ Development Editor 编译；父类、Drone、BPLM 三个 Blueprint 编译；
实际调用继承的 Blueprint 函数验证四个方向及角度跨界，并确认 Mesh Transform 和 Follow 偏移保持原值。
没有进行游戏画面的 PIE 视觉验收。
