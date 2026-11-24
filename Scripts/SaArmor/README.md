# Sa 起跑金属叮当声

资源目录：`/Game/SFX/SaArmor`。保留原始 1.61932 秒钥匙声，并拆成三个连续的约 0.53977 秒片段。播放片段为单声道 PCM，每段 5 毫秒淡入防爆音，最后约 0.30 秒淡出；不循环。

`BP_Player_Sa → PlayerAudio → Armor Foley`：
- `Armor Run Start Sounds`：三个 SoundWave，起跑时等概率随机选一个。
- `Armor Volume`：默认 0.45。
- `Restricted Run Speed Threshold`：默认 300，位于现有受限步行 273 和受限跑步 340 之间。

普通模式读取 `IsSprinting`；`NoRunning!SlowDownWalk=true` 时读取实际移动组件速度上限，因为该蓝图分支不设置 `IsSprinting`。两者都要求在地面上实际移动，并沿用攻击、闪避、死亡和隐藏状态限制。持续跑动只触发一次，停下后再次起跑才重新触发。裸装由当前 Mesh 与已有 `BarefootMesh` 比较；裸装期间不播放，中途换装不会被当作起跑。

声音附着角色 Mesh，复用脚步的空间衰减。淡出写入音频片段，持续跑步也会自然结束。每帧检测起跑；下水道声学射线仍保持 0.5 秒间隔。

导入脚本：`Scripts/SaArmor/import_audio.py`（PythonScript commandlet）。配置：`-run=VTGSewerAudio -ArmorOnly`；只读复验：`-run=VTGSewerAudio -VerifyOnly`。备份角色蓝图在 `Scripts/SaArmor/Backup`。构建与检查日志保存在本目录。

未进行游戏内人工试听。

三个片段分别调整至约 -12 dBFS 峰值，避免原录音电平过低；原始立体声文件保持不变。可用 prepare_audio.ps1 重建处理后的 WAV。构建及配置、重新加载后的只读测试均通过（0 errors）；现有资产警告仍有 135 条。
