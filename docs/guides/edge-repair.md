# 边缘修复：抠图残留的"绿边"

对象：`desktop_pet_image/<action>/NNN.png`（已抠好的 RGBA 帧）。
工具：`tools/fix_edges.py`。

---

## 1. 症状不是诊断

用户看到的是"绿边"，实测出来是**青色环**：

| 动作 | 环上"青色度" `min(G,B)−R` | 环上"绿色度" `G−max(R,B)` | 结论 |
|---|---:|---:|---|
| drag | **+58.7** | −9.1 | 青色环 |
| peek / busy | **+48.0 / +47.3** | −6.7 / −6.9 | 青色环 |
| walking_left / right | **+45.4** | −8.6 | 青色环 |
| shy | **+38.2** | −6.7 | 青色环 |
| idle | +10.4 | −30.0 | 轻微 |
| clicked / idle2 / walking_down / walking_up | −4 ~ +0.5 | −31 ~ −38 | **干净** |

典型像素 `(98, 135, 135)`：R 被压低，G 与 B 并排抬高。
原因是**绿色键 + 蓝色角色**——绿 `(0,255,0)` 混进蓝紫 `(80,74,127)`，
中间色落在 `G≈B≫R`，视觉上是青而不是绿。

> 所以所有 "green despill" 都是**恒等变换**：标准实现是
> `G = min(G, max(R,B))`，对这个像素 `G=135`、`max(R,B)=135`，改完一模一样。
> 跑一百遍也不会变 —— 这就是"处理不掉"的真正原因。

## 2. 同时存在的另外两个缺陷

| 缺陷 | 证据 | 后果 |
|---|---|---|
| `α=0` 的像素仍带 RGB | `clicked` **100%** 的透明像素带色，最大值 255；部分帧残留纯 `(0,255,0)` | 一旦缩放/预乘，颜色被拽回边缘 |
| 孤立的低 alpha 白点 | 每帧 **200~870 个** `α<16` 的亮点，浮在轮廓外 1~2px | 深色桌面上是"毛边/脏边" |

`α=0` 这一条尤其危险：GDI+ 对 `PixelFormat32bppARGB`（straight alpha）做
`InterpolationModeHighQualityBicubic` 时是**逐通道**重采样，不预乘。
透明像素里存的是什么颜色，就会渗进边缘像素。

## 3. 工具选型：能不能治这个症状

| 工具 | 能力 | 对青色环 |
|---|---|---|
| ffmpeg `despill` | 只 clamp 指定通道 | ❌ 恒等变换 |
| OBS / 剪映 / Premiere 抠像 | 阈值 + 羽化 | ❌ |
| DaVinci Resolve Delta Keyer + Spill Suppressor | Spill Suppressor 也是 clamp 型 | ⚠️ 半效，要配 Matte Control 的边缘处理 |
| After Effects Keylight | Screen Shrink/Grow + **Edge Colour Correction** + Despill Bias | ✅ 业界标准 |
| Blender Compositor Keying | 免费、可脚本化 | ⚠️ 有 despill，无边缘取色 |
| rembg / `isnet-anime` / BiRefNet / SAM2 | 不依赖键色，直接出 alpha | ✅ 最省心（需要源） |
| **`tools/fix_edges.py`** | 用内侧颜色重建环，不猜键色 | ✅ 已验证 |

**关键前提：这些"重抠"方案都需要原视频。** 见第 6 节。

## 4. `tools/fix_edges.py` 做什么

对每一帧：

1. **丢弃孤立白点** —— `0 < α < 16` 且不与 `α ≥ 64` 相邻的像素，`α := 0`。
   正常柔边因为紧贴实心像素而被 `dilate(a>=64, 1)` 保护，不会被误删。
2. **建立"可信内侧"** —— `α ≥ 250` 且向内腐蚀 2px。
3. **传播最近的内侧颜色** —— 8 邻域迭代扩散，得到每个像素"本该是什么颜色"。
   不再猜键色，所以绿的、青的、蓝的、白的素材都能用。
4. **只修环上被污染的那部分** —— 同时满足：
   - 距轮廓 ≤ `--ring`（默认 2px）
   - 相对内侧颜色有 > `--diff`（默认 12）的通道偏移
   - 色偏方向是绿（`G−max(R,B) > 8`）或青（`G−R > 20 且 B−R > 20`）
   
   中间色按 `--strength` 混合，默认 1.0。
5. **处理透明区的 RGB** —— `--alpha-rgb extend`（默认）把内侧颜色向外延
   `ring+3` px，再零化其余部分。**不要用 `zero`**：透明的黑像素在 0.7 双三次
   缩放下会在边缘拉出暗边；填成内侧颜色才是干净的。

### 用法

```powershell
# 先看会改什么，不写盘
python tools/fix_edges.py desktop_pet_image --recursive --dry-run

# 单个动作，原地修改（默认保留 .orig 备份）
python tools/fix_edges.py desktop_pet_image\walking_left

# 全部动作；仓库里帧已被 git 跟踪时，可以省掉备份
python tools/fix_edges.py desktop_pet_image --recursive --no-backup

# 保守模式：只替换 50%，或只处理青色不碰绿色
python tools/fix_edges.py desktop_pet_image\shy --strength 0.5
python tools/fix_edges.py desktop_pet_image\idle --green-gain 255
```

`emoji/`、`tmp/`、`视频文件/`、`_unused/` 会被跳过；只处理 `NNN.png`。

### 实测效果

全量跑过一遍（308 帧，11 个动作目录，72 秒 ≈ 230 ms/帧，纯 numpy + PIL，无 scipy）：

| 指标 | 修复前 | 修复后 |
|---|---|---|
| 环上青色度（6 个问题动作） | +38 ~ +59 | **−4.7 ~ +1.7** |
| 环上绿色度 | −6.7 ~ −9.1 | −31 ~ −44（正常，蓝紫色角色本来就该是负的） |
| 透明区纯绿幽灵像素 | 每帧 200~250 | **0** |
| 孤立白点（面积 ≤4 的小岛） | 每帧 16~82 个岛 | **0** |
| PNG 体积（11 个动作合计） | 44.34 MB | 44.88 MB（+1.2%，边缘延色让压缩率略降） |

幂等：第二遍全部报 `[clean]`，不写盘。

### 验收标准

```powershell
python tools/fix_edges.py desktop_pet_image --recursive --dry-run
# 期望：summary ... clean=308 would-fix=0
```

单帧指标：`rim_cyan ≤ 8`、`rim_green ≤ 6`、`ghost_px = 0`、`speck_px = 0`。

---

## 5. 什么时候该重抠而不是修边

修边只改 1~2px 的环，**信息是不可恢复的**——它只能让错误变得不可见。
以下情况必须回到源：

- 边缘发丝/半透明布料被抠没了
- 环宽 > 3px（`--ring` 调大也只是把更多边缘吃掉）
- 角色身上出现大面积绿色反弹（白毛、白袜子先中招）
- 需要 4:4:4 级质量的边缘

---

## 6. 硬标准：原视频必须保存

> **每个动作的原始视频 / 原始帧序列必须留档，永不删除。**

理由（这次就是代价）：

- `NNN.png` 是**派生产物**。抠图、despill、画布迁移都是有损且不可逆的。
- 一旦键错了，唯一的干净解法是**用源重抠**；源没了就只能修边。
- 这批帧的源**已经丢了**：仓库 `desktop_pet_image/视频文件/` 里只有 10 个 mp4，
  且**全是女仆装**（另一套皮肤）。当前白兜帽这套的源不在仓库里，
  所以只能走 `tools/fix_edges.py` 修边。

要求：

1. 源放在 `desktop_pet_image/视频文件/<action>/`（或用户在仓库外的固定目录），
   **不要**在抠图后删除。
2. 源文件大于 50 MB 时不要进 git，改为在 `AGENTS.md` / 动作目录的 `SOURCE.txt`
   写明绝对路径或网盘位置。
3. 动作目录里留一行 `SOURCE.txt`：源文件名 + 抽帧命令 + 抠图参数，
   保证半年后还能复现。
4. 删任何 `NNN.png` 之前，先确认对应源还在。**没有源的帧不要覆盖、不要重命名。**

---

## 7. 以后生成绿幕素材的约束

| 约束 | 原因 |
|---|---|
| 导出 **PNG 序列**，不要 h264/h265 | 4:2:0 的色度只有一半分辨率，边缘 1~2px 的色度已被糊掉，事后救不回 |
| 绿幕要**均匀打光**，角色离幕 ≥ 0.5 个身位 | 绿色反弹会落在白毛、白袜、白兜帽上 |
| 抠图用 **unmix（反解）**，不是 despill | `F = (C − (1−α)·B) / α` 才是恢复前景色；clamp 只是把错误藏起来 |
| 白色区域单独检查 spill | 白毛最容易吃绿，而轮廓检测看不到它 |
| 保留源 | 见第 6 节 |
