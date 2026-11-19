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

### Changed
- `README.md` 精简为「项目导航 + 用户手册」；工具链、编译细则、技术要点下沉到 `docs/`。
- 修正 README 两处自相矛盾：预编译 exe 随仓库分发的说法、资产帧数的过期标注。

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
