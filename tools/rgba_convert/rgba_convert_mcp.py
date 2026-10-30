#!/usr/bin/env python3
"""MCP server exposing the desktop-pet background-removal converter as a tool.

Wraps tools/convert_rgba.py. Because MCP stdio uses stdout for the JSON-RPC
protocol, the wrapped functions' stdout output is redirected into a buffer and
returned as the tool result.
"""

from __future__ import annotations

import contextlib
import io
import sys
from argparse import Namespace
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import convert_rgba  # noqa: E402
from mcp.server.mcpserver import MCPServer  # noqa: E402

VALID_MODES = ("flood", "chroma")

server = MCPServer(
    name="rgba-convert",
    instructions=(
        "Background removal (抠图) for images with a near-uniform background. "
        "Call convert_to_rgba with absolute paths to strip the background and "
        "save transparent RGBA PNGs."
    ),
)


@server.tool(
    name="convert_to_rgba",
    description=(
        "Batch-convert images (PNG/JPG/JPEG/BMP/WEBP) that have a near-uniform "
        "background into RGBA PNGs with the background made transparent. "
        "Animated GIFs are also supported: each frame is background-removed and "
        "written as a PNG frame sequence into a sibling folder named after the GIF "
        "(e.g. pet_down.gif -> pet_down/000.png, 001.png, ...), preserving full "
        "alpha. "
        "mode='flood' (default) removes only background connected to the image "
        "border, preserving background-coloured parts of the subject; "
        "mode='chroma' removes every near-background pixel (also gaps inside the "
        "subject, but may punch holes in light subject areas). "
        "Returns a per-file log and a summary line. Output PNG names match the "
        "input basenames so existing frame-loading conventions keep working. "
        "With clear_corners=True, small isolated non-transparent islands at the "
        "four corners (moving AI watermarks) are erased; the subject itself is a "
        "single large component and is never erased."
    ),
)
def convert_to_rgba(
    paths: list[str],
    recursive: bool = False,
    mode: str = "flood",
    threshold: int = 60,
    feather: int = 25,
    backup: bool = False,
    delete_source: bool = False,
    force: bool = False,
    dry_run: bool = False,
    clear_corners: bool = False,
    corner_size: str = "96x48",
    corner_max_px: int = 2000,
) -> str:
    if not paths:
        return "error: 'paths' must contain at least one file or directory"
    if mode not in VALID_MODES:
        return f"error: mode must be one of {VALID_MODES}, got {mode!r}"
    if threshold < 0 or feather < 0:
        return "error: threshold and feather must be >= 0"
    if corner_max_px < 0:
        return "error: corner_max_px must be >= 0"
    try:
        parsed_corner_size = convert_rgba.parse_size(corner_size)
    except ValueError as exc:
        return f"error: {exc}"

    args = Namespace(
        paths=paths,
        recursive=recursive,
        mode=mode,
        threshold=threshold,
        feather=feather,
        backup=backup,
        delete_source=delete_source,
        force=force,
        dry_run=dry_run,
        clear_corners=clear_corners,
        corner_size=parsed_corner_size,
        corner_max_px=corner_max_px,
    )

    counts = {"ok": 0, "skip": 0, "fail": 0}
    missing = 0
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        for raw in paths:
            if not Path(raw).exists():
                print(f"[fail] {raw}: no such file or directory")
                missing += 1
        for path in convert_rgba.collect_files(paths, recursive):
            counts[convert_rgba.process_image(path, args)] += 1
        total = sum(counts.values()) + missing
        failed = counts["fail"] + missing
        print(
            f"summary: total={total} processed={counts['ok']} "
            f"skipped={counts['skip']} failed={failed}"
        )
    log = buffer.getvalue().strip()
    return log if log else "no files matched the given paths"


def main() -> None:
    server.run(transport="stdio")


if __name__ == "__main__":
    main()
