# 桌面宠物演示（Desktop Pet Demo）

基于 Win32 分层窗口 + GDI+ 的透明悬浮桌面宠物。读取 `desktop_pet_image/<动作>/`
下的 PNG 帧序列（`000.png`、`001.png` …），约 10 fps 循环播放，窗口透明、置顶、无边框。

> 想参与开发？先看 [`CONTRIBUTING.md`](CONTRIBUTING.md) 和
> [`docs/index.md`](docs/index.md)（全仓库文档索引）。

## 运行

把 `pet_demo.exe` 放在任意目录，并保证同级存在资源目录 `desktop_pet_image/`。
`build.bat` 构建后会在仓库根生成 exe；仓库不随附预编译 exe。

```
pet_demo.exe                   无参数 = 加载基底待机池（当前只有 idle）
pet_demo.exe idle              强制单个待机动作，不做随机切换
pet_demo.exe walking_right     强制播放其它单个动作
pet_demo.exe idle <资源目录>   第二个参数指定资源目录（默认 desktop_pet_image）
```

资源路径从**可执行文件所在目录**解析，不依赖当前工作目录；exe 必须和
`desktop_pet_image/` 同级，否则弹错误框（会列出资源目录和实际可用的动作）。

### 当前行为

- **待机**：`idle` 单帧静止；停留约 3~10 秒后随机选一个方向移动。
- **四向移动**：左右是走路循环；`walking_up` 素材是**飞起**（正放再倒放），
  `walking_down` 是**俯冲**（必须播到落地末帧）。窗口每 tick 移动 5 px，
  撞主屏边缘或时长到后回到待机。
- **拖后等待**：拖拽松手后约 2~3 分钟只待机、不自动移动；单击仍有反应但不重置计时。
- **扫头害羞**：鼠标在头顶左右扫过（两次换向）播放一次 `shy`，约 2.5 秒冷却。
- **气泡表情**：每 20~60 秒在头顶弹出一张 `emoji/` 里的表情图，约 3 秒消失；
  独立窗口、点击穿透、跟随宠物。`emoji/` 缺失则只禁用气泡。
- **右键工具栏与托盘**：在宠物像素上右键，在宠物下方弹出一条横向工具栏
  （显示/隐藏、关于、活动占位、退出），不再误触退出；点宠物或按钮或再右键关闭。
  工具栏打开时宠物暂停每秒置顶，避免把自己盖在工具栏上；点宠物拖动会先关工具栏、
  拖拽照常生效，不会吞掉这次点击。若工具栏仍开着（如右键后继续拖），它会随宠物
  移动而重新定位，不会留在原地。托盘图标左键切换
  显示/隐藏、右键弹同一工具栏，图标取 `idle/000.png`。`NIM_ADD` 失败会退回
  右键直接退出。
- **点击穿透**：只有画到宠物身上的像素响应鼠标，透明留白处点击/右键会落到下层窗口。

### 操作方式

| 操作 | 效果 |
|---|---|
| 左键单击 | 播放一次 `clicked` 反应 |
| 左键按住拖动 | 跟随光标，循环 `drag` 挣扎动画；松手进入等待 |
| 鼠标在头顶左右扫 | 播放一次 `shy` |
| 在宠物像素上右键 | 在宠物下方弹出横向工具栏：显示/隐藏、关于、活动（占位）、退出；再右键或点别处关闭 |
| 工具栏左键 | 点击按钮执行对应动作；点宠物或再右键关闭工具栏 |
| 托盘图标左键 | 显示/隐藏宠物（隐藏时气泡一并暂停） |
| 托盘图标右键 | 弹出同一横向工具栏 |
| ESC | 退出程序（需窗口有焦点） |

## 帧与素材约定

- 帧为 **PNG + alpha 通道（RGBA）**；动作帧命名 `NNN.png`（3 位），从 `000` 起连续，
  遇第一个缺号停止，最多 120 帧。GDI+ **不解码 WebP**。
- 所有动作统一画布 384×512；窗口按 `scale_factor=0.70` 缩放 → 269×358。
- 每个动作可带 `bounds.txt`（每行一帧的 `x y w h`，源画布坐标），供气泡定位与扫头判定用。
- `idle2` / `busy` / `peek` 有帧但**未注册**，当前不会播放（见
  [`06-decisions-and-risks.md` R3](docs/architecture/06-decisions-and-risks.md#r3-未注册的动作)）。
- 实测帧数：`idle` 1、`walking_left/right/down` 30、`walking_up` 27、
  `clicked` 30、`drag` 30、`shy` 40。

补帧、抠图、GIF 逐帧与 MCP 工具见 [`docs/guides/asset-toolchain.md`](docs/guides/asset-toolchain.md)；
修绿边见 [`edge-repair.md`](docs/guides/edge-repair.md)。
**硬规则：永不删除动作的源视频 / 源帧导出**——`NNN.png` 是有损派生产物。

## 参数配置（`pet.ini`，可选）

把 [`config/pet.example.ini`](config/pet.example.ini) 复制到
`desktop_pet_image/pet.ini` 即可不重编译调参数；文件不存在则用 `src/core/config.h`
默认值。可覆盖待机/等待时长、头部带、扫头阈值、缩放、气泡尺寸与间隔，
以及每个行为 section 的 `min_ticks` / `max_ticks` / `weight` / `dx` / `dy`。

> 注释必须单独成行（不支持行尾 `;` 注释）；值非法会静默回退默认；`scale_factor` 改后需重启。
> 完整键表见 [`04-assets-and-config.md`](docs/architecture/04-assets-and-config.md)。

## 代码结构

| 位置 | 职责 |
|---|---|
| `src/main.cpp` | `WinMain` 装配：资源 → 建宠物窗/气泡窗 → 消息循环 |
| `src/core/` | 基础功能：config、settings + inifile、bounds、gdi、util、assets、render |
| `src/pet/` | 桌宠本体：`behavior.h` + `behaviors.cpp`（行为注册表）、`pet.*`、纯逻辑 walk/drag/sweep |
| `src/bubble/` | 发表情包：`bubble.*`（独立窗口与定时器）、`bubble_logic.h` |
| `src/tray/` | 系统托盘与横向工具栏：`tray.*`（托盘图标、自绘横排工具栏、点击命中），动作由 `WinMain` 回调注入 |

加行为只需在 `MutableBehaviors()` 追加一条 `Spec` 并放好帧目录，`pet.cpp` 不用改。
分层边界与接口契约见 [`03-modules-and-contracts.md`](docs/architecture/03-modules-and-contracts.md)。

## 编译与测试

```powershell
taskkill /IM pet_demo.exe /F    # 关掉运行中的宠物，否则 exe 被锁
cmd /c build.bat                # 依次尝试 cl → g++ → CMake
```

也可直接用 CMake / MinGW / MSVC 命令，细则与 8 个测试的跑法见
[`docs/architecture/05-build-and-tests.md`](docs/architecture/05-build-and-tests.md)。
行为/视觉需在 Windows 上实际运行验证；纯逻辑有 8 个断言测试。

## 文档导航

完整索引见 [`docs/index.md`](docs/index.md)。常用入口：

| 文档 | 内容 |
|---|---|
| [`AGENTS.md`](AGENTS.md) | 协作约定与踩坑（构建、测试、资产命名） |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | 分支、提交、改代码与文档同步流程 |
| [`CHANGELOG.md`](CHANGELOG.md) | 变更记录 |
| [`docs/architecture/README.md`](docs/architecture/README.md) | 当前代码事实（改代码前先读） |
| [`docs/guides/README.md`](docs/guides/README.md) | 方案对比、实测、工具链、实验索引 |
| [`docs/archives/README.md`](docs/archives/README.md) | 带日期的不可变工程快照 |

`output/` 下约 509 MB 图像生成 / Spine 拆件实验产物的说明见
[`docs/guides/experiments.md`](docs/guides/experiments.md)。
