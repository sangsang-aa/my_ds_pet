# AGENTS.md — Desktop Pet Demo (Windows copy)

Win32 + GDI+ layered-window desktop pet. Single C++17 app, no framework, no test
framework. `README.md` documents behaviour; only non-obvious, hard-won facts here.

## Workflow expectation
- The user runs this repo under the `ai-coding-protocol` skill: **present a proposal and
  wait for approval before editing anything.** Bug fixes too.
- `E:\pet_demo` is the Windows build copy with its own git. A separate WSL checkout lives at
  `/home/zh180/pet_demo`. Don't sync files between them unless asked.
- Work branch is `AIvideo` and it has **no upstream** (origin only has `main` / `image` /
  `tool`). Commits stay local — don't push.

## Build
- `cmd /c build.bat` — tries `cl`, then `g++` (on PATH at `E:\mingw64\bin`), then CMake.
- **Close the running pet first**, or linking fails with
  `cannot open output file pet_demo.exe: Permission denied` (Windows locks the exe):
  `taskkill /IM pet_demo.exe /F`.
- `build.bat`'s g++ path writes `build\sources.txt` with **forward slashes**: g++ treats `\`
  inside an `@file` as an escape and would mangle `src\main.cpp`.
- `pet_demo.exe` must sit **next to** `desktop_pet_image/`; asset paths resolve from the exe dir.

## Tests
Pure-logic assert programs (no `<windows.h>`, no framework). All six compile and pass with
the MinGW g++ here in about a second each:

```powershell
foreach ($t in 'behavior_test','bubble_logic_test','drag_logic_test',
              'inifile_test','sweep_logic_test','walk_logic_test') {
  g++ -std=c++17 -Wall -Wextra -O2 "tests\$t.cpp" -o "build\$t.exe"; & "build\$t.exe"
}
```

Behaviour and visuals can only be verified by running the app on Windows.

## Architecture (not visible from filenames)
- `src/main.cpp` — WinMain, DPI-awareness (`SetProcessDpiAwarenessContext`), wiring only.
- `src/core/` — `config.h` (constants) · `util` · `assets` (Frame, LoadFrames, bounds.txt) ·
  `bounds` (current subject box, read by bubble and head-sweep) · `render`
  (`render::PresentLayered`) · `inifile.h` + `settings`.
- `src/pet/` — `behaviors.cpp` is the **behavior registry**; `pet.cpp` is a registry-driven
  player; `walk_logic` / `drag_logic` / `sweep_logic` are pure and unit-tested.
- `src/bubble/` — own window + timer, click-through, follows the pet HWND.
- **To add a behavior: append one `pet_behavior::Spec` to `MutableBehaviors()` and drop in
  `desktop_pet_image/<action>/NNN.png`.** `pet.cpp` needs no change.
- `Playback`: `Boomerang` (forward then backward) only for clips that are **not** closed
  loops; a closed walk cycle must be `Loop`, otherwise it moonwalks on the return half.
  `FullCycle` stops on the last frame.
- Tunables: an optional `desktop_pet_image/pet.ini` overrides `core/config.h` (one section
  per action). Absent file ⇒ all defaults. No such file is checked in.
- Keep `behavior.h` / `walk_logic.h` / `drag_logic.h` / `sweep_logic.h` / `bubble_logic.h` /
  `inifile.h` free of Windows headers so `tests/` keeps building anywhere.
- GDI+ decodes png/jpg/gif/bmp only — **never WebP**.
- Frames: `desktop_pet_image/<action>/NNN.png` (3 digits), loading stops at the first missing
  frame, max 120, 10 fps (`kTimerElapseMs = 100`), drawn at `kScaleFactor = 0.70` ⇒ a 384×512
  frame is a 269×358 window (and `269×358` is the expected size at 150% DPI).
- `bounds.txt` (one `x y w h` per line) drives the bubble and the head-sweep band.

## Asset naming lies
- `walking_left` / `walking_right` — real walk cycles, side view (right is a mirror).
- **`walking_up` = fly up** (arm raised, feet off the ground) and **`walking_down` = dive**.
  They are *not* walks; only the window delta moves up/down.
- `drag` = held up by the scruff (`OnDragStart`, `Loop`). The grabbing "hand" is the mouse
  cursor and is never drawn.
- `idle` is deliberately a single frame (no breathing); `idle2` exists on disk but is not
  registered in the behavior table.
- `desktop_pet_image/_unused/` holds the maid-outfit skin (gitignored) — a different costume,
  not a variant of the current one.

## Conventions
- Commit messages: imperative English (`Add …`, `Fix …`, `Document …`).
- Replacing a clip: delete stale `NNN.png` beyond the new frame count, then rewrite
  `bounds.txt`; the loader would otherwise read leftover frames.
- Don't rescale or re-time frames in this repo — assets arrive as 384×512 RGBA from
  `E:\pet-motion-prompt`, subject height ≈0.68, baseline y ≈460.
- `emoji/` is gitignored via `desktop_pet_image/.gitignore`; it is bubble stickers, not an action.
