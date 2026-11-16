#!/usr/bin/env python3
"""Build desktop_pet_image/emoji/emoji_NN.png from any images or GIFs.

The bubble looks for emoji_01.png, emoji_02.png ... and stops at the first
missing index, so the numbering has to be continuous. This tool appends after
the highest existing index and renumbers nothing that already exists.

Inputs may be PNG / JPG / WEBP / BMP / GIF / APNG. A GIF contributes its first
frame (the bubble shows still stickers, not animation).

Usage:
    python tools/make_emoji.py D:\\stickers\\*.png
    python tools/make_emoji.py D:\\stickers            (every supported file)
    python tools/make_emoji.py cute.gif --size 440 --dry-run
"""

from __future__ import annotations

import argparse
import os
import re
import sys

from PIL import Image

EMOJI_RE = re.compile(r"^emoji_(\d+)\.png$")
SUPPORTED = (".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif")


def existing(out_dir: str) -> dict[int, str]:
    found: dict[int, str] = {}
    if not os.path.isdir(out_dir):
        return found
    for name in os.listdir(out_dir):
        match = EMOJI_RE.match(name)
        if match:
            found[int(match.group(1))] = name
    return found


def collect(paths: list[str]) -> list[str]:
    files: list[str] = []
    for path in paths:
        if os.path.isdir(path):
            for name in sorted(os.listdir(path)):
                if name.lower().endswith(SUPPORTED):
                    files.append(os.path.join(path, name))
        elif os.path.isfile(path):
            files.append(path)
        else:
            print(f"[skip] not found: {path}", file=sys.stderr)
    return files


def normalise(path: str, size: int) -> Image.Image:
    with Image.open(path) as im:
        if getattr(im, "is_animated", False):
            im.seek(0)  # stickers are stills; take the first frame
        frame = im.convert("RGBA")
    scale = min(size / frame.width, size / frame.height, 1.0)
    if scale < 1.0:
        frame = frame.resize(
            (max(1, round(frame.width * scale)), max(1, round(frame.height * scale))),
            Image.Resampling.LANCZOS,
        )
    return frame


def main() -> int:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    parser = argparse.ArgumentParser(
        description="Turn images/GIFs into a numbered emoji_NN.png sticker set.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("paths", nargs="+", help="image/GIF files or a directory")
    parser.add_argument(
        "--out",
        default=os.path.join(root, "desktop_pet_image", "emoji"),
        help="output directory",
    )
    parser.add_argument(
        "--size", type=int, default=440, help="longest side (px); the bubble scales anyway"
    )
    parser.add_argument("--start", type=int, default=0, help="override the first index")
    parser.add_argument("--dry-run", action="store_true", help="report only, write nothing")
    opts = parser.parse_args()

    files = collect(opts.paths)
    if not files:
        print("no supported images found", file=sys.stderr)
        return 2

    current = existing(opts.out)
    next_index = opts.start if opts.start > 0 else (max(current) + 1 if current else 1)
    print(f"emoji dir: {opts.out}")
    print(f"existing: {len(current)} sticker(s); first new index: {next_index:02d}")

    written = 0
    for path in files:
        try:
            frame = normalise(path, opts.size)
        except Exception as exc:  # noqa: BLE001 - report and keep going
            print(f"[fail] {path}: {exc}", file=sys.stderr)
            continue
        target = os.path.join(opts.out, f"emoji_{next_index:02d}.png")
        if opts.dry_run:
            print(f"[dry] {os.path.basename(path)} -> {os.path.basename(target)} {frame.width}x{frame.height}")
        else:
            os.makedirs(opts.out, exist_ok=True)
            frame.save(target, "PNG", optimize=True)
            print(f"[ok] {os.path.basename(path)} -> {os.path.basename(target)} {frame.width}x{frame.height}")
        next_index += 1
        written += 1

    print(f"summary: {written} sticker(s) {'would be ' if opts.dry_run else ''}written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
