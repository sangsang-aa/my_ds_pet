# rgba-convert — 批量抠图工具（MCP + CLI）

把**近似纯色背景**的图片批量转成背景透明的 RGBA PNG，用来准备桌宠动作帧。
同一份核心逻辑既能在命令行直接用，也能作为 MCP 工具挂到 OpenCode 里。

## 目录内容

| 文件 | 作用 |
|------|------|
| `convert_rgba.py` | 抠图核心逻辑 + 命令行入口（纯本地，无网络） |
| `rgba_convert_mcp.py` | MCP stdio server，把 `convert_to_rgba` 暴露成工具 |
| `test_convert_rgba.py` | 单元测试（pytest，16 个用例，不需要网络） |
| `requirements.txt` | 依赖：`pillow`、`mcp>=2.2` |

## 安装

```bash
cd tools/rgba_convert
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
```

## 挂到 OpenCode（MCP）

在 `~/.config/opencode/opencode.json` 的 `mcp` 段里加一项（路径改成你本机的位置）：

```json
{
  "mcp": {
    "rgba-convert": {
      "type": "local",
      "command": ["python3", "/path/to/tools/rgba_convert/rgba_convert_mcp.py"],
      "enabled": true
    }
  }
}
```

重启 OpenCode 后即可用 `convert_to_rgba` 工具，参数如下：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `paths` | 必填 | 绝对路径的文件或目录列表 |
| `recursive` | `false` | 递归扫描目录 |
| `mode` | `"flood"` | `flood` 只删与边框连通的背景；`chroma` 删除所有接近背景色的像素 |
| `threshold` | `60` | 与边框背景色的 RGB 距离阈值 |
| `feather` | `25` | 平滑过渡带宽度（smoothstep） |
| `trim` | `1` | 把 alpha 边缘向内收 N 像素；1 可去掉抗锯齿留下的白/绿细边 |
| `decontaminate` | `true` | 用最近的不透明像素颜色替换半透明边缘色，避免白边/绿边 |
| `despill` | `true` | 在边缘压掉背景的主色通道（绿幕溢色） |
| `clear_corners` | `false` | 擦除四角的小块孤立非透明岛（AI 水印），主体不受影响 |
| `corner_size` | `"96x48"` | 四角探测框尺寸 `WxH` |
| `corner_max_px` | `2000` | 探测框内所有连通块都小于该像素数时才擦除 |
| `backup` | `false` | 覆盖前把原文件备份成 `<path>.orig` |
| `delete_source` | `false` | 转换成功后删除原文件（源不是 `.png` 时） |
| `force` | `false` | 已有透明通道也强制重新抠图 |
| `dry_run` | `false` | 只打印计划，不写文件 |

工具返回逐个文件的处理日志 + 一行 `summary:` 统计（`total / processed / skipped / failed`）。

## 命令行用法

```bash
# 单张图：原地生成同名 PNG（覆盖同目录同名 PNG）
python3 convert_rgba.py ../desktop_pet_image/walking_right/003.png

# 整个动作目录，顺手清掉四角水印
python3 convert_rgba.py ../../desktop_pet_image/idle --recursive --clear-corners

# 先看计划再动手，并保留 .orig 备份
python3 convert_rgba.py shot.jpg --dry-run --backup

# 去掉主体内部的背景色空洞（chroma 更激进，可能打穿浅色区域）
python3 convert_rgba.py frames/ --mode chroma --threshold 45
```

常用开关与上表一致：`--recursive`、`--mode`、`--threshold`、`--feather`、
`--clear-corners`、`--corner-size`、`--corner-max-px`、`--backup`、
`--delete-source`、`--force`、`--dry-run`。

## 行为细节

- 背景色取**四边全部像素的中位数**，所以背景要近似纯色；渐变背景效果会差。
- `flood` 模式从边框做 BFS 连通扩展，只删与外界连通的背景，主体内部同色区域会保留；
  `chroma` 模式不做连通性判断，所有接近背景色的像素都变透明。
- 输出 PNG 沿用输入文件名（`a.jpg` → `a.png`），保持 `000.png` 这种帧序列命名不变。
- 已经是透明图的输入默认跳过（打印 `[skip]`），加 `--force` 才会重抠；
  加了 `--clear-corners` 时则只清角、不重抠。
- **动图（GIF）**：逐帧抠图后写成同级同名文件夹的 PNG 序列
  （`pet_down.gif` → `pet_down/000.png`、`001.png` …），保留 alpha。
- 桌宠渲染端（GDI+）只认 png/jpg/gif/bmp，**不支持 WebP**；本工具能读 WebP，
  但转出来的素材请用 PNG。

### 白边 / 白框是怎么来的

只改 alpha 的抠图，会把**边缘像素原来的颜色**留在半透明区域：

- 白底素材 → 半透明边缘仍是白色，合成到绿幕/桌面时就是一圈白边；
- 绿幕素材 → 半透明边缘仍是绿色，合成后是一圈绿边。

现在默认做三步：

1. `trim=1`：alpha 向内收 1px，直接切掉最外圈抗锯齿；
2. `decontaminate=True`：把半透明边缘的 RGB 换成最近的不透明像素颜色；
3. `despill=True`：把边缘残留的背景主色（通常是绿）压掉。

对白底图实测：`turnaround.jpg` 的半透明白边像素从 **1491** 降到 **1**。
如果素材特别糊、边缘特别宽，可以加大 `--trim 2`；但 2px 以上会开始啃角色轮廓。

> 注意：缩小图片（LANCZOS）会重新引入半透明边缘。正确顺序是
> **先在原分辨率抠干净 → 再用预乘 alpha 缩放**，不要在缩放后又用普通 RGBA 缩放。

## 测试

```bash
python3 -m pytest test_convert_rgba.py -q
```

覆盖 `WxH` 解析、四角探测框、连通块标记、四角水印擦除、已透明图的清角/跳过等，
全部在临时目录里跑，无网络、无外部依赖。
