# 文档索引

本仓库全部文档的单一入口。按你的目的选一篇读即可。

## 我想…

| 目的 | 去这里 |
|---|---|
| 运行程序、看操作方式 | [`README.md`](../README.md) |
| 了解代码怎么工作（改代码前） | [`docs/architecture/README.md`](architecture/README.md) |
| 参与开发 / 提交改动 | [`CONTRIBUTING.md`](../CONTRIBUTING.md)、[`AGENTS.md`](../AGENTS.md) |
| 构建、跑测试、手工验收 | [`docs/architecture/05-build-and-tests.md`](architecture/05-build-and-tests.md) |
| 调 `pet.ini` 参数 | [`config/pet.example.ini`](../config/pet.example.ini)、[`04-assets-and-config.md`](architecture/04-assets-and-config.md) |
| 处理素材 / 抠图 / 修绿边 | [`docs/guides/asset-toolchain.md`](guides/asset-toolchain.md)、[`edge-repair.md`](guides/edge-repair.md) |
| 看懂 `output/` 里那 509 MB 是什么 | [`docs/guides/experiments.md`](guides/experiments.md) |
| 看某个历史时间点的工程状态 | [`docs/archives/README.md`](archives/README.md) |
| 看变更记录 | [`CHANGELOG.md`](../CHANGELOG.md) |

## 文档分层

| 目录 | 写什么 | 是否随代码更新 |
|---|---|---|
| `README.md` | 用户视角：怎么运行、按键、可见行为 | 是 |
| `AGENTS.md` | 协作约定与踩坑记录（构建、测试、资产命名） | 是 |
| `CONTRIBUTING.md` | 人/AI 协作流程：分支、提交、改代码、文档同步 | 是 |
| `CHANGELOG.md` | 值得记录的变更，按时间倒序 | 是 |
| `docs/architecture/` | **当前代码事实**、模块边界、数据契约、风险 | 是，改代码时同步 |
| `docs/guides/` | 方案对比、实测、工具链、实验产物索引 | 阶段性 |
| `docs/archives/` | 带日期的不可变工程快照 | 只增不改 |
| `config/` | 可直接复制的配置样例 | 按需 |

原则：**代码与文档冲突时以代码为准**。发现漂移先在 `docs/archives/` 留证据，
再决定回写哪一份。不要在多个目录复制同一段说明——用链接指向本索引里的权威篇目。

## 架构文档阅读顺序

1. [`01-features.md`](architecture/01-features.md) — 能做什么，怎么触发
2. [`02-runtime-architecture.md`](architecture/02-runtime-architecture.md) — 启动时序、状态机、数据流
3. [`03-modules-and-contracts.md`](architecture/03-modules-and-contracts.md) — 文件职责与接口契约
4. [`04-assets-and-config.md`](architecture/04-assets-and-config.md) — 帧 / bounds / emoji / pet.ini 格式
5. [`05-build-and-tests.md`](architecture/05-build-and-tests.md) — 构建、测试、验收
6. [`06-decisions-and-risks.md`](architecture/06-decisions-and-risks.md) — 设计决策与 20 条风险

## 指南目录

- [`reference-dsh-whale-musume.md`](guides/reference-dsh-whale-musume.md) — 与参考桌宠工程的逐层对照
- [`asset-packaging.md`](guides/asset-packaging.md) — PNG 图集 vs 动画 WebP 实测（结论：GDI+ 不支持 WebP）
- [`edge-repair.md`](guides/edge-repair.md) — 青色/绿色边缘环的诊断与修复，含「原视频必须保存」硬标准
- [`asset-toolchain.md`](guides/asset-toolchain.md) — 抠图工具、GIF 逐帧、MCP server 配置
- [`experiments.md`](guides/experiments.md) — `output/` 实验产物索引
