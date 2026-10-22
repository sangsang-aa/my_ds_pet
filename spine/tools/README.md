# spine/tools — Spine 绑定与验证工具

这三个脚本只依赖 Python 3 + Pillow，没有外部服务，也不需要联网（第一次生成
预览除外，见下）。

## `render.py`

离线的 Spine 4.2 渲染器，只支持 region 附件，用于生成 `verify/` 和核对清单。
坐标系：世界坐标 Y 向上，画布坐标 Y 向下（左上角原点）。

```
python3 spine/tools/render.py out.png --json spine/front/whale_girl.json \
        --atlas spine/front/whale_girl --clip wave --time 0.5
```

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

## 建议顺序

改完骨架后：

```
python3 spine/tools/regen_manifests.py front
python3 spine/tools/make_verify.py front
python3 spine/tools/make_preview.py front
git diff --stat spine spine-parts-v2-20261020
```

## 注意

- Spine 编辑器**重新导出**会覆盖 JSON 里手改的东西（正面袖子、侧面毛边、背面
  裙摆都中过一次），导出后先 `git diff` 再提交。
- `render.py` 只处理 region 附件和骨骼动画，不处理网格、IK、约束、蒙皮。
