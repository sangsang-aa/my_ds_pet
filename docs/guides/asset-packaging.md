# 帧序列要不要打包成一个文件？（WebP vs PNG 图集）

问题：`desktop_pet_image/<action>/NNN.png` 现在是一个动作 30~40 个文件，
**是否合并成单个 WebP 更好？**

结论先行：

> **"合并成一个文件"是对的；"用 WebP"是错的。**
> 真正的方案是**每个动作一张 PNG 图集 + 一个纯文本清单**，格子按该动作的最大内容框裁剪。
> GDI+ 没有 WebP 解码器，这条路在当前渲染栈上根本走不通。

本文所有数字均为本机实测（见第 6 节复现方法），不是估算。

---

## 1. 现状实测

### 磁盘

| 动作 | 帧数 | 体积 | 均帧 | 画布 |
|---|---:|---:|---:|---|
| idle | 1 | 0.14 MB | 144 KB | 384×512 |
| idle2 | 30 | 4.63 MB | 158 KB | 384×512 |
| walking_left / right | 30 / 30 | 3.78 / 3.78 MB | 129 KB | 384×512 |
| walking_up / down | 27 / 30 | 4.15 / 4.02 MB | 137–158 KB | 384×512 |
| clicked / drag | 30 / 30 | 4.59 / 4.21 MB | 144–157 KB | 384×512 |
| shy | 40 | 6.05 MB | 155 KB | 384×512 |
| busy / peek | 30 / 30 | 4.58 / 4.41 MB | 150–156 KB | 384×512 |
| **合计** | **308** | **44.34 MB** | ≈147 KB | 全部 RGBA |

### 运行期（这才是瓶颈）

`Pet::Load()` 把 idle + 7 个行为 clip **一次性全解码**成 `HBITMAP`：

```
预载帧数        218
GDI+ 解码耗时   2 331 ms      ← 启动时全部发生在 WinMain 里，窗口还没出现
DIB 占用       163.5 MB       ← 218 × 384 × 512 × 4 B，且每帧一个 GDI 对象
```

> 耗时用 PowerShell + `System.Drawing`（即 GDI+）测得，含脚本层每帧建对象的开销，
> 绝对值偏大；第 2 节的**同口径对比**（散帧 vs 图集）才是可用的结论。

### 内容占比（决定"裁剪"能省多少）

`bounds.txt` 里逐帧主体框的实测平均：

```
全部 308 帧：平均框 269 × 350，内容面积只占画布的 47.9%
```

也就是说 **163.5 MB 里有 85 MB 是全透明 padding**。

---

## 2. 三种候选方案实测对比

以 `walking_left`（30 帧，3.78 MB）为样本：

| 方案 | 体积 | 相对 | GDI+ 解码 | 位图内存 |
|---|---:|---:|---:|---:|
| 30 个独立 PNG（现状） | 3.78 MB | — | **215 ms** | 22.5 MB |
| PNG 图集 6×5（2304×2560，不裁剪） | 3.58 MB | **−5%** | **121 ms** | 22.5 MB |
| PNG 图集 6×5（裁剪到 247×355 格） | 3.50 MB | **−7%** | **56 ms** | **10.0 MB** |
| 动画 APNG | 3.56 MB | −6% | 不支持 | — |
| 动画 WebP（lossless） | 2.42 MB | −36% | **不支持** | — |
| 动画 WebP（q90） | 0.86 MB | −77% | **不支持** | — |

三个反直觉的结论：

1. **打包对磁盘体积几乎没用**（−5%）。PNG 对整片 alpha=0 的区域压缩率极高，
   308 个文件的头部开销分摊到 147 KB/帧 里可以忽略。
   —— 想靠"合并成一个文件"省仓库体积，这个动机不成立。
2. **裁剪对磁盘也几乎没用**（−7%），但对**内存**是腰斩（22.5 MB → 10 MB）。
   因为透明区域在磁盘上几乎免费，在 DIB 里却照样按 4 B/px 付费。
3. **图集真正的收益是时间和对象数**：解码 215 → 121 ms，裁剪后再降到 56 ms；
   `HBITMAP` 从 218 个变成一个动作 1 个。

---

## 3. 为什么不能是 WebP

### 硬理由一：GDI+ 解不了

当前渲染栈是 `Gdiplus::Bitmap` → `GetHBITMAP` → `UpdateLayeredWindow`。
GDI+ 内置解码器只有 **BMP / GIF / JPEG / PNG / TIFF / ICO / WMF-EMF**，没有 WebP
（`AGENTS.md` 里已经写明"never WebP"）。换成 WebP 意味着：

- 要么引入 **libwebp**：多一个第三方源码树，且必须同时喂饱 cl / MinGW g++ / CMake
  三条构建路径、x86 + x64 两个产物、以及"exe 必须和素材同目录"这个约束
- 要么走 **WIC**：`IWICImagingFactory` + COM 初始化。但 WebP 的 WIC 编解码器来自
  Microsoft Store 的「Webp Image Extensions」，**不是所有 Win10 / LTSC / 全新装机上都有**。
  一旦缺失就是"装好了但看不见宠物"，正好踩中本工程最讨厌的那类线上问题。

### 硬理由二：动画 WebP 更不可靠

动画 WebP 需要 `WebPAnimDecoder`（libwebp demux）。WIC 那条路对多帧 WebP 的支持
不是可依赖的契约（不同版本的 Store 编解码器行为不一致）。
本工程还要 10 fps 稳定推进，不能接受"某些机器上只出第一帧"。

### 硬理由三：性价比不成立

- lossless WebP 只比 PNG 图集小 36%（2.42 vs 3.50 MB），却换来一个新依赖 + 两套构建
- q90 lossy 小 77%，但这是 10 fps 的硬边线稿角色，逐帧振铃 / 边缘蠕动会非常明显
  （静态立绘可以接受，动起来每一帧都在闪）

**结论：容器换成 WebP 是净负债。**

---

## 4. 推荐方案：每动作一张 PNG 图集 + 清单

### 收益（按上一节实测外推）

| 指标 | 现在 | 图集 + 裁剪 | 变化 |
|---|---:|---:|---|
| 启动解码 | 2 331 ms | **≈ 400–600 ms** | −75% |
| 运行内存 | 163.5 MB | **≈ 75–85 MB** | −50% |
| GDI 位图对象 | 218 | **8** | −96% |
| 切动作耗时 | 0 | 一个 `SelectObject` | ≈ 0 |
| 资产原子性 | 半替换的目录就是坏资产 | **一个文件 = 一个 clip** | 消除一整类故障 |

最后一条比性能更重要：现在"删掉多余的 `NNN.png`"是 `AGENTS.md` 里的口头约定，
忘了就会读到上一版残留帧；换成图集后，这个坑从设计上消失。

### 清单格式：复用仓库自己的解析风格

不引 JSON（C++ 侧无解析器）。用和 `bounds.txt` 一样的"逐行 sscanf"风格：

```
# atlas.txt
# columns rows cell_w cell_h canvas_w canvas_h
6 5 247 355 384 512
# 每帧一行：cell_x cell_y offset_x offset_y
# cell_x/cell_y 是图集里的格子坐标，offset 是该格左上角在 384×512 画布上的位置
0 0 68 101
1 0 68 96
...
```

`bounds.txt` 的语义**完全不变**（画布坐标下的主体框），继续给气泡和扫头判定用。

### 加载器改动

1. `assets.cpp`：新增 `LoadAtlas()`，一次 `gdi::Bitmap(path)` + 一次 `GetHBITMAP`
2. `Frame` 增加 `srcX, srcY, drawX, drawY`（图集内位置 / 画布位置）
3. `render::PresentLayered()`：先用 `BitBlt(SRCCOPY)` 把 `srcRect` 贴到 `memDC` 的
   `(drawX, drawY)`，再 `UpdateLayeredWindow`。每帧多一次 247×355 的 blit，10 fps 下可忽略
4. **兼容期**：目录里有 `atlas.txt` 就走图集，没有就回退现有的 `NNN.png` 逐帧加载。
   保留"丢一个目录就能跑"的作者体验，也让 `tools/migrate_canvas.py` 这类脚本继续可用

### 工具

`tools/pack_atlas.py`（一次性、可重跑、幂等）：

```
读 action 目录 → 取全部帧的最大 alpha bbox → 定格子尺寸
  → 6 列排布（保持单边 ≤ 4096）→ 写出 atlas.png + atlas.txt
  → 原 NNN.png 不删（先并存，验证通过后再删）
```

### 验收

`pet_demo.exe --selftest`（见 `reference-dsh-whale-musume.md` 第 4 节）新增一条：
**图集逐帧像素与散帧逐帧像素一致**（整帧 hash 或按 `bounds.txt` 采样比对），
确保打包无损。

---

## 5. 如果只想改一处

按投入产出比排序：

| 诉求 | 最小改动 | 收益 |
|---|---|---|
| **启动慢**（2.3 s 白屏） | 打包图集（第 4 节） | 2.3 s → 0.5 s |
| **内存 163 MB** | 只裁剪帧到 bbox（不动加载器结构，`Frame` 加 draw 偏移即可） | −50% |
| **仓库 44 MB** | 都不划算：图集 −5%、WebP 需新依赖。真正的办法是**少提交动作**或拆资源包 | — |
| **"忘了删残留帧"这类 bug** | 打包图集 | 从设计上消除 |

> 一句话：**磁盘体积不是问题，启动时间和内存才是问题；而这两个都由"每帧一张整幅画布"
> 造成，跟文件格式无关。**

---

## 6. 复现方法

```powershell
# 磁盘 / 画布
Add-Type -AssemblyName System.Drawing
Get-ChildItem desktop_pet_image -Directory | ForEach-Object {
  $p = Get-ChildItem $_.FullName -Filter *.png | Where-Object { $_.Name -match '^\d{3}\.png' }
  "{0}: {1} frames, {2:N2} MB" -f $_.Name, $p.Count, (($p | Measure-Object Length -Sum).Sum/1MB)
}

# GDI+ 真实解码耗时（System.Drawing 就是 GDI+）
$t=[Diagnostics.Stopwatch]::StartNew()
Get-ChildItem desktop_pet_image\walking_left -Filter *.png |
  ForEach-Object { $b = New-Object System.Drawing.Bitmap($_.FullName) }
$t.ElapsedMilliseconds

# 内容占比（读 bounds.txt）
Get-Content desktop_pet_image\walking_left\bounds.txt | ForEach-Object {
  $p = $_ -split '\s+'; [int]$p[2] * [int]$p[3]
} | Measure-Object -Average
```

图集体积对比用 PIL（本机 12.3.0）：

```python
imgs = [Image.open(f).convert("RGBA") for f in frames]
sheet = Image.new("RGBA", (w*cols, h*rows), (0,0,0,0))   # 6×5
sheet.save("atlas.png", "PNG", optimize=True)
# 对照：imgs[0].save("a.webp", "WEBP", save_all=True, append_images=imgs[1:], lossless=True)
```
