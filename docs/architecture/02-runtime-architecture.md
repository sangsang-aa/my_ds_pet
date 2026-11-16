# 运行时架构

本文描述程序从 `WinMain` 到消息循环、再到每个 tick 的实际路径。重点是**数据怎么流、状态怎么变、谁拥有资源**。

## 1. 分层与依赖

```
┌──────────────────────────────────────────────────────────────┐
│ 入口 / 装配            src/main.cpp                          │
│  DPI、参数、GDI+、pet.ini、建窗、消息循环                     │
├──────────────────────────────────────────────────────────────┤
│ 领域层                src/pet/                               │
│  Pet 状态机 / 行为注册表 / 纯逻辑（walk、drag、sweep）        │
├──────────────────────────────────────────────────────────────┤
│ 展示层                src/bubble/                            │
│  气泡窗口 / 布局与调度纯逻辑                                 │
├──────────────────────────────────────────────────────────────┤
│ 基础层                src/core/                              │
│  assets / render / bounds / settings / inifile / util /      │
│  alpha_mask / config                                         │
├──────────────────────────────────────────────────────────────┤
│ 平台                  Win32 + GDI+                           │
└──────────────────────────────────────────────────────────────┘
```

实际 include 依赖（箭头表示“依赖”）：

```
main.cpp
 ├─> core/settings.* ──> core/inifile.h
 │                    └─> pet/behaviors.h        ← 已知反向依赖
 ├─> core/assets.cpp ──> core/settings.h, core/gdi.h, core/config.h
 ├─> pet/pet.cpp ──────> core/assets.h, core/bounds.h, core/render.h,
 │                  │    core/settings.h, core/util.h
 │                  ├─> pet/behaviors.h
 │                  └─> walk_logic.h / drag_logic.h / sweep_logic.h
 ├─> bubble/bubble.cpp ─> core/assets.h, core/bounds.h, core/config.h,
 │                    │   core/gdi.h, core/render.h, core/settings.h,
 │                    │   core/util.h
 │                    └─> bubble_logic.h
 └─> core/render.cpp ──> core/gdi.h
```

边界说明：

- `core/` 名义上是基础层，但 `core/settings.cpp` 为了把 `[action]` 段写回行为注册表，直接 include 了 `pet/behaviors.h`。这是当前唯一的 **core → 领域层** 反向依赖。
- 纯逻辑头（`behavior.h`、`behaviors.h`、`alpha_mask.h`、`bounds.h`、`inifile.h`、`settings.h`、`util.h`、`walk_logic.h`、`drag_logic.h`、`sweep_logic.h`、`bubble_logic.h`）不包含 `<windows.h>`，所以 7 个测试可以在任意主机编译。
- `config.h` 需要 `UINT_PTR`，因此包含 `<windows.h>`；纯逻辑头不要 include 它。

## 2. 启动时序

`WinMain` 按以下顺序执行：

1. **DPI 感知**：动态加载 `SetProcessDpiAwarenessContext`，优先 `PER_MONITOR_AWARE_V2`；旧系统回退 `SetProcessDPIAware()`。
2. **解析参数**：`__argc/__argv`；`argv[1]` 是显式基底动作，`argv[2]` 是资源目录名。
3. **GDI+ 初始化**：栈上的 `GdiplusSession`，RAII；失败弹框退出。
4. **定位 exe 目录**：`GetExecutableDir()`，得到带尾随反斜杠的路径。
5. **应用 `pet.ini`**：`ApplySettingsFile(exeDir + assetsDir + "\\pet.ini")`；不存在就跳过。
6. **`Pet::Load`**：
   - 显式动作：只加载该动作到 `basePool`；
   - 无参数：只尝试 `idle` 到 `basePool`；
   - 遍历 `Behaviors()`，为每条 spec 加载一个 clip；
   - 每帧：GDI+ 解码 → 按 `scale_factor` 缩放 → `GetHBITMAP` → 生成 `AlphaMask`；全部帧加载完后读一次 `bounds.txt`；
   - 初始化 `cooldowns` 和第一次待机目标。
7. **`Pet::InitGeometry`**：取 `basePool[0][0]` 的位图尺寸作为窗口尺寸；记录主屏宽高。
8. **注册窗口类并创建宠物窗口**：居中，`WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`，`WS_POPUP`。
9. **`SetTimer(hwnd, kTimerId, 100, nullptr)`**。
10. **`Bubble::Init`**：加载 emoji 路径列表，注册气泡窗口类，创建隐藏的 owned window，启动自己的定时器。
11. **`PresentBase` + `ShowWindow(SW_SHOWNOACTIVATE)` + `SetFocus`**。
12. **消息循环**：`GetMessageW` / `TranslateMessage` / `DispatchMessageW`。

> 顺序要点：`pet.ini` 必须在 `Pet::Load` 之前应用；`Bubble::Init` 必须在宠物窗口存在之后；第一次 `PresentBase` 在 `ShowWindow` 之前，避免闪白。

## 3. 两个窗口、两个定时器

| | 宠物窗口 | 气泡窗口 |
|---|---|---|
| 类名 | `DesktopPetWindowClass` | `DesktopPetBubbleClass` |
| 样式 | `WS_POPUP` + `WS_EX_LAYERED \| WS_EX_TOPMOST \| WS_EX_TOOLWINDOW` | `WS_POPUP` + `WS_EX_LAYERED \| WS_EX_TOPMOST \| WS_EX_TOOLWINDOW \| WS_EX_NOACTIVATE \| WS_EX_TRANSPARENT` |
| 尺寸 | 269×358（由首帧缩放后决定） | 252×252（默认参数） |
| 所有者 | 无 | `petHwnd`（owned window） |
| 定时器 | `kTimerId = 1`，100 ms | `kBubbleTimerId = 2`，100 ms |
| 输入 | 命中测试、点击、拖拽、扫头、退出 | 完全穿透，不接收鼠标 |
| 职责 | 帧播放、移动、状态机 | 定时弹出/隐藏、布局、重绘、跟随 |
| 销毁 | `WM_DESTROY` 释放帧/DC | 作为 owned window 随宠物销毁；`OnDestroy` 释放 DC/位图 |

两个定时器都在 **同一个 UI 线程** 上串行执行，没有锁。`UpdateLayeredWindow` 每帧调用一次；`WM_PAINT` / `WM_ERASEBKGND` 基本是空操作。

`SetTimer` 不保证精确 100 ms：消息可能被合并或延迟。所有时长都以 **tick 数** 计，不按墙钟补帧；UI 卡顿时动画会变慢，而不是跳帧追赶。

## 4. 状态机

`PetState` 只有两个值：`Base` 和 `Acting`。“等待”不是第三个枚举：

```
                         auto pick（Base 且 waitTicks == 0）
          ┌──────────────────────────────────────────────────┐
          │                                                  ▼
     ┌─────────┐      StartBehavior(OnClick / OnDragStart / HeadSweep)
     │  Base   │ ────────────────────────────────────────► ┌──────────┐
     │  idle   │                                           │  Acting  │
     └─────────┘ ◄──────────────────────────────────────── └──────────┘
          ▲          OneShot 播完 / Loop 到时或撞边
          │          FullCycle 到末帧 / Boomerang 回首帧
          │
          │ 拖拽松手 / 捕获丢失
     ┌──────────────────┐
     │ Base + Waiting   │  waitTicks > 0，只待机，不自动移动
     └──────────────────┘
```

要点：

- `Waiting` 是 `state == Base && waitTicks > 0`；`EnterWaiting` 先 `EnterBase` 再设 `waitTicks`。
- `StartBehavior` **不检查当前 state**，所以事件行为可以抢占正在播放的自动行为（走路中途被点击/扫头/拖拽打断）。
- 拖拽开始只取决于鼠标位移是否超过系统阈值；如果 `drag` clip 缺失，窗口仍然会移动，只是状态机不进入 `drag` 动画。
- `WM_CAPTURECHANGED` 取消拖拽；如果当前 active clip 是 `OnDragStart`，进入等待。

相关字段（`Pet`）：

| 字段 | 含义 |
|---|---|
| `basePool` / `basePoolIndex` / `baseIndex` | 基底动画池、当前池项、当前帧 |
| `clips` / `cooldowns` | 每条注册 spec 的帧与冷却 |
| `activeClip` / `activeIndex` / `activeStep` | 当前行为、帧索引、Boomerang 方向 |
| `activeTicks` / `activeTarget` | 已播放 tick、目标 tick（0 = 无时间上限） |
| `behaviorTicks` / `behaviorTarget` | 待机已停留 tick、下一次自动行为的阈值 |
| `waitTicks` | 拖后等待剩余 tick |
| `mouseDown` / `dragging` / `dragOffset*` | 输入状态 |
| `currentFrame` | 屏幕上这一帧，供命中测试 |
| `memDC` / `petW` / `petH` | 绘制 DC 与窗口尺寸 |

## 5. 每 tick 的处理顺序

`WM_TIMER` → `Pet::OnTimer`：

1. **重新置顶**：`++topmostTicks >= kTopmostReassertTicks`（10）时 `SetWindowPos(HWND_TOPMOST, ...)`。
2. **递减所有冷却**：`cooldowns[i] > 0` 则减 1。
3. **扫头状态衰减**：`pet_sweep::Idle(sweep, kSweepResetTicks)`（累计位移减半，超时清空）。
4. **Acting 分支**：
   1. 按 `Playback` 推进 `activeIndex`；
   2. `PresentFrame`（`UpdateLayeredWindow` + 发布 `SubjectBounds`）；
   3. 如果 spec 有 `dx/dy`，计算新窗口位置；越界则不移动并记 `atEdge`；
   4. `activeTicks += 1`；
   5. `OneShot` 播完 → `EnterBase`；
   6. 有 `activeTarget` 时判断 `done` 和 `safeToStop`；满足则 `EnterBase`。
5. **Base 分支**：
   1. 推进基底帧；播完一轮时若池大于 1，随机换池项；
   2. `PresentBase`；
   3. `waitTicks > 0`：递减，归零时重置待机计时；
   4. 否则 `behaviorTicks += 1`，到达 `behaviorTarget` 时 `PickNextAuto`，挑到就 `StartBehavior`。

> **`PresentFrame` 在窗口移动之前执行**：它用当前窗口矩形计算 `SubjectBounds`，随后 `SetWindowPos` 才移动窗口。因此 `SubjectBounds` 在移动后的最多 1 tick 内仍是旧位置；气泡/扫头会滞后约 100 ms。视觉上 layered window 的内容会随窗口一起移动，不受影响。

## 6. Playback 语义

| 模式 | 帧推进 | 停止条件 | 当前使用者 |
|---|---|---|---|
| `Loop` | `index = (index + 1) % size` | `activeTarget` 到、撞边，或外部事件 | `walking_left/right`、`drag` |
| `OneShot` | 递增到 `size` 后标记 `ended` | 播完最后一帧（忽略 `activeTarget` 的提前停止） | `clicked`、`shy` |
| `FullCycle` | 同 `Loop`（会回绕） | `done` 且 `AtEndFrame(index, size)` | `walking_down` |
| `Boomerang` | 正向到 `size-1`，反向到 `0` | `done` 且 `AtStartFrame(index)` | `walking_up` |

细节：

- **`Loop`**：可以在任意帧停止；走路和拖拽不需要“落地姿势”。
- **`OneShot`**：最后一帧在上一 tick 已经显示；本 tick 只负责 `EnterBase`。即使 `maxTicks > 0`，也不会在末帧前因 `activeTarget` 停止。
- **`FullCycle`**：名字容易误解。它不是“只播一遍”，而是“必须停在末帧”。如果 `activeTarget` 小于帧数，它到第一个末帧就停；如果大于帧数，它会循环多轮，直到某个末帧时 `activeTicks >= activeTarget`。
- **`Boomerang`**：周期是 `2 * size - 2` tick（首尾不重复）。`walking_up` 27 帧，周期 52 tick，正好等于 `kWalkUpMinTicks/MaxTicks`；目标到达时刚好回到第 0 帧。
- `size <= 2` 时 `Boomerang` 走普通递增路径，不会进入回摆；当前没有这种 clip。
- `activeTarget == 0`（`maxTicks <= 0`）表示没有时间上限：`Loop` 靠外部事件结束，`OneShot` 靠帧结束，`Boomerang` 会一直回摆（当前没有这种配置）。

### 冷却的语义

`StartBehavior` 会把 `cooldowns[clip] = spec.minTicks`。`CanStart` 要求冷却 <= 0。

- 对 Auto 行为，`minTicks` 同时是“本次持续时间下限”和“启动冷却”；因为行为本身通常就播这么久，冷却在结束时基本已经归零，实际不构成额外限制。
- 对 `shy`，`minTicks = 25`，但 clip 有 40 帧；冷却在动画后半段就归零，所以可以再次触发。
- 对 `clicked` / `drag`，`minTicks = 0`，没有冷却。
- `weight` 只在 `PickAuto` 里使用，对事件行为无意义。

## 7. 数据流

```
desktop_pet_image/<action>/NNN.png
        │  GDI+ Bitmap（源 384×512 RGBA）
        ▼
  按 scale_factor 缩放（默认 0.70，269×358）
        │
        ├─► GetHBITMAP ──────────────► Frame.hbmp
        ├─► LockBits(ARGB) ──────────► Frame.mask（1 bit/px）
        └─► 源像素格式 ──────────────► Frame.hasAlpha

desktop_pet_image/<action>/bounds.txt
        │  x y w h（源画布坐标）
        └─► LoadFrameBounds ─────────► Frame.bx/by/bw/bh
                                        │
                                        ▼
                              Pet::PresentFrame
                                   │        │
                                   │        └─► SetSubjectBounds（屏幕坐标）
                                   │                  │
                                   ▼                  ├─► Bubble 布局
                          UpdateLayeredWindow         └─► 扫头判定带
                                   │
                                   └─► currentFrame ──► HitTest（客户区）
```

关键点：

- `Frame` 同时携带 **位图**、**alpha 掩码** 和 **主体边界**；三者都来自同一帧，但 `bounds.txt` 的坐标单位与缩放后的位图不一致（见下节）。
- `SubjectBounds` 是全局单例（`core/bounds.cpp`），每次 `PresentFrame` 覆盖；气泡和扫头都读它。
- `Settings` 也是全局单例；`Behaviors()` 是可变的全局注册表。程序假设只有一个 `Pet` 和一个 `Bubble` 实例。
- `Rng()` 是共享的 `std::mt19937`，宠物和气泡都从同一个引擎取随机数（同一线程，无锁）。

## 8. 坐标系

| 坐标系 | 范围 | 用途 |
|---|---|---|
| 源画布 | 384×512 | PNG 文件、`bounds.txt`、`tools/` 生成逻辑 |
| 缩放帧 / 客户区 | 269×358 | `Frame.hbmp`、`AlphaMask`、`HitTest`、窗口客户区 |
| 屏幕 | 主屏物理像素 | 窗口位置、`SubjectBounds`、气泡放置、扫头鼠标坐标 |

转换关系：

- 客户区 → 屏幕：`windowRect.left/top + clientX/Y`；
- 屏幕 → 客户区：`ScreenToClient`（`WM_NCHITTEST`）；
- `bounds.txt` 的 `bx/by/bw/bh` 目前被 **直接** 当作缩放帧坐标使用，没有乘 `scale_factor`。这是已知缺陷，详见 [`06-decisions-and-risks.md`](06-decisions-and-risks.md#r1-bounds-未缩放p1)。

主屏限制：`InitGeometry` 存的是 `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)`；走路边缘、拖拽夹取、气泡夹取都用这一组值，不感知多显示器/每显示器 DPI。

## 9. 资源所有权与生命周期

| 资源 | 拥有者 | 释放点 |
|---|---|---|
| GDI+ token | `GdiplusSession`（`WinMain` 栈） | `~GdiplusSession`，消息循环之后 |
| `basePool` / `clips` 的 `HBITMAP` | `Pet` | `Pet::DestroyResources`（`WM_DESTROY`） |
| 宠物内存 DC | `Pet` | `DestroyResources`，**先删 DC 再删位图** |
| 气泡内存 DC / 位图 | `Bubble` | `Bubble::OnDestroy`（owned window 销毁时） |
| `currentFrame` | 指向 `basePool`/`clips` 内部 | 不拥有；`DestroyResources` 后失效 |
| `Settings` / `Behaviors` / `Rng` / `SubjectBounds` | 全局/函数静态 | 进程结束 |

`Pet` 和 `Bubble` 都是 `WinMain` 栈对象；窗口的 `GWLP_USERDATA` 保存它们的地址。消息循环结束、窗口销毁之后才离开作用域，所以不存在悬空窗口指针。

## 10. 线程与并发

- 单 UI 线程：所有窗口过程、定时器、GDI/GDI+ 调用都在同一线程。
- 没有 `CreateThread`、`std::thread`、异步 IO、锁。
- 因此 `Settings`、`Behaviors`、`SubjectBounds`、`Rng` 的全局可变状态是安全的（单实例前提下）。
- 代价：任何阻塞（例如大量磁盘 IO、慢速解码）都会卡住动画与输入；当前所有帧在启动时一次性加载，运行期没有磁盘读取。

## 11. 事件优先级（当前实现）

从高到低：

1. 右键 / ESC（直接销毁窗口）；
2. 左键拖拽（超过系统阈值后抢占当前行为）；
3. 左键单击（未超过阈值时，在抬起时启动 `clicked`）；
4. 扫头（未按左键且在头部带内）；
5. 自动行为（只在 Base 且 `waitTicks == 0` 时由计时器挑选）。

注意：拖拽开始后 `mouseDown == true`，扫头被抑制；拖拽结束进入等待，不会立刻恢复自动移动。
