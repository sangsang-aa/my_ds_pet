#!/usr/bin/env python3
"""Repair keying artefacts in already-cut RGBA frame sequences.

Chroma keying leaves three defects that "despill" cannot fix:

  1. A cast ring on the silhouette. On a blue-violet character a green key does
     not leave *green* (G > R and G > B); it leaves *cyan* (G and B both above
     R), e.g. (98, 135, 135). A standard despill clamps G to max(R, B) — that is
     an identity transform on cyan, which is why running despill again does
     nothing. Measured on the shipped frames: rim "greenness" G-max(R,B) = -8.5
     while rim "cyanness" min(G,B)-R = +45.
  2. Key-colour RGB left in fully transparent pixels (some frames carry pure
     (0, 255, 0) at alpha 0). Invisible until something resamples the image.
  3. Detached near-white specks at alpha 2..8 floating 1-2 px off the matte.

Fix: rebuild the ring colour from the nearest trusted interior colour, keep the
alpha exactly as authored, then extend that colour a few pixels outward so a
downscale never samples garbage. Nothing here guesses a key colour, so it works
for green, cyan, blue or white footage.

Usage:
    python tools/fix_edges.py desktop_pet_image\\walking_left
    python tools/fix_edges.py desktop_pet_image --recursive --dry-run
    python tools/fix_edges.py desktop_pet_image\\shy --alpha-rgb zero --strength 0.5

Skips emoji/, tmp/, 视频文件/, _unused/ and anything that is not NNN.png.
Idempotent: a frame that already measures clean is reported and left alone
(use --force to reprocess).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
from collections import deque

import numpy as np
from PIL import Image

FRAME_RE = re.compile(r"^\d{3}\.png$")
SKIP_DIRS = {"emoji", "tmp", "视频文件", "_unused"}

# "Clean" thresholds for the before/after report.
CLEAN_CYAN = 8.0
CLEAN_GREEN = 6.0

NEIGHBOURS = (
    (-1, 0),
    (1, 0),
    (0, -1),
    (0, 1),
    (-1, -1),
    (-1, 1),
    (1, -1),
    (1, 1),
)


# --------------------------------------------------------------------------- #
# morphology (numpy only, no scipy)
# --------------------------------------------------------------------------- #


def _shift_axis(mask: np.ndarray, k: int, axis: int, fill: bool) -> np.ndarray:
    """Shift a bool array by k along one axis, filling the gap with `fill`."""
    out = np.full_like(mask, fill)
    if k == 0:
        out[:] = mask
        return out
    src = [slice(None), slice(None)]
    dst = [slice(None), slice(None)]
    size = mask.shape[axis]
    if k > 0:
        src[axis] = slice(0, size - k)
        dst[axis] = slice(k, size)
    else:
        src[axis] = slice(-k, size)
        dst[axis] = slice(0, size + k)
    out[tuple(dst)] = mask[tuple(src)]
    return out


def _morph(mask: np.ndarray, n: int, fill: bool, combine) -> np.ndarray:
    """Separable square-window morphology; 2n shifts per axis instead of (2n+1)^2."""
    if n <= 0:
        return mask
    out = mask
    for axis in (0, 1):
        base = out
        out = _shift_axis(base, 0, axis, fill)
        for k in range(1, n + 1):
            out = combine(out, _shift_axis(base, k, axis, fill))
            out = combine(out, _shift_axis(base, -k, axis, fill))
    return out


def dilate(mask: np.ndarray, n: int) -> np.ndarray:
    return _morph(mask, n, False, np.logical_or)


def erode(mask: np.ndarray, n: int) -> np.ndarray:
    return _morph(mask, n, True, np.logical_and)


def shift(a: np.ndarray, dy: int, dx: int) -> np.ndarray:
    """Move the array by (dy, dx), filling the vacated edge with zeros."""
    out = np.zeros_like(a)
    h, w = a.shape[:2]
    if dy < 0:
        src_y, dst_y = slice(0, h + dy), slice(-dy, h)
    else:
        src_y, dst_y = slice(dy, h), slice(0, h - dy)
    if dx < 0:
        src_x, dst_x = slice(0, w + dx), slice(-dx, w)
    else:
        src_x, dst_x = slice(dx, w), slice(0, w - dx)
    out[dst_y, dst_x] = a[src_y, src_x]
    return out


# --------------------------------------------------------------------------- #
# measurement
# --------------------------------------------------------------------------- #


def edge_band(alpha: np.ndarray, inside: int = 2, outside: int = 1) -> np.ndarray:
    solid = alpha > 0
    return dilate(solid, inside) & ~erode(solid, outside) & solid & (alpha < 250)


def speck_mask(alpha: np.ndarray, max_alpha: int, max_area: int) -> np.ndarray:
    """Low-alpha pixels that form isolated islands (area <= max_area).

    A soft edge ramp is one large connected component, so this never eats it;
    only genuinely detached dots are returned.
    """
    mask = (alpha > 0) & (alpha < max_alpha)
    if not mask.any():
        return mask
    ys, xs = np.where(mask)
    coords = set(zip(ys.tolist(), xs.tolist()))
    out = np.zeros_like(mask)
    seen: set[tuple[int, int]] = set()
    for start in coords:
        if start in seen:
            continue
        seen.add(start)
        queue = deque([start])
        component = [start]
        while queue:
            y, x = queue.popleft()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    neighbour = (y + dy, x + dx)
                    if neighbour in coords and neighbour not in seen:
                        seen.add(neighbour)
                        queue.append(neighbour)
                        component.append(neighbour)
        if len(component) <= max_area:
            for y, x in component:
                out[y, x] = True
    return out


def metrics(rgba: np.ndarray, speck_alpha: int = 16, speck_area: int = 4) -> dict:
    a = rgba[..., 3].astype(np.int16)
    r = rgba[..., 0].astype(np.int16)
    g = rgba[..., 1].astype(np.int16)
    b = rgba[..., 2].astype(np.int16)
    band = edge_band(a)
    clear = a == 0
    specks = speck_mask(a, speck_alpha, speck_area)
    return {
        "rim_cyan": float((np.minimum(g[band], b[band]) - r[band]).mean()) if band.any() else 0.0,
        "rim_green": float((g[band] - np.maximum(r[band], b[band])).mean()) if band.any() else 0.0,
        "ghost_px": int(((g > 200) & (r < 120) & (b < 120) & clear).sum()),
        "speck_px": int(specks.sum()),
        "clear_rgb_pct": float(((r[clear] > 0) | (g[clear] > 0) | (b[clear] > 0)).mean() * 100)
        if clear.any()
        else 0.0,
    }


def is_clean(m: dict) -> bool:
    # Both metrics are "excess" measures: negative means the ring is anti-green /
    # anti-cyan, which is normal for a blue character and perfectly fine.
    return (
        m["rim_cyan"] <= CLEAN_CYAN
        and m["rim_green"] <= CLEAN_GREEN
        and m["ghost_px"] == 0
        and m["speck_px"] == 0
    )


# --------------------------------------------------------------------------- #
# repair
# --------------------------------------------------------------------------- #


def repair(rgba: np.ndarray, opts: argparse.Namespace) -> tuple[np.ndarray, int]:
    full_alpha = rgba[..., 3]
    margin = opts.ring + opts.core + 4

    # Work inside the subject bbox (+margin): propagation is the expensive part
    # and the rest of the canvas is empty anyway.
    ys, xs = np.where(full_alpha > 0)
    if ys.size == 0:
        return rgba, 0
    y0 = max(0, int(ys.min()) - margin)
    y1 = min(rgba.shape[0], int(ys.max()) + margin + 1)
    x0 = max(0, int(xs.min()) - margin)
    x1 = min(rgba.shape[1], int(xs.max()) + margin + 1)

    result = rgba.copy()
    clear_full = full_alpha == 0
    result[..., :3][clear_full] = 0  # nothing may hide under the matte

    crop = rgba[y0:y1, x0:x1]
    a = crop[..., 3].copy()
    rgb = crop[..., :3].astype(np.float32)

    # 1) drop detached low-alpha specks; the alpha ramp is one big component and
    #    is therefore left alone.
    specks = speck_mask(a, opts.speck_alpha, opts.speck_area)
    if specks.any():
        a[specks] = 0

    # 2) trusted interior: opaque and `core` px away from the matte edge.
    trusted = erode(a >= 250, opts.core)
    if not trusted.any():
        trusted = a >= 250
    if not trusted.any():
        return rgba, 0

    # 3) nearest trusted colour for every pixel near the silhouette
    domain = dilate(a > 0, opts.ring + 3)
    colour = rgb.copy()
    known = trusted.copy()
    for _ in range(opts.core + opts.ring + 6):
        todo = domain & ~known
        if not todo.any():
            break
        for dy, dx in NEIGHBOURS:
            src_colour = shift(colour, dy, dx)
            src_known = shift(known, dy, dx)
            take = todo & src_known
            colour[take] = src_colour[take]
            known[take] = True

    # 4) locate the cast ring: near the matte AND shifted away from the interior
    #    colour AND tinted toward green or cyan.
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    tr, tg, tb = colour[..., 0], colour[..., 1], colour[..., 2]
    near = dilate(a > 0, opts.ring) & ~erode(a > 0, opts.ring)
    cyan = (g - r > opts.cyan_gain) & (b - r > opts.cyan_gain)
    green = g - np.maximum(r, b) > opts.green_gain
    shifted = (
        (np.abs(r - tr) > opts.diff)
        | (np.abs(g - tg) > opts.diff)
        | (np.abs(b - tb) > opts.diff)
    )
    zone = near & shifted & (cyan | green)
    count = int(zone.sum())

    if count and opts.strength > 0:
        s = float(opts.strength)
        rgb[zone] = rgb[zone] * (1.0 - s) + colour[zone] * s

    # 5) what the matte ignores must not carry colour: extend the interior
    #    colour a few px outward (a downscale then samples a sane colour), or
    #    zero it if the caller prefers.
    clear = a == 0
    if opts.alpha_rgb == "extend":
        band = clear & domain & known
        rgb[band] = colour[band]
        rgb[clear & ~band] = 0.0
    elif opts.alpha_rgb == "zero":
        rgb[clear] = 0.0

    result[y0:y1, x0:x1, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    result[y0:y1, x0:x1, 3] = a
    return result, count


# --------------------------------------------------------------------------- #
# driver
# --------------------------------------------------------------------------- #


def collect(paths: list[str], recursive: bool) -> list[str]:
    files: list[str] = []
    for path in paths:
        if os.path.isfile(path):
            files.append(path)
            continue
        if not os.path.isdir(path):
            print(f"[skip] not found: {path}", file=sys.stderr)
            continue
        if recursive:
            for root, dirs, names in os.walk(path):
                dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.endswith(".bak")]
                files.extend(
                    os.path.join(root, n) for n in sorted(names) if FRAME_RE.match(n)
                )
        else:
            files.extend(
                os.path.join(path, n) for n in sorted(os.listdir(path)) if FRAME_RE.match(n)
            )
    return files


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Repair chroma-key artefacts (cyan/green cast ring, ghost RGB under "
        "alpha, detached low-alpha specks) in RGBA frame sequences.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("paths", nargs="+", help="frame file, action directory, or root")
    parser.add_argument("--recursive", action="store_true", help="walk subdirectories")
    parser.add_argument("--out", help="write here instead of in place (same file names)")
    parser.add_argument("--ring", type=int, default=2, help="px of cast ring to repair")
    parser.add_argument("--core", type=int, default=2, help="px of interior treated as trusted")
    parser.add_argument("--diff", type=int, default=12, help="min channel delta vs interior")
    parser.add_argument("--cyan-gain", type=int, default=20, help="min (G-R) and (B-R) for cyan")
    parser.add_argument("--green-gain", type=int, default=8, help="min G-max(R,B) for green spill")
    parser.add_argument("--strength", type=float, default=1.0, help="0..1 blend of the repair")
    parser.add_argument(
        "--alpha-rgb",
        choices=("extend", "zero", "keep"),
        default="extend",
        help="what to do with RGB under alpha==0",
    )
    parser.add_argument("--speck-alpha", type=int, default=16, help="alpha below which specks drop")
    parser.add_argument("--speck-area", type=int, default=4, help="max island size treated as a speck")
    parser.add_argument("--backup", dest="backup", action="store_true", default=True,
                        help="keep <frame>.orig (default)")
    parser.add_argument("--no-backup", dest="backup", action="store_false")
    parser.add_argument("--force", action="store_true", help="process frames that already measure clean")
    parser.add_argument("--dry-run", action="store_true", help="measure only, write nothing")
    parser.add_argument("--report", help="write a JSON report here")
    parser.add_argument("--quiet", action="store_true", help="only print the summary")
    opts = parser.parse_args()

    files = collect(opts.paths, opts.recursive)
    if not files:
        print("no NNN.png frames found", file=sys.stderr)
        return 2

    report = []
    changed = clean = would_fix = failed = 0

    for index, path in enumerate(files, 1):
        try:
            with Image.open(path) as im:
                if im.mode != "RGBA":
                    if not opts.quiet:
                        print(f"[skip] {path}: not RGBA ({im.mode})")
                    clean += 1
                    continue
                rgba = np.asarray(im.convert("RGBA")).copy()
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"[fail] {path}: {exc}", file=sys.stderr)
            failed += 1
            continue

        before = metrics(rgba, opts.speck_alpha, opts.speck_area)
        entry = {"file": path, "before": before, "clean": is_clean(before)}

        fixed, repaired_px = repair(rgba, opts)
        after = metrics(fixed, opts.speck_alpha, opts.speck_area)
        entry.update({"after": after, "repaired_px": repaired_px, "action": "fixed"})

        if np.array_equal(fixed, rgba) and not opts.force:
            entry["action"] = "clean"
            report.append(entry)
            clean += 1
            if not opts.quiet:
                print(f"[clean] {os.path.basename(path)} rim cyan {before['rim_cyan']:+.1f}")
            continue

        if opts.dry_run:
            entry["action"] = "would-fix"
            report.append(entry)
            would_fix += 1
            if not opts.quiet:
                print(
                    f"[dry] {path}: cyan {before['rim_cyan']:+.1f} -> {after['rim_cyan']:+.1f}"
                    f"  green {before['rim_green']:+.1f} -> {after['rim_green']:+.1f}"
                    f"  ring px {repaired_px}"
                )
            continue

        target = path
        if opts.out:
            target = os.path.join(opts.out, os.path.basename(path))
            os.makedirs(opts.out, exist_ok=True)
        if opts.backup and target == path and not os.path.exists(path + ".orig"):
            shutil.copy2(path, path + ".orig")

        Image.fromarray(fixed, "RGBA").save(target, "PNG", optimize=True)
        report.append(entry)
        changed += 1
        if not opts.quiet:
            print(
                f"[{index}/{len(files)}] {os.path.basename(path)}: "
                f"cyan {before['rim_cyan']:+.1f} -> {after['rim_cyan']:+.1f}  "
                f"green {before['rim_green']:+.1f} -> {after['rim_green']:+.1f}  "
                f"ghosts {before['ghost_px']} -> {after['ghost_px']}  "
                f"specks {before['speck_px']} -> {after['speck_px']}  ring px {repaired_px}"
            )

    print(
        f"summary: total={len(files)} fixed={changed} clean={clean} "
        f"would-fix={would_fix} failed={failed}"
        + (" (dry-run)" if opts.dry_run else "")
    )

    if opts.report:
        with open(opts.report, "w", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=2)
        print(f"report: {opts.report}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
