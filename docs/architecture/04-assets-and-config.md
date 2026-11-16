# 资产与配置

本文定义程序读什么、怎么读、读不到时怎么办。所有路径都相对于 **可执行文件所在目录**，不是当前工作目录。

## 1. 目录结构

```
desktop_pet_image/
├─ pet.ini                    # 可选；不存在则全用 config.h 默认值
├─ idle/
│  ├─ 000.png
│  └─ bounds.txt
├─ walking_left/
├─ walking_right/
├─ walking_up/
├─ walking_down/
├─ clicked/
├─ drag/
├─ shy/
├─ idle2/                     # 有帧，但当前未注册
├─ busy/                      # 有帧，但当前未注册
├─ peek/                      # 有帧，但当前未注册
├─ emoji/
│  ├─ emoji_01.png .. emoji_06.png
│  ├─ index.json              # 当前代码不读取
│  └─ README.md
├─ _unused/                   # 女仆装皮肤，gitignored
├─ 视频文件/                  # 源视频/源帧，本地资产
└─ tmp/                       # 空，预留
```

`desktop_pet_image/.gitignore` 还会忽略 `emoji/*`（保留 README）、`视频文件V2/`、`*.orig`、备份目录等。**不要把 emoji 或源视频当代码资产提交。**

## 2. 帧文件契约

| 项 | 约定 |
|---|---|
| 格式 | PNG，带 alpha 通道（RGBA）；GDI+ 也能解码 jpg/gif/bmp，但动作帧统一用 PNG |
| 命名 | `NNN.png`，3 位零填充：`000.png`、`001.png` … |
| 连续 | 从 000 开始；遇到第一个缺失文件就停止加载，不跳过缺号 |
| 上限 | 最多 120 帧（`kMaxFrames`，000~119） |
| 画布 | 所有动作应同尺寸；当前统一 384×512 |
| 主体位置 | 人物高度约 0.68H，脚底基线约 y=460（AGENTS 约定） |
| 缩放 | 加载时按 `scale_factor`（默认 0.70）用 HighQualityBicubic 缩放；384×512 → 269×358 |
| 帧率 | 宠物定时器 100 ms，约 10 fps；不是按帧文件元数据播放 |
| 解码 | **GDI+ 不支持 WebP**；不要放 `.webp` 动作帧 |

窗口尺寸取自 **基底池第一帧** 缩放后的尺寸；如果后续动作画布不同，运行时不会报错，只会出现裁切/留白/命中不准。

## 3. 当前实际资产清单

以下为 2026-11-16 工作副本实测（连续帧数、`bounds.txt` 行数、是否注册）：

| 目录 | 连续帧 | `bounds.txt` | 注册为 | 用途 |
|---|---:|---|---|---|
| `idle` | 1 | 1 行 | 基底池 | 当前唯一基底；单帧静止 |
| `idle2` | 30 | 30 行 | 否 | 代码中刻意禁用 |
| `walking_left` | 30 | 30 行 | Auto / `Loop` | 侧视走路 |
| `walking_right` | 30 | 30 行 | Auto / `Loop` | 侧视走路（镜像） |
| `walking_up` | 27 | 27 行 | Auto / `Boomerang` | **飞起**，不是走路 |
| `walking_down` | 30 | 30 行 | Auto / `FullCycle` | **俯冲**，不是走路 |
| `clicked` | 30 | 30 行 | `OnClick` / `OneShot` | 点击反应 |
| `drag` | 30 | 30 行 | `OnDragStart` / `Loop` | 被拎起挣扎 |
| `shy` | 40 | 40 行 | `HeadSweep` / `OneShot` | 扫头害羞 |
| `busy` | 30 | 30 行 | 否 | 未注册 |
| `peek` | 30 | 30 行 | 否 | 未注册 |
| `emoji` | 6 | — | 气泡 | 本地表情包，不随仓库分发 |

所有动作帧实测为 384×512 RGBA。当前实际预载帧数：`1 + 30 + 30 + 27 + 30 + 30 + 30 + 40 = 218`（idle + 7 条注册行为）。总资产帧数（含未注册）为 308。

> 根 `README.md` 的帧数已经过期（`idle 30`、`clicked 33`、`shy 30`、`walking_right 27`、`walking_up 30` 等）。以本表为准；详细对照见 [`../archives/2026-11-16-baseline.md`](../archives/2026-11-16-baseline.md#文档漂移对照)。

## 4. `bounds.txt`

每个动作目录可带一个 `bounds.txt`，每行一帧：

```
x y w h
x y w h
...
```

- 单位是 **源画布坐标**（384×512），由外部 `prepare_action.py` / `tools/migrate_canvas.py` 生成。
- `LoadFrameBounds` 按顺序把有效行写入 `Frame.bx/by/bw/bh`；一行必须有 4 个整数且 `w > 0 && h > 0`，否则跳过且不推进帧索引（后续行会前移一帧）。
- 行数少于帧数：未赋值的帧保持 0，`PresentFrame` 退回整块窗口；气泡也退回窗口矩形。
- 行数多于帧数：多余行忽略。
- 没有 `bounds.txt`：所有帧都按整块窗口处理。

消费者：

- 气泡：`BubblePetRect` 读 `SubjectBounds`，决定气泡放在头顶还是侧面；
- 扫头：`Pet::OnMouseMove` 用 `SubjectBounds` 计算头部带；
- 命中测试 **不用** bounds，用逐帧 `AlphaMask`。

### 已知缺陷：bounds 未随缩放换算（R1）

`bounds.txt` 是 384×512 源画布坐标，但 `LoadFrameBounds` 直接赋值；`PresentFrame` 又把这些值直接加到 269×358 窗口的屏幕坐标上，**没有乘 `scale_factor`**。

以 `idle/bounds.txt` 第一行为例：

```
源坐标:   61 110 262 353      （384×512 画布）
按 0.70:  43  77 183 247      （269×358 窗口，期望）
实际使用: 61 110 262 353      （代码现状）
```

后果：

- 气泡水平中心算成 `61 + 262/2 = 192`，而窗口中心是 `134.5`，整体偏右约 57 px；
- 上方气泡的底边会压到实际头顶附近，侧面气泡会离身体更远；
- 扫头判定带按源高度 353 计算并向下偏移，落在脸部/上半身而不是头顶；
- 命中测试不受影响，因为 `AlphaMask` 是在缩放后的帧上生成的。

修复方向：在 `LoadFrameBounds` 里按 `scale_factor` 缩放并夹到帧边界，或保留源坐标、在 `PresentFrame` 统一换算；修完需要回归气泡四向放置和扫头触发。**本次文档提交没有改代码。**

## 5. emoji 契约

- 文件名：`emoji_01.png`、`emoji_02.png` …（**两位**零填充；`EmojiName` 只补 1 位，所以 1~9 是 `01`~`09`，10 以后是 `10`…）。
- 从 `01` 开始连续；遇到第一个缺失就停止；最多 120 张。
- PNG，带 alpha；尺寸任意，显示时按最长边缩放到 `emoji_size_px`（默认 220），居中放进气泡主体。
- 当前工作副本有 6 张；`emoji/` 被 gitignore，只有 README 入库。新克隆没有表情包时气泡功能自动禁用。
- `emoji/index.json` 当前 **不被代码读取**，只是素材目录的说明文件。
- 生成工具：`python tools/make_emoji.py <图片或目录>`；会编号、缩放、处理 GIF（取第一帧）。

## 6. `pet.ini` 键表

路径：`<exeDir><assetsDir>\pet.ini`。文件不存在 ⇒ 全部用 `config.h` 默认值。

解析规则（`core/inifile.h`）：

- 支持 `[section]`、`key = value`；
- **只支持整行注释**：行首（trim 后）是 `#` 或 `;` 才跳过；
- **不支持行尾注释**；`idle_min_ticks = 5 ; comment` 会被 `IntOr` 判为解析失败，回退默认值；
- 键和值会 trim；重复键最后一个生效；没有 section 的键落在 `""`；
- 值必须整段可解析为数字，否则回退默认；
- 非 ASCII 的行为名会被 `AsciiFromWide` 变成 `?`，无法匹配 section；当前 action 都是 ASCII。

### `[pet]`

| 键 | 默认 | 作用 | 备注 |
|---|---:|---|---|
| `idle_min_ticks` | 30 | 待机停留下限 | tick = 100 ms；约 3 秒 |
| `idle_max_ticks` | 100 | 待机停留上限 | 约 10 秒 |
| `wait_min_ticks` | 1200 | 拖后等待下限 | 约 2 分钟 |
| `wait_max_ticks` | 1800 | 拖后等待上限 | 约 3 分钟 |
| `head_band_percent` | 45 | 头部带的上界 | 实际底边还要减去 `kHeadBandBottomInsetPercent = 20`，默认有效带是 0~25% |
| `head_band_top_percent` | 0 | 头部带顶部跳过比例 | README 示例没有列出，但代码支持 |
| `sweep_threshold_px` | 50 | 一次有效 stroke 的最小水平位移 | 方向反转时检查 |
| `max_sweep_delta_px` | 80 | 单次 mousemove 允许的最大水平跳变 | 超过则忽略，过滤鼠标重入 |
| `scale_factor` | 0.70 | 帧缩放 | 加载时生效；改后必须重启 |

### `[bubble]`

| 键 | 默认 | 作用 |
|---|---:|---|
| `emoji_size_px` | 220 | emoji 最长边缩放尺寸；也决定气泡主体大小 |
| `pad_px` | 10 | emoji 与气泡边缘的间距 |
| `radius_px` | 10 | 圆角半径 |
| `tail_w` | 18 | 尾巴基部宽度 |
| `tail_h` | 12 | 尾巴高度；同时是窗口在尾巴方向的额外尺寸 |
| `gap_px` | 4 | 尾巴尖与宠物主体之间的间隙 |
| `min_ticks` | 200 | 两次气泡的最小间隔（约 20 秒） |
| `max_ticks` | 600 | 最大间隔（约 60 秒） |
| `show_ticks` | 30 | 每次显示时长（约 3 秒） |

默认窗口尺寸：`220 + 2*10 + 12 = 252`，所以 `252×252`；主体 `240×240`。

### 行为 section

section 名 = action 名，例如：

```ini
[walking_up]
min_ticks = 52
max_ticks = 52
weight    = 1
dy        = -5
```

可覆盖：

| 键 | 作用 |
|---|---|
| `min_ticks` | Auto：持续时间下限 + 启动冷却；事件：冷却（从启动算） |
| `max_ticks` | Auto：持续时间上限；`<= 0` 表示无时间上限；OneShot 不会因此提前停止 |
| `weight` | 仅 Auto 的加权选择；`<= 0` 不可被自动选中 |
| `dx` / `dy` | Acting 期间每 tick 的窗口位移 |

不能通过 INI 覆盖：`trigger`、`playback`、`gatedByWait`。改这些要改 `behaviors.cpp`。

### 不能通过 INI 调的值

以下仍是编译期常量：

| 常量 | 默认 | 说明 |
|---|---:|---|
| `kTimerElapseMs` | 100 | 宠物 tick |
| `kBubbleTimerElapseMs` | 100 | 气泡 tick |
| `kTopmostReassertTicks` | 10 | 约 1 秒重新置顶 |
| `kMaxFrames` | 120 | 动作/emoji 最大数量 |
| `kHitAlpha` | 16 | 命中测试 alpha 阈值 |
| `kHeadBandBottomInsetPercent` | 20 | 头部带底边内缩比例 |
| `kSweepPassesToTrigger` | 2 | 需要几次 stroke |
| `kSweepResetTicks` | 20 | 扫头状态超时 |
| `kWalkStepPx` | 5 | 四个 walk spec 的默认 `dx/dy`（可被 INI 覆盖） |
| `kShyCooldownTicks` | 25 | `shy` spec 的默认冷却（可被 `[shy] min_ticks` 覆盖） |

### 配置陷阱

1. **行尾注释会静默回退默认值**。README 的示例用了行尾 `; ...`，直接复制会得到默认值而不是你写的值。要注释请单独一行。
2. 没有范围校验：`scale_factor <= 0`、负数 tick、`max < min` 都可能产生异常行为；`RandomInRange` 在 `hi <= lo` 时返回 `lo`。
3. `head_band_percent` 不是字面上的“顶部百分比”；有效底边 = `head_band_percent - 20`（再受最小高度约束）。
4. `emoji_size_px` / `tail_h` 改变气泡窗口尺寸；启动时生效，改完需重启。
5. `scale_factor` 改变帧尺寸，必须重启；运行中改无效。

## 7. 加载顺序与失败模式

```
读取 exe 目录
  → 读取 pet.ini（若存在）
  → Pet::Load：
       基底帧（idle 或 argv[1]）
       7 条注册行为的帧
       每帧：解码 → 缩放 → HBITMAP → AlphaMask；全部帧后读一次 bounds.txt
  → InitGeometry（窗口尺寸 = 首帧）
  → 建宠物窗 + 定时器
  → Bubble::Init：
       扫描 emoji_01..NN
       建气泡窗 + 定时器
  → 首帧呈现 + 显示窗口
```

| 失败 | 结果 |
|---|---|
| `pet.ini` 缺失 | 用默认值，正常启动 |
| `pet.ini` 某键非法 | 该键回退默认，其余继续 |
| 基底帧缺失 | 错误框 + 退出 |
| 某行为帧缺失 | 该行为禁用，启动日志 `loaded=0/120` |
| 帧中间缺号 | 只加载前缀，不报错 |
| `bounds.txt` 缺失 | 整块窗口作为主体 |
| emoji 缺失 | 气泡禁用，其余正常 |
| emoji 存在但解码失败 | 气泡主体照画，里面没有图 |
| GDI+ 初始化失败 | 错误框 + 退出 |
| 窗口类/窗口创建失败 | 错误框 + 退出 |

## 8. 源素材与工具链

硬规则（来自 `AGENTS.md`）：

- **永远不要删除动作的源视频 / 源帧导出。** `NNN.png` 是派生产物；抠图、去溢色、画布迁移都是有损且不可逆的。
- 当前白色兜帽套装的源素材已经丢失；`desktop_pet_image/视频文件/女仆装/` 是 **另一套女仆装皮肤**，不能当作当前动作的源。
- 每个动作目录按约定应有 `SOURCE.txt`（源文件名 + 抽帧命令 + 抠图参数）；大文件放仓库外并记录绝对路径。
- 绿色键在蓝色角色上会留下 **青色环**（`G≈B≫R`），`despill` 的 `G=min(G,max(R,B))` 对青色是恒等变换，修不掉；详见 `docs/guides/edge-repair.md`。

工具：

| 工具 | 用途 |
|---|---|
| `tools/fix_edges.py` | 修青色/绿色边缘环、透明区幽灵 RGB、孤立白点 |
| `tools/migrate_canvas.py` | 384×456 / 384×384 → 384×512，顶部补透明；同步/重建 bounds |
| `tools/make_emoji.py` | 图片/GIF → `emoji_NN.png` |
| `rgba-convert` MCP | 批量纯色背景 → RGBA PNG；支持 GIF 逐帧 |
| `docs/guides/asset-packaging.md` | PNG 图集 vs 动画 WebP 的实测；结论：GDI+ 不支持 WebP，图集方案未落地 |
| `docs/guides/edge-repair.md` | 边缘环诊断与修复流程 |

不要在本仓库里重新缩放/重新计时帧；素材以 384×512 RGBA、主体高度约 0.68H、基线约 460 的约定进入。
