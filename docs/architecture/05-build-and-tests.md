# 构建与测试

## 1. 环境

- Windows（当前工作副本 `E:\pet_demo`）。
- C++17。
- 任选一条工具链：
  - MSVC（`cl.exe`，VS 2022）；
  - MinGW-w64 g++（当前在 `PATH` 上是 `E:\mingw64\bin\g++.exe`，16.2.0）；
  - CMake 3.16+。
- 链接库：`gdiplus`、`user32`、`gdi32`。

## 2. 构建

### 推荐：一键脚本

```powershell
cmd /c build.bat
```

`build.bat` 的路径顺序：

1. `cl`（MSVC 一行命令）；
2. `g++`（把源文件列表写进 `build\sources.txt`，用 `@` 响应文件传给 g++）；
3. CMake（Visual Studio 生成器 → MinGW Makefiles）。

产物：

- `cl` / `g++`：仓库根目录 `pet_demo.exe`；
- CMake VS：`build\Release\pet_demo.exe`；
- CMake MinGW：`build-mingw\pet_demo.exe`。

### 先关闭正在运行的宠物

Windows 会锁住 exe；链接时会报：

```
cannot open output file pet_demo.exe: Permission denied
```

先执行：

```powershell
taskkill /IM pet_demo.exe /F
```

### MSVC 手动命令

在 “x64 Native Tools Command Prompt for VS 2022” 里：

```bat
cl /nologo /EHsc /std:c++17 /O2 src\*.cpp src\core\*.cpp src\pet\*.cpp src\bubble\*.cpp /I src gdiplus.lib user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS /OUT:pet_demo.exe
```

32 位改用 x86 Native Tools Prompt。

### CMake

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

或：

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles"
cmake --build build-mingw
```

### MinGW 手动命令

```powershell
g++ -std=c++17 -O2 -mwindows -Isrc src/main.cpp src/core/util.cpp src/core/assets.cpp src/core/bounds.cpp src/core/render.cpp src/core/settings.cpp src/pet/behaviors.cpp src/pet/pet.cpp src/bubble/bubble.cpp -lgdiplus -luser32 -lgdi32 -o pet_demo.exe
```

注意：

- `cmd.exe` **不展开通配符**，`g++` 自己也不展开；不要写成 `src/*.cpp`。
- `build.bat` 的 `@build\sources.txt` 里必须使用**正斜杠**；g++ 会把 `@file` 里的 `\` 当转义，`E:\pet_demo\src\main.cpp` 会变成 `E:pet_demosrcmain.cpp`。
- `-mwindows` / `/SUBSYSTEM:WINDOWS` 保证运行时不弹控制台。

### exe 与资源的位置

`pet_demo.exe` 必须和 `desktop_pet_image\` **同级**；资源路径从 exe 目录解析。把 exe 单独拷走会导致找不到帧并弹错误框。

## 3. 测试

7 个纯逻辑断言程序，不依赖 `<windows.h>`，可在任意主机用 g++ 编译：

```powershell
foreach ($t in 'alpha_mask_test','behavior_test','bubble_logic_test','drag_logic_test',
              'inifile_test','sweep_logic_test','walk_logic_test') {
  g++ -std=c++17 -Wall -Wextra -O2 "tests\$t.cpp" -o "build\$t.exe"
  & "build\$t.exe"
}
```

每个程序成功时打印 `<name>: all tests passed` 并返回 0；失败时 `assert` 终止（非 0）。

### 覆盖矩阵

| 测试 | 覆盖的行为 | 不覆盖 |
|---|---|---|
| `alpha_mask_test` | 空掩码、Set/Test、越界、Reset 清零、33 位列跨字边界、0×0 | alpha 阈值生成（`BuildHitMask`）、Win32 命中测试 |
| `behavior_test` | 只选 eligible、0 权重排除、1:3 加权比例、无候选返回 -1、`FindByTrigger` | `Pet::PickNextAuto` 的帧/等待过滤、实际播放 |
| `bubble_logic_test` | 上方居中、屏幕左右夹取、上方放不下转右/左、Y 夹取、极矮屏转下方、headPercent 居中、间隔范围/退化区间 | `RenderBubble` 绘制、真实窗口/DPI、emoji 解码 |
| `drag_logic_test` | 界内不动、负向归零、越右/下贴边、精确贴边、窗口大于屏幕 | 真实鼠标阈值、`SetWindowPos`、多显示器 |
| `inifile_test` | section/key、整行注释、trim、重复键最后生效、CRLF、缺尾换行、整数/浮点回退 | **行尾注释不支持**；`settings.cpp` 的键映射/范围校验 |
| `sweep_logic_test` | 单次不触发、一次换向算 1 pass、两次触发并清空、长扫不换向不触发、位移不足、dx=0、Idle 超时 | 头部带几何、真实鼠标事件、冷却/抢占 |
| `walk_logic_test` | 末帧/首帧判断、空 clip、size=2 | `FullCycle`/`Boomerang` 的实际推进、撞边 |

**测试盲区**：`Pet::OnTimer`、`Pet::OnMouseMove`、`Pet::HitTest`、`Bubble` 窗口、GDI+ 解码/缩放、`bounds.txt` 缩放都没有单元测试；这些只能构建后手工验证，或后续补集成测试。

### 新增测试

1. 逻辑放在纯头文件（不 include `<windows.h>`）；
2. 新建 `tests/<name>_test.cpp`，用 `assert` 或显式返回码；
3. 用上面的循环命令编译运行，保持 `-Wall -Wextra` 零警告；
4. 把测试名加入 `AGENTS.md` 的循环列表，保证以后不会漏跑。

## 4. 本机基线（2026-11-16）

实际执行的命令与结果：

```text
$ where.exe cl
（无输出：cl 不在 PATH）

$ where.exe g++
E:\mingw64\bin\g++.exe

$ g++ --version
g++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r1) 16.2.0

$ cmd /c build.bat
[pet] build OK: pet_demo.exe via g++

$ 7 个测试
alpha_mask: all tests passed
behavior: all tests passed
bubble_logic: all tests passed
drag_logic: all tests passed
inifile: all tests passed
sweep_logic: all tests passed
walk_logic: all tests passed
```

产物：

| 项 | 值 |
|---|---|
| `pet_demo.exe` 大小 | 266,913 bytes |
| 构建时间 | 2026-11-16 22:37:43 |
| 构建路径 | g++（`build.bat` 的第二个分支） |
| `pet.ini` | 不存在，全部默认值 |
| emoji | 6 张，本地存在（gitignored） |

启动验证：实际运行 3 秒，找到窗口类 `DesktopPetWindowClass`、标题 `Desktop Pet`，并用 `WM_CLOSE` 正常关闭；stdout 日志见 [`../archives/2026-11-16-baseline.md`](../archives/2026-11-16-baseline.md#启动日志实测)。

未在本机复跑：MSVC `cl` 路径、CMake 两条生成器路径；它们由 `build.bat`/`CMakeLists.txt` 提供，但本次 g++ 先成功，脚本没有继续尝试。

## 5. 手工验收清单

构建和 7 个测试只能证明纯逻辑与链接通过；以下必须运行程序确认：

| 项 | 预期 |
|---|---|
| 启动 | 无控制台窗口；宠物出现在主屏中央；无错误框 |
| 尺寸 | 269×358（150% DPI 下也是物理像素 269×358） |
| 待机 | `idle` 单帧静止；约 3~10 秒后开始自动移动 |
| 四向移动 | 左右走动；上=飞起回摆；下=俯冲落地；撞主屏边缘后完成安全帧再待机 |
| 左键单击 | 播放 `clicked` 一次，回到待机/等待 |
| 拖拽 | 超过系统拖拽阈值后跟随光标；播放 `drag`；不能拖出主屏 |
| 拖后等待 | 松手后 2~3 分钟不自动移动；单击仍可反应；再拖重置 |
| 扫头 | 在头顶左右各扫一次（累计 >=50 px/段）触发 `shy`；冷却约 2.5 秒 |
| 气泡 | 20~60 秒内弹出，约 3 秒消失；跟随宠物；点击穿透 |
| 点击穿透 | 在透明留白上左/右键，事件落到桌面/下层窗口；右键不退出 |
| 右键退出 | 在宠物像素上右键退出 |
| ESC | 点击宠物取得焦点后按 ESC 退出 |
| 缺资源 | 临时改名 `idle` 后启动，应弹错误框并列出可用动作 |
| 缺 emoji | 临时改名 `emoji` 后启动，应日志 `bubble disabled`，其余正常 |

> 当前已知的 `bounds.txt` 缩放缺陷会影响气泡位置和扫头判定带；验收时如果发现气泡偏右/侧面气泡离身体远，先看 [`06-decisions-and-risks.md`](06-decisions-and-risks.md#r1-bounds-未缩放p1)。

## 6. 常见失败

| 现象 | 原因 | 处理 |
|---|---|---|
| `cannot open output file pet_demo.exe: Permission denied` | 宠物还在运行，exe 被锁 | `taskkill /IM pet_demo.exe /F` 后重试 |
| g++ 报 `Invalid argument` / 找不到 `src/*.cpp` | 在 cmd 里用了通配符 | 用 `build.bat` 或逐个列源文件 |
| g++ 报路径乱码/找不到文件 | `@file` 里用了反斜杠 | 改成正斜杠 |
| 启动弹“No frames found” | exe 不在资源目录旁、动作名拼错、资源目录名错 | 看错误框里的资源路径和 available actions |
| 只有部分动作 | 某目录缺 `000.png` 或中间缺号 | 看启动日志的 `loaded=N/120`；补帧或删多余帧 |
| 运行时没有气泡 | `emoji/` 缺失/为空，或没有 `emoji_01.png` | 放图或跑 `tools/make_emoji.py`；看日志 |
| 宠物有白色矩形边框 | 没做 DPI 感知，窗口被系统虚拟化 | 确认 `SetProcessDpiAwarenessContext` 生效；不要删 DPI 代码 |
| `pet.ini` 改了没生效 | 行尾注释导致整段解析失败，或值非法 | 注释单独一行；先用默认值验证 |
| 动作帧显示为空白 | PNG 是 WebP 扩展名/内容，或没有 alpha | GDI+ 不支持 WebP；换 PNG |
| CMake 找不到 gdiplus | 工具链/生成器不匹配 | 用 `build.bat` 的 cl/g++ 路径，或检查 CMake 生成器 |
