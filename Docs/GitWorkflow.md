# Git 提交流程

在 Git 的世界里，你必须先 **commit**（存档），然后才能 **push**（上传到云端）。

如果你修改了文件，直接运行 `git push` 或 `git lfs push`，Git 并不会把你刚刚修改的东西传上去——它只认已经 commit 过的历史记录。

---

## ⚠️ 2026-09-07 起的变化：C++ 改动要连 DLL 一起交

编译出来的编辑器 DLL 现在也纳入 git（走 LFS）了，这样组里不装 Visual Studio 的人 pull 下来就能直接开编辑器，不用编译。

代价是：**你改完 C++ 必须先重新编译，再提交。**

如果只交了源码没交 DLL，别人拉下去是「新源码 + 旧 DLL」，蓝图里所有调用到改动函数的节点会直接报
`Could not find a function named ...`，节点上每个 pin 都会变成孤立状态。他们一旦保存那个资产，连线就全丢了。

好消息：**`git add .` 会自动把 DLL 带上**（Binaries 已经不在 `.gitignore` 里），你不需要记额外的命令。
唯一要记的就是「先编译」。而且忘了的话，pre-commit hook 会拦住你（见下）。

---

## 永远正确的提交流程

### 第 0 步：改了 C++ 吗？改了就先编译

只动了美术资产、蓝图、关卡、Sequence → **跳过这步**。

动了 `Source/` 或 `Plugins/*/Source/` 里的 `.h` / `.cpp` → 先关掉编辑器，然后：

```bash
"/g/Epic/UE_5.3/Engine/Build/BatchFiles/Build.bat" VertigoEditor Win64 Development -Project="G:/Epic/Unreal projects/VertigoGit/Vertigo.uproject" -WaitMutex
```

编译不过就先修，别往下走。

### 第 1 步：查看状态（可选，但强烈建议养成习惯）

```bash
git status
```

这会用红色的字告诉你，哪些文件被修改了但还没被 Git 记录。

### 第 2 步：把所有改动放进暂存区（准备存档）

```bash
git add .
```

注意 `add` 后面有个空格和一点 `.`，意思是「把当前目录所有改动都算上」。
**要在项目根目录执行**——在子目录里跑的话，`.` 只代表那个子目录。

刚才编译出来的 DLL 会在这一步被自动带上。

### 第 3 步：正式 Commit（真正意义上的存档！）

```bash
git commit -m "这里写你这次改了什么，比如: 添加了黑市的NPC逻辑"
```

执行完这一步，你的改动才真正变成了像 `sprint1` 那样的存档点。这时候就算你手滑敲了 `reset`，也能通过 `reflog` 找回来。

### 第 4 步：推送到远端服务器（云端备份）

```bash
git push origin main
```

LFS 的大文件（`.uasset` / `.umap` / `.dll`）会自动跟着一起传，**不需要**专门敲 `git lfs push`。
只有上传中断了、想强制重传 LFS 文件的时候才需要它。

---

## 怎么检查自己到底有没有 Commit 成功？

如果你不确定自己刚才到底存没存上，敲这个：

```bash
git status
```

- 输出是 `nothing to commit, working tree clean` → 恭喜，所有改动都妥妥地 commit 进去了，当前工作区干净，非常安全。
- 输出里有红色或绿色的文件列表 → **警报！** 你有还没 commit 的东西。这时候**千万不要 `reset`**，赶紧回去走 `git add .` 和 `git commit`。

---

## Hook 会替你兜底

`.git/hooks/pre-commit` 里装了两道检查，出问题会直接拒绝 commit，不会让你把坏东西推上去。

**1. 资产没坏（旧检查）**

`.uasset` / `.umap` 如果实际内容是 LFS 指针文本而不是真的 Unreal 包（没跑 `git lfs pull`，或者是没解决的合并冲突），拦下来。Unreal 资产是二进制的，合并冲突只能二选一：

```bash
git checkout --ours   <文件>   # 保留你的版本
git checkout --theirs <文件>   # 保留对方的版本
git add <文件>
```

**2. C++ 和 DLL 对得上（新检查）**

| 情况 | 结果 |
|---|---|
| 改了 C++，忘了编译 | ❌ 拦下，告诉你没有 DLL，并打印 build 命令 |
| 编译了，之后又改了 C++ 才提交（DLL 是旧的） | ❌ 拦下，列出哪些文件是编译之后才动过的 |
| 先改后编，顺序正确 | ✅ 放行 |
| 只提交美术资产 / 没碰 C++ | ✅ 放行，完全不打扰 |

第二种最阴险：DLL 确实在 commit 里，但内容是旧的。hook 靠比对文件修改时间抓它。

确实想只交源码不交 DLL（很少见），用 `git commit --no-verify` 绕过。

### 重新 clone 之后要装一次 hook

Git 运行 hook 的目录是 `.git/hooks/`，而这个目录**不受 git 管理**——新 clone 下来的仓库里是空的。
所以真正的脚本存在 `Build/GitHooks/`，clone 完跑一次这个就行：

```bash
sh Build/GitHooks/install.sh
```

装完可以随手验一下（应该看到 hook 拦你）：

```bash
git status
```

> **为什么不用 `core.hooksPath`？**
> 那个设置会**整个替换**掉 `.git/hooks` 目录，而 Git LFS 自己在那里装了四个钩子
> （`post-checkout`、`post-commit`、`post-merge`、`pre-push`）。其中 `pre-push` 负责上传 LFS 大文件，
> 被架空的话 push 上去的资产全是空指针。所以老老实实用复制的方式。

以后 hook 内容有更新，改 `Build/GitHooks/pre-commit` 并提交，其他人 pull 之后重跑一次 `install.sh` 即可。
装的时候如果发现你本地已有的 hook 内容不一样，会先备份成 `pre-commit.bak` 再覆盖，不会直接吞掉。

---

## 协作者那边（不写 C++ 的人）

```bash
git pull
git lfs pull
```

**`git lfs pull` 千万别漏。** 不然拉到的 `.uasset` 和 `.dll` 都只是几百字节的文本指针，
Unreal 加载会直接崩、或者把模块当成缺失。

拉完直接开编辑器，不用装 Visual Studio、不用编译。

### 前提：引擎版本必须一致

DLL 里带着一个 BuildId，对不上的话 UE 会判定过期、照样要求重新编译，这套共享就白搭了。

- 引擎版本：**UE 5.3.2**
- BuildId：**`27405482`**

对方自己核对这个文件里的 `BuildId` 字段：

```
<引擎安装路径>/Engine/Binaries/Win64/UnrealEditor.modules
```

不一致的话，去 Epic Launcher 装 5.3.2。

---

## 仓库里存了什么、没存什么

| | |
|---|---|
| ✅ 存 | `Binaries/Win64/UnrealEditor-*.dll` 和 `Plugins/*/Binaries/Win64/UnrealEditor-*.dll` |
| ✅ 存 | 对应的 `UnrealEditor.modules` 清单（几百字节的 JSON） |
| ❌ 不存 | `.pdb` 调试符号（单个 60–300 MB） |
| ❌ 不存 | `.exe`、打包产物、`Vertigo-Win64-Shipping.*` |
| ❌ 不存 | `boost_*.dll` / `tbb*.dll` / `OpenImageDenoise.dll` 等第三方依赖 |

DLL 总共约 6.5 MB，每次改 C++ 重编的增量差不多就是这个量级。
