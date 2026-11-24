# BPLM Sequence 倍速恢复（2026-10-15）

按 `/Game/LevelNSequence/BPLM` 的实际继承关系扫描：父蓝图 + 14 个子蓝图。检查 MovieSceneSequencePlayer.SetPlayRate 的输入来源、CreateLevelSequencePlayer 的 Settings.PlayRate、关联变量默认值及写入节点。动画 Montage 的播放速度不在修改范围内。

修改 4 个资产，保留现有变量和节点连接：

| 蓝图 | 变量 | 原值 | 新值 |
|---|---|---:|---:|
| `/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment` | `Play Rate` | 10 | 1 |
| `/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight` | `Play Rate` | 5 | 1 |
| `/Game/LevelNSequence/In_Level/L4Tunnel_NearMarket/BPLM_Tunnel_NearMarket` | `Play Rate` | 5 | 1 |
| `/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/BPLM_ShoppingCenter_ClinicYard` | `Test_SeqPlayRate` | 4 | 1 |

`BPLM_Clinic.Seq Play Rate` 原本为 1，保持不变。未发现额外的 Play Scale 倍速变量。CreateLevelSequencePlayer 未设置 Settings 时使用引擎默认 PlayRate=1。

维护入口：在上述蓝图 Class Defaults 搜索对应变量；节点上的 Set Play Rate / Create Level Sequence Player → Settings → Play Rate 也应保持为 1。

跳过剧情的 20 倍速由 `Source/Vertigo/Private/UI/VTGSequenceSkipSubsystem.cpp` 独立控制，本次未修改该功能或允许跳过的剧情范围。

检查/修复工具：`Source/VertigoEditor/Private/VTGBPLMRatesCommandlet.cpp`。默认只审计；`-Apply` 写入；`-VerifyOnly` 发现非 1 数值时返回失败。修改前资产备份位于本目录 `Backup/`。

验证记录：`build.log` 编译；`apply.log` 四个蓝图编译和保存；`verify.log` 独立进程重新读取已保存资产。此次验证针对蓝图默认值与图表，不含逐关卡游戏实测或关卡实例覆盖值扫描。
