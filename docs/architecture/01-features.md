# 功能清单

本文按“用户能看到 / 触发什么”组织，同时给出代码入口。所有参数默认值见
[`04-assets-and-config.md`](04-assets-and-config.md)；行为注册表的实现见
[`02-runtime-architecture.md`](02-runtime-architecture.md)。

## 功能矩阵

| 功能 | 触发 | 结果 | 依赖资产 | 代码入口 |
|---|---|---|---|---|
| 启动 / 资源装载 | 双击或命令行 | 加载帧、创建窗口、弹错误框或继续 | `desktop_pet_image/` | `WinMain`、`Pet::Load` |
| 待机 | 默认状态 | 循环播放基底动画 | `idle/`（当前只有 1 帧） | `Pet::PresentBase` |
| 自动移动 | 待机停留随机时长后 | 四方向之一，循环动画 + 窗口移动 | `walking_left/right/up/down` | `Pet::OnTimer`、`PickNextAuto` |
| 左键单击 | 按下后位移未超过系统拖拽阈值 | 播放一次 `clicked` | `clicked/` | `OnLButtonDown` / `OnLButtonUp` |
| 拖拽 | 按下后位移超过系统拖拽阈值 | 跟随光标，循环 `drag`；松手进入等待 | `drag/` | `OnMouseMove` / `OnLButtonUp` |
| 拖后等待 | 松手 | 2~3 分钟只待机、不自动移动 | 无 | `EnterWaiting` |
| 扫头害羞 | 鼠标在头部横向来回扫 | 播放一次 `shy` | `shy/` | `OnMouseMove` + `pet_sweep` |
| 气泡表情 | 每 20~60 秒 | 头顶/侧边弹出随机 emoji，约 3 秒 | `emoji/emoji_01.png ...` | `Bubble::OnTimer` |
| 右键退出 | 在宠物像素上右键 | 销毁窗口并退出 | 无 | `WM_RBUTTONUP` |
| ESC 退出 | 窗口有焦点时按 ESC | 销毁窗口并退出 | 无 | `WM_KEYDOWN` |
| 点击穿透 | 点在透明留白上 | 事件落到下面的桌面/窗口 | 逐帧 alpha 掩码 | `WM_NCHITTEST` + `Pet::HitTest` |
| 参数覆盖 | 资源目录存在 `pet.ini` | 不重编译改参数 | `pet.ini` | `ApplySettingsFile` |

## 1. 启动与资源装载

启动入口是 `src/main.cpp` 的 `WinMain`。命令行参数：

```
pet_demo.exe                         # 基底池：当前代码只加载 idle
pet_demo.exe idle                    # 强制单个基底动作，不做随机切换
pet_demo.exe walking_right           # 强制其它单个动作作为基底
pet_demo.exe idle desktop_pet_image  # 第二个参数指定资源目录
```

- 资源路径以 **可执行文件所在目录** 为基准（`GetExecutableDir()`），不依赖当前工作目录。
- 默认资源目录是 `desktop_pet_image`；第二个参数可以改名，但仍然是相对 exe 目录拼接。
- 启动时先应用 `<exeDir><assetsDir>\pet.ini`（若存在），再加载帧，所以 `scale_factor` 会作用于本次加载。
- 基底池：有显式动作参数时只放这一个动作；无参数时只尝试 `idle`。`idle2` 的循环在 `Pet::Load` 中被刻意注释掉（当前只有 `idle` 一项）。
- 无论基底是什么，注册表中的 7 条行为都会尝试加载；缺帧的行为只是被禁用，不会让程序退出。
- 没有任何基底帧时弹错误框并退出；错误框会列出资源目录和实际可用的动作。

启动日志（stdout；GUI 程序没有控制台，从终端启动才看得到）示例：

```
[pet] idle 'idle' loaded=1/120 frames
[pet] random idle enabled across 1 animations
[pet] walking_left loaded=30/120 frames
...
[pet] emoji loaded=6 stickers
[pet] bubble enabled (252x252, 6 stickers)
```

## 2. 待机

- 宠物定时器 `kTimerElapseMs = 100`，约 10 fps。
- 基底池是一个 `std::vector<std::vector<Frame>>`。当前只有一个 `idle`，且 `idle` 只有 1 帧，所以视觉上是静止的。
- 如果以后往池里放多个动画：每播完一轮，`baseIndex` 归零，并随机切换到池中的另一个动画（`PickRandomBaseIndex`）。
- 待机停留时长由 `[pet] idle_min_ticks` / `idle_max_ticks` 决定，默认 30~100 tick（约 3~10 秒）。时间到后从可用的 Auto 行为中按权重随机挑一个。
- 每 10 tick（约 1 秒）重新 `SetWindowPos(HWND_TOPMOST, ...)`，避免被其它置顶/全屏窗口压下去。

## 3. 自动移动（四方向）

注册表里的四条 Auto 行为：

| 动作 | 素材语义 | Playback | 每 tick 位移 | 目标时长（默认） | 等待期抑制 |
|---|---|---|---|---:|---|
| `walking_left` | 侧视走路 | `Loop` | `dx = -5` | 15~45 tick | 是 |
| `walking_right` | 侧视走路（镜像） | `Loop` | `dx = +5` | 15~45 tick | 是 |
| `walking_up` | **飞起**，不是走路 | `Boomerang` | `dy = -5` | 固定 52 tick | 是 |
| `walking_down` | **俯冲**，不是走路 | `FullCycle` | `dy = +5` | 15~45 tick（实际取下一个末帧） | 是 |

选择规则（`pet_behavior::PickAuto`）：

- 只考虑 `Trigger::Auto`；
- 帧必须加载成功；
- `weight > 0`；
- 不在拖后等待期（或该行为没有 `gatedByWait`）；
- 按权重做一次随机；当前四条权重都是 1。

移动与停止：

- 每个 tick 先推进动画帧，再计算 `newLeft/newTop`；如果新矩形会越出主屏边缘，就 **不移动**，并记 `atEdge`。
- `Loop` 可以在任意帧停止；`FullCycle` 必须等到末帧；`Boomerang` 必须回到首帧。这样竖向动作不会停在半空或起跳前的预备姿势。
- `walking_down` 的 `maxTicks` 只是“最早可以停”的时间：如果目标 45 tick 时还没到末帧，它会继续循环到下一个末帧（默认 30 帧时通常在第 59 tick 左右停）。因此实际时长可能大于 `maxTicks`。
- 撞边缘后同样要等到安全帧才回待机；期间窗口不再移动，但动画继续。

> `walking_up` / `walking_down` 的文件夹名是历史遗留：前者是飞起，后者是俯冲。不要把它们的帧当“上下走路”重新计时。

## 4. 左键单击

- `WM_LBUTTONDOWN`：记录按下点，`SetCapture`。
- `WM_MOUSEMOVE`：位移达到 `SM_CXDRAG` / `SM_CYDRAG` 才进入拖拽；否则仍算点击。
- `WM_LBUTTONUP`：如果没进入拖拽，查找 `Trigger::OnClick` 的行为（`clicked`），在可启动时播放一次 `OneShot`。
- `clicked` 播完自动回基底；如果它缺失，单击不产生反应。
- 拖后等待期间单击仍然有效，且 **不会** 重置等待计时。

## 5. 拖拽

- 进入拖拽时记录光标相对窗口左上角的偏移，之后窗口跟随光标，抓取点保持在光标下。
- 拖拽期间循环播放 `drag`（`OnDragStart`，`Loop`，无时间上限）；如果 `drag` 缺失，窗口仍然会跟随移动，只是没有挣扎动画。
- `pet_drag::ClampToScreen` 把窗口限制在主屏内；否则窗口越界后走路的边缘检查会取消所有移动，宠物看起来“卡死”。
- 松手进入拖后等待（见下节）。
- 捕获丢失（`WM_CAPTURECHANGED`，例如被其它窗口抢走）会取消拖拽；如果当前正在播 `drag`，同样进入等待。

## 6. 拖后等待

- `EnterWaiting` 先回到基底，再设置 `waitTicks = random(1200, 1800)`，约 2~3 分钟。
- 等待不是独立的 `PetState`，而是 `state == Base && waitTicks > 0`。
- 等待期间只随机切换基底动画，不启动任何 `gatedByWait` 的 Auto 行为；当前四条移动都带这个标记。
- 再次拖拽会重新计时；等待结束后重置 `behaviorTicks` 和下一次待机目标，再恢复“待机 ↔ 移动”链。

## 7. 扫头害羞

触发条件（`Pet::OnMouseMove`）：

1. 左键没有按下；
2. 光标在宠物 **主体边界** 的头部带内；
3. 水平位移占主导：`|dx| > |dy|`；
4. `|dx| <= max_sweep_delta_px`（默认 80），过滤鼠标重新进入窗口造成的跳变；
5. `pet_sweep::Feed` 认为完成了一次有效换向。

头部带的实际公式（默认值）：

```
bandTop    = by + bh * head_band_top_percent / 100          // 默认 0
bandBottom = by + bh * head_band_percent / 100
                 - bh * kHeadBandBottomInsetPercent / 100    // 默认 45% - 20% = 25%
```

即默认判定带是主体顶部 **0%~25%**，不是字面上的 45%。如果算出来高度小于 `bh / 8`，会补到 `bh / 8`；再夹到主体底边以内。

扫动识别（`pet_sweep`）：

- 同方向累计水平位移；
- 方向反转且上一段累计 >= `sweep_threshold_px`（默认 50）时，算完成一次 stroke；
- 需要 `kSweepPassesToTrigger = 2` 次 stroke 才触发；
- 每 tick 衰减一半累计位移；超过 `kSweepResetTicks = 20`（约 2 秒）没有有效移动就清空；
- 触发后播放一次 `shy`（`OneShot`），冷却从启动时算 `minTicks = 25`（约 2.5 秒）。

注意：`shy` 有 40 帧（约 4 秒），而冷却只有 2.5 秒，所以动画后半段可以再次被扫头重启。当前没有“动画播完才开始冷却”的逻辑。

事件优先级：`CanStart` 不检查当前 `state`，所以点击、拖拽、扫头都可以 **抢占** 正在播放的自动行为（例如走路中途被扫头打断）。

## 8. 气泡表情

- 气泡是独立窗口，不是宠物窗口的一部分；它有自己的 100 ms 定时器和 `countdown/showLeft`。
- 弹出间隔：200~600 tick（约 20~60 秒）；显示 30 tick（约 3 秒）。
- 窗口尺寸：`emoji_size_px + 2 * pad_px + tail_h = 220 + 20 + 12 = 252`，所以是 **252×252**；其中气泡主体 240×240，尾巴 12 px。
- 每次随机选一张 `emoji/emoji_01.png ...`；图片按最长边缩放到 220 px，居中画在圆角主体里。
- 放置顺序：优先宠物头顶上方；上方放不下时放在头部右侧，再左侧；都不行才放下方。位置会被夹到主屏内。
- 气泡样式含 `WS_EX_TRANSPARENT`，鼠标事件穿透，不挡宠物点击/拖拽；`WS_EX_NOACTIVATE` 不抢焦点。
- 每次宠物帧更新会发布 `SubjectBounds`；气泡用 `BubblePetRect` 读取它，所以会随宠物移动。若 bounds 无效则退回宠物窗口矩形。
- 宠物每约 1 秒重新置顶，可能把气泡压到下面；气泡定时器发现自己在宠物后面时会再次 `HWND_TOPMOST`。
- `emoji/` 缺失或为空时只禁用气泡；其余功能不受影响。`emoji/index.json` 当前 **不被代码读取**。

## 9. 退出、焦点与托盘

- 在宠物像素上右键：`WM_RBUTTONUP` → `Pet::onRightClick` 回调（由 `WinMain` 接到 `Tray::ShowBar`），在宠物下方弹出横向工具栏，不再直接退出。透明留白上的右键仍被 `WM_NCHITTEST` 的 `HTTRANSPARENT` 挡掉，不会弹工具栏。
- **横向工具栏**是自绘分层窗口（`WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`），不用原生 `TrackPopupMenu`（后者条目只能竖排）。四个按钮按 `GDI+ MeasureString` 实测宽度横向排列：`显示 / 隐藏`（随状态切换文字）、`关于`、`活动`（灰显占位）、`退出`；圆角白底 + 竖分隔线，绘制成 32bpp ARGB 位图后用 `render::PresentLayered` 显示。
- 定位：以 `GetWindowRect(petHwnd)` 为锚，工具栏居中贴在宠物**下方** `kBarGapPx` 处，并夹到主屏内。宠物每次移动（拖拽或自动游走）后 `Pet::onMoved` → `Tray::FollowPet()` → `PlaceBar()` 重新定位，所以工具栏显示期间始终贴在宠物下方，不会被拖走后留在原地。
- 关闭：点某个按钮执行动作后关闭、点宠物（`Pet::onLeftClick` → `Tray::HideBar`）关闭、再右键（再次触发 `ShowBar` 时 toggle）关闭。工具栏**不** `SetCapture`：它位于宠物下方且不与宠物矩形重叠，捕获鼠标会把本该给宠物的左键（拖动）抢走，造成“工具栏开着就拖不动、要点两次”的延迟；不捕获后鼠标消息自然落到各自窗口，点宠物那一次点击也不会被吞（`OnLButtonDown` 先关栏再照常 `SetCapture` 拖拽）。每次关闭都恢复宠物置顶。
- **根治覆盖**：工具栏打开期间 `TrayActions::pauseTopmost(true)` → `Pet::PauseTopmost(true)`，`Pet::OnTimer` 的每秒 `SetWindowPos(HWND_TOPMOST)` 被跳过；关闭时恢复并清零计数。两个窗口不再争 z-order，宠物不会盖住工具栏。
- 系统托盘：图标为 `idle/000.png` 缩放的 32×32 HICON（首帧解码失败回退 `IDI_APPLICATION`）。左键单击切换显示/隐藏，右键弹同一横向工具栏。`src/tray/` 用消息-only 窗口接收 `Shell_NotifyIcon` 回调，与 Pet/Bubble 解耦。
- 隐藏宠物时 `Bubble::SetSuspended(true)`：立即隐藏气泡并让气泡定时器停止，避免气泡孤零零冒出；重新显示时重置气泡倒计时。
- `显示 / 隐藏` 和 `退出` 的具体动作由 `WinMain` 以 lambda 注入 `TrayActions`，tray 模块不知道 `hwnd` 以外的宠物内部状态。
- 按 ESC：需要窗口有焦点；`WM_KEYDOWN` 收到 `VK_ESCAPE` 后销毁窗口（保持直接退出，不改）。启动时 `SetFocus`，点击时也会 `SetFocus`。
- `WM_DESTROY` → `Pet::DestroyResources()`：先删内存 DC，再删所有帧的 `HBITMAP`；气泡作为被拥有窗口一并销毁，`Bubble::OnDestroy` 删除自己的 DC/位图；消息循环退出后 `Tray::Shutdown()` 执行 `NIM_DELETE` 并释放工具栏位图/DC/窗口、托盘图标与消息窗，最后 `PostQuitMessage`。
- 托盘 `NIM_ADD` 失败时降级：右键退回旧行为（直接退出），宠物本身照常运行。见 [D16](06-decisions-and-risks.md#d16-横排工具栏--系统托盘托盘逻辑自成一模块) 与 [R21](06-decisions-and-risks.md#r21-托盘图标与横向工具栏无自动化验证)。

## 10. 点击穿透

宠物窗口始终是完整的 269×358 矩形，但 sprite 只占一部分：

- `Frame.AlphaMask` 在加载时按缩放后的帧生成，1 bit/像素；
- alpha >= `kHitAlpha`（16）的像素才可点击；
- `WM_NCHITTEST` 把屏幕坐标转成客户区坐标，调用 `Pet::HitTest`；不在 mask 内返回 `HTTRANSPARENT`；
- 没有 alpha 通道的帧、或 mask 构建失败时，整个矩形都可点击（保持旧行为）。

副作用：半透明边缘（alpha 1~15）也被当作透明；这不是逐像素命中，而是 1-bit 覆盖。

## 11. 参数配置

- 可选文件：`<exeDir><assetsDir>\pet.ini`；不存在则全部用 `src/core/config.h` 默认值。
- 支持整行 `#` / `;` 注释（不支持行尾注释）、`[section]`、`key = value`；重复键最后一个生效；写错的值回退默认。
- `[pet]` 和 `[bubble]` 覆盖全局参数；每个行为名（如 `[walking_up]`）可以覆盖该行为的 `min_ticks` / `max_ticks` / `weight` / `dx` / `dy`。
- 参数在启动时应用一次；改 `scale_factor` 必须重启，因为帧在加载时就已经缩放。
- 完整键表与语义见 [`04-assets-and-config.md`](04-assets-and-config.md#6-petini-键表)。

## 12. 当前没有实现

- 开机自启、安装包（系统托盘已实现）；
- 声音、喂食/培养/存档等产品化功能；
- 多显示器游走（坐标只使用主屏 `SM_CXSCREEN/SM_CYSCREEN`，拖拽也被夹在主屏）；
- 单实例限制、设置界面、热更新；
- 资源损坏/画布尺寸不一致的校验；
- 自动化 GUI 测试（8 个测试只覆盖纯逻辑）。

## 13. 失败与降级

| 情况 | 行为 | 用户可见结果 |
|---|---|---|
| 没有任何基底帧 | `Pet::Load` 返回 false | 错误框，列出资源目录和可用动作，程序退出 |
| 某条行为缺帧 | 该 clip 为空 | 启动日志 `loaded=0/120`；该行为被禁用，其余继续 |
| `bounds.txt` 缺失/行数不足 | 未赋值的帧 bounds 为 0 | 气泡和扫头退回完整窗口矩形 |
| `bounds.txt` 行非法 | 跳过该行，不推进帧索引 | 后续行整体前移一帧，bounds 错位；不报错 |
| `emoji/` 缺失/为空 | `Bubble::Init` 返回 false | 气泡禁用；其余功能正常 |
| emoji 文件存在但解码失败 | 气泡主体仍绘制 | 气泡里没有图（空白气泡），不报错 |
| GDI+ 初始化失败 | `GdiplusSession::ok()` 为 false | 错误框，退出 |
| `RegisterClassW` / `CreateWindowExW` 失败 | 返回错误码 | 错误框，退出 |
| 帧序列中间缺号 | `LoadFrames` 在第一个缺失处停止 | 只循环已加载的前缀，不报错 |
| 画布尺寸不一致 | 窗口尺寸取第一帧 | 后续帧可能被裁切/留白；没有校验 |
| `pet.ini` 值越界 | 直接使用，不校验 | 可能变成 1 px 帧、异常时长或奇怪判定带 |
