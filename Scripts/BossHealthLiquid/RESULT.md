# Boss 血条液体材质（2026-11-21）

把 `/Game/Widget/BossHealthBar/M_BossHealth_Caustics` 从「平面焦散贴图」重建为分层的玻璃+液体
UI 材质，并新建 `MI_BossHealth_Caustics` 作为调参入口，`WBP_BossHealthBar` 的 `HealthBar`
Fill Image 改指向这个实例。

## 改哪里

**日常调整只改材质实例 `/Game/Widget/BossHealthBar/MI_BossHealth_Caustics`，不要改母材质。**
母材质 `M_BossHealth_Caustics` 由 commandlet 生成，重跑会整个重建、覆盖手工改动；实例里的覆盖值
不会被动到。

血条尺寸是 1098 x 12 像素（见 `render.log` 的 `HUD BAR geometry`），所有默认值按这个长宽比调过：
U/V tiling 相差约 90 倍才能让焦散单元保持方形，横向描边也因此比纵向细得多。改条的尺寸后这几个值
要重调。

## 参数分组

材质实例里按组排好，顺序就是叠加顺序。

**1 - Color** — 每层一个颜色，互不影响
- `CausticTexture` 焦散贴图
- `DeepColor` / `ShallowColor` 液体本体：顶部进光、底部吃光的渐变两端
- `CausticColor` 焦散光斑
- `SurfaceColor` 液面高光带
- `RimColor` 四边玻璃壁
- `LeadingEdgeColor` 血条填充末端（液面边缘）

**2 - Liquid**
- `CausticTilingU` / `CausticTilingV` 焦散密度，两轴分开
- `FlowSpeed` 流动速度（0 = 静止）
- `CausticGain` / `CausticContrast` 光斑亮度与锐利度
- `RefractionStrength` 第一次采样扭曲第二次采样的强度，折射感就来自这里
- `DepthFalloff` 深度渐变曲线

**3 - Glass**
- `SurfacePosition` / `SurfaceWidth` / `SurfaceIntensity` 液面高光带的位置、宽度、强度
- `RimWidthY` / `RimWidthX` / `RimIntensity` 玻璃壁，纵横分开
- `LeadingEdgeWidth` / `LeadingEdgeIntensity` 填充末端那一道亮边

**4 - Opacity** — 分层透明度，每层单独一项
- `BodyOpacity` 液体本体
- `CausticOpacity` / `SurfaceOpacity` / `RimOpacity` / `LeadingEdgeOpacity` 各层把像素往不透明方向拉
  多少；设成 0 等于这层不参与透明度，不影响其他层
- `GlobalOpacity` 整体

**5 - Levels** — 色阶，颜色和透明度分开
- `ColorBlackPoint` / `ColorWhitePoint` / `ColorGamma`
- `OpacityBlackPoint` / `OpacityWhitePoint` / `OpacityGamma`

某一层想整个关掉：把它的 Intensity（或 `CausticGain`）和对应的 Opacity 都设 0。

## 成本

两次贴图采样、一个 sampler，和被替换的旧材质一样；其余全是 UV 运算。实测
`131 pixel instructions, 3 samplers`（`verify.log`）。`VTGBossHealthLiquid` 的校验里压了
200 条指令 / 3 个 sampler 的上限作为回归保护。焦散的两次采样写在 Custom 节点里，所以不会计进
统计的 texture samples 一栏，那里显示 0 是正常的。

## 工具与验证

- 源码：`Source/VertigoEditor/Private/VTGBossHealthLiquidCommandlet.cpp/.h`。HLSL 在
  `LiquidCode()` 里，一行一句，改这里要重编 `VertigoEditor` 再重跑 `-Apply`。
- 重建资产：
  `UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGBossHealthLiquid -Apply -AllowCommandletRendering -RenderOffscreen -unattended -nosound -nop4`
- 只校验：同上去掉 `-Apply`。会重新加载资产，逐个核对 27 个 scalar、6 个 vector、1 个 texture
  参数的默认值、着色器编译错误、成本上限，以及 `HealthBar` 的 Fill 是否指向实例。
  `apply.log` / `verify.log` 为最近一次运行结果，两次都 PASS。
- `shader-preview.html`：用 JS 复刻同一段 HLSL，在真实的 1098 x 12 尺寸下离线出图，用来试参数而不用
  开编辑器。页面里内嵌了焦散贴图的 256x256 副本。改了 `LiquidCode()` 要同步改页面里的 `liquid()`。
- 旧版 `M_BossHealth_Caustics.uasset` 和 `WBP_BossHealthBar.uasset` 备份在 `Backup/`。

## 坑

`UMaterial` 的参数默认值查询走的是 cached expression data，不是表达式本身。`RecompileMaterial`
里的 `PostEditChange` 不会把这份缓存刷新成新建表达式的值，所以必须在 recompile 之后、保存之前手动
调 `UpdateCachedExpressionData()`，否则每次 `-Apply` 存盘的默认值都慢一代。材质实例同理，
重新设同一个 parent 不会让它丢掉缓存，要显式 `UpdateCachedData()`。两处都加了，并在保存前把
所有默认值回读 `checkf` 一遍。

离屏预览 `-Preview=<png>`（走 `VTGCombatPolishPreview.cpp`）画不出 Fill Image，旧版同样画不出，
不能用它看这个材质。想在游戏里看，用 `VTG.PreviewCombatHUD` 控制台命令。
