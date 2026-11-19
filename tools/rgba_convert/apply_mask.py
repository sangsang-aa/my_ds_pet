#!/usr/bin/env python3
"""Apply hand-drawn background-gap masks to a whole sprite sequence.

A mask is drawn on one or more KEY frames (white = "this is a background gap,
make it transparent"). Each frame is assigned to the most suitable key frame,
and the mask is transferred using that key frame's subject bbox as the
alignment anchor. This absorbs inter-frame jitter; drawing several key masks
handles large pose changes (e.g. the middle of a click animation).

Assignment modes
  nearest (default): each frame uses the mask of the numerically closest key.
                     Draw a handful of key masks and let the script distribute.
  range:             every key declares an explicit frame range; frames outside
                     all ranges are copied through untouched.

Safety
  * only pixels BOTH covered by the mask AND near-white are cleared, so even a
    slightly wrong assignment cannot punch a hole in dark/coloured art;
  * reads/writes PNG only, writes to a NEW directory, never overwrites input;
  * --dry-run reports what would happen without writing anything.

Examples
  # single key
  python apply_mask.py FRAMES --out OUT --anchor 000.png=mask.png

  # multiple keys, nearest assignment (recommended)
  python apply_mask.py FRAMES --out OUT \
      --anchor 000.png=mask_000.png \
      --anchor 015.png=mask_015.png \
      --anchor 029.png=mask_029.png

  # explicit ranges
  python apply_mask.py FRAMES --out OUT --assign range \
      --anchor "000.png=mask_000.png@000-011" \
      --anchor "015.png=mask_015.png@012-021" \
      --anchor "029.png=mask_029.png@022-029"

  # JSON config (see --write-example-config)
  python apply_mask.py FRAMES --out OUT --config masks.json
"""
from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from PIL import Image


def label_components(mask: bytes, width: int, height: int):
    """4-connected labelling; returns (labels, sizes). Mirrors convert_rgba."""
    total = width * height
    labels = [-1] * total
    sizes: list[int] = []
    stack: list[int] = []
    last = width - 1
    for start in range(total):
        if not mask[start] or labels[start] != -1:
            continue
        cid = len(sizes); labels[start] = cid; stack.append(start); count = 0
        while stack:
            i = stack.pop(); count += 1; col = i % width
            for j, ok in ((i - 1, col > 0), (i + 1, col != last),
                          (i - width, i >= width), (i + width, i < total - width)):
                if ok and mask[j] and labels[j] == -1:
                    labels[j] = cid; stack.append(j)
        sizes.append(count)
    return labels, sizes


@dataclass
class Anchor:
    key_name: str                      # e.g. "015.png"
    mask_path: Path
    frames: set[int] | None = None     # for --assign range; else None
    key_num: int = -1
    mask: Image.Image | None = None
    bbox: tuple[int, int, int, int] | None = None
    loaded: bool = False
    warnings: list[str] = field(default_factory=list)


def frame_number(path: Path) -> int:
    m = re.search(r"(\d+)", path.stem)
    return int(m.group(1)) if m else -1


def parse_range(spec: str) -> set[int]:
    """'000-011,020,025-029' -> {0..11, 20, 25..29}."""
    out: set[int] = set()
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-", 1)
            out.update(range(int(a), int(b) + 1))
        else:
            out.add(int(part))
    return out


def subject_bbox(img: Image.Image, alpha_min: int = 1) -> tuple[int, int, int, int] | None:
    a = img.getchannel("A")
    lut = [0] * alpha_min + [255] * (256 - alpha_min)
    return a.point(lut).getbbox()


def load_mask(path: Path) -> Image.Image:
    m = Image.open(path).convert("L")
    return m.point([0] * 128 + [255] * 128)   # white (>=128) = clear


def gate_mask_by_shape(frame: Image.Image, mask: Image.Image,
                       shift: tuple[int, int], whiteness: int,
                       min_iou: float) -> tuple[Image.Image, list[tuple[int, float]]]:
    """Keep only mask strokes whose shape still matches a white gap at the
    target location. A stroke that drifted onto e.g. a white tail edge gets a
    low IoU and is dropped, preventing misaligned cuts. Returns the filtered
    mask and a list of (component_area, iou) for logging."""
    w, h = frame.size
    dx, dy = shift
    placed = Image.new("L", (w, h), 0)
    placed.paste(mask, (dx, dy))
    mb = bytes(255 if v >= 128 else 0 for v in placed.getdata())
    labels, sizes = label_components(mb, w, h)

    px = frame.convert("RGBA").load()
    # white, opaque pixels of the target frame = the real white gaps.
    # Use a strict whiteness floor (>=248) so anti-aliased grey edges don't
    # inflate the union and skew IoU downward.
    gate_white = max(248, whiteness)
    white = bytearray(1 if (px[x, y][3] >= 250 and min(px[x, y][:3]) >= gate_white)
                      else 0 for y in range(h) for x in range(w))

    # per-stroke bbox, so IoU compares the stroke only against the white gap
    # at its own location (not the whole image)
    boxes: list[tuple[int, int, int, int] | None] = [None] * len(sizes)
    for i in range(w * h):
        cid = labels[i]
        if cid < 0:
            continue
        x, y = i % w, i // w
        bx0, by0, bx1, by1 = boxes[cid] or (x, y, x, y)
        boxes[cid] = (min(bx0, x), min(by0, y), max(bx1, x), max(by1, y))

    out = bytearray(w * h)
    info: list[tuple[int, float]] = []
    for cid, size in enumerate(sizes):
        if not size or boxes[cid] is None:
            continue
        bx0, by0, bx1, by1 = boxes[cid]
        inter = union = 0
        for y in range(by0, by1 + 1):
            for x in range(bx0, bx1 + 1):
                i = y * w + x
                m = 1 if labels[i] == cid else 0
                t = white[i]
                if m and t: inter += 1
                if m or t: union += 1
        iou = inter / union if union else 0.0
        info.append((size, iou))
        if iou >= min_iou:
            for i in range(w * h):
                if labels[i] == cid:
                    out[i] = 255
    return Image.frombytes("L", (w, h), bytes(out)), info


def apply_one(frame: Image.Image, mask: Image.Image, shift: tuple[int, int],
              whiteness: int, feather: int,
              shape_gate: float = 0.0) -> tuple[Image.Image, int, str]:
    rgba = frame.convert("RGBA")
    w, h = rgba.size
    dx, dy = shift
    gate_note = ""
    if shape_gate > 0:
        mask, info = gate_mask_by_shape(rgba, mask, shift, whiteness, shape_gate)
        kept = [s for s, i in info if i >= shape_gate]
        gate_note = f" shape_gate={len(kept)}/{len(info)}"
    placed = Image.new("L", (w, h), 0)
    placed.paste(mask, (dx, dy))
    px, mk = rgba.load(), placed.load()
    cleared = 0
    feather_lo = 255 - feather if feather > 0 else 128
    for y in range(h):
        for x in range(w):
            if mk[x, y] < feather_lo:
                continue
            r, g, b, a = px[x, y]
            if a == 0 or min(r, g, b) < whiteness:
                continue
            factor = (mk[x, y] - feather_lo) / feather if feather > 0 else 1.0
            new_a = round(a * (1.0 - factor))
            px[x, y] = (r, g, b, new_a)
            if new_a == 0:
                cleared += 1
    return rgba, cleared, gate_note


def parse_anchor_arg(raw: str, frames_dir: Path) -> Anchor:
    """'KEY=MASK' or 'KEY=MASK@RANGE'. Relative paths resolve next to frames dir."""
    spec = raw
    frames_spec = None
    if "@" in spec:
        spec, frames_spec = spec.split("@", 1)
    if "=" not in spec:
        raise ValueError(f"--anchor expects KEY=MASK, got {raw!r}")
    key_name, mask_raw = spec.split("=", 1)
    mask_path = Path(mask_raw)
    if not mask_path.is_absolute() and not mask_path.exists():
        mask_path = frames_dir / mask_path
    return Anchor(key_name=key_name, mask_path=mask_path,
                  frames=parse_range(frames_spec) if frames_spec else None)


def load_config(path: Path, frames_dir: Path) -> list[Anchor]:
    data = json.loads(path.read_text(encoding="utf-8"))
    raw_list = data["anchors"] if isinstance(data, dict) else data
    out = []
    for item in raw_list:
        key_name = str(item["key"])
        mask_path = Path(item["mask"])
        if not mask_path.is_absolute() and not mask_path.exists():
            mask_path = path.parent / mask_path
        frames = None
        if item.get("frames"):
            frames = parse_range(str(item["frames"]))
        out.append(Anchor(key_name=key_name, mask_path=mask_path, frames=frames))
    return out


def prepare_anchor(a: Anchor, frames_dir: Path, frame_size: tuple[int, int]) -> None:
    key_path = Path(a.key_name)
    if not key_path.is_absolute() and not key_path.exists():
        key_path = frames_dir / key_path
    a.key_num = frame_number(key_path)
    if not key_path.exists():
        a.warnings.append(f"key frame not found: {key_path}")
        return
    if not a.mask_path.exists():
        a.warnings.append(f"mask not found: {a.mask_path}")
        return
    key_img = Image.open(key_path).convert("RGBA")
    a.mask = load_mask(a.mask_path)
    if a.mask.size != key_img.size:
        a.warnings.append(f"mask {a.mask.size} != key {key_img.size}; resized")
        a.mask = a.mask.resize(key_img.size, Image.NEAREST)
    if a.mask.size != frame_size:
        a.warnings.append(f"key/mask {a.mask.size} != frames {frame_size}; resized")
        a.mask = a.mask.resize(frame_size, Image.NEAREST)
    a.bbox = subject_bbox(key_img)
    if a.bbox is None:
        a.warnings.append("key frame has no visible subject")
        return
    a.loaded = True


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("frames", help="directory of RGBA PNG frames")
    ap.add_argument("--out", required=True, help="output directory (new, never overwrites input)")
    ap.add_argument("--anchor", action="append", default=[], metavar="KEY=MASK[@RANGE]",
                    help="a key frame and its mask; repeatable. Add @RANGE for --assign range")
    ap.add_argument("--config", help="JSON config with an 'anchors' list")
    ap.add_argument("--assign", choices=("nearest", "range"), default="nearest",
                    help="how frames pick a mask (default nearest)")
    ap.add_argument("--whiteness", type=int, default=235,
                    help="only clear pixels with min(R,G,B) >= this (default 235)")
    ap.add_argument("--feather", type=int, default=2,
                    help="soften mask edge by N levels (default 2; 0 = hard)")
    ap.add_argument("--no-align", action="store_true",
                    help="disable bbox alignment; use raw mask coordinates")
    ap.add_argument("--shape-gate", type=float, default=0.0, metavar="IOU",
                    help="drop mask strokes whose shape no longer matches a white "
                         "gap at the target location (IoU below IOU, e.g. 0.4). "
                         "Prevents misaligned cuts on pose-transition frames; "
                         "0 disables (default)")
    ap.add_argument("--dry-run", action="store_true", help="report only, write nothing")
    ap.add_argument("--write-example-config", metavar="PATH",
                    help="write a sample config JSON and exit")
    args = ap.parse_args()

    if args.write_example_config:
        example = {"anchors": [
            {"key": "000.png", "mask": "mask_000.png", "frames": "000-011"},
            {"key": "015.png", "mask": "mask_015.png", "frames": "012-021"},
            {"key": "029.png", "mask": "mask_029.png", "frames": "022-029"},
        ]}
        Path(args.write_example_config).write_text(
            json.dumps(example, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"wrote {args.write_example_config}")
        return 0

    frames_dir = Path(args.frames)
    out_dir = Path(args.out)
    frames = sorted(p for p in frames_dir.glob("*.png") if not p.name.endswith(".orig"))
    if not frames:
        print("no PNG frames found")
        return 1
    frame_size = Image.open(frames[0]).size

    # build anchor list
    anchors: list[Anchor] = []
    if args.config:
        anchors.extend(load_config(Path(args.config), frames_dir))
    for raw in args.anchor:
        anchors.append(parse_anchor_arg(raw, frames_dir))
    if not anchors:
        ap.error("provide at least one --anchor (e.g. --anchor 000.png=mask.png) or --config")

    for a in anchors:
        prepare_anchor(a, frames_dir, frame_size)
        tag = f"{a.key_name} -> {a.mask_path.name}"
        if a.frames:
            tag += f" [range {min(a.frames):03d}-{max(a.frames):03d}]"
        print(f"anchor: {tag}")
        for w in a.warnings:
            print(f"   WARN: {w}")
    valid = [a for a in anchors if a.loaded]
    if not valid:
        print("no usable anchor (check key/mask paths)")
        return 1

    if not args.dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)

    total_cleared = 0
    skipped = 0
    for f in frames:
        n = frame_number(f)
        chosen = None
        if args.assign == "range":
            for a in valid:
                if a.frames and n in a.frames:
                    chosen = a
                    break
        else:
            chosen = min(valid, key=lambda a: abs(n - a.key_num))
        if chosen is None:
            # range mode, frame outside every range: copy through untouched
            skipped += 1
            print(f"  {f.name}: no range -> copied unchanged")
            if not args.dry_run:
                Image.open(f).convert("RGBA").save(out_dir / f.name, "PNG", optimize=True)
            continue

        img = Image.open(f).convert("RGBA")
        if img.size != frame_size:
            img = img.resize(frame_size, Image.LANCZOS)
        shift = (0, 0)
        if not args.no_align and chosen.bbox:
            bb = subject_bbox(img)
            if bb:
                shift = (bb[0] - chosen.bbox[0], bb[1] - chosen.bbox[1])
        out_img, cleared, gate_note = apply_one(
            img, chosen.mask, shift, args.whiteness, args.feather, args.shape_gate)
        total_cleared += cleared
        print(f"  {f.name}: key={chosen.key_name} shift={shift} "
              f"cleared_px={cleared}{gate_note}")
        if not args.dry_run:
            out_img.save(out_dir / f.name, "PNG", optimize=True)

    print(f"summary: frames={len(frames)} anchors={len(valid)} "
          f"total_cleared={total_cleared} passthrough={skipped} "
          f"assign={args.assign} dry_run={args.dry_run} out={out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
