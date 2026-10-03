# Plane 图片循环屏幕

蓝图：`/Game/Movie/TVshow/PlaneSlideshow/BP_PlaneSlideshow`

材质：`/Game/Movie/TVshow/PlaneSlideshow/M_PlaneSlideshow`

## 使用

1. 将 BP_PlaneSlideshow 拖进关卡。
2. 选中实例，在 Details → Slideshow → Images 添加数组元素，将 Content Browser 中的 Texture2D 图片放入，数组顺序就是播放顺序。磁盘上的 PNG 需要先导入 Unreal。
3. 编辑器显示第一张有效图片；按 Play 后自动循环。
4. Hold Seconds 是完整显示每张图片的时间，默认 4 秒；Fade Seconds 是随后交叉淡入淡出的时间，默认 0.5 秒。设为 0 可直接切换。
5. Screen Width / Screen Height 控制屏幕宽高，单位厘米，默认 80 × 45。可正常移动、旋转和缩放整个 Actor。

Auto Play 控制游戏开始时是否播放，Loop 控制末尾是否回到第一张。关闭 Loop 后停在最后一张。Brightness 控制自发光强度，默认为 1。

图片铺满屏幕，比例不一致时会拉伸；建议使用相同比例的图片，并让屏幕宽高匹配它们。默认是竖直的双面屏幕，无碰撞、不投影。只有一张有效图片时保持静态；空槽会跳过，完全没有图片时隐藏屏幕。

## 蓝图控制

- Play：播放或恢复。暂停淡入淡出后会从暂停位置继续；静态画面暂停后恢复会重新计算停留时间。
- Pause：停止定时器，并冻结正在进行的淡入淡出。
- Restart：返回第一张有效图片并播放。
- Next Image：切换到下一张，使用淡入淡出；遵循 Loop。暂停时手动切换只播放该次过渡。
- Show Image(Index)：立刻显示指定数组下标的图片，下标从 0 开始。不会自动恢复暂停的播放；无效下标和空槽不产生变化。

播放逻辑在 C++ 父类 AVTGPlaneSlideshow 中，材质实例为每个屏幕独立创建。停留期间用定时器，逐帧更新仅发生在淡入淡出期间。原有 BPM_TVep1Draft 和关卡未修改。

## 验证和维护

新增 C++ 需要编译项目。生成器仅用于首次安装，不覆盖已有蓝图/材质：

```powershell
& 'G:/Epic/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/Epic/Unreal projects/VertigoGit/Vertigo.uproject' -run=VTGPlaneSlideshow -Verify -unattended -nop4
```

验证会重新载入蓝图并检查编译、淡入淡出、暂停/恢复、循环、末尾停止、无效图片、空数组、单图和实例隔离。日志在 Saved/Logs。自动检查不替代关卡内实际画面检查。
