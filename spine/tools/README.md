# spine/tools — Spine 绑定与验证工具

这些脚本只依赖 Python 3 + Pillow，没有外部服务，也不需要联网（第一次生成
预览除外，见下）。动作相关的结构检查与语义 diff 见 `anim_lint.py` / `anim_diff.py`。

## `render.py`

离线的 Spine 4.2 渲染器，只支持 region 附件，用于生成 `verify/` 和核对清单。
坐标系：世界坐标 Y 向上，画布坐标 Y 向下（左上角原点）。

```
python3 spine/tools/render.py out.png --json spine/front/whale_girl.json \
        --atlas spine/front/whale_girl --clip wave --time 0.5
```

`--curves` 才会按贝塞尔缓动采样；默认线性是历史 `verify/` 的约定，不要拿默认
输出判断缓动改动。`make_verify.py` 也是线性。

## `make_verify.py`

按骨架重出某个视图的 `verify/`：`still.png`（静止姿势）+ 每个动作 4 帧拼成的
`sheet_<clip>.png`。前视图额外保留 16 张逐帧图。

```
python3 spine/tools/make_verify.py front        # 写入
python3 spine/tools/make_verify.py side --check # 只比对，不写
```

取样时刻固定为 0.0 / 0.25 / 0.5 / 0.75 **秒**（帧名是百分比，历史上从未按动作
时长换算），这样重出一遍可以和仓库里已有的图逐像素对齐——`--check` 就是干这个
的，三个视图目前都是 0 差异。

## `make_preview.py`

把预览页重建成**完全离线**的单文件：骨架 JSON、atlas、图集 PNG，以及官方 Spine
Web Player 的 JS/CSS 全部内嵌为 base64 data URI，双击即可打开。

```
python3 spine/tools/make_preview.py front
python3 spine/tools/make_preview.py all
```

播放器是第三方代码，不入库；首次运行会从 npmmirror 下载
`@esotericsoftware/spine-player@4.2.120` 的 tarball，解出
`spine-player.js` / `spine-player.css` 缓存在 `spine/tools/.cache/`
（该目录已 gitignore）。之后离线可用。

## `regen_manifests.py`

从骨架反推拆件包的清单。**骨架是唯一权威**，`layout.json`、`parts.csv`、
`<view>-setup-spine42.json`、`validation.json` 都由它生成。

```
python3 spine/tools/regen_manifests.py front        # 写入
python3 spine/tools/regen_manifests.py back --check # 只报差异
```

每个槽位写入：`draw_order`（真实层序）、`parent`（驱动骨骼的父级）、
`pivot_xy`（骨骼世界坐标换算成画布像素）、`bbox_xywh`（该零件的 PNG 落在画布
上的位置，宽高恒等于 PNG 尺寸）。当骨架把美术画得比 PNG 大时（正面的分体手臂在
图集里被预热成约 1.09×/1.26×），额外记 `atlas_size` 与 `region_scale`。

清单分四段：

| 段 | 含义 |
|---|---|
| `parts` | 有独立 PNG、且已绑定的零件 |
| `derived_parts` | 骨架里有槽位、但美术是图集里的派生裁切，`parts/` 下没有独立文件（袜子、眉毛、侧面远腿等） |
| `unbound_parts` | 有 PNG、还没绑进骨架（正面的两个拳头） |
| `deprecated_parts` | 有 PNG、已被别的零件取代（旧的整片袖子） |

`*-setup-spine42.json` 保留骨架的全部骨骼，附件按「同一中心、同一旋转、尺寸 =
骨架尺寸」重建，所以它能在 `parts/` 上复现骨架的静止四边形（当前误差
< 0.0001 px）。

## `test_render.py`

`render.py` 的四边形规则测试：附件尺寸 == 区域尺寸时 1:1 绘制；附件比区域大时把
区域拉伸到附件大小（Spine 运行时就是这个语义，已用官方播放器实测确认）；两轴独立
缩放；`x/y` 平移中心；`resample` 参数生效。

```
python3 spine/tools/test_render.py     # 8/8 passed
```

## `sync_parts.py`

**默认只读**：只报告 `parts/` 与图集尺寸不一致或缺失的槽位，不改任何文件。加
`--write` 才会覆盖 `parts/*.png`。

```
python3 spine/tools/sync_parts.py front --select differ,missing     # 只报告
python3 spine/tools/sync_parts.py front --select differ --write     # 真要覆盖
```

`--select` 可选 `differ`（尺寸与图集不符）、`missing`（包里缺文件）、`all`。

为什么默认只读：图集里的区域是**重采样后的副本**，拿它覆盖 `parts/` 会把重采样
固化成唯一副本（2026-10-22 的 `d642d69` 就是这么发生的，已在 `69aed6a` 之前恢复）。
`parts/` 与图集尺寸不一致是**记录在案的状态**，见 `layout.json` 的
`expected_differences`，不是待修的 bug。

## `anim_lint.py`

检查 `bones` / `slots` / `skins` / `animations` 的结构：骨骼重名、父级缺失或
晚于子级、附件缺失、动画引用了不存在的骨骼/槽位、key 时间倒序或重复、曲线不是
4/8 个数、NaN、末 key 超过 duration。`--loop` 指定的 clip 接缝超过阈值算错误。

```
python3 spine/tools/anim_lint.py spine/front/whale_girl.json --loop idle,walk
```

退出码 0 = 无 error（warning 不算失败），1 = 有 error，2 = 读不了文件。

## `anim_diff.py`

两个 rig 的**动作语义** diff：按 clip / 骨骼 / 通道比较，输出最大差值、出现的
时刻、key 数量变化、是否只改了 curve；同时报告每个 clip 的循环接缝。只比较
`animations`，不比较几何/图集。

```
python3 spine/tools/anim_diff.py /tmp/before.json spine/front/whale_girl.json --clip idle
```

退出码 0 = 语义相同，1 = 有差异，2 = 读不了文件。判断动作改动时，先看它，不要
看 `git diff`——仓库的 JSON 是单行 minified，diff 没有可读性。

## `anim_common.py`

上面两个工具共用的解析：首 key 省略 `time` 视为 0、4/8 值曲线、`stepped`、
通道默认值、循环接缝。没有 CLI，不要单独运行。

## 测试

```
python3 spine/tools/test_render.py       # 8/8
python3 spine/tools/test_anim_diff.py    # 10/10
python3 spine/tools/test_anim_lint.py    # 15/15
```

## 建议顺序

改完骨架后：

```
python3 spine/tools/regen_manifests.py front
python3 spine/tools/make_verify.py front
python3 spine/tools/make_preview.py front
git diff --stat spine spine-parts-v2-20261020
```

只改动作时：

```
cp spine/front/whale_girl.json /tmp/before.json
# ... 改 animations ...
python3 spine/tools/anim_lint.py spine/front/whale_girl.json --loop idle,walk
python3 spine/tools/anim_diff.py /tmp/before.json spine/front/whale_girl.json --clip idle
python3 spine/tools/make_preview.py front
python3 spine/tools/make_verify.py front --check    # 几何必须仍是 0 differing
```

## 注意

- Spine 编辑器**重新导出**会覆盖 JSON 里手改的东西（正面袖子、侧面毛边、背面
  裙摆都中过一次），导出后先 `git diff` 再提交。
- `render.py` 只处理 region 附件和骨骼动画，不处理网格、IK、约束、蒙皮。
- `make_verify.py` 是**线性采样**，用了 `curves=False`；它只能证明几何没动，
  不能证明缓动对。动作请用 `render.py --curves` 或 `preview.html`。
- Rigby2D（MIT/Apache）能无损读写 4.2 JSON；但它的 `File ▸ Save` 写的是
  `.rigby` 工程，`File ▸ Export` 会**重打包图集**。只拿导出的 `.json`，
  `.atlas` / `.png` 一律丢弃，然后跑 `anim_diff.py`。
- Rev2D（MIT）导入干净，但导出会加 synthetic root、把 region 烤成 mesh、重打
  图集、按 30fps 采样；只能当沙盒/自检，不能当权威文件。
- 外部编辑器导出的 JSON 可能省略首 key 的 `"time"`（合法，等于 0），
  `render.py` / `anim_diff.py` 已兼容；`make_verify.py` 的产物仍以仓库格式为准。
