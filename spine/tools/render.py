#!/usr/bin/env python3
"""Minimal Spine 4.2 rig renderer (region attachments only).

Repository copy: spine/tools/render.py.  Used by make_verify.py and by the
manifest checks, so the verify images can be reproduced with nothing outside
the repository except Pillow.

Renders the rest pose, or a clip at time t, using PIL.  Coordinate conventions:
  * world: Y up, bone local +Y up (Spine)
  * screen: Y down, pixel (0,0) = top-left
  * screen_x = world_x - skeleton.x
  * screen_y = skeleton.y + height - world_y

Bone world transform: parent chain of local T(x,y) * R(rotation) * S(scaleX,scaleY).
Rotation is CCW positive with Spine's convention (a=cos, b=-sin, c=+sin, d=cos;
stored row-major as a,b / c,d here).  Region attachments are placed from the
atlas rect (xy/size/rotate), scaled by the bone world scale, rotated by the bone
world rotation, and drawn in slot order with alpha compositing.

Animation channels are deltas on top of the setup pose: translate is additive
(bone-local units), rotate is additive (degrees), scale is multiplicative.

CLI:
  python3 render.py OUT.png --json X.json --atlas X.atlas [--clip idle --time 0.5]
"""
import json
import math
import sys

from PIL import Image


# ---------------------------------------------------------------- atlas ----
def parse_atlas(path):
    pages = {}
    cur = None
    region = None
    with open(path, "r", encoding="utf-8") as fh:
        lines = [ln.rstrip("\n") for ln in fh]
    for ln in lines:
        if not ln.strip():
            continue
        if ":" not in ln and ln.strip().endswith(".png"):
            cur = ln.strip()
            pages[cur] = {}
            continue
        if ln.startswith(" ") or ln.startswith("\t"):
            if region is None:
                continue
            key, _, val = ln.strip().partition(":")
            key = key.strip()
            val = val.strip()
            d = pages[cur][region]
            if key in ("xy", "size", "orig", "offset"):
                d[key] = tuple(int(x) for x in val.split(","))
            elif key == "rotate":
                d[key] = val
            elif key == "index":
                d[key] = int(val)
            continue
        name = ln.strip()
        if name in ("size", "format", "filter", "repeat", "pma"):
            continue
        region = name
        pages[cur][region] = {"rotate": "false"}
    return pages


def load_atlas(base):
    """Return {region_name: dict(page_image, xy, size, orig, offset, rotate)}."""
    atlas = base + ".atlas"
    png = base + ".png"
    pages = parse_atlas(atlas)
    out = {}
    for page_name, regions in pages.items():
        if not page_name.endswith(".png"):
            continue
        img = None
        for reg, d in regions.items():
            if img is None:
                img = Image.open(png).convert("RGBA")
            size = d.get("size", (0, 0))
            out[reg] = {
                "page": img,
                "xy": d.get("xy", (0, 0)),
                "size": size,
                "orig": d.get("orig", size),
                "offset": d.get("offset", (0, 0)),
                "rotate": d.get("rotate", "false"),
            }
    return out


def region_image(entry):
    """Crop the region out of the page, un-rotating if needed (rotate=90)."""
    x, y = entry["xy"]
    w, h = entry["size"]
    page = entry["page"]
    rot = entry["rotate"]
    if rot in (True, "true", "90"):
        return page.crop((x, y, x + h, y + w)).rotate(-90, expand=True)
    return page.crop((x, y, x + w, y + h))


# ---------------------------------------------------------------- bones ----
class Bone:
    __slots__ = ("name", "parent", "x", "y", "rot", "sx", "sy", "srx", "sry",
                 "ax", "ay", "arot", "ascx", "ascy", "shear_x", "shear_y",
                 "a", "b", "c", "d", "wx", "wy")


def build_bones(data):
    bones = {}
    order = []
    for bd in data["bones"]:
        b = Bone()
        b.name = bd["name"]
        b.parent = bd.get("parent")
        b.x = float(bd.get("x", 0.0))
        b.y = float(bd.get("y", 0.0))
        b.rot = float(bd.get("rotation", 0.0))
        b.sx = float(bd.get("scaleX", 1.0))
        b.sy = float(bd.get("scaleY", 1.0))
        b.srx = float(bd.get("shearX", 0.0))
        b.sry = float(bd.get("shearY", 0.0))
        b.a = 1.0
        b.b = 0.0
        b.c = 0.0
        b.d = 1.0
        b.wx = 0.0
        b.wy = 0.0
        bones[b.name] = b
        order.append(b.name)
    return bones, order


def compute_world(bones, order, anim=None, time=0.0, curves=False):
    """Compute local and world transforms for every bone."""
    for name in order:
        b = bones[name]
        b.ax, b.ay = b.x, b.y
        b.arot = b.rot
        b.ascx, b.ascy = b.sx, b.sy
        b.shear_x = b.srx
        b.shear_y = b.sry

    if anim:
        for name, tl in anim.get("bones", {}).items():
            b = bones.get(name)
            if b is None:
                continue
            # Animation channels are offsets on top of the setup pose.
            if "translate" in tl:
                b.ax = b.x + sample(tl["translate"], time, "x", 0.0, curves)
                b.ay = b.y + sample(tl["translate"], time, "y", 0.0, curves)
            if "rotate" in tl:
                b.arot = b.rot + sample(tl["rotate"], time, "value", 0.0, curves)
            if "scale" in tl:
                b.ascx = b.sx * sample(tl["scale"], time, "x", 1.0, curves)
                b.ascy = b.sy * sample(tl["scale"], time, "y", 1.0, curves)

    for name in order:
        b = bones[name]
        rot = math.radians(b.arot)
        sx, sy = b.ascx, b.ascy
        # Spine 4.2 local transform = translate * rotate * shear * scale.
        # CCW-positive: a=cos, b=-sin, c=+sin, d=cos.
        la = math.cos(rot + math.radians(b.shear_x)) * sx
        lb = -math.sin(rot + math.radians(b.shear_y)) * sy
        lc = math.sin(rot + math.radians(b.shear_x)) * sx
        ld = math.cos(rot + math.radians(b.shear_y)) * sy
        p = bones.get(b.parent) if b.parent else None
        if p is None:
            b.wx, b.wy = b.ax, b.ay
            b.a, b.b, b.c, b.d = la, lb, lc, ld
        else:
            b.wx = p.a * b.ax + p.b * b.ay + p.wx
            b.wy = p.c * b.ax + p.d * b.ay + p.wy
            b.a = p.a * la + p.b * lc
            b.b = p.a * lb + p.b * ld
            b.c = p.c * la + p.d * lc
            b.d = p.c * lb + p.d * ld


def _bezier_y(p0x, p0y, c1x, c1y, c2x, c2y, p3x, p3y, x):
    """Cubic-bezier evaluation with x as the time axis (monotonic)."""
    lo, hi = 0.0, 1.0
    for _ in range(60):
        u = (lo + hi) * 0.5
        mu = 1.0 - u
        bx = (mu ** 3) * p0x + 3 * (mu ** 2) * u * c1x + 3 * mu * (u ** 2) * c2x + (u ** 3) * p3x
        if bx < x:
            lo = u
        else:
            hi = u
    u = (lo + hi) * 0.5
    mu = 1.0 - u
    return (mu ** 3) * p0y + 3 * (mu ** 2) * u * c1y + 3 * mu * (u ** 2) * c2y + (u ** 3) * p3y


def sample(keys, time, field, default, curves=False):
    """Sample a timeline at `time`.

    curves=False reproduces the historical verify pipeline (linear segments);
    curves=True honours the optional Spine `curve` bezier control points.
    """
    if not keys:
        return default
    # Spine lets the first key omit "time"; it means 0.  Editors such as
    # Rigby2D and Rev2D write it that way, and the file is still valid 4.2.
    t0 = keys[0].get("time", 0.0)
    if time <= t0:
        return float(keys[0].get(field, default))
    t_last = keys[-1].get("time", 0.0)
    if time >= t_last:
        return float(keys[-1].get(field, default))
    for i in range(len(keys) - 1):
        k0, k1 = keys[i], keys[i + 1]
        t0 = k0.get("time", 0.0)
        t1 = k1.get("time", 0.0)
        if t0 <= time <= t1:
            v0 = float(k0.get(field, default))
            v1 = float(k1.get(field, default))
            if t1 == t0:
                return v1
            curve = k0.get("curve")
            if curves and curve and len(curve) >= 4:
                off = 4 if (field == "y" and len(curve) >= 8) else 0
                cx1, cy1, cx2, cy2 = curve[off:off + 4]
                return _bezier_y(t0, v0, cx1, cy1, cx2, cy2,
                                 t1, v1, time)
            f = (time - t0) / (t1 - t0)
            return v0 + (v1 - v0) * f
    return default


def world_to_screen(skel, h, wx, wy):
    return wx - skel.get("x", 0.0), skel.get("y", 0.0) + h - wy


# ----------------------------------------------------------------- draw ----
def draw_region(canvas, img, w, h, bone, att, skel, cw, ch, resample=Image.NEAREST):
    """Draw one region attachment.  Region local frame: x right, y up."""
    ax = float(att.get("x", 0.0))
    ay = float(att.get("y", 0.0))
    aw = float(att.get("width", w))
    ah = float(att.get("height", h))
    arot = float(att.get("rotation", 0.0))
    ascx = float(att.get("scaleX", 1.0))
    ascy = float(att.get("scaleY", 1.0))
    c, s = math.cos(math.radians(arot)), math.sin(math.radians(arot))

    # sprite px -> attachment-local (x right, y up), pixel-centre aligned
    # -> bone local -> world -> screen
    def to_screen(sx, sy):
        # The attachment's width/height define the quad; the image is mapped
        # across it (Spine stretches the region when the two differ).
        # Corner-accurate quad mapping: the image spans exactly width/height.
        # The 0.5 * scale term keeps the historical half-pixel nudge, which is
        # what the committed verify images were produced with.
        lx = (sx / w - 0.5) * aw * ascx + 0.5 * ascx
        ly = (0.5 - sy / h) * ah * ascy - 0.5 * ascy
        rx = c * lx - s * ly + ax
        ry = s * lx + c * ly + ay
        wx = bone.a * rx + bone.b * ry + bone.wx
        wy = bone.c * rx + bone.d * ry + bone.wy
        return world_to_screen(skel, ch, wx, wy)

    p00 = to_screen(0, 0)
    p10 = to_screen(w, 0)
    p01 = to_screen(0, h)
    # p = p00 + sx/w*(p10-p00) + sy/h*(p01-p00)
    ex = (p10[0] - p00[0]) / w
    ey = (p10[1] - p00[1]) / w
    fx = (p01[0] - p00[0]) / h
    fy = (p01[1] - p00[1]) / h
    det = ex * fy - ey * fx
    if abs(det) < 1e-9:
        return
    ia = fy / det
    ib = -fx / det
    ic = -ey / det
    id_ = ex / det
    ie = -(ia * p00[0] + ib * p00[1])
    iff = -(ic * p00[0] + id_ * p00[1])
    layer = img.transform((cw, ch), Image.AFFINE, (ia, ib, ie, ic, id_, iff),
                          resample=resample)
    canvas.alpha_composite(layer)


def find_attachment(atts, slot):
    """Look an attachment up by slot name, the way Spine groups skins.

    A missing attachment is a data error.  Warn and skip; never borrow a
    same-named attachment from another group, because that silently hides the
    mistake (the front cuffs were duplicated under their bone's group for a
    while and two lookups made it work).
    """
    name = slot.get("attachment")
    if name is None:
        return None
    group = atts.get(slot.get("name"), {})
    if name in group:
        return group[name]
    print("warning: slot %r has no attachment %r, skipped"
          % (slot.get("name"), name), file=sys.stderr)
    return None


def render(json_path, atlas_base, out_path=None, clip=None, time=0.0,
           scale=1.0, curves=False, resample=Image.NEAREST):
    with open(json_path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    skel = data["skeleton"]
    cw = int(round(skel.get("width", 756)))
    ch = int(round(skel.get("height", 1008)))
    regions = load_atlas(atlas_base)
    skin = data["skins"][0] if isinstance(data["skins"], list) else data["skins"]
    atts = skin.get("attachments", {})
    bones, order = build_bones(data)
    anim = data["animations"][clip] if clip else None
    compute_world(bones, order, anim, time, curves)

    canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    for slot in data["slots"]:
        name = slot["attachment"]
        if not name:
            continue
        col = slot.get("color", "ffffffff")
        if len(col) == 8 and col[6:8].lower() == "00":
            continue

        bname = slot["bone"]
        a = find_attachment(atts, slot)
        if not a or a.get("type", "region") != "region":
            continue
        entry = regions[a.get("path", name)]
        img = region_image(entry)
        draw_region(canvas, img, img.width, img.height, bones[bname], a, skel, cw, ch,
                    resample=resample)
    if scale != 1.0:
        canvas = canvas.resize((int(cw * scale), int(ch * scale)), Image.LANCZOS)
    if out_path:
        canvas.save(out_path)
    return canvas


def duration(data, clip):
    last = 0.0
    for tl in data["animations"][clip].get("bones", {}).values():
        for keys in tl.values():
            if keys:
                last = max(last, keys[-1].get("time", 0.0))
    return last


if __name__ == "__main__":
    import argparse

    ap = argparse.ArgumentParser(description="Offline Spine 4.2 region renderer")
    ap.add_argument("out")
    ap.add_argument("--json", required=True)
    ap.add_argument("--atlas", required=True, help="atlas base path without extension")
    ap.add_argument("--clip")
    ap.add_argument("--time", type=float, default=0.0, help="seconds into the clip")
    ap.add_argument("--curves", action="store_true",
                    help="honour bezier easing (the verify pipeline uses linear)")
    args = ap.parse_args()
    render(args.json, args.atlas, args.out, args.clip, args.time, curves=args.curves)
    print(f"wrote {args.out}")
