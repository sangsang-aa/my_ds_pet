"""Batch-convert images with a near-uniform background into RGBA PNGs with the
background removed. Intended for preparing desktop-pet sprite frames.
"""

from __future__ import annotations

import argparse
import math
import re
import shutil
import sys
from collections import deque
from pathlib import Path
from typing import Optional, Sequence

from PIL import Image, ImageChops, ImageFilter, ImageSequence

try:
    from PIL import ImageMath
except ImportError:  # pragma: no cover
    ImageMath = None  # type: ignore[assignment]

IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif"}


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert near-uniform-background images to RGBA PNGs.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("paths", nargs="+", metavar="PATH",
                        help="image file(s) and/or director(y|ies)")
    parser.add_argument("--recursive", action="store_true",
                        help="scan directories recursively")
    parser.add_argument("--mode", choices=("flood", "chroma"), default="flood",
                        help="flood removes only background connected to the border; "
                             "chroma removes all near-background pixels")
    parser.add_argument("--threshold", type=int, default=60, metavar="N",
                        help="max RGB distance from the border color to count as background")
    parser.add_argument("--feather", type=int, default=25, metavar="N",
                        help="smoothstep band width centered on the threshold")
    parser.add_argument("--trim", type=int, default=1, metavar="N",
                        help="erode the alpha edge by N pixels; 1 removes the "
                             "white/green halo left by anti-aliasing")
    parser.add_argument("--decontaminate", action=argparse.BooleanOptionalAction,
                        default=True,
                        help="replace translucent edge RGB with the nearest "
                             "opaque color so no background halo remains")
    parser.add_argument("--despill", action=argparse.BooleanOptionalAction,
                        default=True,
                        help="reduce the background's dominant color on "
                             "translucent edge pixels (green-screen spill)")
    parser.add_argument("--dry-run", action="store_true", dest="dry_run",
                        help="print planned actions without writing anything")
    parser.add_argument("--backup", action="store_true",
                        help="copy the file being overwritten (or the source) to <path>.orig")
    parser.add_argument("--delete-source", action="store_true", dest="delete_source",
                        help="delete the original file after a successful conversion")
    parser.add_argument("--force", action="store_true",
                        help="process even if the image already has transparency")
    parser.add_argument("--clear-corners", action="store_true", dest="clear_corners",
                        help="erase small isolated non-transparent islands at the four "
                             "corners (moving AI watermarks); the subject is a single "
                             "large component and is left untouched")
    parser.add_argument("--corner-size", default="96x48", metavar="WxH",
                        help="probe box size at each corner for --clear-corners")
    parser.add_argument("--corner-max-px", type=int, default=2000, dest="corner_max_px",
                        metavar="N",
                        help="a corner box is cleared only when every component "
                             "touching it is smaller than N pixels")
    args = parser.parse_args(argv)
    if args.threshold < 0 or args.feather < 0:
        parser.error("--threshold and --feather must be >= 0")
    if args.trim < 0:
        parser.error("--trim must be >= 0")
    if args.corner_max_px < 0:
        parser.error("--corner-max-px must be >= 0")
    try:
        args.corner_size = parse_size(args.corner_size)
    except ValueError as exc:
        parser.error(str(exc))
    return args


def collect_files(paths: Sequence[str], recursive: bool) -> list[Path]:
    files: set[Path] = set()
    for raw in paths:
        path = Path(raw)
        if path.is_file():
            if path.suffix.lower() in IMAGE_EXTENSIONS:
                files.add(path)
        elif path.is_dir():
            iterator = path.rglob("*") if recursive else path.iterdir()
            for item in iterator:
                if item.is_file() and item.suffix.lower() in IMAGE_EXTENSIONS:
                    files.add(item)
    return sorted(files, key=str)


def has_real_alpha(img: Image.Image) -> bool:
    """True if the image carries any pixel with alpha < 255."""
    if "A" in img.getbands():
        return img.getchannel("A").getextrema()[0] < 255
    if img.mode == "P" and "transparency" in img.info:
        transparency = img.info["transparency"]
        if isinstance(transparency, int):
            return True
        return any(a < 255 for a in transparency)
    return False


def background_color(rgb: Image.Image) -> tuple[int, int, int]:
    """Median RGB of every border pixel (all four edges, full width/height)."""
    width, height = rgb.size
    pixels = rgb.load()
    values: list[tuple[int, int, int]] = []
    for x in range(width):
        values.append(pixels[x, 0])  # type: ignore[index]
        values.append(pixels[x, height - 1])  # type: ignore[index]
    for y in range(height):
        values.append(pixels[0, y])  # type: ignore[index]
        values.append(pixels[width - 1, y])  # type: ignore[index]
    return (
        _median([v[0] for v in values]),
        _median([v[1] for v in values]),
        _median([v[2] for v in values]),
    )


def _median(values: list[int]) -> int:
    values = sorted(values)
    return values[len(values) // 2]


def _imagemath_eval(expression: str, **kw: object) -> Image.Image:
    # Pillow >= 12 renamed eval() to unsafe_eval(); older versions only have eval().
    if hasattr(ImageMath, "unsafe_eval"):
        return ImageMath.unsafe_eval(expression, **kw)  # type: ignore[attr-defined]
    return ImageMath.eval(expression, **kw)  # type: ignore[attr-defined]


def distance_image(rgb: Image.Image, bg: tuple[int, int, int]) -> Image.Image:
    """Per-pixel Euclidean RGB distance from the background color, as an L image."""
    diff = ImageChops.difference(rgb, Image.new("RGB", rgb.size, bg))
    r, g, b = diff.split()
    if ImageMath is not None:
        try:
            # Work in F mode so the pow operator is not truncated to 8-bit integers.
            r, g, b = (ch.convert("F") for ch in (r, g, b))
            dist = _imagemath_eval("(r*r+g*g+b*b) ** 0.5", r=r, g=g, b=b)
            return dist.convert("L")
        except Exception:
            pass
    return _distance_loop(rgb, bg)


def _distance_loop(rgb: Image.Image, bg: tuple[int, int, int]) -> Image.Image:
    """Fallback distance computation for Pillow builds without ImageMath."""
    width, height = rgb.size
    src = rgb.load()
    out = Image.new("L", (width, height))
    dst = out.load()
    br, gg, bb = bg
    for y in range(height):
        for x in range(width):
            pr, pg, pb = src[x, y]  # type: ignore[misc]
            dr, dg, db = pr - br, pg - gg, pb - bb
            dst[x, y] = int(math.sqrt(dr * dr + dg * dg + db * db))
    return out


def smoothstep_lut(inner: float, outer: float) -> list[int]:
    """Alpha LUT over the band [inner, outer]: 0 at the inner edge (fully
    transparent), 255 at the outer edge (fully opaque), smoothstep in between."""
    if outer <= inner:  # zero-width band (feather=0): hard cut at the threshold
        return [0 if v <= outer else 255 for v in range(256)]
    span = outer - inner
    lut: list[int] = []
    for v in range(256):
        if v <= inner:
            lut.append(0)
        elif v >= outer:
            lut.append(255)
        else:
            t = (v - inner) / span
            t = t * t * (3.0 - 2.0 * t)  # smoothstep
            lut.append(round(255.0 * t))
    return lut


def flood_fill_mask(dist: Image.Image, outer: float) -> tuple[bytearray, int]:
    """BFS from all border pixels through candidate pixels (dist <= outer).
    Returns the visited mask plus the number of candidate pixels not reachable
    from the border (enclosed near-background regions that are preserved)."""
    width, height = dist.size
    size = width * height
    cand = dist.point([255 if v <= outer else 0 for v in range(256)]).tobytes()
    visited = bytearray(size)
    queue: deque[int] = deque()
    push, pop = queue.append, queue.popleft
    bottom_row = (height - 1) * width
    last_col = width - 1
    for x in range(width):
        for i in (x, bottom_row + x):
            if not visited[i] and cand[i] == 255:
                visited[i] = 1
                push(i)
    for y in range(height):
        row = y * width
        for i in (row, row + last_col):
            if not visited[i] and cand[i] == 255:
                visited[i] = 1
                push(i)
    while queue:
        i = pop()
        col = i % width
        j = i - 1
        if col and not visited[j] and cand[j] == 255:
            visited[j] = 1
            push(j)
        j = i + 1
        if col != last_col and not visited[j] and cand[j] == 255:
            visited[j] = 1
            push(j)
        j = i - width
        if j >= 0 and not visited[j] and cand[j] == 255:
            visited[j] = 1
            push(j)
        j = i + width
        if j < size and not visited[j] and cand[j] == 255:
            visited[j] = 1
            push(j)
    enclosed = cand.count(255) - visited.count(1)
    return visited, enclosed


def build_alpha(dist: Image.Image, mode: str, inner: float, outer: float,
                visited: Optional[bytearray] = None) -> Image.Image:
    alpha = dist.point(smoothstep_lut(inner, outer))
    if mode == "flood" and visited is not None:
        width, height = dist.size
        mask = Image.frombytes("L", (width, height), bytes(visited)).point(
            [0, 255] + [0] * 254
        )
        alpha = ImageChops.composite(
            alpha, Image.new("L", (width, height), 255), mask
        )
    return alpha


def transparent_percent(alpha: Image.Image) -> float:
    histogram = alpha.histogram()
    pixels = alpha.width * alpha.height
    return 100.0 * sum(histogram[:255]) / pixels


def trim_alpha(alpha: Image.Image, pixels: int) -> Image.Image:
    """Erode the alpha edge by N pixels.

    A 1 px erosion removes the anti-aliased white/green fringe that survives the
    color-distance key, without visibly shrinking the subject.
    """
    if pixels <= 0:
        return alpha
    return alpha.filter(ImageFilter.MinFilter(pixels * 2 + 1))


def decontaminate_edges(rgb: Image.Image, alpha: Image.Image,
                        radius: int = 2) -> Image.Image:
    """Replace translucent edge RGB with the nearest opaque pixel's color.

    The key only changes alpha, so edge pixels keep the old background color
    (white or green). When the sprite is composited on another background, that
    old color shows up as a halo. Propagating the nearest interior color fixes
    it without touching the interior.
    """
    if radius <= 0:
        return rgb
    opaque = alpha.point([255 if v >= 250 else 0 for v in range(256)])
    edge = alpha.point([255 if 0 < v < 250 else 0 for v in range(256)])
    known = opaque
    fill = rgb.copy()
    for _ in range(radius):
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            shifted = ImageChops.offset(fill, dx, dy)
            shifted_known = ImageChops.offset(known, dx, dy)
            take = ImageChops.multiply(
                edge,
                ImageChops.multiply(ImageChops.invert(known), shifted_known),
            )
            fill = Image.composite(shifted, fill, take)
            known = ImageChops.lighter(known, take)
    return fill


def despill_edges(rgb: Image.Image, alpha: Image.Image,
                  background: tuple[int, int, int],
                  strength: float = 1.0) -> Image.Image:
    """Reduce the background's dominant color on translucent edge pixels."""
    if strength <= 0:
        return rgb
    values = list(background)
    dominant = max(range(3), key=lambda i: values[i])
    others = sorted(values)
    if values[dominant] - others[-2] < 15:  # near-grey background: no spill
        return rgb

    channels = list(rgb.split())
    dominant_channel = channels[dominant]
    limit = ImageChops.lighter(
        channels[(dominant + 1) % 3], channels[(dominant + 2) % 3]
    )
    diff = ImageChops.subtract(dominant_channel, limit)
    reduction = diff.point(lambda v: int(v * strength))
    cleaned = ImageChops.subtract(dominant_channel, reduction)

    edge = ImageChops.subtract(alpha, trim_alpha(alpha, 1)).point(
        [0] + [255] * 255
    )
    channels[dominant] = Image.composite(cleaned, dominant_channel, edge)
    return Image.merge("RGB", channels)


def parse_size(text: str) -> tuple[int, int]:
    """Parse a 'WxH' size string (such as '96x48') into (width, height)."""
    match = re.fullmatch(r"\s*(\d+)\s*[xX]\s*(\d+)\s*", text)
    if not match:
        raise ValueError(f"invalid size {text!r}, expected WxH (e.g. 96x48)")
    width, height = int(match.group(1)), int(match.group(2))
    if width <= 0 or height <= 0:
        raise ValueError(f"size must be positive, got {text!r}")
    return width, height


def corner_boxes(width: int, height: int,
                 size: tuple[int, int]) -> list[tuple[int, int, int, int]]:
    """The four probe boxes (left, top, right, bottom) flush with each corner of
    the image, clamped so they never exceed it."""
    box_w = min(size[0], width)
    box_h = min(size[1], height)
    return [
        (0, 0, box_w, box_h),
        (width - box_w, 0, width, box_h),
        (0, height - box_h, box_w, height),
        (width - box_w, height - box_h, width, height),
    ]


def label_components(mask: bytes, width: int, height: int
                     ) -> tuple[list[int], list[int]]:
    """4-connected labelling of the non-zero pixels of `mask`.

    Returns (labels, sizes): labels[i] is the component id of pixel i (-1 when
    transparent), sizes[c] is the pixel count of component c.
    """
    total = width * height
    labels = [-1] * total
    sizes: list[int] = []
    stack: list[int] = []
    last_col = width - 1
    for start in range(total):
        if mask[start] == 0 or labels[start] != -1:
            continue
        component = len(sizes)
        labels[start] = component
        stack.append(start)
        count = 0
        while stack:
            i = stack.pop()
            count += 1
            col = i % width
            j = i - 1
            if col and mask[j] and labels[j] == -1:
                labels[j] = component
                stack.append(j)
            j = i + 1
            if col != last_col and mask[j] and labels[j] == -1:
                labels[j] = component
                stack.append(j)
            j = i - width
            if j >= 0 and mask[j] and labels[j] == -1:
                labels[j] = component
                stack.append(j)
            j = i + width
            if j < total and mask[j] and labels[j] == -1:
                labels[j] = component
                stack.append(j)
        sizes.append(count)
    return labels, sizes


def clear_corner_islands(alpha: Image.Image, size: tuple[int, int],
                         max_px: int) -> tuple[int, Image.Image]:
    """Erase any corner probe box whose non-transparent pixels all belong to
    small isolated components — i.e. a watermark, not the subject (which is one
    large component and is therefore never erased).

    Returns (boxes_cleared, alpha).
    """
    width, height = alpha.size
    mask = alpha.point([0] + [1] * 255).tobytes()
    labels, sizes = label_components(mask, width, height)
    data = bytearray(alpha.tobytes())
    cleared = 0
    for x0, y0, x1, y1 in corner_boxes(width, height, size):
        touched: set[int] = set()
        for y in range(y0, y1):
            row = y * width
            for x in range(x0, x1):
                component = labels[row + x]
                if component >= 0:
                    touched.add(component)
        if not touched or any(sizes[c] >= max_px for c in touched):
            continue
        for y in range(y0, y1):
            row = y * width
            for x in range(x0, x1):
                data[row + x] = 0
        cleared += 1
    if not cleared:
        return 0, alpha
    return cleared, Image.frombytes("L", (width, height), bytes(data))


def process_image(source: Path, args: argparse.Namespace) -> str:
    try:
        with Image.open(source) as img:
            if has_real_alpha(img) and not args.force:
                return _clear_existing_corners(source, img, args)
            if getattr(img, "n_frames", 1) > 1:
                return _process_animation(source, img, args)
            if args.dry_run:
                _print_dry_run(source, source.with_suffix(".png"), args)
                return "ok"
            rgb = img.convert("RGB")
        return _convert_and_save(source, source.with_suffix(".png"), rgb, args)
    except Exception as exc:
        print(f"[fail] {source}: {exc}")
        return "fail"


def _remove_background(
        rgb: Image.Image,
        args: argparse.Namespace) -> tuple[Image.Image, Optional[int], int]:
    bg = background_color(rgb)
    dist = distance_image(rgb, bg)
    inner = args.threshold - args.feather / 2.0
    outer = args.threshold + args.feather / 2.0
    enclosed: Optional[int] = None
    if args.mode == "flood":
        visited, enclosed = flood_fill_mask(dist, outer)
        alpha = build_alpha(dist, args.mode, inner, outer, visited)
    else:
        alpha = build_alpha(dist, args.mode, inner, outer)

    trim = getattr(args, "trim", 1)
    if trim > 0:
        alpha = trim_alpha(alpha, trim)
    if getattr(args, "decontaminate", True):
        rgb = decontaminate_edges(rgb, alpha, radius=2)
    if getattr(args, "despill", True):
        rgb = despill_edges(rgb, alpha, bg)

    corners = 0
    if getattr(args, "clear_corners", False):
        corners, alpha = clear_corner_islands(alpha, args.corner_size,
                                              args.corner_max_px)
    rgba = rgb.convert("RGBA")
    rgba.putalpha(alpha)
    return rgba, enclosed, corners


def _clear_existing_corners(source: Path, img: Image.Image,
                            args: argparse.Namespace) -> str:
    """Already-transparent image: background removal is skipped, but corner
    watermark islands are still erased when --clear-corners is on."""
    if not args.clear_corners:
        print(f"[skip] {source} (already transparent)")
        return "skip"
    if args.dry_run:
        _print_dry_run(source, source.with_suffix(".png"), args)
        return "ok"
    rgba = img.convert("RGBA")
    cleared, alpha = clear_corner_islands(rgba.getchannel("A"), args.corner_size,
                                          args.corner_max_px)
    if cleared == 0:
        print(f"[skip] {source} (already transparent, no corner islands)")
        return "skip"
    rgba.putalpha(alpha)
    return _save(source, source.with_suffix(".png"), rgba, args,
                 f"corners_cleared={cleared}")


def _save(source: Path, output: Path, rgba: Image.Image,
          args: argparse.Namespace, detail: str) -> str:
    if args.backup:
        target = output if output.exists() else source
        backup = Path(str(target) + ".orig")
        if not backup.exists():
            shutil.copy2(target, backup)
            print(f"[backup] {target} -> {backup}")
    rgba.save(output, "PNG", optimize=True)
    if args.delete_source and source.suffix.lower() != ".png":
        source.unlink()
        print(f"[delete] {source}")
    print(f"[ok] {source} -> {output} ({detail})")
    return "ok"


def _convert_and_save(source: Path, output: Path, rgb: Image.Image,
                      args: argparse.Namespace) -> str:
    rgba, enclosed, corners = _remove_background(rgb, args)
    pct = transparent_percent(rgba.getchannel("A"))
    detail = f"transparent={pct:.1f}%"
    if enclosed is not None:
        detail += f", enclosed_preserved={enclosed}"
    if corners:
        detail += f", corners_cleared={corners}"
    return _save(source, output, rgba, args, detail)


def _process_animation(source: Path, img: Image.Image,
                       args: argparse.Namespace) -> str:
    out_dir = source.with_suffix("")
    frame_count = getattr(img, "n_frames", 1)
    if args.dry_run:
        print(f"[dry] {source} -> {out_dir}/000.png .. "
              f"{frame_count - 1:03d}.png ({frame_count} frames, mode={args.mode})")
        return "ok"
    if args.backup:
        backup = Path(str(source) + ".orig")
        if not backup.exists():
            shutil.copy2(source, backup)
            print(f"[backup] {source} -> {backup}")
    out_dir.mkdir(parents=True, exist_ok=True)
    for index, frame in enumerate(ImageSequence.Iterator(img)):
        rgba, _, _ = _remove_background(frame.convert("RGB"), args)
        rgba.save(out_dir / f"{index:03d}.png", "PNG", optimize=True)
    if args.delete_source and source.suffix.lower() != ".png":
        source.unlink()
        print(f"[delete] {source}")
    print(f"[ok] {source} -> {out_dir}/ ({frame_count} frames)")
    return "ok"


def _print_dry_run(source: Path, output: Path, args: argparse.Namespace) -> None:
    print(f"[dry] {source} -> {output} "
          f"(mode={args.mode} threshold={args.threshold} feather={args.feather})")
    if args.backup:
        target = output if output.exists() else source
        print(f"[dry] backup: {target} -> {Path(str(target) + '.orig')}")
    if args.clear_corners:
        print(f"[dry] clear-corners {args.corner_size[0]}x{args.corner_size[1]} "
              f"max-px={args.corner_max_px}")
    if args.delete_source and source.suffix.lower() != ".png":
        print(f"[dry] delete: {source}")


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    counts = {"ok": 0, "skip": 0, "fail": 0}
    for raw in args.paths:
        if not Path(raw).exists():
            print(f"[fail] {raw}: no such file or directory")
            counts["fail"] += 1
    for path in collect_files(args.paths, args.recursive):
        counts[process_image(path, args)] += 1
    total = sum(counts.values())
    print(f"summary: total={total} processed={counts['ok']} "
          f"skipped={counts['skip']} failed={counts['fail']}")
    return 1 if counts["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
