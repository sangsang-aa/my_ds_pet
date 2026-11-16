# 模块与契约

本文是“改代码前先看”的边界说明：每个文件负责什么、哪些接口是稳定的、哪些不变量不能破坏。

## 1. 文件地图

### 入口

| 文件 | 职责 | 关键接口 | 平台依赖 |
|---|---|---|---|
| `src/main.cpp` | `WinMain` 装配：DPI、参数、GDI+、`pet.ini`、建窗、消息循环 | `WinMain` | Win32 + GDI+ |

### `src/core/`（基础层）

| 文件 | 职责 | 关键接口 | 平台依赖 | 纯逻辑 |
|---|---|---|---|---|
| `config.h` | 编译期默认值：定时器、尺寸、行为时长、扫头、气泡 | `constexpr` 常量 | `<windows.h>`（`UINT_PTR`） | 否 |
| `util.h/.cpp` | exe 目录、错误框、可用动作列表、UTF-8→宽字符、共享 RNG | `GetExecutableDir`、`ShowFatalError`、`WideFromUtf8`、`Rng`、`RandomInRange` | Win32 | 否 |
| `assets.h/.cpp` | 帧与 emoji 加载、缩放、`HBITMAP`、alpha 掩码、`bounds.txt` | `Frame`、`LoadFrames`、`LoadEmojiPaths`、`FramePath` | Win32 + GDI+ | 否 |
| `alpha_mask.h` | 1 bit/像素覆盖掩码 | `AlphaMask::Reset/Set/Test/Empty` | 无 | **是** |
| `bounds.h/.cpp` | 全局“当前主体矩形”（屏幕坐标） | `SetSubjectBounds`、`GetSubjectBounds`、`SubjectBounds` | 无 | **是**（`.cpp` 也纯） |
| `render.h/.cpp` | GDI+ 生命周期、`UpdateLayeredWindow` 合成 | `GdiplusSession`、`render::PresentLayered` | Win32 + GDI+ | 否 |
| `gdi.h` | 统一包含 Win32 + GDI+，定义 `gdi = Gdiplus` 别名 | — | Win32 + GDI+ | 否 |
| `inifile.h` | 极简 INI 解析 | `pet_ini::Parse/Find/IntOr/DoubleOr` | 无 | **是** |
| `settings.h/.cpp` | 运行期参数：默认值 + `pet.ini` 覆盖 + 行为段覆盖 | `Settings`、`GetSettings`、`ApplySettingsText/File` | `.h` 纯；`.cpp` Win32 | `.h` 是 |

### `src/pet/`（领域层）

| 文件 | 职责 | 关键接口 | 平台依赖 | 纯逻辑 |
|---|---|---|---|---|
| `behavior.h` | 行为 spec、触发器、播放模式、加权选择 | `Spec`、`Trigger`、`Playback`、`PickAuto`、`FindByTrigger` | 无 | **是** |
| `behaviors.h` | 行为注册表接口 | `Behaviors`、`MutableBehaviors` | 无 | **是** |
| `behaviors.cpp` | 7 条默认 spec | `MutableBehaviors` | `core/config.h`（Win32） | 否 |
| `pet.h/.cpp` | 状态机、帧推进、输入、窗口过程、资源释放 | `Pet`、`PetWndProc` | Win32 + GDI+ | 否 |
| `walk_logic.h` | 竖向播放的安全停帧判断 | `pet_walk::AtEndFrame/AtStartFrame` | 无 | **是** |
| `drag_logic.h` | 拖拽夹取到屏幕内 | `pet_drag::ClampToScreen` | 无 | **是** |
| `sweep_logic.h` | 扫头 stroke 识别与衰减 | `pet_sweep::State/Feed/Idle` | 无 | **是** |

### `src/bubble/`（展示层）

| 文件 | 职责 | 关键接口 | 平台依赖 | 纯逻辑 |
|---|---|---|---|---|
| `bubble.h/.cpp` | 气泡窗口、emoji 加载、圆角+尾巴绘制、定时器、跟随、z-order | `Bubble`、`BubbleWndProc` | Win32 + GDI+ | 否 |
| `bubble_logic.h` | 气泡放置与弹出间隔 | `pet_bubble::PlaceBubble/NextIntervalTicks` | 无 | **是** |

### 测试与工具

| 位置 | 职责 |
|---|---|
| `tests/*.cpp` | 7 个纯逻辑断言程序；不包含 `<windows.h>` |
| `tools/fix_edges.py` | 修青色/绿色边缘环、透明区幽灵 RGB、孤立白点 |
| `tools/migrate_canvas.py` | 旧画布顶部补透明到 384×512；同步/重建 `bounds.txt` |
| `tools/make_emoji.py` | 任意图片/GIF → 编号连续的 `emoji/emoji_NN.png` |

## 2. 关键数据契约

### `Frame`（`src/core/assets.h`）

```cpp
struct Frame {
    HBITMAP hbmp;
    bool hasAlpha;
    int bx, by, bw, bh;
    AlphaMask mask;
};
```

- `hbmp`：已经按 `scale_factor` 缩放后的位图，尺寸是窗口尺寸（默认 269×358）；拥有权归 `Frame`。
- `hasAlpha`：缩放后的位图是否带 alpha 通道；决定 `UpdateLayeredWindow` 的 `AlphaFormat`。
- `bx/by/bw/bh`：主体框；**按 `bounds.txt` 原样读入**。`bounds.txt` 是源画布（384×512）坐标，但当前消费者把它当缩放帧坐标用，见 [R1](06-decisions-and-risks.md#r1-bounds-未缩放p1)。
- `mask`：按缩放后的帧生成，1 bit/像素；空表示“没有掩码”，命中测试会退回整块矩形。

### `pet_behavior::Spec`（`src/pet/behavior.h`）

```cpp
struct Spec {
    std::wstring action;
    Trigger trigger;
    Playback playback;
    int minTicks, maxTicks, weight, dx, dy;
    bool gatedByWait;
};
```

语义（以当前 `StartBehavior` / `OnTimer` 为准，**不是**按注释字面）：

| 字段 | 实际语义 |
|---|---|
| `action` | `desktop_pet_image/<action>/` 目录名，也是 `pet.ini` 的 section 名 |
| `trigger` | `Auto` / `OnClick` / `OnDragStart` / `OnDragEnd` / `HeadSweep`。当前没有 spec 使用 `OnDragEnd`；拖拽结束由 `OnLButtonUp` 直接 `EnterWaiting` |
| `playback` | `Loop` / `OneShot` / `FullCycle` / `Boomerang`；停止规则见 `02-runtime-architecture.md` |
| `minTicks` | Auto：本次持续时间下限 + 启动冷却；事件：冷却（从启动算） |
| `maxTicks` | Auto：本次持续时间上限；`<= 0` 表示无时间上限。OneShot 不会因它提前停止 |
| `weight` | 仅 `PickAuto` 使用；`<= 0` 表示不可被自动选中 |
| `dx/dy` | Acting 期间每 tick 的窗口位移；任一非 0 即 `IsMoving` |
| `gatedByWait` | 拖后等待期间从 `PickAuto` 候选中排除 |

> 注释里写的 “Auto: dwell before firing” 与代码不一致。待机停留来自 `Settings::idleMinTicks/MaxTicks`，不是 spec 的 `minTicks`。这是已知的注释/语义漂移，见 [R5](06-decisions-and-risks.md#r5-specminticks-语义与注释不一致)。

### `Settings`（`src/core/settings.h`）

- 全局单例 `g_settings`，默认值来自 `config.h`。
- `ApplySettingsText` 解析 `[pet]` / `[bubble]`，再按 action 名覆盖行为 spec。
- 必须在 `Pet::Load` **之前** 调用；`scale_factor` 会影响帧尺寸，运行中改无效。
- 没有范围校验；错误值会原样进入运行期。

### `SubjectBounds`（`src/core/bounds.h`）

```cpp
struct SubjectBounds {
    int x, y, w, h;
    bool valid;
};
```

- 屏幕坐标；`valid == false` 时消费者退回窗口矩形。
- 只在 `Pet::PresentFrame` 中写入；每次换帧覆盖。
- 单线程全局变量；没有锁。

### `AlphaMask`（`src/core/alpha_mask.h`）

- 行优先、每像素 1 bit；`Reset(w,h)` 分配并清零，`Set` 越界忽略，`Test` 越界返回 false。
- `Empty()` 表示没有分配；`Pet::HitTest` 把空掩码视为“整块可点”。
- 阈值 `kHitAlpha = 16` 在 `assets.cpp` 生成掩码时使用，不是 `AlphaMask` 自己的逻辑。

### `render::PresentLayered`（`src/core/render.h`）

- 输入：窗口、内存 DC、位图、左上角屏幕坐标、宽高、是否逐像素 alpha。
- 行为：`SelectObject` → `UpdateLayeredWindow(..., ULW_ALPHA)` → 恢复旧对象；空 DC/位图直接返回。
- `perPixelAlpha == true`：`AlphaFormat = AC_SRC_ALPHA`；否则 `AlphaFormat = 0` + `SourceConstantAlpha = 255`。
- 不做缩放、不做裁剪；调用者保证位图尺寸与 `w/h` 一致。

### 纯逻辑接口

| 接口 | 契约 |
|---|---|
| `pet_behavior::PickAuto(specs, eligible, rand01)` | 只在 `eligible && trigger==Auto && weight>0` 中按权重选；总权重 <=0 返回 -1；`rand01` 必须在 `[0,1)` |
| `pet_behavior::FindByTrigger(specs, trigger)` | 返回第一个匹配项的下标，没有则 -1；当前每个 trigger 最多一条 |
| `pet_bubble::PlaceBubble(...)` | 优先上方，其次右侧、左侧，最后下方；结果夹到屏幕内；`headPercent` 只用于侧边垂直居中 |
| `pet_bubble::NextIntervalTicks(min,max,rand01)` | `max <= min` 返回 `min`；否则 `rand01` 映射到闭区间 `[min,max]` |
| `pet_sweep::Feed(state, dx, threshold, passesNeeded)` | 方向反转且上一段累计 >= threshold 才算一次；达到 passesNeeded 时清空 state 并返回 true |
| `pet_sweep::Idle(state, resetTicks)` | 每 tick 累计位移减半；超过 resetTicks 清空 |
| `pet_walk::AtEndFrame(index,size)` | `size <= 1` 或 `index >= size-1` |
| `pet_walk::AtStartFrame(index)` | `index <= 0` |
| `pet_drag::ClampToScreen(x,y,w,h,sw,sh)` | 把窗口左上角夹到 `[0, sw-w] × [0, sh-h]`；窗口比屏幕大时上限取 0 |
| `pet_ini::Parse/Find/IntOr/DoubleOr` | 只解析 `[section]` 与 `key=value`；只支持整行 `#`/`;` 注释；重复键最后生效；`IntOr/DoubleOr` 要求整段解析成功，否则回退 |

## 3. 不变量

1. `basePool` 非空，且 `basePool[0]` 至少有一帧；`InitGeometry` 直接取 `[0][0]`。
2. 帧从 `000.png` 连续加载，遇到第一个缺失就停；最多 120 帧。不会跳过缺号。
3. 同一动作的所有帧应当同尺寸；**运行期不校验**。窗口尺寸只由第一帧决定。
4. `bounds.txt` 的有效行按顺序映射到已加载帧；非法行会被跳过且不推进索引（后续行前移一帧），缺失行不会报错，未赋值的帧保持 0。
5. `Frame.mask` 为空时，命中测试视为整块不透明。
6. `cooldowns.size() == clips.size()`；`StartBehavior` 写 `cooldowns[activeClip]`。
7. `currentFrame` 指向 `basePool`/`clips` 内部，`DestroyResources` 后不得再使用。
8. `SubjectBounds` 每次 `PresentFrame` 更新；消费者不能假设它在窗口移动后立即准确。
9. `Settings` 在 `Pet::Load` 前应用；`Behaviors()` 的覆盖也发生在加载前。
10. 只有一个 `Pet` / `Bubble` 实例；全局 `Settings`、`Behaviors`、`Rng`、`SubjectBounds` 不支持多实例。
11. `PetWndProc` / `BubbleWndProc` 在 `WM_CREATE` 里把 `lpCreateParams` 写入 `GWLP_USERDATA`；之前的消息不依赖该指针。

## 4. 扩展点

### 加一条行为（不需要改 `pet.cpp`）

1. 在 `MutableBehaviors()` 追加一个 `Spec`；
2. 放 `desktop_pet_image/<action>/000.png ...` 和 `bounds.txt`；
3. 如果是 Auto：设置 `minTicks/maxTicks/weight/dx/dy/gatedByWait`；
4. 如果是事件：复用现有 `Trigger`，并确保事件路径已经调用 `FindByTrigger`（当前 OnClick / OnDragStart / HeadSweep 已接好；OnDragEnd 未接）；
5. 可选：在 `pet.ini` 加同名 section 覆盖参数。

限制：新增 **Trigger** 或 **Playback** 需要在 `Pet::OnTimer` / 输入处理里补逻辑，不能只加数据。

### 加一个配置键

1. `Settings` 加字段；
2. `settings.cpp` 的 `g_settings` 初始化加默认值；
3. `ApplySettingsText` 加 `set_int` / `DoubleOr`；
4. 使用点从 `GetSettings()` 读；
5. 如果影响加载（如 `scale_factor`），必须在 `Pet::Load` 之前。
6. 更新 `04-assets-and-config.md` 的键表。

### 加一个测试

1. 纯逻辑放头文件，不 include `<windows.h>`；
2. `tests/<name>_test.cpp` 用 `assert` 或显式 `return 1`；
3. 加入 `AGENTS.md` 的测试循环；
4. 保持 `g++ -std=c++17 -Wall -Wextra -O2` 零警告。

### 加一个独立功能模块

- 建议 `src/<feature>.h/.cpp`，只依赖 `core/config.h` / `core/util.h`，由 `main.cpp` 装配；
- 不要把新状态塞进 `Pet`，除非它必须和当前帧/窗口位置同步；
- 如果需要在宠物窗口上画，优先复用 `render::PresentLayered` 或新建自己的 layered window（参考 `Bubble`）。

## 5. 已知边界问题

| 问题 | 位置 | 影响 |
|---|---|---|
| core 反向依赖 pet | `src/core/settings.cpp` → `pet/behaviors.h` | 分层不纯；未来非宠物 shell 复用 core 时要拆 |
| 全局可变状态 | `Settings`、`Behaviors`、`Rng`、`SubjectBounds` | 单实例假设；测试并行/多宠物会互相污染 |
| 加载顺序隐式 | `assets.cpp` 读 `GetSettings().scaleFactor` | 必须先应用 `pet.ini` 再 `LoadFrames`；否则缩放用默认值 |
| 无资产校验 | `LoadFrames` / `LoadFrameBounds` | 混合画布、错误 bounds 不会报错，只在视觉上出问题 |
| `bounds.txt` 单位不一致 | `assets.cpp` + 消费者 | 气泡/扫头偏移；见 R1 |
| 路径拼接 | `exeDir + assetsDir` | `assetsDir` 传绝对路径会拼坏；当前只支持相对目录名 |
