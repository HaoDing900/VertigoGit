# 视差滚动系统（Parallax Scroll System）

> 2026-07 添加。用于"车/船不动，靠背景滚动伪造前进"的固定机位场景（隧道、海上钻井平台天际线等）。把关卡里的 mesh 分层，各层以不同速度往同一方向滚，滚出镜头的自动瞬移到队尾，形成无限循环背景。

核心类：`UVTGParallaxScrollComponent`（`Source/Vertigo/.../Environment/`）

---

## 1. 这是什么

一个 **Actor Component**。你给它若干"层"（Layer），每层是一组会循环滚动的 actor（A、B、可选 C…）。

- 每层自己配 **方向 + 速度 + tile 宽度**。
- 前景快、后景慢 → 自动产生伪视差深度感。
- 每个 tile 滚到前方尽头会**瞬移回队尾**，接缝发生在镜头外，看起来是无限背景。
- 用的是**取模自绕回**，不是"A 盯着 B 触发"。所以各层速度不同也不会算错，AB 轮换节奏自动成立。

---

## 2. 如何使用

### 第一步：准备 mesh

- 每层要一块**干净的可移动 mesh actor**。
- **Mobility 必须是 Movable**（Static 的 actor 运行时不能移动，会不动或报 attach 错误）。
- 如果原本是一堆散 mesh 或嵌套 kit BP，用 **Tools → Merge Actors** 拍平成**单个 static mesh 资产**，再放一个 actor，设 Movable。别用带嵌套 Child Actor 的 kit BP，容易踩 Static attach 雷。

### 第二步：挂组件

找一个 Actor（空的 director BP 或关卡里现成的管理 BP，如 BPLM 都行），Add Component → **VTGParallax Scroll**。

### 第三步：在关卡实例上填参数

> ⚠️ 必须在**关卡里摆好的实例**上填，不能在 BP 编辑器 class default 里填 —— 因为 `Groups` 指的是关卡 actor，资产引用不到关卡 actor（那栏会是灰的 / 只能 None）。
> World Outliner 里选中挂了组件的那个 actor，在 Details 面板填。

`Layers` 数组，一层一条（一般两条：前景、后景）。每层字段：

| 字段 | 含义 | 建议 |
|---|---|---|
| `Layer Name` | 只是标签，方便区分前景/后景 | `Foreground` / `Background` |
| `Groups` | 这层的 A、B（、C…）actor | 见下方 |
| `Direction` | 世界空间滚动方向（会自动归一化） | 从右往左 = `(-1, 0, 0)` |
| `Speed` | 沿 Direction 的速度（cm/s） | 前景大、后景小，如 200 / 40 |
| `Tile Width` | 一块 tile 的宽度 = 相邻组间距（cm） | 留 `0` 自动用 A 的包围盒宽；想无缝就手填内容真实周期 |

### Groups 怎么填

- **Index [0] = A = 锚点**。**只有 A 的位置有意义**，它就是这层的起始点。
- Index [1] = B，Index [2] = C…… 想要几块填几块。
- **B/C 摆哪都行**（丢到镜头外也行），组件 BeginPlay 时会按 `Tile Width` 自动排好间距。
- **只填 A、B 留 None** → 组件自动克隆一份 A 当第二块，你连 B 都不用摆。
- None / 无效的槽会被跳过，中间留洞也不会崩。

**最省事用法：每层只拖一个 A 进去，设好 Speed / Direction，其余交给组件。**

---

## 3. 运行时控制（可选）

组件暴露了两个 BlueprintCallable：

| 函数 | 作用 |
|---|---|
| `Set Layer Speed(LayerIndex, NewSpeed)` | 单独改某一层速度 |
| `Set Global Speed Scale(Scale)` | 按比例缩放所有层速度（相对各自初始值） |
| `bScrolling`（变量） | 总开关 |

**停车效果**：把 `Set Global Speed Scale` 的 Scale 从 `1` 插值到 `0`，前后景一起平滑停下。这样只需一个知识点控制整体节奏。

---

## 4. 需要注意什么

1. **Mobility 必须 Movable** —— 最常见的"不动"原因。前景后景所有被 `Groups` 直接引用的 actor 都要是 Movable。
2. **必须在关卡实例上填 Groups**，不是 BP class default。
3. **锚点是 Index[0]，别乱换 Groups 顺序**。
4. **Tile Width 与内容对齐**：自动值用包围盒，如果 mesh 有空边导致接缝对不齐，手填精确的内容周期。
5. **绕回接缝要在镜头外**：把 A（锚点）放在内容该"退出"的那一侧边缘附近。如果两块盖不满镜头（滚动时露缝），加第三块 C（组件自动支持，绕回距离会按组数重算）。
6. **A、B 同内容同深度**：垂直于滚动方向的分量（深度/高度）统一取自 A，所以 B 摆歪了没关系，但也意味着 B 不能和 A 不同深度。
7. **自动克隆是 `RF_Transient`**：不会存进关卡，PIE 结束即销毁，干净。
8. **改了 C++（新增 UCLASS/字段）要重新编译**，Live Coding 热重载对新类型无效，重启编辑器。

---

## 5. 典型配置示例（海上背景，两层）

- **前景**（钻井平台剪影）：Groups=[前景A]（B 自动克隆），Direction=(-1,0,0)，Speed=200，Tile Width=0。
- **后景**（远处水线）：Groups=[后景A]，Direction=(-1,0,0)，Speed=40，Tile Width=0。

前景以 5 倍于后景的速度滚，固定机位下相对运动制造深度。停车时 `SetGlobalSpeedScale(0)`。
