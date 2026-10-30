"""Unit tests for convert_rgba — corner-watermark removal. No network."""
from __future__ import annotations

import argparse

import pytest
from PIL import Image

import convert_rgba

SUBJECT_BOX = (150, 150, 250, 300)
SUBJECT_PROBE = (200, 200)
WATERMARK_BOX_BR = (300, 356, 360, 380)
WATERMARK_PROBE_BR = (330, 370)
CORNER_ISLAND_BOX_TL = (0, 0, 60, 24)
CORNER_ISLAND_PROBE_TL = (5, 5)
CORNER_BLOCK_BOX = (0, 0, 96, 48)
SMALL_SUBJECT_BOX = (70, 50, 130, 70)
SMALL_SUBJECT_PROBE = (100, 60)


def alpha_image(width: int, height: int) -> Image.Image:
    return Image.new("L", (width, height), 0)


def fill_gray(img: Image.Image, rect: tuple[int, int, int, int],
              value: int = 255) -> None:
    x0, y0, x1, y1 = rect
    for y in range(y0, y1):
        for x in range(x0, x1):
            img.putpixel((x, y), value)


def fill_color(img: Image.Image, rect: tuple[int, int, int, int],
               color: tuple[int, int, int] | tuple[int, int, int, int]) -> None:
    x0, y0, x1, y1 = rect
    for y in range(y0, y1):
        for x in range(x0, x1):
            img.putpixel((x, y), color)


def test_parse_size_accepts_wxh():
    assert convert_rgba.parse_size("96x48") == (96, 48)
    assert convert_rgba.parse_size(" 96 x 48 ") == (96, 48)
    assert convert_rgba.parse_size("96X48") == (96, 48)


@pytest.mark.parametrize("bad", ["96", "96x", "0x10", "-1x5", "axb", "96x48x2"])
def test_parse_size_rejects_bad(bad):
    with pytest.raises(ValueError):
        convert_rgba.parse_size(bad)


def test_corner_boxes_flush_with_corners():
    assert convert_rgba.corner_boxes(384, 384, (96, 48)) == [
        (0, 0, 96, 48),
        (288, 0, 384, 48),
        (0, 336, 96, 384),
        (288, 336, 384, 384),
    ]


def test_corner_boxes_clamp_to_small_images():
    assert convert_rgba.corner_boxes(40, 20, (96, 48)) == [(0, 0, 40, 20)] * 4


def test_label_components_separates_blobs():
    img = alpha_image(20, 20)
    fill_gray(img, (0, 0, 5, 5))
    fill_gray(img, (10, 10, 18, 18))
    _, sizes = convert_rgba.label_components(
        img.point([0] + [1] * 255).tobytes(), 20, 20)
    assert sorted(sizes) == [25, 64]


def test_clear_corner_islands_clears_small_corner_only():
    img = alpha_image(384, 384)
    fill_gray(img, SUBJECT_BOX)
    fill_gray(img, WATERMARK_BOX_BR)
    cleared, out = convert_rgba.clear_corner_islands(img, (96, 48), 2000)
    assert cleared == 1
    assert out.getpixel(WATERMARK_PROBE_BR) == 0
    assert out.getpixel(SUBJECT_PROBE) == 255


def test_clear_corner_islands_keeps_large_component_in_corner():
    img = alpha_image(200, 120)
    fill_gray(img, CORNER_BLOCK_BOX)
    cleared, out = convert_rgba.clear_corner_islands(img, (96, 48), 2000)
    assert cleared == 0
    assert out.getpixel((10, 10)) == 255


def test_clear_corner_islands_noop_when_empty():
    img = alpha_image(64, 64)
    cleared, out = convert_rgba.clear_corner_islands(img, (96, 48), 2000)
    assert cleared == 0
    assert out.getpixel((0, 0)) == 0


def make_args(**over: object) -> argparse.Namespace:
    base = dict(mode="flood", threshold=60, feather=25, trim=0,
                decontaminate=True, despill=True, remove_holes=False,
                hole_min_area=100, clear_corners=False,
                corner_size=(96, 48), corner_max_px=2000,
                dry_run=False, backup=False, delete_source=False, force=False)
    base.update(over)
    return argparse.Namespace(**base)


def watermarked_rgb() -> Image.Image:
    rgb = Image.new("RGB", (384, 384), (255, 255, 255))
    fill_color(rgb, SUBJECT_BOX, (40, 80, 140))
    fill_color(rgb, WATERMARK_BOX_BR, (150, 150, 150))
    return rgb


def test_remove_background_corner_clearing_toggle():
    rgb = watermarked_rgb()
    rgba_off, _, corners_off = convert_rgba._remove_background(
        rgb, make_args(clear_corners=False))
    rgba_on, _, corners_on = convert_rgba._remove_background(
        rgb, make_args(clear_corners=True))

    assert corners_off == 0 and corners_on == 1
    assert rgba_off.getpixel(WATERMARK_PROBE_BR)[3] > 0
    assert rgba_on.getpixel(WATERMARK_PROBE_BR)[3] == 0
    assert rgba_on.getpixel(SUBJECT_PROBE)[3] == 255


def cut_frame_with_watermark() -> Image.Image:
    rgba = Image.new("RGBA", (200, 120), (0, 0, 0, 0))
    fill_color(rgba, SMALL_SUBJECT_BOX, (40, 80, 140, 255))
    fill_color(rgba, CORNER_ISLAND_BOX_TL, (150, 150, 150, 255))
    return rgba


def test_already_transparent_image_gets_corners_cleared(tmp_path):
    src = tmp_path / "cut.png"
    cut_frame_with_watermark().save(src)

    assert convert_rgba.process_image(src, make_args(clear_corners=True)) == "ok"
    out = Image.open(src).convert("RGBA")
    assert out.getpixel(CORNER_ISLAND_PROBE_TL)[3] == 0
    assert out.getpixel(SMALL_SUBJECT_PROBE)[3] == 255


def test_already_transparent_image_skipped_without_flag(tmp_path):
    src = tmp_path / "cut.png"
    cut_frame_with_watermark().save(src)

    assert convert_rgba.process_image(src, make_args(clear_corners=False)) == "skip"
    assert Image.open(src).convert("RGBA").getpixel(CORNER_ISLAND_PROBE_TL)[3] == 255


def test_trim_alpha_erodes_one_pixel():
    alpha = Image.new("L", (10, 10), 0)
    fill_gray(alpha, (2, 2, 8, 8), 128)
    fill_gray(alpha, (3, 3, 7, 7), 255)

    out = convert_rgba.trim_alpha(alpha, 1)
    assert out.getpixel((2, 2)) == 0
    assert out.getpixel((5, 5)) == 255


def test_decontaminate_edges_copies_interior_color():
    rgb = Image.new("RGB", (9, 9), (255, 255, 255))
    fill_color(rgb, (2, 2, 7, 7), (255, 200, 200))  # white-ish fringe
    fill_color(rgb, (3, 3, 6, 6), (200, 0, 0))      # interior
    alpha = Image.new("L", (9, 9), 0)
    fill_gray(alpha, (2, 2, 7, 7), 128)
    fill_gray(alpha, (3, 3, 6, 6), 255)

    out = convert_rgba.decontaminate_edges(rgb, alpha, radius=2)
    r, g, b = out.getpixel((2, 2))
    assert r > 150 and g < 80 and b < 80


def test_despill_edges_reduces_dominant_background_channel():
    rgb = Image.new("RGB", (9, 9), (0, 177, 64))
    fill_color(rgb, (2, 2, 7, 7), (100, 200, 100))
    alpha = Image.new("L", (9, 9), 0)
    fill_gray(alpha, (2, 2, 7, 7), 128)
    fill_gray(alpha, (3, 3, 6, 6), 255)

    out = convert_rgba.despill_edges(rgb, alpha, (0, 177, 64), strength=1.0)
    r, g, b = out.getpixel((2, 2))
    assert g <= max(r, b) + 1


def test_remove_white_holes_keeps_tinted_fur():
    rgba = Image.new("RGBA", (30, 30), (0, 0, 0, 0))
    fill_color(rgba, (4, 4, 26, 26), (200, 0, 0, 255))
    fill_color(rgba, (9, 9, 21, 21), (255, 255, 255, 255))  # 144 px neutral hole
    fill_color(rgba, (5, 5, 8, 8), (250, 245, 255, 255))    # tinted fur

    out = convert_rgba.remove_white_holes(
        rgba.convert("RGB"), rgba.getchannel("A")
    )
    assert out.getpixel((15, 15)) == 0
    assert out.getpixel((6, 6)) == 255
