# DontLookBack1 音频拖入后不显示

检查当前保存的 `/Game/LevelNSequence/In_Level/L4Tunnel_NearMarket/LS_L3Tunnel_DontLookBack1`，未修改资产。

- Sequence 支持 Audio Track，未保存为 bReadOnly，音频片段自身也未锁定。
- 现有 `MovieSceneAudioTrack_1` 有 5 个片段，其中用户指定的 `CutsceneSFX_L3Tunnel_Music` 已有 4 个实例，分别在 row 3（1.3667s）、row 4（1.4333s）、row 1（9.4333s）、row 0（2.1667s）。另一个片段为 NoMusic。
- MovieScene.EditorData 中 `MovieSceneAudioTrack_1` 的 bExpanded=False；音频分布在多条子行，折叠的父轨无法直接逐一显示各子行片段。
- 因此已有确证是音频引用已经加入并保存，并非 SoundWave 不受支持。先展开 Audio 左侧三角；也可以选中 Audio 按 V（Sequencer 的 Expand/Collapse Nodes 快捷键），展开子行检查。若实时 UI 仍未刷新，关闭并重新打开此 Sequence。
- 当前已有多个重叠音乐片段；应在展开确认后由用户决定保留哪段，避免继续重复拖入并叠加播放。

桌面窗口列表已确认有独立 Sequencer 窗口，但系统画面捕获超时，未直接观察到用户当前 UI；以上依据资产保存状态和本地 UE 5.3 轨道模型实现。检查资料为本目录 `.t3d` 和 `report.json`。
