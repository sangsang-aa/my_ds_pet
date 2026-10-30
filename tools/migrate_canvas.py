#!/usr/bin/env python3
"""Migrate desktop_pet_image actions to the 384x512 canvas.

The old assets use 384x456 (or 384x384). The new contract is 384x512 (3:4),
matching the video model's native 3:4 ratio. Migration is top-padding only:
the character stays at the bottom, so the feet baseline and subject size are
unchanged.

Rules:
  * 384x456 -> add 56 transparent rows at the top
  * 384x384 -> add 128 transparent rows at the top
  * already 384x512 -> skip (idempotent)
  * bounds.txt -> shift y by the same padding
  * missing bounds.txt -> regenerate from the alpha channel

Skipped: emoji/, tmp/, 视频文件/, any directory ending in .bak
"""

from __future__ import annotations

import argparse
import os
import re
import sys

from PIL import Image

FRAME_RE = re.compile(r"^\d{3}\.png$")
TARGET_W = 384
TARGET_H = 512
SKIP_DIRS = {"emoji", "tmp", "视频文件"}


def frame_files(directory: str) -> list[str]:
    return sorted(f for f in os.listdir(directory) if FRAME_RE.match(f))


def bounds_from_alpha(path: str) -> tuple[int, int, int, int] | None:
    with Image.open(path) as im:
        alpha = im.convert("RGBA").getchannel("A")
        box = alpha.getbbox()
    if not box:
        return None
    left, top, right, bottom = box
    return left, top, right - left, bottom - top


def read_bounds(path: str) -> list[tuple[int, int, int, int]]:
    rows: list[tuple[int, int, int, int]] = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if len(parts) == 4:
                rows.append(tuple(int(p) for p in parts))  # type: ignore[arg-type]
    return rows


def write_bounds(path: str, rows: list[tuple[int, int, int, int]]) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for x, y, w, h in rows:
            f.write(f"{x} {y} {w} {h}\n")


def migrate_action(directory: str, dry_run: bool) -> bool:
    frames = frame_files(directory)
    if not frames:
        return False

    first_path = os.path.join(directory, frames[0])
    with Image.open(first_path) as im:
        width, height = im.size

    if (width, height) == (TARGET_W, TARGET_H):
        return False

    if width != TARGET_W:
        raise ValueError(f"{directory}: expected width {TARGET_W}, got {width}")

    pad = TARGET_H - height
    if pad < 0:
        raise ValueError(f"{directory}: already taller than {TARGET_H} ({height})")

    bounds_path = os.path.join(directory, "bounds.txt")
    old_bounds = read_bounds(bounds_path) if os.path.exists(bounds_path) else []
    bounds_ok = len(old_bounds) == len(frames)
    bounds_note = f"bounds +{pad}y" if bounds_ok else "bounds will be regenerated"

    if not dry_run:
        for name in frames:
            path = os.path.join(directory, name)
            with Image.open(path) as im:
                rgba = im.convert("RGBA")
                if rgba.size == (TARGET_W, TARGET_H):
                    continue
                canvas = Image.new("RGBA", (TARGET_W, TARGET_H), (0, 0, 0, 0))
                canvas.alpha_composite(rgba, (0, pad))
                canvas.save(path, "PNG")

        if bounds_ok:
            new_bounds = [(x, y + pad, w, h) for x, y, w, h in old_bounds]
            write_bounds(bounds_path, new_bounds)
            bounds_note = f"bounds +{pad}y"
        else:
            new_bounds = []
            for name in frames:
                box = bounds_from_alpha(os.path.join(directory, name))
                new_bounds.append(box or (0, 0, TARGET_W, TARGET_H))
            write_bounds(bounds_path, new_bounds)
            bounds_note = "bounds regenerated"

    print(
        f"{os.path.basename(directory):16s} {width}x{height} -> "
        f"{TARGET_W}x{TARGET_H}  pad={pad}  frames={len(frames)}  {bounds_note}"
        + ("  [dry-run]" if dry_run else "")
    )
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "desktop_pet_image"),
        help="desktop_pet_image directory (default: repo/desktop_pet_image)",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    root = args.root
    if not os.path.isdir(root):
        print(f"not found: {root}", file=sys.stderr)
        return 2

    changed = 0
    for name in sorted(os.listdir(root)):
        directory = os.path.join(root, name)
        if not os.path.isdir(directory):
            continue
        if name in SKIP_DIRS or name.endswith(".bak"):
            continue
        try:
            if migrate_action(directory, args.dry_run):
                changed += 1
        except Exception as exc:  # noqa: BLE001
            print(f"ERROR {name}: {exc}", file=sys.stderr)
            return 1

    print(f"\n{'would migrate' if args.dry_run else 'migrated'} {changed} action(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
