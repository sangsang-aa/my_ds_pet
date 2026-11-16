# 设计决策与风险

本文记录“为什么这样做”和“哪里会坏”。决策是当前实现的理由，不是不可改的教条；风险按影响排序，修复建议只是方向，不代表已经排期。

> R1（bounds 缩放）与 R2（README 漂移）已在本次提交解决；条目保留是为了对照历史基线 `d88b53e`，当前状态以各条目的“状态”行为准。

## 1. 关键决策

### D1. 分层窗口 + `UpdateLayeredWindow`，不用 `SetLayeredWindowAttributes`

- **选择**：`WS_EX_LAYERED` + 每帧 `UpdateLayeredWindow(..., ULW_ALPHA)`。
- **理由**：需要逐像素 alpha；`SetLayeredWindowAttributes` 只能整窗透明度或颜色键，边缘会硬。
- **代价**：每帧一次全窗口合成；必须自己维护内存 DC 和 `HBITMAP`；位图尺寸必须与窗口一致。

### D2. 用逐帧 alpha 掩码做命中测试

- **选择**：`WM_NCHITTEST` 查 `Frame.AlphaMask`，透明处返回 `HTTRANSPARENT`。
- **理由**：窗口是 269×358 的矩形，sprite 只占一部分；不做的话，透明留白会吞掉点击，右键“空桌面”会退出宠物（历史 issue #1）。
- **代价**：每帧多一份 1 bit/px 掩码；半透明边缘按阈值二值化。

### D3. 行为注册表驱动

- **选择**：`MutableBehaviors()` 里追加 `Spec`，`Pet` 按 registry 加载和播放。
- **理由**：加一条动作不需要改 `pet.cpp`；事件/自动/Auto 的差异用数据描述。
- **边界**：新增 **Trigger** 或 **Playback** 仍需改代码；注册表只覆盖已有枚举。

### D4. 按素材语义选择 Playback

- **选择**：
  - 闭合走路循环 → `Loop`（左右）；
  - 单向飞起 → `Boomerang`（上去再回来）；
  - 俯冲 → `FullCycle`（必须落到末帧）；
  - 点击/害羞 → `OneShot`；
  - 拖拽 → `Loop`，松手由事件结束。
- **理由**：避免“倒放走路”像月球漫步、竖向动作停在半空、点击反应循环不止。
- **代价**：`FullCycle` 的“到时”不是精确时长；`Boomerang` 的周期必须与目标 tick 对齐（当前 27 帧 ↔ 52 tick）。

### D5. 用 `bounds.txt` 把主体从画布里分离

- **选择**：每个动作目录一份逐帧 `x y w h`；`SubjectBounds` 发布屏幕坐标；气泡和扫头读它。
- **理由**：384×512 画布有大量透明留白，气泡按整块画布定位会离头很远。
- **坐标转换**：`bounds.txt` 是源画布坐标；`PresentFrame` 用 `ScaleSubjectBounds` 换算到窗口坐标后再发布。历史缺陷见 [R1](#r1-bounds-未缩放p1)。

### D6. 气泡是独立窗口 + 独立定时器

- **选择**：`Bubble` 自己建 layered window、自己 100 ms tick、`WS_EX_TRANSPARENT` 点击穿透、owned by pet。
- **理由**：气泡的弹出/隐藏/跟随不污染宠物状态机；点击穿透不挡宠物交互；独立 z-order 处理。
- **代价**：多一个窗口和定时器；需要轮询宠物主体矩形和 z-order；宠物重新置顶时可能压住气泡，需要补一次置顶。

### D7. 只用 PNG 帧，不上 WebP

- **选择**：`NNN.png` 序列；GDI+ 解码。
- **理由**：GDI+ 没有 WebP 解码器；换 WebP 需要引入 libwebp 或换渲染栈。
- **代价**：磁盘文件多、启动全量解码；图集方案在 `docs/guides/asset-packaging.md` 里评估过，尚未落地。

### D8. 保留源素材，帧是派生产物

- **选择**：源视频/源帧导出不删；`NNN.png` 可重新 key；边缘修复只做保守修补。
- **理由**：抠图、去溢色、画布迁移不可逆；青色环无法靠 `despill` 修复。
- **代价**：仓库/本地资产体积大；`SOURCE.txt` 约定需要维护。

### D9. exe 相对路径 + 可选 `pet.ini`

- **选择**：资源从 `GetExecutableDir()` 解析；参数只覆盖资源目录名；`pet.ini` 启动时应用。
- **理由**：绿色部署；不写死绝对路径；调参不重编译。
- **代价**：exe 必须和资源同级；INI 解析器很简陋，没有范围校验和行尾注释。

### D10. 进程级 DPI 感知

- **选择**：优先 `PER_MONITOR_AWARE_V2`，回退 `SetProcessDPIAware`。
- **理由**：layered window 的位图按物理像素创建；不做 DPI 感知时 Windows 会虚拟化窗口，269×358 位图填不满 404×537 窗口，出现白边。
- **代价**：没有处理 `WM_DPICHANGED`；跨不同 DPI 显示器时不会重算窗口/位图。

### D11. 单 UI 线程 + 两个 `SetTimer`

- **选择**：所有逻辑在消息循环线程；宠物/气泡各自 100 ms timer。
- **理由**：实现简单，无锁；GDI/GDI+ 对象不需要跨线程。
- **代价**：任何阻塞都会卡动画；tick 不补帧，时长会随消息延迟漂移。

### D12. 只使用主屏坐标

- **选择**：`GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)` 作为活动范围；拖拽/走路/气泡都夹在这个矩形。
- **理由**：demo 阶段够用，避免多显示器坐标转换。
- **代价**：不能跨屏游走；拖到副屏会被拉回主屏；高 DPI 副屏上气泡定位可能不对。见 [R10](#r10-只支持主屏--多显示器受限)。

### D13. 保留 idle 池，但当前只放一个动画

- **选择**：`basePool` 仍是 `vector<vector<Frame>>`，`basePoolIndex` 随机切换逻辑保留；`Pet::Load` 目前只放 `idle`。
- **理由**：`idle2` 的 V2 动画还在评估；保留代码路径，以后加回一项即可。
- **代价**：当前 `random idle enabled across 1 animations`，随机切换是死代码；`idle` 还是单帧。

### D14. `gatedByWait` 而不是硬编码“四个方向”

- **选择**：每个 spec 带 `gatedByWait`；`PickAuto` 在 `waitTicks > 0` 时排除带标记的行为。
- **理由**：拖后等待只抑制游走，不抑制未来可能加的其它自动行为（例如原地小动作）。
- **代价**：当前四条 walk 都标了 true，行为上等价于硬编码；新增 Auto 行为要记得设置。

### D15. `OnDragEnd` 枚举保留但未接事件

- **选择**：`Trigger` 里有 `OnDragEnd`，但 `OnLButtonUp` 直接 `EnterWaiting`，没有 `FindByTrigger(OnDragEnd)`。
- **理由**：当前没有“松手播放一次”的需求；枚举先留着。
- **代价**：以后有人加 `OnDragEnd` spec 会以为会自动触发，实际不会。见 [R19](#r19-ondragend-未接线)。

## 2. 风险清单

| ID | 风险 | 严重度 | 影响 | 证据/位置 | 建议 |
|---|---|---|---|---|---|
| R1 | `bounds.txt` 缩放 | **已修复** | 历史：气泡偏移、侧面气泡离身体远、扫头带落在脸/上身 | `core/bounds.h` `ScaleSubjectBounds`；`pet.cpp` `PresentFrame`；`tests/bounds_scale_test.cpp` | 已换算并夹取；保留回归测试 |
| R2 | README 文档漂移 | **已同步** | 历史：用户按错帧数/行为理解工程；调参踩坑 | `README.md` 第 20~22、35~40、93~96 行等 | 已按代码/实测回写；历史对照见 baseline |
| R3 | `idle2` / `busy` / `peek` 未注册 | P2 | 有帧但不播；维护者以为坏了 | `pet.cpp` `Pet::Load` 只遍历 `idle`；`behaviors.cpp` 无这三项 | 注册、移入 `_unused`，或在资产表标明“未启用” |
| R4 | core 反向依赖 pet | P2 | 分层不纯；复用 core 时被注册表拖住 | `core/settings.cpp` → `pet/behaviors.h` | 把行为覆盖拆成回调/数据结构，或移到 pet 层 |
| R5 | `Spec::minTicks` 语义与注释不一致 | P2 | 调参把“持续时间”当“待机停留”；INI 行为段效果反直觉 | `behavior.h` 注释 vs `StartBehavior` | 拆分 `durationMin/Max` 与 `cooldownTicks`，更新注释/测试 |
| R6 | `FullCycle` 会超过 `maxTicks` | P2 | `walking_down` 实际时长可能到 59 tick，不是 45 | `OnTimer` 的 `safeToStop` | 若需精确时长，按周期数计算；或改名/文档化 |
| R7 | 头部带名义 45%、实际 25% | P2 | 扫头判定比预期低/窄；调 `head_band_percent` 不直观 | `OnMouseMove` 的 `bandBottom` 公式 | 改名为 `head_band_bottom_percent`，或把 inset 改成可配置 |
| R8 | `shy` 冷却短于动画 | P3 | 动画后半段可被再次扫头重启 | `kShyCooldownTicks=25` vs 40 帧 | 冷却从动画结束算，或至少 >= clip 长度 |
| R9 | 全量预载帧 | P2 | 启动解码慢、常驻位图约 84 MB（理论值） | `LoadFrames` 一次性加载；218 帧 | 懒加载/图集；先测量真实峰值再决定 |
| R10 | 只支持主屏 | P2 | 不能跨屏；副屏拖拽被夹回；高 DPI 副屏气泡错位 | `SM_CXSCREEN/SM_CYSCREEN` | 用 `MonitorFromWindow` + work area + `WM_DPICHANGED` |
| R11 | 无资产校验 | P2 | 混合画布/错误 bounds 静默出错 | `LoadFrames` / `LoadFrameBounds` | 启动时校验首帧尺寸/每帧尺寸/bounds 行数，失败弹框或日志 |
| R12 | INI 行尾注释 + 无范围校验 | P2 | `key = 5 ; comment` 静默回退默认；负值/0 值异常 | `inifile.h` `IntOr`；`ApplySettingsText` | 支持行尾注释或明确文档；对关键值 clamp |
| R13 | 全局可变状态 | P3 | 多实例/并行测试互相污染 | `Settings`、`Behaviors`、`Rng`、`SubjectBounds` | 改成显式 `Context` 参数（大改） |
| R14 | Win32/GDI+ 无自动化测试 | P3 | 命中、拖拽、气泡、DPI 回归只能手工发现 | `pet.cpp` / `bubble.cpp` 无测试 | 增加截图/集成测试或至少脚本化启动检查 |
| R15 | 每秒重新置顶 | P3 | 与其它 topmost 应用“打架”；可能盖住用户窗口 | `kTopmostReassertTicks` | 可配置/按需；接受 demo 行为 |
| R16 | 气泡 z-order 轮询 | P3 | 宠物置顶后气泡可能短暂被压住 | `BubbleIsBehindPet` | 每次宠物置顶后主动抬气泡，减少轮询 |
| R17 | `__argv` 按 UTF-8 解释 | P3 | 非 ASCII 动作/目录名可能乱码 | `WideFromUtf8(__argv[1])` | 改用 `GetCommandLineW` + `CommandLineToArgvW` |
| R18 | 无 `WM_DPICHANGED` 处理 | P3 | 跨 DPI 显示器窗口/位图不重算 | `main.cpp` DPI 初始化 | 监听 DPI 变化，重建位图/窗口 |
| R19 | `OnDragEnd` 未接线 | P3 | 新增该 trigger 的 spec 不会触发 | `Trigger` 枚举 vs `OnLButtonUp` | 补事件或删枚举 |
| R20 | emoji 只查存在、不查解码 | P3 | 坏图显示空白气泡 | `LoadEmojiPaths` / `RenderBubble` | 加载时试解码，失败跳过并日志 |

## 3. 详细说明

### R1. bounds 未缩放（P1）

**状态：已修复（本提交）**

**历史现象（修复前）**：气泡偏右/偏高，侧面气泡离身体远；扫头要扫到脸附近才触发。

**证据**：

```
desktop_pet_image/idle/bounds.txt 第一行: 61 110 262 353
源画布: 384×512
窗口/缩放帧: 269×358（scale_factor 0.70）
期望缩放后: 43 77 183 247
修复前实际使用: 61 110 262 353
```

修复前，`LoadFrameBounds` 直接 `frames[index].bx = bx; ...`；`PresentFrame` 直接
`windowRect.left + frame.bx`。它没有乘 `scale_factor`，也没有夹到 269×358。

**历史根因**：`bounds.txt` 由源画布生成（`tools/migrate_canvas.py` 的 `TARGET_W/H = 384/512` 可以佐证），但消费端按缩放帧坐标使用。`Frame.mask` 是在缩放后的位图上生成的，所以命中测试正常，只有 bounds 消费者错。

**历史影响面**：

- `BubblePetRect` → `PlaceBubble` 的水平中心/侧面 x；
- `Pet::OnMouseMove` 的 `bx/by/bw/bh` → 头部带；
- 不影响 `HitTest`（用 mask）。

**修复方式（已实施）**：

1. `src/core/bounds.h` 新增纯函数 `ScaleSubjectBounds`：按 `lround(value * scale)` 换算，负坐标裁掉左边/上边，右下越界夹到帧尺寸；空框/完全越界返回 `valid=false`；
2. `Pet::PresentFrame` 用 `GetSettings().scaleFactor` 和 `petW/petH` 调用它，再加窗口原点发布 `SubjectBounds`；`Frame` 仍保留源坐标语义；
3. `tests/bounds_scale_test.cpp` 覆盖 `idle` 实际数值、负坐标、越界、空框、`scale=1.0`/`0.5`；
4. 无效 bounds 仍回退整块窗口，保持旧行为。

**状态**：已在本次提交修复。`d88b53e` 的基线仍保留“未修复”的历史记录；修复后的回归由 `tests/bounds_scale_test.cpp` 覆盖。

### R2. README 文档漂移（P1）

**状态：已同步（本提交）**

根 `README.md` 是用户入口；修复前有多处与代码/资产不一致。完整历史对照表在
[`../archives/2026-11-16-baseline.md`](../archives/2026-11-16-baseline.md#文档漂移对照)。最影响理解的几条：

- 无参数启动说在 `idle`/`idle2` 间随机；代码只加载 `idle`，且 `idle` 只有 1 帧。
- 说竖向是“完整一上一下、正常循环”；代码是 `Boomerang` / `FullCycle`，素材语义是飞起/俯冲。
- 帧数过期：`clicked 33`、`shy 30`、`walking_right 27`、`idle 30`、`walking_up 30`。
- 气泡日志写 `240x252`，实际 `252x252`。
- `README` 的 `pet.ini` 示例用行尾 `;` 注释，但解析器不支持行尾注释。

**处理结果**：`README.md` 已按当前代码和实测资产回写：基底池、竖向播放方式、头部带、帧数、气泡尺寸、emoji 命名、启动日志、`pet.ini` 注释规则和开发文档索引都已同步；历史漂移仍保留在 baseline，便于追溯。

### R3. 未注册的动作

- `idle2` 30 帧：`Pet::Load` 的循环里只有 `idle`，注释说 V2 动画还在评估。
- `busy` / `peek` 各 30 帧：`behaviors.cpp` 没有对应 spec。
- `idle` 只有 1 帧：随机待机切换目前是死代码。
- 错误文本仍写 `No idle frames found (looked for idle/ and idle2/)`，但代码只尝试 `idle`；按提示补 `idle2` 不会生效。

**影响**：Asset 目录看起来“有内容”，但运行时不播；维护者可能误以为加载失败。启动日志已经明确输出 `idle 'idle' loaded=1/120`，但不会提示“idle2/busy/peek 未注册”。

**选项**：注册为 Auto/事件行为；移入 `_unused/`；或在资产表/日志里标“未启用”。三者选一，不要继续悬空。

### R4. core → pet 反向依赖

`core/settings.cpp` 为了把 `[action]` 段写回 spec，直接：

```cpp
#include "pet/behaviors.h"
...
for (pet_behavior::Spec& spec : MutableBehaviors()) { ... }
```

这让“基础层”依赖“领域层”。当前单产品没问题，但会阻碍：

- 复用 `core/` 到别的 shell；
- 给 `Settings` 写不依赖宠物注册表的单测；
- 未来把 registry 改成运行时插件。

**方向**：把行为覆盖抽成 `BehaviorOverride { name, min, max, weight, dx, dy }`，由 `main.cpp` 先解析再交给 pet 层；或把 `ApplySettingsText` 拆成 `ApplyGlobalSettings` + `ApplyBehaviorSettings(Behaviors&)`。

### R5. `Spec::minTicks` 语义与注释不一致

`behavior.h` 注释：

```cpp
int minTicks = 0;  // Auto: dwell before firing; event: cooldown
```

实际：

- 待机停留由 `Settings::idleMinTicks/MaxTicks` 控制（`EnterBase` 里设置 `behaviorTarget`）；
- `StartBehavior` 用 `spec.minTicks/maxTicks` 计算 `activeTarget`（行为持续时间）；
- `cooldowns[clip] = spec.minTicks`（启动冷却）。

所以 Auto 的 `minTicks` 既不是“dwell”，也不只是 cooldown，而是“持续时间下限 + 启动冷却”。`pet.ini` 的 `[walking_up] min_ticks=15` 会改持续时间，不会改待机停留。

**方向**：拆字段（`durationMinTicks` / `durationMaxTicks` / `cooldownTicks`），或至少改注释并在 `04-assets-and-config.md` 明确。拆字段会动 `behaviors.cpp` 和 `settings.cpp`，需要单独一轮。

### R6. `FullCycle` 的时长下限

`walking_down` 默认 `min=15, max=45`，30 帧。`OnTimer` 的停止条件是：

```
done = (IsMoving && atEdge) || activeTicks >= activeTarget
safeToStop = Loop || (FullCycle && AtEndFrame) || (Boomerang && AtStartFrame)
```

`FullCycle` 不是 `OneShot`，所以帧会回绕。若 `activeTarget=45`，第一次到末帧是第 29 tick（<45，继续），第二次到末帧是第 59 tick（>=45，停）。因此“目标 45”实际约 59 tick（5.9 秒）。如果 `target=15`，第一个末帧第 29 tick 就停。

**影响**：调 `max_ticks` 不能精确控制俯冲时长；需要“只播一遍”时应改用 `OneShot` 或按周期数计算目标。当前素材语义下可接受，但文档必须说清。

### R7. 头部带的实际比例

默认 `head_band_percent=45`、`kHeadBandBottomInsetPercent=20`，实际带是主体顶部 0~25%。如果主体高 247 px（缩放后），带高约 62 px，从头顶往下 62 px；不是 45% 的 111 px。

**影响**：扫头需要在更靠近头顶的位置；`head_band_percent` 调大 10，实际只增加 10% 高度，但仍从顶部算。README 的“上方约 45%”不准确。

**方向**：重命名配置键（`head_band_bottom_percent`）或把 inset 做成可配置；至少保持 01/04 的公式说明。

### R8. `shy` 冷却与动画长度

`kShyCooldownTicks=25`（2.5 秒），`shy` 40 帧（4 秒）。`StartBehavior` 在启动时设冷却 25，动画播到第 25 tick 时冷却归零，剩余 15 tick 内可以再次触发并重启 `shy`。

**影响**：快速连续扫头会让害羞动画反复从头开始，看起来“卡在开头”。

**方向**：冷却从动画结束算，或把默认冷却设为 >= 40；如果产品上希望“可打断”，保持现状并文档化。

### R9. 全量预载

`Pet::Load` 对 218 帧全部解码、缩放、生成 `HBITMAP` 和 `AlphaMask`。按 269×358×4 B/px 计算，位图约 84 MB（不含 GDI+ 解码临时对象、mask、GDI 对象开销）；这是理论下界，不是实测峰值。

**影响**：启动时间、内存占用随动作数线性增长；当前规模可接受，但再加几套动作会明显。注意 `docs/guides/asset-packaging.md` 的 163.5 MB 是按 384×512 源尺寸估算的，不能直接和这里的缩放后位图对比。

**方向**：先测量真实启动耗时/峰值；再考虑懒加载（按动作）、图集、或缓存淘汰。`docs/guides/asset-packaging.md` 有图集方案的实测数据。

### R10. 只支持主屏 / 多显示器受限

`Pet::InitGeometry` 存的是主屏 `SM_CXSCREEN/SM_CYSCREEN`；`ClampToScreen`、走路边缘、气泡 `PlaceBubble` 都用它。`SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` 只影响像素坐标，不会让业务逻辑感知副屏。

**影响**：

- 宠物不能走到副屏；
- 拖到副屏会被拉回主屏；
- 副屏 DPI 不同时，窗口/气泡尺寸不重算。

**方向**：用 `MonitorFromWindow` + `GetMonitorInfo` 的 work area；处理 `WM_DPICHANGED`；气泡按所在显示器夹取。

### R11. 资产校验缺失

`LoadFrames` 不检查：

- 同一动作后续帧尺寸是否与首帧一致；
- `bounds.txt` 行数是否等于帧数；
- `bx/by/bw/bh` 是否落在画布内；
- PNG 是否真的带 alpha。

`LoadFrameBounds` 对非法行只是跳过；`GetHBITMAP` 失败会静默截断。混合画布/错误 bounds 不会让程序退出，只会视觉错位。

**方向**：加载时收集警告，启动日志或错误框列出“动作/帧/问题”；至少对首帧尺寸和 bounds 范围做断言。

### R12. INI 行尾注释与范围校验

`inifile::Parse` 只把 **行首** `#`/`;` 当注释；`IntOr` 要求整段值可解析。因此：

```ini
idle_min_ticks = 5 ; 5 秒
```

`stoi` 解析到 `5` 后还有 `" ; 5 秒"`，`used != value.size()`，回退默认 30。README 的示例正是这种写法，用户会以为改成功了。

另外没有 clamp：`scale_factor = 0` 会让帧缩到 1 px；`idle_min_ticks = -1` 会让 `RandomInRange` 返回 -1，行为时长异常。

**方向**：解析时去掉行尾注释（注意值里可能合法包含 `;` 的情况），或明确“注释必须单独一行”；对 `scale_factor`、tick、尺寸做下限校验。

### R13. 全局可变状态

`Settings`、`Behaviors`、`Rng`、`SubjectBounds` 都是全局/函数静态。当前单实例、单线程，没问题；但：

- 测试并行会互相污染；
- 未来多宠物/多窗口需要显式上下文；
- `Behaviors()` 返回 const ref，但 `MutableBehaviors()` 暴露可写引用，调用方可以绕过封装。

**方向**：如果产品化，引入 `PetContext`/`AppContext`，把 settings/registry/bounds/rng 收进去；这是较大重构，不建议在 demo 阶段做。

### R14. Win32/GDI+ 层没有自动化测试

8 个测试全部是纯逻辑；`Pet::OnTimer`、`HitTest`、鼠标阈值、`UpdateLayeredWindow`、气泡绘制、DPI 都没有测试。当前只能靠手工验收清单。

**方向**：至少加“启动后窗口存在/尺寸正确/日志正确”的脚本检查；进一步可以做截图对比，但 GDI+ 渲染在不同 Windows 版本上可能有细微差异。

### R15. 每秒重新置顶

`SetWindowPos(HWND_TOPMOST, ...)` 每 10 tick 一次。它保证宠物不被其它窗口压住，但也可能盖住用户刚打开的全屏/置顶窗口，且与其它 topmost 应用互相抢 z-order。

**方向**：demo 可接受；产品化可改为“失焦/被遮挡时再置顶”，或提供开关。

### R16. 气泡 z-order 轮询

宠物重新置顶后可能跑到气泡上面；`BubbleIsBehindPet` 从宠物往上遍历 z-order，发现气泡在下面才再次 `HWND_TOPMOST`。这是轮询补丁，不是原子保证。

**方向**：在宠物每次 `SetWindowPos(HWND_TOPMOST)` 后主动抬气泡；或把气泡设为宠物 owned window 并依赖 owned 关系（但 topmost 会破坏）。

### R17. 命令行编码

`__argv` 是窄字符，`WideFromUtf8` 按 UTF-8 解释。Windows 的 `__argv` 通常来自 ANSI 代码页，非 ASCII 动作名/目录名可能转换失败，回退默认动作/目录。

**方向**：用 `GetCommandLineW` + `CommandLineToArgvW`，全程宽字符。

### R18. DPI 变化

启动时设置 per-monitor V2，但没有 `WM_DPICHANGED` 处理；窗口位图在创建时按当时 DPI 的物理像素生成。拖到不同 DPI 的显示器时，系统可能缩放窗口，位图不会重算。

**方向**：监听 `WM_DPICHANGED`，按新 DPI 重建帧/窗口；或明确“仅主屏、单 DPI”限制。

### R19. `OnDragEnd` 未接线

`Trigger::OnDragEnd` 在枚举里，但没有 spec 使用，`OnLButtonUp` 也没有 `FindByTrigger(OnDragEnd)`。如果有人按“注册表驱动”的直觉加一条 `OnDragEnd` 行为，它永远不会触发。

**方向**：在 `OnLButtonUp` 的拖拽分支里触发一次 `OnDragEnd`（如果存在），或删除枚举并在文档里说明“拖拽结束直接进等待”。

### R20. emoji 只检查文件存在

`LoadEmojiPaths` 只 `GetFileAttributesW`；`RenderBubble` 解码失败时只跳过 emoji，仍然显示空白气泡主体。损坏的 `emoji_01.png` 不会被日志报告。

**方向**：加载时试解码，失败跳过并打印；或在气泡渲染失败时隐藏气泡。

## 4. 建议的处理顺序（不是承诺）

| 优先级 | 事项 | 理由 |
|---:|---|---|
| 已完成 | R1 bounds 缩放 | 直接影响气泡/扫头正确性，改动小、可测试 |
| 已完成 | R2 README 同步 | 用户入口，错误信息会反复误导 |
| P1 | 决定 R3 未注册动作 | 资产维护成本，越拖越乱 |
| P2 | R4 / R5 边界与语义 | 影响后续加行为、调参 |
| P2 | R6 / R7 / R8 行为细节 | 可在文档/参数层解决，未必改代码 |
| P2 | R9 / R10 / R11 / R12 | 性能、多屏、健壮性；按产品方向取舍 |
| P3 | R13~R20 | 架构/测试/边缘问题，demo 阶段可接受 |

每一项都应单独提案、单独验证；不要在一次提交里同时改 bounds、注册表和 README。
