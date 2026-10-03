# SoapTV ep1 纹理检查

检查的是原纹理资产及其源图；没有保存或修改原资产。平台格式和内存数据由 UE 5.3 Windows D3D12 离屏加载取得。内存为 UE 对全 mip 纹理资源的估算（包含平台布局影响），不是安装包大小。

| 纹理 | 源图与当前平台尺寸 | 设置 | 当前平台格式 | UE 估算内存 |
|---|---|---|---|---|
| ep1_calvin | 3960×2160 | Default、World、NoMipmaps | DXT1 | 4.875 MiB |
| ep1_nika_copy | 3960×2160 | Default、World、NoMipmaps | DXT1 | 4.875 MiB |
| Poster_copy | 3312×4905 | UserInterface2D、World、NoMipmaps | B8G8R8A8 | 63.125 MiB |

三张合计约 72.875 MiB。全部 Never Stream=true，Max Texture Size=0，sRGB=true。

临时副本测试 BC7：两张横图真正得到 BC7，各约 9.6875 MiB；Poster_copy 仍得到 B8G8R8A8，约 63.125 MiB。说明同尺寸横图换 BC7 会增加内存，海报仅切压缩下拉选项不足以优化。

建议横图优先按原比例缩到 2640×1440；想进一步省可用 1980×1080。原横图不是严格16:9，保持原比例后通过 Cover 裁切，避免直接拉伸。

竖图滚动建议海报宽度1920，高度约2844（按原比例缩小并进行极少量尺寸取整）。这是接近原比例、两轴都是4的倍数的候选尺寸；再测试BC7并确认纹理 Format。实际优化后清晰度与内存还需重新检查。

画面观察：两张横图的背景柔焦适合缩小，人物轮廓仍需保持清楚；Nika 的网状衣服与三张图的颗粒/网点纹理应在最终播放尺寸下检查是否产生摩尔纹。海报底部文字、主体细节与竖图滚动用途使其需要比整张竖图缩小显示更高的宽度。

机器读取报告：`Saved/SlideshowTextureReview/TextureReport.txt`；源图预览同目录。没有比较压缩后的实际画面，BC7测试只确认平台格式及估算内存。

## 已执行的优化

| 纹理 | 优化后尺寸 | 实际平台格式 | UE 估算纹理内存 |
|---|---|---|---|
| ep1_calvin | 2640×1440 | DXT1 | 2 MiB |
| ep1_nika_copy | 2640×1440 | DXT1 | 2 MiB |
| Poster_copy | 1920×2844 | BC7 | 5.625 MiB |

合计从 72.875 MiB 降到 9.625 MiB，约减少 86.8%。这是 Windows D3D12 平台纹理资源估算，不等于打包文件大小。

三张纹理的 Source 已缩小，Texture Group 改为 UI，保留 NoMipmaps 和 sRGB。海报压缩后的 Format 已验证真正为 BC7。查看了从实际 GPU 块数据解码出的图片，人物轮廓、Nika 网状衣服、海报文字及主要构图仍可辨识；未做游戏内放大和移动的实测。

维持紧凑源数据存储：原资产使用 JPEG 源数据，优化后也使用 JPEG（质量92）。优化后重导入源图在项目的 `SourceArt/SoapTV/ep1/`，纹理的导入路径已指向这里，因此普通 Reimport 不会重新导入原先的大图。该目录不在 Content，不作为游戏纹理额外烘焙。

三个 uasset 合计从 3,150,869 bytes 降到 2,557,922 bytes（约减少18.8%），但这不包含新增的重导入源图与 Saved 备份。安装包是否缩小以及缩小多少仍需实际 Cook/打包确认。

原始 uasset 备份在 `Saved/SlideshowTextureBackup/`。资产名称、路径和 Slideshow 的引用保持不变。

优化报告与压缩后预览在 `Saved/SlideshowTextureReview/Optimized/`；GPU 块数据 DDS 及解码 PNG 都保留，可用于后续比较。
