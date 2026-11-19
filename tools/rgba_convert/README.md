# rgba-convert — 批量抠图工具（MCP + CLI）

把**近似纯色背景**的图片批量转成背景透明的 RGBA PNG，用来准备桌宠动作帧。
同一份核心逻辑既能在命令行直接用，也能作为 MCP 工具挂到 OpenCode 里。

## 目录内容

| 文件 | 作用 |
|------|------|
| `convert_rgba.py` | 抠图核心逻辑 + 命令行入口（纯本地，无网络） |
| `rgba_convert_mcp.py` | MCP stdio server，把 `convert_to_rgba` 暴露成工具 |
| `test_convert_rgba.py` | 单元测试（pytest，不需要网络） |
| `requirements.txt` | 依赖：`pillow`、`mcp>=2.2` |
| `mask_editor.html` | 人工 mask 标注工具（浏览器单文件，纯前端） |
| `mask_refine.py` | 精修 mask：填充手绘笔画内部的漏涂针孔 |
| `apply_mask.py` | 把人工 mask 批量对齐应用到帧序列（支持多关键帧） |
| `diagnose_enclosed.py` | 内部闭合区域抠图缺陷诊断工具（合成帧复现） |
| `_diag_images/` | 诊断着色图，可作标注参考底图（非交付素材） |

工具分两组：

- **自动抠图**：`convert_rgba.py` / `rgba_convert_mcp.py` —— 处理背景、边缘、封闭键色块。
- **内部白块人工修正**：`mask_editor.html` → `apply_mask.py` —— 处理自动判据搞不定的头发间白色缝隙（见 [内部白块](#内部白块--孔洞)）。

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
| `trim` | `0` | 把 alpha 边缘向内收 N 像素；默认 0，避免吃掉眼睛、发丝等细结构 |
| `decontaminate` | `true` | 用最近的不透明像素颜色替换半透明边缘色，避免白边/绿边 |
| `despill` | `true` | 在边缘压掉背景的主色通道（绿幕溢色） |
| `remove_holes` | `false` | **不推荐**（见 [内部白块](#内部白块--孔洞)）：开启后按"中性白+面积+环亮度"删孔洞，会误删人物白色装饰、漏掉大白缝，本项目改用人工 mask |
| `hole_min_area` | `100` | `remove_holes` 的最小面积，保护眼睛高光和小白点 |
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
`--delete-source`、`--force`、`--dry-run`。此外还有 CLI 专用（MCP 未暴露）：
`--punch-enclosed/--no-punch-enclosed`（封闭键色块打孔，默认开）、
`--clear-rect X0,Y0,X1,Y1`（矩形擦除，可重复）、
`--trim`、`--remove-holes`、`--hole-min-area`。

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

现在默认只做两步：

1. `decontaminate=True`：把半透明边缘的 RGB 换成最近的不透明像素颜色；
2. `despill=True`：把边缘残留的背景主色（通常是绿）压掉。

**不再默认 `trim=1`**：1px 内收会吃掉眼睛高光、睫毛、细发丝，导致绿幕从五官里露出来。需要收边时显式加 `--trim 1`，并在预览里确认没有伤到细结构。

### 内部绿块 / 封闭背景

发圈、帽子卷边、尾巴围成的小口袋会圈住一块**纯绿背景**。flood 抠图只从画面四边往里走，到不了这些口袋，于是它们保持不透明，成品里就是一块死绿。

默认 `--punch-enclosed` 会把**所有和键色一致的像素**打成透明，不管连通性。角色本身不含键色时这是安全的；如果角色真的有绿色，用 `--no-punch-enclosed` 关掉。

### 客户端水印

豆包的“豆包AI生成”是半透明白字压在绿幕上，颜色和背景只差一点点，抠不掉；它又离角色太近，`--clear-corners` 的探测框会碰到主体而放弃。

直接用矩形擦：

```bash
python convert_rgba.py frames/ --clear-rect 545,875,720,960
```

坐标是 `X0,Y0,X1,Y1`，可重复。擦之前确认角色不会经过这个矩形。

### 内部白块 / 孔洞

头发之间、外套和尾巴之间的白色缝隙是**背景**，但颜色和人物身上的白色装饰（围裙、荷叶边、星星闪光、袖口）几乎一样，**自动判据无法可靠区分两者**。

#### ✅ 首选方案：FramePacker 取色填充（实测最佳）

经真实 `clicked` 图集验证，**用 [FramePacker](https://www.framepacker.cn/) 的「换色/取色填充」处理这类白缝效果最好，且最省事**：取色器点一下白缝区域，把该颜色容差内的色块统一转成透明，再「批量应用到所有帧」。

实测对比（导出帧 vs 本目录手工 mask 成品）：

- `000`、`015` 两帧的 alpha 与手工 mask **逐像素完全一致**（diff=0）；
- `016`、`023` 帧 FramePacker 反而多清了 262/758px 白缝（更干净），目视核对人物装饰完整、无误抠洞。

它之所以有效：白缝是同一种纯色块，且与人物白装饰被深色线框隔开、不成同色连通域——取色填充正好按"颜色+连通"区分，绕开了纯阈值判据的两难。

操作要点：

1. 把 PNG 序列（或源视频/GIF）拖进 framepacker.cn；
2. 编辑器里用取色器点白缝，调「容差」「边缘清理」到边缘干净；残留水印用矩形工具盖掉；
3. 点「批量应用」把这一帧的编辑重放到所有帧，再逐帧过一遍精修；
4. 导出 PNG 序列 / Sprite Sheet（换色、批量应用、自定义分辨率为付费功能）。

> 注意导出的帧号可能整体 +1（本项目导出为 `001`–`030`，对应原 `000`–`029`），入库前需对齐命名。
>
> 若不想用在线工具，才用下面的本地人工 mask 工作流。

#### 为什么 `remove_holes` 不可靠（诊断结论）

`remove_holes` 的判据是「中性白(min≥248) + 面积 100–1000 + 通道差<4 + 外环亮度<210」。在真实 `clicked` 图集上实测（`diagnose_enclosed.py` + 分类着色图）：

- **误删**：人物身上的白色装饰若恰好满足条件，会被抠穿，透出背景（如帽檐内侧、脸部高光、袖口、裙摆）。
- **漏删**：真正的背景白缝若面积 >1000px（头顶帽檐、裙摆两侧的大白块），或外环亮度 ≥210（线框外紧挨白色装饰），会被判为"保留"。

两类白块在颜色/邻域特征上**同构**，调阈值只会在"误删装饰"和"漏掉白缝"之间来回摆动。**结论：本项目素材不要用 `remove_holes`，走人工 mask。**

> 注意（绿幕素材例外）：若背景是纯绿幕，头发间隙的白块 `dist≈255` 进不了 flood 候选集，`punch_enclosed` 也只认键色，同样无解——仍走人工 mask。

#### 人工 mask 工作流（推荐）

```text
① mask_editor.html 在关键帧上画 mask  →  ② apply_mask.py 对齐应用到全部帧  →  ③ QA
```

**为什么要多关键帧**：点击动作中间帧（如 012–021）人物姿态变化大，单张 mask 靠 bbox 对齐会失效（位移 −7~−11px，清除量骤降）。在动作的头/中/尾各画一张 mask，脚本按帧号自动选用最近的关键帧。

##### ① 标注：`mask_editor.html`

浏览器直接打开这个 HTML 文件（双击即可，纯前端、无需服务器）：

1. 「选择关键帧 PNG」载入要标注的帧（如 `000.png`）。
2. 可选：「参考分类图」载入 `_diag_images/CLASSIFIED_000.png` 作对照——**红=自动判据会删（多为背景）、橙=面积超限的背景、蓝=环过亮的背景、品红=非中性白、黄=过小**。
3. 用红色画笔画要抠掉的白缝，橡皮擦修正；开启「标记处预览透明」在棋盘格上看实际抠图效果。
4. 「导出 Mask PNG」保存黑白图（**白=要抠掉，黑=保留**）。

画布操作：

| 操作 | 效果 |
|------|------|
| 鼠标左键拖动 | 用当前工具（画笔/橡皮）绘制 |
| 鼠标右键 | 撤销一步 |
| 滚轮 | 以鼠标位置为中心缩放（10%–1000%） |
| 按住滚轮（中键）拖动 | 平移画面 |
| `空格` + 左键拖动 | 平移画面（无中键时替代） |
| `F` 或「适应窗口」按钮 | 整图缩放居中 |
| 笔刷半径滑块 | 调整画笔/橡皮大小（原图像素） |

在姿态变化大的段落各标一张（如 `mask_000.png`、`mask_015.png`、`mask_029.png`）。

**精修（可选但推荐）**：手绘笔刷容易在笔画内部留下 1–4px 的漏涂针孔，整图上看不出来，但抠图后会变成残留的小白点。用 `mask_refine.py` 自动填平这些"被白色包围的黑洞"：

```bash
python mask_refine.py mask_000.png -o mask_000_refined.png --preview refine_preview.png
```

它只填笔画**内部**的漏点（与外背景连通的黑色区域不动），不会扩大笔画外形。`--dilate N` 可让笔画整体外扩 N px（默认 0，通常不需要）。把输出的 `_refined` mask 交给下一步即可。

##### ② 批量应用：`apply_mask.py`

只写 PNG、输出到新目录、不覆盖原图；`--dry-run` 可先看每帧选用的关键帧和清除量。

```bash
# 多关键帧（推荐）：最近关键帧自动分配
python apply_mask.py E:/pet_demo/desktop_pet_image/clicked \
  --out E:/pet_demo/desktop_pet_image/clicked_masked \
  --anchor 000.png=mask_000.png \
  --anchor 015.png=mask_015.png \
  --anchor 029.png=mask_029.png --dry-run

# 确认无误后去掉 --dry-run 正式写出
```

```bash
# 显式帧区间（未覆盖的帧原样透传）
python apply_mask.py FRAMES --out OUT --assign range \
  --anchor "000.png=mask_000.png@000-011" \
  --anchor "015.png=mask_015.png@012-021" \
  --anchor "029.png=mask_029.png@022-029"
```

> **往返动作要用 `range`，别用 `nearest`**：像 clicked 这种"站姿 → 抬手 → 站姿"的往返动作，首尾姿态相同但帧号相隔很远。`nearest` 按帧号中点分配，会把末尾的站姿帧错分给中段的抬手 mask，导致清除量暴跌。应按姿态段用 `range` 显式划分，同一张 mask 可重复用于多个区间：
>
> ```bash
> # 头尾站姿共用 000 的 mask，中段抬手用 015 的 mask
> python apply_mask.py FRAMES --out OUT --assign range \
>   --anchor "000.png=mask_000.png@000-011" \
>   --anchor "015.png=mask_015.png@012-021" \
>   --anchor "000.png=mask_000.png@022-029"
> ```
>
> 用 `--dry-run` 时留意每帧的 `cleared_px`：若某帧骤降到几十（正常几百到一千多），说明分错了 mask 或该段需要单独标注。姿态切换的临界帧（如 012、022）清除量偏低属正常。

##### 过渡帧误抠：`--shape-gate`

即便按姿态段分配，过渡帧（如 011、022）的身体在摆动，mask 平移过去后某些笔画会落到**尾鳍白边、装饰白边**上——这些像素和真白缝一样是纯白，靠颜色/面积/连通域大小都无法区分，于是被误抠出透明洞。

解法是**形状门控**：`--shape-gate 0.35` 会把 mask 按连通域拆成笔画，逐块计算它与当前帧对应位置白色区域的 IoU；IoU 低于 0.35 的笔画（说明形状已不吻合、位置漂移了）自动跳过，只在形状对得上的笔画上抠。

```bash
python apply_mask.py FRAMES --out OUT --assign range \
  --anchor "000.png=mask_000.png@000-011" \
  --anchor "015.png=mask_015.png@012-021" \
  --anchor "000.png=mask_000.png@022-029" \
  --shape-gate 0.35
```

日志里每帧会多一项 `shape_gate=保留数/总数`，例如 `011.png: ... shape_gate=3/9` 表示该帧 9 个笔画只用了 3 个（其余判定为错位、跳过）。效果：**误抠的洞消失，对齐的白缝照常抠除**；代价是过渡帧抠得更保守（白缝可能留一点），但这比抠错洞好得多。

阈值选择：关键帧自身应接近全保留（`shape_gate=9/9`）；若关键帧都被削太多，说明阈值过高，往下调（0.3）。本项目实测 0.35 合适。该选项默认关闭，不影响原有行为。

参数（完整列表见 `python apply_mask.py --help`）：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `frames`（位置参数） | 必填 | RGBA PNG 帧目录 |
| `--out` | 必填 | 输出目录（新建，不覆盖输入） |
| `--anchor KEY=MASK[@RANGE]` | 至少 1 个 | 关键帧及其 mask；可重复；`@000-011` 形式的区间用于 `--assign range` |
| `--config JSON` | 无 | 用 JSON 提供 anchors（可用 `--write-example-config` 生成模板） |
| `--assign` | `nearest` | `nearest`=按帧号就近选用；`range`=按显式区间 |
| `--whiteness` | `235` | 只清除 `min(R,G,B)≥235` 的白像素，保护彩色人物不被 misalign 误伤 |
| `--feather` | `2` | mask 边缘软化级数；`0` 为硬边 |
| `--no-align` | 关 | 关闭 bbox 对齐，直接用 mask 原始坐标 |
| `--shape-gate IOU` | `0`（关） | 形状门控：丢弃与当前帧白缝形状不吻合的笔画（IoU 低于阈值），见下 |
| `--dry-run` | 关 | 只打印，不写文件 |
| `--write-example-config PATH` | 无 | 写出配置样例后退出 |

**对齐原理**：用每帧与关键帧的「非透明区域 bbox」偏移做平移，吸收 1–5px 的帧间抖动。`--whiteness` 是第二道保险：即使分配/对齐有偏差，也只有白色像素会被清除，深色线稿和彩色装饰不受影响。

> **坐标系坑**：`clicked/bounds.txt` 是旧批次坐标（x1=264, y1=350），与实际成品 bbox（约 `60,104,324,454`）不符，**不要用它做对齐**。`apply_mask.py` 用的是每帧 alpha bbox，不受此影响。

##### ③ QA

写出后抽查 mask 覆盖区域是否已透明、白色装饰是否完整保留、有无透背景漏点。重点看动作中间帧（012–021）的对齐是否正确——若某段清除量异常偏低，说明该段需要单独加关键帧 mask。

#### 诊断工具：`diagnose_enclosed.py`

合成帧复现内部闭合区域的抠图行为，打印 flood 候选/访问、punch 命中数、`remove_holes` 判定等中间指标，用于定位"白块为何没被抠掉"。

```bash
python diagnose_enclosed.py
```

内置 A–E 五个场景（绿幕/白底、键色块/白块、大面积、浅色环）。`_diag_images/` 里保留了真实帧的分类着色图，可直接给 `mask_editor.html` 当参考底图。

### 抠图 QA（生成前必做）

把结果合成到 **绿色** 和 **中灰** 两种背景，放大 2–3 倍检查：

- 眼睛、睫毛、发丝完整，没有绿点/绿缝；
- 五官没有被削掉；
- 白色毛绒保留；
- 内部白块是否真的是背景；
- 外轮廓没有白边/绿边。

不过这一关，不要送进视频模型。视频模型会把 1px 的问题放大成整块白框。

> 实测：`trim=1` + 小面积孔洞移除会在眼睛和脸缘产生绿色斑点；默认关闭后，脸部完整。

> 注意：缩小图片（LANCZOS）会重新引入半透明边缘。正确顺序是
> **先在原分辨率抠干净 → 再用预乘 alpha 缩放**，不要在缩放后又用普通 RGBA 缩放。

## 测试

```bash
python3 -m pytest test_convert_rgba.py -q
```

覆盖 `WxH` 解析、四角探测框、连通块标记、四角水印擦除、已透明图的清角/跳过等，
全部在临时目录里跑，无网络、无外部依赖。
