# VTG Headache：可拖入关卡的后处理 Volume

已新增与原有 `NE_PP_SaHeadache` / `PP_Sa_Headache` 的音画播放集成，见 [Narrative Event 使用说明](NARRATIVE.md)。下面的拖入预览蓝图仍保持独立。

## 直接使用

1. 在 Content Browser 打开 **Content / VTG / Blueprints**。
2. 将 **BP_HeadachePostProcess** 拖入关卡。默认 `HeadacheIntensity = 1`，可直接预览增强后的效果。
3. 选中蓝图，在 Details → **Headache** 调整 **Headache Intensity**：0 恢复原画面，0.4 较轻，0.7 中等，1 完整强度。
4. 默认全屏生效，与放置位置无关。一个关卡放一个即可。删除蓝图或设为 0 可关闭效果。
5. 动态颗粒需要编辑器视口启用 Realtime（Ctrl+R）；PIE 中会正常播放。无需启动任何事件。

该独立预览蓝图只提供 Volume，不会自行触发 Narrative 播放或声音。需要头痛事件及音画淡入淡出时，使用上方 Narrative 集成说明中的现有事件。

## 相比第一版

- 暗部颗粒明显增强：两种空间尺度、圆润颗粒形状和不规则团簇，刷新之间平滑过渡，避免均匀方格静电。
- 残影不再只有极短时间窗口才出现；高反差边缘持续保留偏血红的双像，位移缓慢漂移，满强度约 5～8 输出像素。仅对偏移重影保留红通道并抑制绿蓝通道，形成红色色差感，不对整幅画面统一染红。
- 周边压迫更强，略微不对称，并带少量局部去饱和。使用偏暖的炭黑乘色，避免明显的受伤红边。中心保留可读性。
- 不加入闪白、全屏亮度脉冲、相机抖动、FOV 变化或全屏模糊。

## Blueprint 设置

蓝图父类为原生 **Actor**，包含一个 **PostProcessComponent**（命名为 HeadachePostProcess），作用等同一个全屏后处理 Volume：

- Infinite Extent / Unbound：True
- Enabled：True
- Priority：100
- 仅一个 Weighted Blendable：`MI_PP_Headache_FullStrength`，Weight=1
- 无曝光、色调、景深、运动模糊等内建后处理 override
- 无 Event Tick，Actor Tick 关闭

Construction Script：

```text
Construction Script
  → Clamp(HeadacheIntensity, 0, 1)
  → Set HeadachePostProcess.BlendWeight
```

这是纯蓝图资产，不依赖新增的 C++ 运行时类。C++ commandlet 仅用于编辑器中生成和验证资产。

`MI_PP_Headache_FullStrength` 的材质参数为 1，父材质默认参数为 0；UE 后处理 Volume 的 Blend Weight 会把参数从中性默认值插值到实例值，因此蓝图的单一强度控制同时影响三种效果。权重为 0 时跳过此 Volume 的材质。不会改动其他 Volume 的设置。

此版本主要用于编辑器拖入和调效果。若运行时直接修改蓝图变量，需要再次应用组件权重；本次未加入运行时播放接口。

## 材质节点（UE 5.3）

资产：`/Game/VTG/Materials/PostProcess/Headache/M_PP_Headache`

- Material Domain：Post Process
- Blendable Location：After Tonemapping
- SceneTexture：PostProcessInput0 → Custom.Base
- Time（Ignore Pause=False）→ Custom.Clock
- Scalar Parameter：**HeadacheIntensity**，默认 0，范围 0～1 → Custom.HeadacheIntensity
- Custom 输出类型：Float3；完整代码为 `headache.hlsl`
- Custom → Emissive Color

仅一个公开 scalar 参数；其余视觉比例固定在 HLSL 中。`update_material.py` 负责生成节点和内嵌代码。运行和打包不需要外部 Python/HLSL 文件。后续修改 `headache.hlsl` 后运行 `refresh_shader.py`，仅同步现有 Custom 节点，不重设实例或蓝图。

之前的 Light / Medium / Heavy 预览实例已更新为新材质，强度为 0.4 / 0.7 / 1。旧 `M_PP_VTG_Headache` 与 LowCost 实例保留以免破坏旧引用；新蓝图不使用它们。`create.py`、`ghost.hlsl`、`finish.hlsl` 是第一版历史实现，不要用它们重建新版。

## 性能和验证

设计为一个 After Tonemapping pass，读取两次场景颜色（原图和单次偏移）。无噪声贴图、深度/GBuffer/速度读取、历史帧缓存或模糊循环。新版颗粒比第一版增加少量数学计算；实际 GPU 毫秒数需在目标关卡和分辨率下用 `profilegpu` 对比强度 0 / 1。

- `update.log`：实际 D3D SM6 着色器编译。
- `volume.log`：蓝图编译、生成实例、反复重跑 Construction Script。检查 0 / 0.65 / 1，以及越界钳制，始终只有一个组件和一个 Blendable。
- `verify.py`：重载材质验证单一参数和实例强度。

离屏 SceneCapture 测试未产生有效的场景画面，因此未用作视觉验收，也不附带测试截图。尚未在真实关卡视口做视觉或 GPU 耗时验收；请按上述拖入步骤预览。

只读重载检查蓝图：

```powershell
& 'G:/Epic/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/Epic/Unreal projects/VertigoGit/Vertigo.uproject' -run=VTGHeadacheVolume -VerifyOnly -NullRHI -unattended -nosplash -NoSound -nop4
```

参考：[Epic 后处理材质与 Blendable 插值说明](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials?application_version=5.3)。
