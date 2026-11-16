# 架构文档

本目录记录 **当前代码实际如何工作**，以及继续开发时必须遵守的契约。它面向要改代码的人（或 AI），不是用户手册。

> 基线：分支 `AIvideo` @ `d88b53e`（2026-11-16）；当前代码已修复 R1/R2。
> 验证：MinGW-w64 g++ 16.2.0 构建通过；8/8 测试通过；启动日志已抓取。
> 历史快照：[`../archives/2026-11-16-baseline.md`](../archives/2026-11-16-baseline.md)

## 和现有文档的分工

| 位置 | 写什么 | 是否随代码更新 |
|---|---|---|
| `README.md` | 用户视角：怎么运行、按键、可见行为 | 已按当前代码同步 |
| `AGENTS.md` | 协作约定：构建、测试、资产命名、踩坑 | 是 |
| `docs/guides/` | 方案对比与实测：图集 vs WebP、抠图修边、参考工程 | 阶段性 |
| `docs/architecture/` | **当前代码事实**、模块边界、数据契约、风险 | **是，改代码时同步** |
| `docs/archives/` | 带日期的不可变快照 | 只增不改 |

原则：**代码与文档冲突时，以代码为准**。发现漂移先记录到归档，再决定是否回写 `README.md`。

## 先读哪篇

| 顺序 | 文档 | 回答的问题 |
|---:|---|---|
| 1 | [`01-features.md`](01-features.md) | 这个程序能做什么？每个功能怎么触发、依赖什么资产？ |
| 2 | [`02-runtime-architecture.md`](02-runtime-architecture.md) | 启动后发生了什么？两个窗口、两个定时器、状态机怎么协作？ |
| 3 | [`03-modules-and-contracts.md`](03-modules-and-contracts.md) | 每个文件负责什么？关键结构体和接口的契约是什么？ |
| 4 | [`04-assets-and-config.md`](04-assets-and-config.md) | 帧、`bounds.txt`、emoji、`pet.ini` 的格式与加载规则 |
| 5 | [`05-build-and-tests.md`](05-build-and-tests.md) | 怎么构建、怎么跑测试、怎么手工验收、常见失败 |
| 6 | [`06-decisions-and-risks.md`](06-decisions-and-risks.md) | 为什么这样设计？哪些地方会坏、哪些是已知债务？ |

## 一分钟事实

| 项 | 当前值 |
|---|---|
| 平台 / 语言 | Windows，C++17，Win32 + GDI+，无第三方框架 |
| 入口 | `src/main.cpp` 的 `WinMain`，只做装配 |
| 宠物窗口 | `WS_EX_LAYERED \| WS_EX_TOPMOST \| WS_EX_TOOLWINDOW`，`WS_POPUP`，269×358 |
| 画布 / 缩放 | 源 PNG 384×512 RGBA，按 `kScaleFactor = 0.70` 缩放 |
| 帧率 | 宠物定时器 100 ms（约 10 fps） |
| 行为 | 7 条注册行为 + 1 个 idle 基底池；注册表在 `src/pet/behaviors.cpp` |
| 气泡 | 独立分层窗口，自己的 100 ms 定时器，点击穿透，跟随宠物 |
| 线程 | 单 UI 线程；没有工作线程、锁或异步 IO |
| 构建 | `build.bat` 依次尝试 `cl` → `g++` → CMake |
| 测试 | 8 个纯逻辑断言程序，不依赖 `<windows.h>` |

## 维护规则

1. **改行为、渲染、资产契约或配置键时，同一提交更新对应架构文档。**
2. 文档里用**符号名**（如 `Pet::OnTimer`、`pet_behavior::Playback`）而不是容易漂移的行号。
3. 不在本目录复制大段 `README.md` 行为说明；需要时链接过去。
4. 历史快照、一次性排障记录、实测数据放 `docs/archives/`，不要回写旧快照。
5. 新增“硬约束”（例如“不能删源视频”）时，先写进 `AGENTS.md`，再在架构文档里引用。
6. 发现“文档说 A、代码做 B”时，不要静默改文档：在归档里保留证据，再单独修代码或修文档。

## 最近修复

- **R1：`bounds.txt` 缩放**。`ScaleSubjectBounds` 在 `Pet::PresentFrame` 中把源画布坐标换算到窗口并夹取；新增 `tests/bounds_scale_test.cpp`。详见 [`04-assets-and-config.md`](04-assets-and-config.md#缩放换算r1已修复)。
- **R2：README 同步**。基底池、竖向播放、头部带、帧数、气泡尺寸、emoji 命名、启动日志、`pet.ini` 注释规则与开发文档索引已按代码/实测回写；历史漂移保留在 baseline。

## 当前已知问题（摘要）

- **`idle2` / `busy` / `peek` 有帧但未注册**：当前只有 `idle` 进入基底池；其余三个动作不会被播放。详见 [`06-decisions-and-risks.md`](06-decisions-and-risks.md#r3-未注册的动作)。
- **`core/settings.cpp` 反向依赖 `pet/behaviors.h`**：能工作，但破坏了“core 不依赖领域层”的边界。详见 [`06-decisions-and-risks.md`](06-decisions-and-risks.md#r4-core--pet-反向依赖)。
