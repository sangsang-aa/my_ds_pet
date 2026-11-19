# 素材工具链与 MCP

把纯色背景图片做成透明 PNG 帧序列的工具，以及把它包装成 MCP server 的用法。
本文从 `README.md` 下沉，面向要**补帧 / 改素材**的人；只运行程序不需要读。

## 1. RGB → RGBA 抠图

`/home/zh180/MCPS/convert_rgba.py`（WSL 侧脚本，不在本仓库）批量把带纯色背景的
图片转成透明背景 RGBA PNG。用于给桌宠素材抠掉白色或任意纯色背景：

```
python3 /home/zh180/MCPS/convert_rgba.py desktop_pet_image --recursive
```

| 参数 | 作用 |
|---|---|
| `--mode flood`（默认） | 只抠与图片边缘连通的背景，保留角色身上同色部分（如白皮肤） |
| `--mode chroma` | 抠掉所有接近背景色的像素，更激进，可能误伤浅色部位 |
| `--threshold 60` | 背景色判定阈值（RGB 距离） |
| `--feather 25` | 边缘羽化宽度 |
| `--backup` | 覆盖前复制为 `<文件>.orig` |
| `--delete-source` | 转换后删除非 PNG 源文件 |
| `--clear-corners` | 擦掉四角孤立残留（会移动的 AI 水印）；角色是超大连通域不会被误擦 |
| `--corner-size WxH` | 角框大小，默认 96x48 |
| `--corner-max-px N` | 「多大算角色」下限，默认 2000 |

- 已透明的 RGBA 图自动跳过（幂等），补新帧后重跑即可；带 `--clear-corners` 时例外——
  这类图跳过抠图、只擦四角水印。
- 输出始终为同名 `.png`，符合帧加载约定 `<action>/NNN.png`。
- 补帧流程：新帧放进 `desktop_pet_image/<action>/` → 跑上面的命令 → 重开 demo。

## 2. 作为 MCP 工具调用（推荐）

`/home/zh180/MCPS/rgba_convert_mcp.py` 把上面的功能封装成 MCP server，注册在
`~/.config/opencode/opencode.jsonc`（名称 `rgba-convert`）。重启 opencode 后，
让 AI 调用 `convert_to_rgba` 即可，无需手敲命令行。

脚本统一放 `/home/zh180/MCPS/`：`rgba_convert_mcp.py` 通过脚本目录自动 import
`convert_rgba.py`。

工具参数：`paths`（必填，文件/目录列表）、`recursive`、`mode`、`threshold`、
`feather`、`backup`、`delete_source`、`force`、`dry_run`；返回逐文件日志 + 汇总行。

> 实现要点：MCP stdio 用 stdout 传 JSON-RPC，而抠图函数会 print 到 stdout；
> server 用 `contextlib.redirect_stdout` 把输出重定向到缓冲区再作为工具结果返回，
> 避免污染协议。

## 3. GIF 支持

输入可以是动图 GIF。多帧 GIF 会**逐帧抠图**并输出成 PNG 帧序列到同名同级文件夹：

```
视频文件/pet_parachute_down.new.gif
  → 视频文件/pet_parachute_down.new/000.png … 023.png
```

GIF 本身只有 1-bit 透明，故输出用 PNG 帧保留完整 alpha。已含透明通道的 GIF 会被跳过
（可用 `force` 强制处理）。

## 4. 配套工具（本仓库 `tools/`）

| 脚本 | 用途 | 文档 |
|---|---|---|
| `tools/make_emoji.py` | 图片/GIF → `emoji/emoji_NN.png`（编号 + 缩放） | `desktop_pet_image/emoji/README.md` |
| `tools/migrate_canvas.py` | 384×456 / 384×384 → 384×512，顶部补透明；同步 bounds | `tools/README.md` |
| `tools/fix_edges.py` | 修青色/绿色边缘环、透明区幽灵 RGB、孤立白点 | [`edge-repair.md`](edge-repair.md) |

## 5. 硬规则

- **永不删除源视频 / 源帧导出**；`NNN.png` 是有损派生产物，key 错了只能从原片重做。
- 当前白兜帽套装的源素材已丢失；`desktop_pet_image/视频文件/女仆装/` 是另一套皮肤，
  不是当前动作的源。
- 青色环（`G≈B≫R`）无法靠 `despill`（`G=min(G,max(R,B))`，对青色是恒等变换）修复，
  只能从原片重新 key 或做保守边缘修补，详见 [`edge-repair.md`](edge-repair.md)。
