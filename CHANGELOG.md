# Changelog

本项目所有值得注意的变更记录。格式参考 [Keep a Changelog](https://keepachangelog.com/)，
按时间倒序；每条尽量对应一个可复现的 commit。

工程快照（构建命令、测试输出、启动日志、文档漂移对照）不放这里，见
[`docs/archives/`](docs/archives/README.md)。

## [Unreleased]

### Added
- 工程规范入口：`CHANGELOG.md`、`CONTRIBUTING.md`、`config/pet.example.ini`。
- `docs/index.md`：全仓库文档总索引。
- `docs/guides/asset-toolchain.md`：抠图工具链与 MCP 配置（从 `README.md` 下沉）。
- `docs/guides/experiments.md`：`output/` 下图像生成与 Spine 拆件实验产物索引。
- 系统托盘与右键横向工具栏（`src/tray/`）：托盘图标取 `idle/000.png`，左键显示/隐藏、右键在宠物下方弹一条自绘横排按钮栏（显示隐藏/关于/活动占位/退出）；`Pet::onRightClick` 与 `PauseTopmost`、Bubble 的 `SetSuspended` 作为回调接出，显示/隐藏/关于/退出由 `WinMain` 注入。退出执行 `NIM_DELETE`，不留残影。

### Changed
- `README.md` 精简为「项目导航 + 用户手册」；工具链、编译细则、技术要点下沉到 `docs/`。
- 修正 README 两处自相矛盾：预编译 exe 随仓库分发的说法、资产帧数的过期标注。
- 修复右键界面被人物覆盖：右键菜单改为自绘横排工具栏并放到宠物下方；工具栏打开期间 `Pet::PauseTopmost(true)` 暂停每秒置顶重置，关闭时恢复，根治 z-order 争抢。
- 修复工具栏打开时无法拖动宠物（输入延迟）：工具栏不再 `SetCapture` 抢鼠标，鼠标消息自然分发到宠物/工具栏；点宠物时经 `Pet::onLeftClick` 先关工具栏、本次左键照常用于拖动，不吞点击。
- 修复拖拽后工具栏留在原地：新增 `Pet::onMoved` → `Tray::FollowPet()`，宠物移动（拖拽/游走）时工具栏重新定位，显示期间始终贴在宠物下方。
- 右键从直接退出改为弹工具栏；ESC 仍直接退出。三条构建路径补 `shell32` 链接，
  CMake/`build.bat` 的 cl 路径补 `src/tray/tray.cpp`。
- 决策 D16 与风险 R21 记入 `docs/architecture/06-decisions-and-risks.md`。

## [2026-11-16] — bounds 缩放修复与架构文档

对应 commits：`58881d2` → `be9d2a0` → `a55512f`（分支 `AIvideo`）。

### Fixed
- **R1：`bounds.txt` 未按 `scale_factor` 缩放**。新增纯函数 `ScaleSubjectBounds`，
  `Pet::PresentFrame` 把源画布坐标换算并夹取到窗口后再发布 `SubjectBounds`；
  气泡偏右、侧面气泡离身远、扫头带下移一并修复。回归：`tests/bounds_scale_test.cpp`。
- **R2：README 文档漂移**。按代码与实测回写基底池、竖向播放、头部带、帧数、
  气泡尺寸、emoji 命名、启动日志、`pet.ini` 注释规则。

### Added
- 架构文档 `docs/architecture/`（6 篇 + README）与归档 `docs/archives/`。
- 第 8 个测试 `bounds_scale_test`，8/8 通过。

### Notes
- 基线快照 `d88b53e` 与修复归档 `58881d2` 见 [`docs/archives/`](docs/archives/README.md)。
- 15 条决策与 20 条风险见 [`docs/architecture/06-decisions-and-risks.md`](docs/architecture/06-decisions-and-risks.md)。

## [2026-11-05] — 资产处理与行为整理

### Fixed
- 修复各动作的绿边/溢色：全面 despill、清除残留色 casts、重建 bounds。
- `clicked` 移除烘焙在地面的阴影。

### Changed
- `busy` 动作帧替换为悬浮笔记本版本。

## [2026-09-12] — 项目初始化

### Added
- 初始提交；Win32 分层窗口 + GDI+ 桌宠基本功能。
- 随机待机切换、基础帧加载与播放。
