# AGENTS.md — Desktop Pet Demo

Win32 + GDI+ layered-window desktop pet. Single C++17 app, no framework, no test
framework. `README.md` documents behaviour; only non-obvious, hard-won facts here.

## Workflow expectation
- The user runs this repo under the `ai-coding-protocol` skill: **present a
  proposal and wait for approval before editing anything.** Bug fixes too.

## Where things live
- Canonical source = this WSL tree, `/home/zh180/pet_demo`.
- `/home/zh180/desktop_pet_image` is a **symlink** into `pet_demo/desktop_pet_image`.
- A separate **Windows build copy** exists at `E:\pet_demo` (own `.git`). Never sync
  files between the two unless the user explicitly asks.
- Tooling lives **outside** the repo in `/home/zh180/MCPS/` (that dir is NOT a git repo).

## Build
- Windows: `build.bat` (tries `cl`, else CMake). MinGW:
  `x86_64-w64-mingw32-g++ -std=c++17 -O2 -mwindows src/*.cpp -lgdiplus -luser32 -lgdi32 -o pet_demo.exe`
- **Close the running pet first** or linking fails with
  `cannot open output file pet_demo.exe: Permission denied` (Windows locks the exe):
  `taskkill /IM pet_demo.exe /F 2>nul`
- This WSL box has **no mingw / cmake / wine** installed — only g++, so nothing here can
  build or run the GUI as-is. Cross-compiling is possible with a throwaway rootless
  toolchain (`apt-get download` + `dpkg -x` into `/tmp`); it does not survive a `/tmp` wipe.

## Tests
Pure-logic assert programs (no framework); they exercise the `<windows.h>`-free headers:
```
g++ -std=c++17 -Wall -Wextra -O2 tests/bubble_logic_test.cpp -o /tmp/bt && /tmp/bt
g++ -std=c++17 -Wall -Wextra -O2 tests/walk_logic_test.cpp   -o /tmp/wt && /tmp/wt
python3 -m pytest /home/zh180/MCPS/test_convert_rgba.py /home/zh180/MCPS/test_fetch_emoji.py
```
Behaviour/visual changes can only be verified by the user on Windows.

## Architecture (not visible from filenames)
- `src/main.cpp` is WinMain wiring only. Behaviour is in modules: `config.h` (all
  constants) · `util.*` · `assets.*` (`Frame`, `LoadFrames`) · `render.*`
  (`render::PresentLayered`) · `pet.*` (`struct Pet`: idle↔walk state machine, input) ·
  `bubble.*` (`struct Bubble`: own window + timer, click-through, follows the pet HWND).
- `bubble_logic.h` / `walk_logic.h` are deliberately **pure (no `<windows.h>`)** — keep
  new testable logic there so `tests/` can run on Linux.
- **GDI+ decodes png/jpg/gif/bmp only — never WebP.** Convert webp to PNG for assets.
- Frames: `desktop_pet_image/<action>/NNN.png`; loading stops at the first missing frame
  (max 120); scaled by `kScaleFactor = 0.70`. Paths resolve relative to the **exe dir**,
  so `pet_demo.exe` and `desktop_pet_image/` must be siblings.
- `desktop_pet_image/emoji/` is intentionally gitignored (third-party stickers, local only).

## Git
- Remote is SSH (`git@github.com:sangsang-aa/my_ds_pet.git`). The HTTPS token in
  `~/.git-credentials` **cannot write** (403) — push over SSH.
- Develop on `main`. `image` is an older asset branch, kept as-is.
- Commit messages: imperative English (e.g. `Add ...`, `Fix ...`, `Document ...`).
