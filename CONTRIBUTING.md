# Contributing

本项目的工作约定。面向人，也面向 AI 协作者。工具的底层踩坑记录（构建路径细节、
@file 正斜杠、GDI+ 不支持 WebP 等）在 [`AGENTS.md`](AGENTS.md)，本文讲协作流程。

## 开工前

1. 先读 [`README.md`](README.md)（用户视角）与
   [`docs/architecture/README.md`](docs/architecture/README.md)（当前代码事实）。
2. 改代码前先读 [`docs/architecture/03-modules-and-contracts.md`](docs/architecture/03-modules-and-contracts.md)
   的模块边界与不变量；改行为/配置要同步更新对应的架构文档。
3. **任何改动先提方案、获批再动手**（包括 bug 修复）。不要直接编辑。

## 分支与提交

- 工作分支：`AIvideo`，跟踪 `origin/AIvideo`（remote `sangsang-aa/my_ds_pet`）。
- `E:\pet_demo` 是 Windows 构建副本，另有 WSL 一份在 `/home/zh180/pet_demo`；
  **除非明确要求，不要互相同步文件**。
- 提交信息用英文祈使句：`Add …`、`Fix …`、`Document …`。
- 推送前先确认构建和 8 个测试全部通过；**不要 force-push**。

## 改代码

- 源码在 `src/`，按 `main` / `core` / `pet` / `bubble` 分层；新增功能建议独立
  `src/<feature>.*`，只依赖 `config.h` / `util.h`，不要直接改 `pet.cpp`。
- 加行为：在 `MutableBehaviors()` 追加一条 `Spec`，并放好
  `desktop_pet_image/<action>/NNN.png` 与 `bounds.txt`；`pet.cpp` 不需要改。
- 保持纯逻辑头（`behavior.h`、`bounds.h`、`walk_logic.h`、`drag_logic.h`、
  `sweep_logic.h`、`bubble_logic.h`、`inifile.h`）不含 `<windows.h>`，
  这样测试可以在任意主机编译。

## 构建与测试

```powershell
taskkill /IM pet_demo.exe /F        # 运行中的宠物会锁住 exe，先关掉
cmd /c build.bat                     # 依次尝试 cl → g++ → CMake
```

8 个纯逻辑测试（约 1 秒一个）：

```powershell
foreach ($t in 'alpha_mask_test','behavior_test','bubble_logic_test','drag_logic_test',
              'inifile_test','sweep_logic_test','walk_logic_test','bounds_scale_test') {
  g++ -std=c++17 -Wall -Wextra -O2 "tests\$t.cpp" -o "build\$t.exe"; & "build\$t.exe"
}
```

新增测试：逻辑放纯头文件、`tests/<name>_test.cpp` 用 `assert`、零警告，
并把测试名加入 `AGENTS.md` 的循环列表，避免漏跑。

## 资产约定（硬规则）

- **永不删除动作的源视频 / 源帧导出**。`NNN.png` 是有损派生；key 错了只能从原片重新 key。
- 当前白兜帽套装的源素材已丢失；`desktop_pet_image/视频文件/女仆装/` 是另一套皮肤，
  不能当作当前动作的源。
- 每个动作目录应有 `SOURCE.txt`（源文件名 + 抽帧命令 + 抠图参数）。
- 不在此仓库重新缩放 / 重新计时帧；素材以 384×512 RGBA、主体高约 0.68H、
  基线 y≈460 进入。GDI+ 不解码 WebP，动作帧只能用 PNG。
- `emoji/` 与源视频被 gitignore，属本地素材，不随代码提交。

## 文档维护

| 变化 | 要同步的文档 |
|---|---|
| 行为、渲染、资产契约、配置键 | 同提交更新 `docs/architecture/` 对应篇目 |
| 用户可见行为 | `README.md` |
| 新增硬约束 | 先写 `AGENTS.md`，再在架构文档引用 |
| 构建 / 测试步骤 | `docs/architecture/05-build-and-tests.md` 与本文 |
| 值得记录的变更 | `CHANGELOG.md` |
| 一次性排障、实测快照 | `docs/archives/`，只增不改 |

发现「文档说 A、代码做 B」时，以代码为准：先把证据记入归档，再单独修代码或修文档，
不要静默改数字。文档索引用 [`docs/index.md`](docs/index.md)。
