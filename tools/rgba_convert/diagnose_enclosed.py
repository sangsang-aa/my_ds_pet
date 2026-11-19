#!/usr/bin/env python3
"""Reproduce / verify the ENCLOSED WHITE GAP defect in convert_rgba.

Build synthetic sprite-like frames (an opaque "hair ring" enclosing a white
pocket) and report, stage by stage, whether the pocket survives as opaque.

This is a DIAGNOSTIC tool, not a fix. Run it against the real clicked atlas
frames by swapping in make_frame()/loading a real PNG; the printed fields tell
you exactly which stage leaves the pocket opaque.

    python diagnose_enclosed.py
"""
from __future__ import annotations

import argparse

from PIL import Image

import convert_rgba as cr

W = H = 81
CX = CY = 40
TH = 60
FE = 25
INNER = TH - FE / 2.0
OUTER = TH + FE / 2.0


def make_frame(bg, ring, hole, hole_size=21, ring_thick=6):
    """Opaque ring (hair/body) fully enclosing a white pocket."""
    rgb = Image.new("RGB", (W, H), bg)
    px = rgb.load()
    half = hole_size // 2
    x0, x1 = CX - half, CX + half + 1
    y0, y1 = CY - half, CY + half + 1
    rx0, rx1 = x0 - ring_thick, x1 + ring_thick
    ry0, ry1 = y0 - ring_thick, y1 + ring_thick
    for y in range(H):
        for x in range(W):
            in_hole = x0 <= x < x1 and y0 <= y < y1
            in_ring = rx0 <= x < rx1 and ry0 <= y < ry1 and not in_hole
            if in_hole:
                px[x, y] = hole
            elif in_ring:
                px[x, y] = ring
    return rgb


def args(**over):
    base = dict(mode="flood", threshold=TH, feather=FE, trim=0,
                decontaminate=False, despill=False, remove_holes=False,
                hole_min_area=100, punch_enclosed=True,
                clear_corners=False, corner_size=(96, 48), corner_max_px=2000,
                clear_rects=[], dry_run=False, backup=False,
                delete_source=False, force=False)
    base.update(over)
    return argparse.Namespace(**base)


def stats_for(rgb, alpha):
    """Replicate remove_white_holes' component test on the pocket at center."""
    px, ap = rgb.load(), alpha.load()
    mask = bytearray(W * H)
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            if ap[x, y] >= 250 and min(r, g, b) >= 248:
                mask[y * W + x] = 1
    labels, sizes = cr.label_components(bytes(mask), W, H)
    cid = labels[CY * W + CX]
    if cid < 0:
        return None
    spread_sum, ring_sum, ring_cnt = 0.0, 0.0, 0
    for y in range(H):
        for x in range(W):
            if labels[y * W + x] != cid:
                continue
            r, g, b = px[x, y]
            spread_sum += max(r, g, b) - min(r, g, b)
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = y + dy, x + dx
                if 0 <= ny < H and 0 <= nx < W and labels[ny * W + nx] != cid:
                    nr, ng, nb = px[nx, ny]
                    ring_sum += 0.299 * nr + 0.587 * ng + 0.114 * nb
                    ring_cnt += 1
    return dict(area=sizes[cid], spread=spread_sum / sizes[cid],
                ring=ring_sum / ring_cnt if ring_cnt else 255.0)


GREEN = (0, 177, 64)
DARK = (40, 40, 40)
WHITE = (255, 255, 255)
CREAM = (230, 225, 215)

SCENARIOS = [
    ("A green-bg + dark ring + WHITE hole (441px)", GREEN, DARK, WHITE, 21),
    ("B green-bg + dark ring + KEY-GREEN hole", GREEN, DARK, GREEN, 21),
    ("C white-bg + dark ring + WHITE hole", WHITE, DARK, WHITE, 21),
    ("D green-bg + dark ring + BIG white hole (2025px)", GREEN, DARK, WHITE, 45),
    ("E green-bg + CREAM ring + WHITE hole", GREEN, CREAM, WHITE, 21),
]


def run_case(name, bg, ring, hole, hs):
    frame = make_frame(bg, ring, hole, hole_size=hs)
    bgc = cr.background_color(frame)
    dist = cr.distance_image(frame, bgc)
    visited, enclosed = cr.flood_fill_mask(dist, OUTER)
    cand = dist.point([255 if v <= OUTER else 0 for v in range(256)]).tobytes()
    idx = CY * W + CX

    rgba_def, _, _, punched_def = cr._remove_background(frame, args())
    a_def = rgba_def.getpixel((CX, CY))[3]
    rgba_np, _, _, _ = cr._remove_background(frame, args(punch_enclosed=False))
    a_np = rgba_np.getpixel((CX, CY))[3]
    rgba_holes, _, _, _ = cr._remove_background(frame, args(remove_holes=True))
    a_holes = rgba_holes.getpixel((CX, CY))[3]

    st = stats_for(frame, rgba_def.getchannel("A"))
    print("=" * 72)
    print(name)
    print(f"  bg_color={bgc}  hole_dist={dist.getpixel((CX,CY))}")
    print(f"  hole_is_candidate={cand[idx]==255}  flood_visited={visited[idx]}  enclosed_px={enclosed}")
    print(f"  alpha_center default={a_def}  no_punch={a_np}  remove_holes={a_holes}")
    print(f"  punched(default)={punched_def}")
    if st:
        hit = 100 <= st['area'] <= 1000 and st['spread'] < 4.0 and st['ring'] < 210.0
        print(f"  component area={st['area']} spread={st['spread']:.2f} ring_lum={st['ring']:.1f} "
              f"-> remove_holes judges={'REMOVE' if hit else 'KEEP'}")
    else:
        print("  component: none (hole center not a neutral-white component)")


if __name__ == "__main__":
    for c in SCENARIOS:
        run_case(*c)
