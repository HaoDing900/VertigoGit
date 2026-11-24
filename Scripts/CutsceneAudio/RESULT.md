# 场景声音与过场：使用说明

Audio Manager 只保留 Ambient、Music。独立 OneShot NE 不受这次调整影响。

## 你需要做的两步

1. 在关卡中选中放置的 Sound Cue / Ambient Sound Actor，在 Details → **过场音频** 勾选 **跟随过场环境声**。也可以在 Actor 的 Audio Component 上设置。**默认关闭**，只影响当前声源实例；同一个 Cue 的其他实例独立设置，Cue 资产不变。保存关卡。
2. 选中该 cutscene 的 Level Sequence Actor，在 Details → **过场音频 → 过场声音预设** 选择策略，然后保存 Sequence 资产。

| 过场声音预设 | Ambient 与勾选的场景声源 | Music |
|---|---|---|
| 保留现场声音（默认） | 保留 | 保留 |
| 突出剧情（压低环境，淡出音乐） | 保留原音量的 25% | 静音 |
| 静音环境与背景音乐 | 静音 | 静音 |

Audio Manager 的两个通道自动遵循预设，无需勾选。场景实例勾选本身不会立即静音；它只决定该声源是否跟随正在播放的 Sequence 策略。没有管理器也可控制勾选的场景声源。

Sequence 预设保存在 Sequence 资产中，多个地方复用同一 Sequence 时共用该策略。新建或未配置的 Sequence 默认保留现场声音。不会自动批量静音已有过场。

## 自动处理的情况

- 默认约 0.35 秒淡入淡出；仅调整音量，不主动 Stop / Play，也不替换声源资源。
- 过场暂停时保留策略，停止、跳至结尾停止、销毁后解除；多个过场重叠时取更低的音量，全部解除才恢复。
- 恢复当前音源与当前基准音量，过场中 Stage 换音乐也不会恢复成旧音乐。
- Sequencer 生成的前景 Actor 不会因复制了场景开关而被此规则静音。

任意蓝图的自定义 Audio Component 也可选中组件设置开关；运行时可用 `Set Follow Ambient Sequence Mute`。没有独立组件引用的 `Play Sound` 调用无法通过此实例开关控制。角色脚步、对白、独立 OneShot 不会被自动归类为 Ambient。

预设用可运行的 `UAssetUserData` 保存；实例开关用组件 Tag 保存。不是修改 Sound Class 的全局静音规则。运行时约每秒检查 20 次实际 Sequence 播放状态。

## 验证记录

- Editor Development 编译通过。
- 过场行为 28 项、Ambient/Music 22 项、独立 OneShot 29 项检查通过。
- 单独进程保存与重新加载：同一 Cue 的两个实例开关互不影响，Sequence 预设保留。
- 只读检查 SaApartment、L2_StreetOutsideBar：现有管理器实例仅有两个通道，没有遗留 Psych 组件。
- 测试未开启音频设备，未进行游戏内试听或完整打包。静音时的播放进度仍受 Unreal 原有声音虚拟化配置影响。

迁移前资产备份在 `Scripts/CutsceneAudio/Backup`。未删除闲置的示例声音文件。原有生产地图、Cue、Sequence 未被批量修改。
