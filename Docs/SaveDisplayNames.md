# 存档菜单的显示文字（地点 / 调查阶段）

贴 Confluence 用 `SaveDisplayNames.zh-CN.confluence.html`（内容与本文一致，且更完整）。

> 存档里存的是 key（地图名、stage 名），**玩家看到的文字在画的时候才从表里查**。
> 所以改文案、补翻译对老存档一样生效，而且内部名字永远不会漏给玩家。

## 一、使用

两张 DataTable，行名就是 key，只有一列「显示文字」（FText）：

| 资产 | 行名 | 例 |
|---|---|---|
| `/Game/Widget/SaveLoad/DT_SaveLocationNames` | 地图短名 | `L2_StreetOutsideBar` → Commerce Module / 商业区 |
| `/Game/Widget/SaveLoad/DT_SaveStageNames` | stage 名 | `Stage_BarStreet_L2` → Night / 夜晚 |

**Demo 流程的 14 个地点、17 个阶段已经按 wiki 填好，中英文都在。** 地点写地名，阶段写时段 /
调查状态（Night、Daybreak、Morning、Worktime、Investigation Ongoing），所以多个 stage 共用一个写法是正常的。

两处 wiki 里同一张地图写了两种中文，表按地图存，只能留一个：

| 地图 | 采用 | wiki 另一处还写过 |
|---|---|---|
| `Sa_AptDownAlley` | 公寓楼下 | 居民公寓楼下 |
| `BlackMarketStreet` | “黑市”，商业模块 | “黑市”，商业街 |

`M_MainMenu` 不在表里：主菜单不是存档点。

直接在编辑器里开表加行、改字。两张表的路径记在
**Project Settings → Vertigo → VTG Save Display Names**（写在 `Config/DefaultGame.ini`）。

### 本地化

`Display` 的命名空间固定是 `VTG.SaveDisplay`，key 就是行名。走项目现有那套**手工多语言面板**。

**注意两件事**（都踩过）：

1. **不能用 `FText::AsCultureInvariant` 建文字** —— gather 里有 `Text.IsCultureInvariant()` 的跳过判断，
   那样建的行永远翻译不了。工具用 `FText::ChangeKey` 并有断言守着。
2. **必须勾「确认是玩家可见文本」** —— 编译时 `if(!E->Approved ...)continue;` 会跳过没勾的条目，
   面板里译文框也是禁用的。现有 31 行都已勾好、译文已填、locres 已编译。

3. **字段必须进白名单，折叠译文区才会出现。** `VTGAssetDisplayLocalizationDetails::IsDisplayField`
   是一张 (Owner, Name) 白名单，不在里面的 FText 在 Details 面板里就是个普通输入框，看不到
   「多语言译文」折叠区。`FVTGSaveDisplayRow::Display` 已经加进去了。

`UDataTable` 的行文字是能被收集的，`VTGManualLocalization` 的 gather 对它有专门分支。
定位行来源的 `GetDataTableRowContext`（原 `GetMailContext`，原来写死只认 Mail 的行结构）
已经改成对任何结构体拥有的白名单字段都生效。

### 没配行会怎样

| | 没有对应行时 |
|---|---|
| **地点** | 回退到旧写法（`L2_StreetOutsideBar` → `L2 StreetOutsideBar`），不会空着 —— 看到这种原始样式就说明这张图还没填 |
| **调查阶段** | **整行不显示**。故意不回退：没映射的 stage 名正是不该给玩家看的东西 |

## 二、维护

```
-run=VTGSaveDisplayNames                    只验证（断言 31 行逐行解析正确、都有文本身份）
-run=VTGSaveDisplayNames -Apply             建表 + 补缺行 + 写 settings + 收集 + 勾确认 + 填译文 + 编译 locres
-run=VTGSaveDisplayNames -Apply -Reseed     额外：把已有行改回 C++ 里那份（会盖掉编辑器里改过的字）
```

`-Apply` **不会覆盖已有行**，只补缺的，所以重跑安全。**`-Reseed` 会覆盖**，只在"以 C++ 里那张表为准"时用。

行数据在 `VTGSaveDisplayNamesCommandlet.cpp` 的 `Locations[]` / `Phases[]`，一行三列（行名、英文、中文）。
译文通过 `VTGManualLocalization::SetTranslation`（和面板保存走同一套格式校验、同样写 `reviewedSource`）写进翻译表。

**日常加地点/阶段不用跑这个，也不用改 C++**：在对应表里加一行，行名抄内部名字，文字自己写，
然后走面板填译文。只有想把整张表写进代码当基线时才动 C++。

## 三、原理

### 解析在哪

`UVTGSaveDisplayNames::ResolveLocation(LevelName)` / `ResolvePhase(Stage)`，
由 `UVTGTerminalSavePage` 在画列表行和详情页时调用（主菜单的 Continue 用的是同一个页面，所以一处改两处生效）。

### 为什么不把文字存进存档

存档里继续只存 `Meta.LevelName` 和 `Meta.Stage`。如果把显示文字烤进存档：

- 以后改文案或补翻译，老存档看不到
- 存的时候是什么语言，读的时候就还是什么语言（中文会话里蹦英文）

`DisplayLabel` 就是这个毛病的实例：它是存档时 `FString::Printf(TEXT("Manual Save %02d"))` 烤进去的英文，
渲染时又套了 `AsCultureInvariant`，所以永远不会翻译。详情页那一行已经删掉了 ——
它和正上方的标题 `MANUAL SAVE {Slot}` 是同一个东西，而标题本来就是 NSLOCTEXT，能翻译。

`DisplayLabel` 字段本身保留在存档结构里，以后要做「玩家自己给存档命名」时还用得上；
那种是用户内容，存字符串、不本地化，是对的。

## 验证

`-run=VTGSaveDisplayNames -Apply -Reseed` 跑出 `SAVE DISPLAY PASS (14 locations, 17 phases)`，
断言：31 行逐行通过 settings 解析成预期文字、两张表行数与基线一一对应、没映射的地图回退成非空、
没映射的 stage 解析成空、每行都带文本身份。C++ 编译 0 错误。

翻译表里 31 条都 `approvedDisplayText: true`、无锁、`en-US` 与 `zh-Hans` 都已填且 `reviewedSource` 与原文一致。
locres 按字节核对过：`zh-Hans` 里有中文、`en-US` 里有英文，互不混。

14 个地图名全部与 `Content` 下的 `.umap` 实名对上；17 个 stage 名全部能在资产里搜到
（`L3PoliceStationToMaket` 的拼写和 `3Day1` 这种写法都是项目里的真名，照抄的）。

回归 `Vertigo.Localization.AssetDisplayInline` 里加了断言：两张表的 Row Editor 各有且只有一个
折叠译文区、`IsSafeText` 通过、`GetDataTableRowContext` 能定位到自己那张表、每行文字非
culture-invariant 且带 key。

**没有进游戏看过**：存档菜单里的实际排版（地点和阶段两行的间距）需要开一次终端确认。
