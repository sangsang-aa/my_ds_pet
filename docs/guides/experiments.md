# `output/` 实验产物索引

`output/`（约 509 MB，gitignore）存放**没进入桌宠运行时的探索产物**：AI 生图关键帧、
Spine 逐版本拆件包。它不属于程序资产，也不是垃圾——这里是索引，便于以后找东西。
**本文件只做索引，不删除、不移动任何产物。**

## 总览

| 目录 | 日期 | 内容 | 与当前桌宠的关系 |
|---|---|---|---|
| `imagegen/crawl-20261012/` | 2026-10-12 | 趴伏抬头六帧关键帧 | 白底关键帧，未抠图、未安装 |
| `imagegen/drag-reaction-20261012/` | 2026-10-12 | 被拎起悬空反应六帧 | 白底关键帧，未安装 |
| `spine-parts-20261020/` 及 `v2`~`v5` | 2026-10-20 / 21 | 鲸鱼少女三视图拆件，逐版修正 | 独立的 Spine 工程探索，与本 C++ 桌宠无运行依赖 |
| `whale-girl-spine-parts*.zip`（5 个） | 2026-10-20 / 21 | 上述各版打包 | 归档包 |
| `run_shy.log` | — | shy 动作的运行日志 | 临时排障产物 |

> 这些是**另一个美术方向**（三视图 + Spine 骨骼）的尝试。当前 `pet_demo.exe` 播放的是
> `desktop_pet_image/` 下的逐帧 PNG，两者互不加载。

## 图像生成关键帧

两组都使用 Sudorelay API / `gpt-image-2.5-sunburst`，各目录自带 README、
`manifest.json`（来源与落点校正）和 `prompts/`（完整提示词）。

### `imagegen/crawl-20261012/`

- 动作：低伏 → 抬起 → 最高点 → 回落 → 低伏半眨眼，6 帧循环（总 0.96 秒）。
- `six-frames/white/001..006.png` 为最终图（1024×1024 白底）；`raw/` 存接口原始输出；
  `preview-loop.gif` 与 `six-frame-contact-sheet.jpg` 是预览。
- 001、003 沿用此前两张，其余四张本次新增；四次独立生图。
- 状态：白底版，未抠透明，未接入代码。

### `imagegen/drag-reaction-20261012/`

- 动作：悬垂 → 左晃 → 左停顿 → 回中 → 右晃 → 回中半眨眼，6 帧循环。
- `white/001..006.png` 为最终图；`drag-reaction-preview.gif`、`six-frame-contact-sheet.jpg`
  为预览。右摆帧曾镜像翻转，已修正，交付用修正版。
- 这是一套新画的悬空动作；此前的撑地动作不在本包。
- 状态：白底版，未接入代码。

## Spine 拆件包（5 版）

每版都是完整交付包：`front/` `side/` `back/` 各含透明 PNG 分件、对齐 PSD、
`layout.json` / `parts.csv`（坐标、层序、轴心、父级）、Spine 4.2 JSON；`qa/` 或
`validation.json` 记录验证；`preview.jpg` 和对比图便于目视。

| 版本 | 部件数（正/侧/背） | 本次变化 | 关键文件 |
|---|---|---|---|
| `spine-parts-20261020` | 45 / 28 / 23（96） | 首次三视图拆件；附闭眼替换图与初始骨架 | `README.md`、`verification.json`、`specs/` |
| `spine-parts-v2-20261020` | 34 / 28 / 21（83） | 返工重拆，PNG 裁紧留 2px 透明边 | `preview.jpg`、`*-clean.psd`、`side/audit/REVIEW.md` |
| `spine-parts-v3-20261020` | 38 / 29 / 23（90） | 恢复正面毛领/双眉、侧面眉线、白袜筒；带 idle/walk/wave/shy 四动作 | `runtime/`、`three-fixes.jpg`、`qa/motion-validation.json` |
| `spine-parts-v4-20261021` | 38 / 31 / 23（92） | 侧面改为两只完整靴子与袜筒；walk 0.8 秒交替迈步 | `side-feet-fix.jpg`、`side-walk.gif`、`qa/feet-validation.json` |
| `spine-parts-v5-20261021` | 38 / 32 / 23（93） | 恢复远侧衣襟/衣角，重配准毛边贴合衣身；本版无新增生图 | `side-hem-fix.jpg`、`side-walk.gif`、`validation.json` |

共同点：

- 验证口径：Spine 4.2.120 官方运行时，三视图四动作采样约 300 姿态，检查搭接与漂移。
- 三视图是独立二维视图，**不能直接插值成转身动画**；水平镜像会翻转不对称饰品。
- `runtime-before.zip` 是替换 WSL 工程前备份，替换只影响指定的 WSL 工程。
- `generated/` / `prompts/` 是 AI 补绘的提示词与原始结果；`*.zip` 为对应版本归档。

## 维护提示

- 这些目录已被根 `.gitignore` 的 `output/` 规则排除，不会进版本库；可放心长期留存。
- 若某天要清理，先确认对应 `README.md` / `validation.json` 已不需要；**不要删
  `desktop_pet_image/视频文件/` 下的源视频或源帧导出**——那是当前运行资产的源，
  受 `AGENTS.md` 硬规则保护，与 `output/` 的实验产物不是一回事。
- 复现拆件脚本需要 Python + Pillow / NumPy / SciPy / psd-tools / attrs；
  初版 README 记录了 `tools/build_parts.py` → `tools/finish_assets.py` 的顺序（脚本用 Windows 字体路径）。
