#!/usr/bin/env python3
"""Refine a hand-drawn mask by filling interior un-painted pinholes.

A pinhole = a black pixel fully enclosed by white paint (not reachable from
the image border through black pixels). These are gaps the brush missed; they
would leave tiny opaque specks in the cut-out. Filling them is safe because
genuine background (outside the strokes) stays connected to the border and is
never filled.

Optional --dilate grows every white stroke by N px to catch gaps at stroke
edges. Kept small (1px) and paired with apply_mask.py --whiteness so coloured
art is still protected.

    python mask_refine.py mask.png -o mask_refined.png
    python mask_refine.py mask.png -o mask_refined.png --dilate 1 --preview out.png
"""
from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw


def interior_holes(g: Image.Image) -> bytearray:
    w, h = g.size
    black = bytearray(1 if v < 128 else 0 for v in g.getdata())
    visited = bytearray(w * h)
    q = deque()
    for x in range(w):
        for i in (x, (h - 1) * w + x):
            if black[i] and not visited[i]:
                visited[i] = 1; q.append(i)
    for y in range(h):
        for i in (y * w, y * w + w - 1):
            if black[i] and not visited[i]:
                visited[i] = 1; q.append(i)
    while q:
        i = q.popleft(); c = i % w
        nb = []
        if c: nb.append(i - 1)
        if c != w - 1: nb.append(i + 1)
        if i >= w: nb.append(i - w)
        if i < w * (h - 1): nb.append(i + w)
        for j in nb:
            if black[j] and not visited[j]:
                visited[j] = 1; q.append(j)
    return bytearray(1 if (black[i] and not visited[i]) else 0 for i in range(w * h))


def refine(g: Image.Image, dilate: int) -> tuple[Image.Image, int]:
    w, h = g.size
    holes = interior_holes(g)
    filled = sum(holes)
    px = bytearray(255 if v >= 128 else 0 for v in g.getdata())
    for i, v in enumerate(holes):
        if v:
            px[i] = 255
    out = Image.frombytes("L", (w, h), bytes(px))
    if dilate > 0:
        # MaxFilter needs odd kernel; apply N times with 3x3
        k = 2 * dilate + 1
        out = out.filter(__import__("PIL.ImageFilter", fromlist=["MaxFilter"]).MaxFilter(k))
    return out, filled


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mask", help="input mask PNG")
    ap.add_argument("-o", "--out", required=True, help="refined mask PNG")
    ap.add_argument("--dilate", type=int, default=0,
                    help="grow white strokes by N px (0 = fill pinholes only)")
    ap.add_argument("--preview", help="optional before/after preview PNG")
    args = ap.parse_args()

    g = Image.open(args.mask).convert("L")
    out, filled = refine(g, args.dilate)
    out.save(args.out)
    print(f"{args.mask}: filled {filled} interior px, dilate={args.dilate} -> {args.out}")

    if args.preview:
        w, h = g.size
        canvas = Image.new("RGB", (w * 2 + 20, h), (245, 245, 245))
        canvas.paste(g.convert("RGB"), (0, 0))
        canvas.paste(out.convert("RGB"), (w + 20, 0))
        d = ImageDraw.Draw(canvas)
        d.text((5, 5), "BEFORE", fill=(0, 0, 0))
        d.text((w + 25, 5), "AFTER", fill=(0, 0, 0))
        canvas.save(args.preview)
        print(f"preview -> {args.preview}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
