# 参考工程对照：dsh-whale-musume

对照对象：<https://github.com/Sutera-Diffusus/dsh-whale-musume>（MIT，214 star，160 commits）

结论先行：**它比本工程强的地方不在"功能多"，而在"把不确定的东西关进了确定性的盒子里"**——
状态、时间、资产、宿主契约、失败模式，各自都有明确边界和回归测试。

本文只记录**可执行的差距**，不评价画风与题材。

---

## 0. 先分清物种，否则比较会失真

| | 本工程 `pet_demo` | 参考工程 `dsh-whale-musume` |
|---|---|---|
| 形态 | 独立原生 Win32 分层窗口 exe | 注入宿主网页的**前端插件**（DSH / Electron） |
| 透明 / 置顶 / DPI | 自己实现（`UpdateLayeredWindow` + `SetProcessDpiAwarenessContext`） | 宿主免费提供 |
| 动画 | 逐帧 PNG 序列，10 fps，真位移 | 92 张**静态立绘** + CSS `translate/rotate` 摇摆 |
| 代码量 | `src/` ≈ 1 900 行 C++ | core 2 448 + presenter 3 742 + client 778 = ≈ 7 000 行 JS |
| 分发 | 仓库内提交 exe | npm 包 + Release zip + SHA256SUMS |
| 生命周期 | 自己跑自己 | 寄生在别的 App 里，必须处理宿主版本漂移 |

所以：**动画质量、独立性、资产生产天花板是本工程的真优势**（见第 3 节），
下面第 1–2 节只讨论"工程纪律"这一层它赢在哪。

---

## 1. 运行时逻辑：一个纯函数吃掉所有判断

### 它怎么做

`assets/whale-moe-core.js`（2 448 行，零 DOM 依赖）的核心只有一个函数：

```js
computeState(prev, signals, now, rng) -> { state, pose, line, speak, since, streak, mode }
```

- 输入只有三样：结构化信号 `signals`、**注入的时间 `now`**、**注入的随机源 `rng`**
- 优先级写死成一条 else-if 链，顺序即语义：
  `error > tool > thinking > success > curious > waiting > afk > idle`
- 输出是纯数据；表现层拿到后只做"贴图 + 说话 + 排版"，不再判断任何东西
- 因为时间可注入，任何历史时刻都能**离线重放**

同目录的 `HIT_ZONES` + `hitZone(nx, ny, poseSet)` 也是纯函数：
归一化 `[0,1]` 的尾 / 头 / 腹三个矩形，越界输入 clamp，未知 poseSet 降级为 peek。

### 本工程现状

`src/pet/pet.cpp:191 Pet::OnTimer()` 把六件事塞在同一个函数里：
冷却递减、扫头计时、状态迁移、`SetWindowPos`、`PresentFrame`、随机决策。
它直接吃 `HWND`，所以"第 300 tick 时宠物应该在哪"**无法离线验证**，只能真跑。

### 怎么落到本工程

把编排从 IO 里切出来，`Pet` 只保留一个薄壳：

```cpp
struct PetInput { int frameCount; bool atEdge; int rng; };   // 纯数据
struct PetOutput { PetState state; int frameIndex; int dx, dy; bool changed; };

PetOutput pet_tick::Step(PetState prev, int tick, const PetInput& in, int& rng);
```

`OnTimer` 只负责把 `PetOutput` 灌进 `SetWindowPos` / `UpdateLayeredWindow`。
做完这一步，现有 6 个测试 + 新增的行为链测试才能真正约束行为，
`pet.cpp` 也才有资格继续重构。

> 优先级：**P0**。其他几条都建立在这条之上。

---

## 2. 测试能覆盖到表现层，而不只是纯逻辑

### 它怎么做

| 项 | 值 |
|---|---|
| 入口 | `npm test` → `node --test test/*test.mjs` |
| 规模 | **209 条**（实测跑通，README 写 203 是旧数字） |
| 覆盖 | 纯逻辑 + **表现层** + **插件装载契约** + 触屏 / 布局 / 存档迁移 |
| 无浏览器手段 | `tools/dom-stub.mjs`（46 KB 手写宿主桩：queue facade / live 模块系统 / slots / 资源路由 + **虚拟定时器**） |
| 接入 | `prepublishOnly` 跑全量；`.github/workflows/test.yml` 在 Node 18 / 22 双版本跑 |
| 回归命名 | 以 bug 命名测试文件：`v251-fixes.test.mjs`（关掉再打开后角色"透明隐身"）、`v252-touch.test.mjs` |

关键那句注释：

> 真执行注入的 core + presenter（不是读源码猜）→ 走真实右键菜单 → 翻牌 → 通关 → 成就落盘

### 本工程现状

6 个 assert 程序，共 ~440 行，纯逻辑（`*_logic.h`、`inifile.h`）；
靠人工 PowerShell `foreach` 循环跑；**`build.bat` 完全不跑测试**；无 CI。
底子其实很好——`behavior.h` / `walk_logic.h` / `drag_logic.h` / `sweep_logic.h` /
`bubble_logic.h` / `inifile.h` 全都不含 `<windows.h>`，只差往上再走一层。

### 怎么落到本工程

1. `build.bat` 末尾追加测试循环（失败即非零退出码）
2. 加一个 ~20 行的 `.github/workflows/test.yml`（Windows runner，`g++` 即可）
3. 第 1 节的 `pet_tick::Step` 落地后，补行为链测试：
   `idle → 权重选行为 → 到点 → 必须播到落地帧 → 回 idle`

> 优先级：**P0**（CI 部分）。目的是把"测试通过"从个人习惯变成合并前提。

---

## 3. 资产管线：母版 + 提示词表 + 审阅关

### 它怎么做

`scripts/gen-assets.py` 的 `POSES` 是一个三元组列表 `(name, face, body, props)`：

```python
("thinking", "eyes looking up to the side with one eyebrow raised...",
             "head tilted, one hand tapping her chin, tail curled into a question mark",
             "one small thought bubble above her head"),
```

配一个固定母版 `BASE = assets/generated/dsh-whale-state-idle-cute.webp`，
提示词里硬写一句（踩坑后才加的）：

> `CRITICAL: do NOT keep the input image's smiling face — redraw the facial expression completely`

后处理是确定性流水线（`scripts/build-assets.py`）：

```
去绿幕（全透明 ≤110 / 羽化 ≤150，带 despill）
  → trim 到 alpha bbox
  → 等比 fit 进 512×512（留 10 px padding）
  → lossless WebP
  → build_contact_sheet() 出 3 列审阅图
```

另有一个本工程没有的东西：`assets/peek-calibration.json`，把每个姿势的绘制区域 +
四边 padding 落成数据，用于把气泡 / 菜单贴到角色身边而不是画布边缘：

```json
"workbench-peek": { "w": 512, "h": 512, "bboxW": 230, "bboxH": 496,
                    "padLeft": 141, "padRight": 141, "padTop": 8, "padBottom": 8 }
```

### 本工程现状（这一块其实更强，但有缺口）

- 抠图（`convert_rgba.py` / MCP `convert_to_rgba`）**比它细致**：flood / chroma 双模式、
  `--clear-corners` 擦 AI 水印、羽化、幂等跳过
- `bounds.txt`（逐帧 `x y w h`）在**动画**场景下信息量比它的单姿势校准更足
- `output/spine-parts-v5-*/` 那套分件流水线（93 个透明 PNG、`layout.json` / `parts.csv`
  记父级与轴心、Spine 4.2 运行时、300 个姿态采样验证、`validation.json`）
  比它"母版出静图"高一个段位

缺口在两处：

1. **没有"母版 + 提示词表"这个单一事实源**。它加姿势只改 `POSES` 一个列表；
   本工程的一致性靠人眼和 commit message（`AGENTS.md` 的 "Asset naming lies" 一节就是这个代价）。
2. **没有自动审阅产物**。它每批生成都出带文件名的 contact sheet；本工程没有。

### 怎么落到本工程

1. 加 `tools/contact_sheet.py`：扫一个 action 目录，出 3 列带帧号的对比图（10 行代码）
2. 加 `desktop_pet_image/actions.ini`（或 JSON）：登记 `display_name` / `kind` / `source`，
   把 `AGENTS.md` 里"walking_up 其实是飞"这类口头知识变成字段
3. `bounds.txt` 升格：追加"哪一侧贴边 / 留白更大"的语义，让气泡和扫头判定读语义而不是猜像素

> 优先级：**P1**。

---

## 4. 宿主契约写成可执行声明，而不是注释

`package.json` 里两块很硬的边界：

```json
"engines": { "dsh": ">=0.1.0-rc.6 <0.3.0-0" },
"dsh": { "compatibility": { "dshReleases": {
  "0.2.0-rc.2": "compatible", "0.2.0-rc.1": "unknown", ... } } }
```

- 大版本变更**主动拒绝启动**（`<0.3.0-0`），而不是"装上了再说"
- 每个宿主版本标 `compatible` / `unknown`，`unknown` 明确写"装得上但不承诺"
- `test/dsh-engines.test.mjs` 把这条门禁也测了

配套 `docs/desktop-0.2.0-rc.2-contract.md`（**94 KB** 的 DOM 契约审计）。里面记的是
"哪些选择器是可靠的活信号、哪些会留残留"。摘一段就知道踩过什么：

> 工具卡 `[data-tool]`……**完成后卡片仍留在 DOM 里**（state 变成 ok/error/stopped），
> 所以只能按状态取值匹配，不能用「存在 `[data-tool]`」当忙信号，
> 否则历史卡片会把桌宠永久钉在工作态。

选择器策略是"**只增不减**"：旧选择器留在 `SIGNAL_BANKS` 里，新宿主加新标记就追加。
这是向后兼容的具体做法，不是口号。

### 本工程现状

`AGENTS.md` 质量很高（`walking_up` 其实是飞、g++ `@file` 里反斜杠是转义、
关 exe 才能链接……），但它是"给人看的经验"，**没有一条被转成可执行断言**。

### 怎么落到本工程

不涉及宿主版本，但同一套思路可以用于**自己对外暴露的契约**：

- `pet_demo.exe --selftest`：加载资源目录 → 校验帧数 / 画布尺寸 / `bounds.txt` 行数 →
  打印报告并以非零退出码失败
- 把 `AGENTS.md` 里"必须同一画布尺寸""`bounds.txt` 行数 = 帧数"这两条变成 selftest 的断言

> 优先级：**P1**。

---

## 5. "失败模式清单"式的产品逻辑

它把设计原则写成三条硬规则：**不打断你 / 不抢焦点 / 不给你添隐私负担**，
每条都对应具体机制：

| 失败模式 | 机制 | 位置 |
|---|---|---|
| 切姿势叠影 / 闪黑 | 双图层 `data-dsh-whale-layer="a\|b"` 交叉，「下压 → 换图 → 弹起」 | `dsh-whale-moe.js:195` |
| 后台标签页动画被暂停 → 卡在中间态 | `pendingSwap` + **1600 ms 超时兜底** | `:214` |
| 忙信号抖一下 → 姿势抽搐 | `STATE_HOLD_MS = { success:3000, failure:3500, curious:2500, tool:1200, thinking:1200 }` | `:1930` |
| 轮询宿主 DOM 拖慢页面 | **单** MutationObserver + 120 ms debounce，**不用常驻 rAF**（文件头第 3 行） | `:15` |
| 冷启动切姿势迟滞 | 先预载 5 张核心，其余**空闲时段每 120 ms 一张**，失败静默 | `:3242` |
| 关掉看板娘后找不回来 | 左下角留 🐋 唤回按钮 | README |
| 关掉再打开 → "透明隐身" | `layerState` 是模块级缓存而 DOM 被重建；**专写回归测试** | `test/v251-fixes.test.mjs` |
| 动画过敏 / 无障碍 | `@media (prefers-reduced-motion: reduce)`；无障碍默认关、`aria-live` 播报 | `dsh-whale-moe.css:219` |

本工程**同源同款**地解决过一批：`kTopmostReassertTicks` 重夺置顶、`safeToStop`
（必须播到落地帧才回 idle）、`WM_CAPTURECHANGED` 取消拖拽、拖拽夹紧到屏幕内、
`kHeadBandBottomInsetPercent`（注释写明"固定像素 inset 会让判定带塌到 9 px，
落进兜帽上方的透明缝，永远触发不了"）。

差别在**纪律性**：

- 它每个修过的坑都有一个以坑命名的测试文件
- 本工程的坑写在代码注释和 `AGENTS.md` 里，下次重构只能靠人记得
- 它留了 `window.__dshWhaleMoeDebug`（`state / pose / holdLeft / layers.pendingSwap /
  workSpeech.left`）做运行时透视；本工程只有启动时 `printf` + 启动失败 MessageBox

### 怎么落到本工程

1. `pet_demo.exe --debug`（或按住 Ctrl 右键）：打印
   `state / clip / frame / tick / cooldown / subjectBounds / windowRect`
2. 每修一个视觉 bug，补一个 `tests/` 用例（哪怕是纯逻辑层的小断言）

> 优先级：**P1**（debug 入口）/ **P2**（回归命名）。

---

## 6. 发布工程

它有：`CHANGELOG.md`（35 KB）+ 每版 `docs/release-notes-vX.md` +
`tools/prepare-release.mjs` / `build-release-zip.mjs` / `create-release.mjs` +
Release 附 `SHA256SUMS` + npm 发布/回滚手册 + Issue/PR 模板 + SECURITY / COC。

本工程：`AGENTS.md` 明确"commits stay local — don't push"，`pet_demo.exe` 直接提交进仓库，
无 CI、无 Release、无版本号。

**这在"自己用的 demo"阶段完全合理**，只在"要发给别人跑"时才变成债务。
如果哪天要发：`--selftest` + zip + SHA256 就够，不需要学它的全套模板。

> 优先级：**P2**。

---

## 7. 反过来：本工程更强的地方

比较时别把这几点丢了。

1. **动画质量**：它 92 张全是静态立绘，"动"靠 CSS `wm-breathe` / `wm-sway` / `wm-hop`
   （±3 px 位移、±1.2° 摇摆）。本工程是 30 帧序列 @ 10 fps 逐帧 `UpdateLayeredWindow`，
   `walking_left` 是完整走路循环——这是真优势。
2. **不依赖宿主**：它只能覆盖 DSH 那个窗口；本工程浮在任何应用之上
   （代价是背上 `WM_CAPTURECHANGED`、置顶被抢、DPI 这一整套 Windows 税）。
3. **资产生产天花板**：`output/spine-parts-v5-*/` 的分件 + 骨架 + 300 姿态采样验证，
   比它"母版出静图"高一个段位。它路线图里还写着"可选角色包：把表现层抽出接口"——
   本工程其实已经在做更硬的那条路。
4. **加行为的成本**：本工程加一个行为 = `MutableBehaviors()` 追加一行 `Spec` + 丢一个目录；
   它加一个姿势要同时动 `POSES`、`LINES`、`HIT_ZONES`、`IDLE_ACTION_POOL`、预载名单
   和 3 742 行的 presenter。**注册表这个设计本工程更干净。**

---

## 8. 借鉴优先级

| 级别 | 事项 | 收益 |
|---|---|---|
| **P0** | `pet_tick::Step` 纯 tick（第 1 节） | 让 `pet.cpp` 可测试、可重构 |
| **P0** | `build.bat` 接测试 + 一个 CI（第 2 节） | 测试从"要记得跑"变成"不跑过不去" |
| **P1** | `--selftest` 契约校验（第 4 节） | 资源问题在启动前暴露 |
| **P1** | `--debug` 运行时透视（第 5 节） | 视觉 bug 的排查成本降一个量级 |
| **P1** | contact sheet + `actions.ini`（第 3 节） | 资产一致性从人眼变成数据 |
| **P2** | `bounds.txt` 加语义 / 回归测试命名 / 发布工程 | 长期可维护性 |

---

## 9. 参考坐标（供后来者定位）

```
assets/whale-moe-core.js        2 448 行  纯状态机（无 DOM）
assets/dsh-whale-moe.js         3 742 行  表现层（单 MutationObserver，无常驻 rAF）
assets/dsh-whale-moe.css          892 行  动效 / reduced-motion
lib/index.js                       65 行  只读资源路由（含 path traversal 防护）
lib/client.js                     778 行  插件注册 + 设置面板
tools/dom-stub.mjs             46 KB     零浏览器宿主桩（虚拟定时器）
test/*.test.mjs                 209 条
scripts/gen-assets.py          POSES 提示词表（母版 + face/body/props）
scripts/build-assets.py        绿幕→trim→fit→WebP→contact sheet
```
