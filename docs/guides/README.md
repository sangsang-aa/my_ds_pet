# Guides

面向"怎么继续把这个工程做下去"的决策记录。与 `AGENTS.md`（给 AI 的工作约定）和
`README.md`（给用户的行为说明）分工不同：这里放**对照、取舍、实测数据**。
全仓库文档入口见 [`../index.md`](../index.md)。

| 文档 | 内容 |
|---|---|
| [`reference-dsh-whale-musume.md`](reference-dsh-whale-musume.md) | 与参考桌宠工程的逐层对照：可借鉴的 6 条工程纪律、本工程反而更强的地方、借鉴优先级 |
| [`asset-packaging.md`](asset-packaging.md) | 帧序列要不要打包：PNG 图集 vs 动画 WebP 的实测对比与迁移方案 |
| [`edge-repair.md`](edge-repair.md) | 抠图残留"绿边"的实测诊断（其实是青色环）、工具选型、`tools/fix_edges.py` 用法、**原视频必须保存**的硬标准 |
| [`asset-toolchain.md`](asset-toolchain.md) | RGB→RGBA 抠图参数、GIF 逐帧、`rgba-convert` MCP 配置（从 README 下沉） |
| [`experiments.md`](experiments.md) | `output/` 下生图关键帧与 Spine 拆件 5 版的索引 |

尚未落地的事项都写在各自文档的"怎么落到本工程"小节里；已落地的只有
`tools/fix_edges.py`（见 `edge-repair.md`），其余**没有对应的代码改动**。
`asset-toolchain.md` 与 `experiments.md` 是说明性文档，不对应代码变更。
