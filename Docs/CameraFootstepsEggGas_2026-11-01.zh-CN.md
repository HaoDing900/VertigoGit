# 锁镜头、脚步与蛋烟雾维护

锁镜头：BP_0CameraLockPoint、BP_1CameraLockAutoTrigger 的相机模板及地图实例关闭 bConstrainAspectRatio。保留 AspectRatio 数值、FOV、位置、后处理等其他设置。工具限定 Content/ThirdPerson/Maps，通过资源引用关系查找其中地图及其外置 Actor 包，只保存发生约束变化的资源。备份位于 Scripts/CameraFootstepMaintenance/Backup，实例检查记录位于 Saved/CameraAspectChanges.txt。

脚步：PortCity 的 SM_Stairs_Metal、SM_Stairs_Small_Metal 显式归 Metal；MI_BarFloorTiles6 及子实例归 Concrete。赤脚仍优先。MM_Sa_Land 在 0.06 秒添加 VTG_Footstep_Landing，调用现有脚步组件、材质分类与音效池。落地 Notify 允许水平速度为零，普通走路 Notify 仍要求移动。

烟雾：原先的 WorldStatic／WorldDynamic 对象类型射线会把不阻挡视线的触发盒也当成障碍，导致范围内伤害被全部拦截。改用 Visibility 遮挡检测并排除 Pawn，真正的墙体仍挡伤害。未改变蛋的伤害倍率、范围、对白保护和闪避规则。

修复前测试已复现：加入 Visibility=Ignore 的触发盒后真实 Sa 在烟雾内维持 100 血。修复后召唤蛋与无召唤标记的普通蛋均通过实际蒙太奇 Notify／技能伤害链检查，范围内血量 100→96→95；实体墙后、范围外、对白或闪避期间不掉血。

维护入口：VTGUIFeedback -CameraAspectBatch（批量应用）／-CameraAspectBatch -VerifyOnly（复查）；-LandingFootstep／-LandingFootstep -VerifyOnly；VTGSewerFollowup -EggDamage；自动化测试 Vertigo.Audio.FootstepSurfaces。
