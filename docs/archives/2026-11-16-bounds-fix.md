# 2026-11-16 修复：`bounds.txt` 缩放

## 元数据

| 项 | 值 |
|---|---|
| 仓库 / 分支 | `E:\pet_demo`，`AIvideo` |
| 基线 commit | `d88b53e991f31430435c35ae7adcdc929d06af56` |
| 修复 commit | `58881d2601cca5e2079b3926de7497d5b67090dc` |
| 修复时间 | 2026-11-16 23:02:09 +0800 |
| 范围 | R1（bounds 缩放）+ R2（README 同步）；不改资产、不重跑抠图 |
| 测试 | 8/8 通过 |

## 问题

`bounds.txt` 是 **384×512 源画布坐标**，但 `Pet::PresentFrame` 直接把它加到 269×358 窗口的屏幕坐标上，没有乘 `scale_factor`。气泡和扫头共用 `SubjectBounds`，所以：

- 气泡水平中心偏右（`idle` 第一帧算成 192 vs 窗口中心 134.5）；
- 侧面气泡离身体更远；
- 扫头判定带下移，落在脸/上半身而不是头顶。

命中测试不受影响：`AlphaMask` 本来就在缩放后的帧上生成。

## 改动

| 文件 | 内容 |
|---|---|
| `src/core/bounds.h` | 新增纯函数 `ScaleSubjectBounds(bx,by,bw,bh,scale,frameW,frameH)`；`lround` 取整、负坐标裁剪、右下越界夹取、空/完全越界返回 `valid=false` |
| `src/pet/pet.cpp` | `PresentFrame` 用 `GetSettings().scaleFactor` 和 `petW/petH` 换算后再发布 `SubjectBounds`；无效 bounds 回退整块窗口 |
| `src/core/assets.h/.cpp` | 注释明确 `Frame.bx/by/bw/bh` 是源画布坐标；`LoadFrameBounds` 不变 |
| `tests/bounds_scale_test.cpp` | 新增第 8 个纯逻辑测试 |
| `AGENTS.md` | 七 → 八个测试；补充 `bounds.txt` 源坐标/换算说明 |
| `README.md` | 同步基底池、竖向播放、头部带、帧数、气泡尺寸、emoji 命名、启动日志、`pet.ini` 注释规则、开发文档索引 |

### 换算规则

```cpp
x = lround(bx * scale);
y = lround(by * scale);
w = lround(bw * scale);
h = lround(bh * scale);
// x<0: w += x, x=0；y<0: h += y, y=0
// 右下越界夹到 frameW/frameH
```

`bounds.txt` 和 `Frame` 的源坐标语义保持不变；转换只发生在 `PresentFrame`。

## 修复前后

`idle/bounds.txt` 第一行：

| | x | y | w | h |
|---|---:|---:|---:|---:|
| 源坐标（384×512） | 61 | 110 | 262 | 353 |
| 修复后（269×358，0.70） | 43 | 77 | 183 | 247 |
| 修复前直接使用 | 61 | 110 | 262 | 353 |

## 验证

### 构建

```text
$ cmd /c build.bat
[pet] build OK: pet_demo.exe via g++
```

| 项 | 值 |
|---|---|
| 编译器 | MinGW-w64 g++ 16.2.0 |
| `pet_demo.exe` | 267,425 bytes |
| 构建时间 | 2026-11-16 22:57:08 |

### 测试

```text
alpha_mask: all tests passed
behavior: all tests passed
bubble_logic: all tests passed
drag_logic: all tests passed
inifile: all tests passed
sweep_logic: all tests passed
walk_logic: all tests passed
bounds_scale: all tests passed
```

`bounds_scale_test` 覆盖：

- `idle` 实际源坐标 → 预期窗口坐标 `43 77 183 247`；
- 负坐标裁剪；
- 右下越界裁剪；
- 完全在窗口外；
- 0 宽/0 高/scale=0/frame 尺寸为 0；
- `scale=1.0` 恒等；
- `scale=0.5` 舍入。

### 运行与气泡目视

用临时 `pet.ini` 把 `[bubble] min_ticks/max_ticks` 调到 10（约 1 秒），运行 6 秒后通过 Win32 查询：

| 窗口 | 可见 | rect（屏幕坐标） |
|---|---|---|
| 宠物 | 是 | `1145,541,1414,899` |
| 气泡 | 是（`IsWindowVisible=True`） | `1160,362,1412,614` |

气泡 rect 在宠物主体上方；截图保存为 `build/bubble_check3.png`（构建产物，gitignored）。临时 `pet.ini` 已删除，工作区没有残留。

启动日志（实测）：

```text
[pet] idle 'idle' loaded=1/120 frames
[pet] random idle enabled across 1 animations
[pet] walking_left loaded=30/120 frames
[pet] walking_right loaded=30/120 frames
[pet] walking_up loaded=27/120 frames
[pet] walking_down loaded=30/120 frames
[pet] clicked loaded=30/120 frames
[pet] drag loaded=30/120 frames
[pet] shy loaded=40/120 frames
[pet] emoji loaded=6 stickers
[pet] bubble enabled (252x252, 6 stickers)
```

## 文档同步

- `docs/architecture/02` / `03` / `04`：更新 `Frame` 坐标契约与 `PresentFrame` 换算位置；
- `docs/architecture/04`：R1 从“已知缺陷”改为“已修复”，保留前后数值；
- `docs/architecture/05`：8 个测试、矩阵、基线产物、气泡回归；
- `docs/architecture/06`：R1/R2 状态改为已修复/已同步，保留历史说明；
- `docs/architecture/README`：测试数 8，R1/R2 从当前已知问题移到“最近修复”；
- `docs/archives/2026-11-16-baseline.md`：**未修改**，保留修复前 7/7 与“未修复”的历史状态。

## 剩余风险

本次没有处理：

- `idle2` / `busy` / `peek` 未注册（R3）；
- `core/settings.cpp` → `pet/behaviors.h` 反向依赖（R4）；
- `Spec::minTicks` 语义/注释不一致（R5）；
- `FullCycle` 时长可能超过 `maxTicks`（R6）；
- 多显示器、DPI 变化、全量预载、Win32 自动化测试等。

完整列表见 [`../architecture/06-decisions-and-risks.md`](../architecture/06-decisions-and-risks.md)。

## 复现

```powershell
cd E:\pet_demo
taskkill /IM pet_demo.exe /F 2>$null
cmd /c build.bat
foreach ($t in 'alpha_mask_test','behavior_test','bubble_logic_test','drag_logic_test',
              'inifile_test','sweep_logic_test','walk_logic_test','bounds_scale_test') {
  g++ -std=c++17 -Wall -Wextra -O2 "tests\$t.cpp" -o "build\$t.exe"
  & "build\$t.exe"
}
```
