# 桌面宠物演示（Desktop Pet Demo）

基于 Win32 分层窗口（Layered Window）API + GDI+ 的透明悬浮桌面宠物演示程序。
读取指定动作目录下的 PNG 帧序列（`000.png`、`001.png` ...），以约 10 fps
循环播放，窗口透明、置顶、无边框。

## 运行

把 `pet_demo.exe` 放在任意目录，并保证其同级存在资源目录
`desktop_pet_image\<动作名>\000.png ...`（仓库里已预编译好
`pet_demo.exe`（x86-64）与 `pet_demo_x86.exe`（32 位））：

```
pet_demo.exe                   无参数 = 随机待机（在 idle / idle2 之间随机切换）
pet_demo.exe idle              强制单个待机动作，不做随机切换
pet_demo.exe walking_right     强制播放其它单个动作
pet_demo.exe idle desktop_pet_image_video   # 第二个参数指定资源目录（默认 desktop_pet_image）
```

**随机待机**：无参数启动时，程序把 `idle` 和 `idle2` 都加载为「待机动画池」，
每播放完一轮就随机切换到池中的另一个待机动画；显式传入第一个参数则固定该动作、
不做随机切换。

程序启动时会加载：待机动画池（1~2 个循环动画）+ 固定的 `clicked` 动作（被点击时
播放一次）+ `shy` 动作（鼠标扫过头顶时播放一次）+ 固定的 `drag` 动作（被拖拽时循环
播放）+ 固定的**四方向移动**动作 `walking_left` / `walking_right` / `walking_up` /
`walking_down`。各文件夹不存在或为空时打印 `... disabled` 并禁用对应行为（移动只有
部分方向时，只在这些方向里随机）。

**待机 ↔ 行走状态链**：宠物在待机状态停留一段随机时间（约 3~10 秒）后，随机选择
**上 / 下 / 左 / 右**四个方向之一移动；移动期间循环播放对应方向的动画，同时**窗口沿
该方向移动**（每 100ms 移动 5px）；走到**屏幕边缘**或移动时长到（约 1.5~4.5 秒）后
回到待机，如此往复。

> 上 / 下方向的两个源视频是**单向动作**（起飞 / 降落），首尾姿势不同，直接循环会看到
> "跳回开头再重播前几帧"。因此 **`walking_up` / `walking_down` 采用来回播放
> （boomerang：正放一遍再倒放一遍）**，从数学上保证无缝、不跳帧；左 / 右是真正的
> 循环步态，仍按正常循环播放。
>
> 竖向走动在**时长到 / 撞边缘**后不会立刻切回待机，而是**继续播到落地帧**（第 0 帧或
> 末帧）再回待机——否则宠物会在半空中突然变回站立姿势，看起来像瞬移。

**拖拽后等待**：把宠物拖到某个位置松手后，宠物进入约 **2~3 分钟**的"等待"阶段——
只随机切换 `idle` / `idle2` 待机动画、**不做移动**；等待计时结束后再恢复上面的
待机 ↔ 行走链。等待期间左键单击只会播放 `clicked` 反应并继续等待，不会打断计时；
再次拖拽会重新计时。

**扫头害羞**：鼠标在宠物**头部（窗口上方约 45%）**左右扫过（累计水平位移超过阈值）
时，播放一次 `shy` 害羞动画；有约 2.5 秒冷却，避免连续触发。

**气泡表情包**：程序会**不定期（约 20~60 秒）**在宠物头顶弹出一个气泡，随机显示
`desktop_pet_image\emoji\emoji_NN.png` 中的一张表情包，约 3 秒后消失。气泡是**独立的
分层窗口**，用 `WS_EX_TRANSPARENT` 实现**点击穿透**（不挡宠物的点击/拖拽），并持续跟随
宠物移动。`emoji\` 目录缺失或为空时打印 `no emoji stickers, bubble disabled` 并禁用该功能。
表情包素材用 `MCPS/fetch_emoji.py` 抓取（见文末）。

**启动失败会有弹窗**：GUI 程序没有控制台，找不到帧时不再静默退出，而是弹出错误框，
写明「资源目录、请求的动作、该目录下实际可用的动作」，方便排查（例如动作名拼错、
资源目录放错位置、素材缺失）。

操作方式：

- 左键单击：播放 `clicked` 反应动画一次，播完回到待机/等待
- 左键按住拖动：移动宠物，同时**循环播放 `drag` 挣扎动画**；松手后进入 2~3 分钟等待
  （位移超过系统拖拽阈值才进入拖动，否则视为点击）
- 鼠标在头顶左右扫过：播放 `shy` 害羞动画一次
- 右键单击：退出程序
- 按 ESC：退出程序

## 帧加载约定

程序以「可执行文件所在目录」为基准（通过 `GetModuleFileNameW` 获取，再截掉文件名），
拼接 `<资源目录>\<action>\NNN.png`（`NNN` 为 3 位零填充序号）。不依赖当前
工作目录，也不写死绝对路径。

- 从 `000.png` 开始连续加载
- 遇到第一个不存在的帧立即停止
- 只循环播放实际加载成功的帧
  （例如 `000.png`、`001.png`、`002.png` 存在而 `003.png` 缺失，则循环播放这 3 帧）
- 最多尝试 120 帧（`000` ~ `119`）
- 只存在 `000.png` 时程序正常运行，循环播放这一帧
- 启动时向 stdout 输出类似 `[pet] idle 'idle' loaded=30/120 frames`、
  `[pet] idle 'idle2' loaded=30/120 frames`、`[pet] random idle enabled across 2 animations`、
  `[pet] walking loaded: left=30 right=27 up=30 down=30 frames`、
  `[pet] shy loaded=30/120 frames`、`[pet] emoji loaded=6 stickers`、
  `[pet] bubble enabled (240x252, 6 stickers)`

> 素材说明：各动作的多帧序列现已合并进默认目录 `desktop_pet_image\`（`idle` 30 帧、
> `idle2` 30 帧、`walking_left` 30 帧、`walking_right` 27 帧、`walking_up` 30 帧、
> `walking_down` 30 帧、`clicked` 33 帧、`drag` 30 帧、`shy` 30 帧、`thinking` 30 帧）。
> 原 `desktop_pet_image_video\` 目录内容与之重复，可删除。

## 图片要求

- 必须为 PNG 格式，带 alpha 通道（RGBA），透明区域才能正确显示
- 窗口大小自动适配第一帧的像素尺寸，并按 `kScaleFactor = 0.70` 缩小显示
  （源图 384x384 → 屏幕上约 269x269）；嫌大/嫌小改 `src/config.h` 里的常量即可

## 去背景工具（RGB → RGBA）

`/home/zh180/MCPS/convert_rgba.py` 批量把带纯色背景的图片（PNG/JPG 等）转换为透明背景 RGBA PNG，
用于给桌宠素材抠掉白色（或任意纯色）背景：

```
python3 /home/zh180/MCPS/convert_rgba.py desktop_pet_image --recursive
```

- 默认 `--mode flood`：只抠掉与图片边缘连通的背景（保留角色身上与背景同色的部分，如白色皮肤）
- `--mode chroma`：抠掉所有与背景色接近的像素（更激进，肢体间缝隙也会抠掉，但可能误伤角色浅色部位）
- `--threshold 60`：背景色判定阈值（RGB 距离）；`--feather 25`：边缘羽化宽度
- `--backup`：覆盖前把原图复制为 `<文件>.orig`；`--delete-source`：转换后删除非 PNG 源文件
- 已透明的 RGBA 图自动跳过（幂等），补新帧后重跑即可
- 输出始终为同名 `.png`，与程序帧加载约定（`<action>/NNN.png`）一致

补帧流程：把新帧图放进 `desktop_pet_image/<action>/` → 跑一遍上面的命令 → 重开 demo。

### 作为 MCP 工具调用（推荐）

`/home/zh180/MCPS/rgba_convert_mcp.py` 把上面的抠图功能封装成 MCP server，已注册在
`~/.config/opencode/opencode.jsonc`（名称 `rgba-convert`）。重启 opencode 后，
直接让 AI 调用 `convert_to_rgba` 即可，无需手敲命令行。

（MCP 相关脚本统一放在 `/home/zh180/MCPS/`，`convert_rgba.py` 与
`rgba_convert_mcp.py` 同级，后者通过脚本目录自动 import 前者。）

工具参数（与 `convert_rgba.py` 一致）：`paths`（必填，文件/目录列表）、
`recursive`、`mode`、`threshold`、`feather`、`backup`、`delete_source`、`force`、
`dry_run`。返回逐文件日志 + 汇总行。

**GIF 支持**：输入也可以是**动图 GIF**。对多帧 GIF，会**逐帧抠图**并输出成
**PNG 帧序列**到一个同名同级文件夹（去扩展名），例如
`视频文件/pet_parachute_down.new.gif` → `视频文件/pet_parachute_down.new/000.png … 023.png`，
保留完整 alpha（GIF 本身只有 1-bit 透明，故不做透明 GIF，改用 PNG 帧）。已含透明通道的
GIF 会被跳过（可用 `force` 强制处理）。

> 实现要点：MCP stdio 用 stdout 传输 JSON-RPC，而抠图函数会 print 到 stdout，
> 因此 server 用 `contextlib.redirect_stdout` 把输出重定向到缓冲区再作为工具结果返回，
> 避免污染协议。

## 代码结构

源码在 `src/`，按职责分模块：

| 文件 | 职责 |
|------|------|
| `src/main.cpp` | `WinMain` 装配：加载资源 → 建宠物窗/气泡窗 → 消息循环 |
| `src/config.h` | 所有可调常量（窗口 / 行为 / 气泡） |
| `src/util.*` | 路径、报错弹窗、UTF-8 转换、随机数 |
| `src/assets.*` | `Frame` 与帧加载（`LoadFrames` / emoji 列表） |
| `src/render.*` | GDI+ 初始化 + 分层窗口绘制 `render::PresentLayered` |
| `src/pet.*` | 宠物：动画池 + 待机/行走/点击/拖拽/害羞状态机（`struct Pet`） |
| `src/bubble.*` | 气泡表情包窗口（`struct Bubble`，独立定时器 + 点击穿透） |
| `bubble_logic.h` · `walk_logic.h` | 纯逻辑（不含 Win32），配 `tests/` 单测 |

新增功能（例如「培养」）建议加一个 `src/<feature>.*` 模块，尽量只依赖 `config.h` / `util.h`，
不要直接改 `pet.*`。

## 编译

### 方法一：MSVC 一行命令（推荐）

打开「x64 Native Tools Command Prompt for VS 2022」（开始菜单 → Visual Studio 2022 →
x64 Native Tools Command Prompt for VS 2022），进入本目录后执行：

```
build.bat
```

或直接手动执行等价命令：

```
cl /nologo /EHsc /std:c++17 /O2 src\*.cpp /I src gdiplus.lib user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS /OUT:pet_demo.exe
```

`build.bat` 优先尝试上面的 `cl` 命令；若当前环境没有 `cl`（不在开发者命令行里），
会自动回退到 CMake + Visual Studio 生成器编译。

如需 32 位（x86），请改用「x86 Native Tools Command Prompt for VS 2022」执行同样的
`cl` 命令（或 CMake 生成器用 `-A Win32`）。

### 方法二：CMake（MSVC）

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物位于 `build\Release\pet_demo.exe`。

### 方法三：MinGW-w64

64 位（x86-64）：

```
x86_64-w64-mingw32-g++ -std=c++17 -O2 -mwindows src/*.cpp -lgdiplus -luser32 -lgdi32 -o pet_demo.exe
```

32 位（x86）：

```
i686-w64-mingw32-g++ -std=c++17 -O2 -mwindows src/*.cpp -lgdiplus -luser32 -lgdi32 -o pet_demo.exe
```

或使用 CMake + MinGW Makefiles：

```
cmake -S . -B build-mingw -G "MinGW Makefiles"
cmake --build build-mingw
```

注意：`WIN32` 子系统（`/SUBSYSTEM:WINDOWS` / `-mwindows`）保证程序运行时不弹出控制台窗口。

## 技术要点

- 全部使用宽字符（`W` 后缀）Win32 API；入口为 `WinMain`，通过 `__argc/__argv`
  取得命令行参数，并用 `MultiByteToWideChar(CP_UTF8, ...)` 转换为宽字符串
- 窗口样式：`WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`，窗口类样式
  `CS_HREDRAW | CS_VREDRAW`，普通窗口过程（非对话框）
- GDI+ 加载 PNG：`GdiplusStartup` + `Gdiplus::Bitmap`（宽字符路径）+
  `GetHBITMAP(Gdiplus::Color(0,0,0,0), &hbmp)` + `GdiplusShutdown`（RAII 封装）
- 合成：`CreateCompatibleDC` + `SelectObject(HBITMAP)` +
  `UpdateLayeredWindow(..., ULW_ALPHA)`；`BLENDFUNCTION` 对 RGBA 帧用
  `AlphaFormat = AC_SRC_ALPHA`（逐像素透明），对 RGB 帧用 `AlphaFormat = 0` +
  `SourceConstantAlpha = 255`（整帧不透明）
- 动画：`SetTimer(hwnd, TIMER_ID, 100, nullptr)` 约 10 fps，`WM_TIMER` 中推进帧序号
  并重新调用 `UpdateLayeredWindow`；基底动作循环播放，`clicked` 动作播完一次自动
  切回基底（断点续播），`drag` 动作在拖拽期间循环、松手切回基底
- 点击/拖拽：`WM_LBUTTONDOWN` 时 `SetCapture` 记录按下位置；`WM_MOUSEMOVE` 中位移
  超过 `SM_CXDRAG/SM_CYDRAG` 判定为拖动（`SetWindowPos` 跟随光标，抓取点保持在
  光标下），否则 `WM_LBUTTONUP` 视为单击 → 触发 `clicked` 反应
- 缩放：加载时用 GDI+ `Graphics::DrawImage`（`InterpolationModeHighQualityBicubic`）
  按 `kScaleFactor=0.70` 缩放；缩放保持源的像素格式（RGB 保持不透明、RGBA 保持
  逐像素 alpha）
- 退出：`WM_RBUTTONUP` 与 `WM_KEYDOWN`（`VK_ESCAPE`）→ `DestroyWindow` → `PostQuitMessage`
- 所有帧在启动时一次性预加载为 `std::vector<Frame>`（`Frame` 含 `HBITMAP` 与
  `hasAlpha` 标记），基底与 `clicked` 两个动作分别存放，退出时全部
  `DeleteObject`；内存 DC 也在退出时 `DeleteDC`，无 GDI 泄漏
