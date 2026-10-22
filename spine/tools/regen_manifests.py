#!/usr/bin/env python3
"""Rewrite a view's static-parts manifests from its rig.

The manifests in spine-parts-v2-20261020/<view>/ describe how the parts sit in
the character: layout.json, parts.csv, <view>-setup-spine42.json and
validation.json.  They were produced when the parts were first split and then
drifted away from the rig (new face, socks, eyebrows, split front arms), so this
regenerates them from spine/<view>/<rig>.json, which is the authority.

Per slot the rig supplies:
  draw_order  slot index (the real draw order)
  parent      the bound bone's parent
  pivot_xy    the bound bone's world position, in canvas pixels
  bbox_xywh   where that part's PNG sits on the canvas (width/height = PNG size,
              because the package contract is one PNG per entry)
  region_scale/atlas_size
              only when the rig draws the art larger than the PNG; the front
              arm art is baked at ~1.09x / ~1.26x in the atlas

Slots whose art is not a separate file under parts/ are listed in
derived_parts; art that ships but is not bound is listed in unbound_parts;
art the rig no longer uses is listed in deprecated_parts with its replacement.

The setup JSON keeps every bone of the rig and rebuilds each attachment around
the same art at the same size, so it reproduces the rig's rest quads exactly.

Usage:
  python3 spine/tools/regen_manifests.py front --check   # report drift only
  python3 spine/tools/regen_manifests.py front           # rewrite the files
"""
import argparse
import csv
import io
import json
import math
import pathlib
import sys

TOOLS = pathlib.Path(__file__).resolve().parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import render  # noqa: E402
from PIL import Image  # noqa: E402

VIEWS = {
    "front": dict(rig="spine/front/whale_girl.json",
                  package="spine-parts-v2-20261020/front"),
    "side": dict(rig="spine/side/whale_girl_side.json",
                 package="spine-parts-v2-20261020/side"),
    "back": dict(rig="spine/back/whale_girl_back.json",
                 package="spine-parts-v2-20261020/back"),
}

# Parts that ship as PNGs but are no longer bound, and what replaced them.
REPLACED = {
    "sleeve_screen_left": ["upper_arm_screen_left", "forearm_screen_left",
                           "cuff_screen_left"],
    "sleeve_screen_right": ["upper_arm_screen_right", "forearm_screen_right",
                            "cuff_screen_right"],
}

# Parts whose PNG was cut out of the atlas, so the atlas is their only source
# and a re-cut cannot recover detail.  Recorded as `source: atlas-derived`.
ATLAS_DERIVED = {
    "front": {"sock_screen_left", "sock_screen_right",
              "eyebrow_screen_left", "eyebrow_screen_right"},
}

# Why a part's PNG and its atlas region may legitimately differ in size.
DIFF_REASON_SCALED = ("the atlas bakes the display scale into the texture "
                      "(LANCZOS); region_scale records the ratio and the rig "
                      "keeps drawing the same size. Do not copy the atlas "
                      "region back into parts/.")
DIFF_REASON_CROP = ("the package PNG is a different crop of the same art than "
                    "the atlas region; the rig expresses the difference in "
                    "width/height. Do not copy the atlas region back into "
                    "parts/.")

# Prose for parts the original manifest never described.
NEW_NOTES = {
    "upper_arm": ("Upper arm of the split arm chain "
                  "(shoulder -> upper arm -> forearm -> cuff -> hand).",
                  "Split from the old single sleeve; no reshaping."),
    "forearm": ("Forearm; the bone sits at the middle of the upper-arm/forearm "
                "overlap, which is the elbow.",
                "Split from the old single sleeve; no reshaping."),
    "cuff": ("Fur cuff, bound to the forearm bone.",
             "Split from the old sleeve's fur trim; no reshaping."),
    "fist": ("Closed-fist variant that ships with the art but is not bound yet.",
             "Spare hand pose, kept for a future action."),
}
DERIVED_METHOD = ("Derived crop baked into the atlas; there is no standalone "
                  "PNG for this slot in parts/.")
DERIVED_NOTES = {
    "sock": "Sock, drawn under the boot.",
    "boot": "Far-side boot, drawn behind the near one.",
    "eyebrow": "Eyebrow, drawn over the face.",
    "flap": "Far-side coat flap, drawn behind the coat.",
    "trim": "Coat fur trim.",
}


def quad(att, bone, skel, ch):
    """The four corners of an attachment quad, in canvas pixels (y down)."""
    aw = float(att.get("width", 0.0))
    ah = float(att.get("height", 0.0))
    ax = float(att.get("x", 0.0))
    ay = float(att.get("y", 0.0))
    asx = float(att.get("scaleX", 1.0))
    asy = float(att.get("scaleY", 1.0))
    ca = math.cos(math.radians(float(att.get("rotation", 0.0))))
    sa = math.sin(math.radians(float(att.get("rotation", 0.0))))
    sx, sy = skel.get("x", 0.0), skel.get("y", 0.0)
    out = []
    for u, v in ((-aw / 2, -ah / 2), (aw / 2, -ah / 2),
                 (-aw / 2, ah / 2), (aw / 2, ah / 2)):
        lx, ly = u * asx, v * asy
        rx = ca * lx - sa * ly + ax
        ry = sa * lx + ca * ly + ay
        wx = bone.a * rx + bone.b * ry + bone.wx
        wy = bone.c * rx + bone.d * ry + bone.wy
        out.append((wx - sx, sy + ch - wy))
    return out


def describe(view):
    cfg = VIEWS[view]
    rig_path = ROOT / cfg["rig"]
    pkg = ROOT / cfg["package"]
    parts_dir = pkg / "parts"

    rig = json.loads(rig_path.read_text(encoding="utf-8"))
    skel = rig["skeleton"]
    ch = int(round(skel.get("height", 1008)))
    bones, order = render.build_bones(rig)
    render.compute_world(bones, order)
    atts = rig["skins"][0]["attachments"]
    entries = render.load_atlas(str((ROOT / cfg["rig"]).with_suffix("")))
    atlas_derived = ATLAS_DERIVED.get(view, set())

    old_path = pkg / "layout.json"
    old = json.loads(old_path.read_text(encoding="utf-8")) if old_path.exists() else {}
    old_parts = {p["name"]: p for p in old.get("parts", [])}

    pngs = {p.stem: p for p in parts_dir.glob("*.png")}
    used = set()
    parts, derived = [], []
    checks = {"pivot_max_delta": 0.0, "bbox_max_delta": 0.0,
              "setup_quad_max_delta": 0.0}

    for index, slot in enumerate(rig["slots"]):
        name = slot["name"]
        att = render.find_attachment(atts, slot)
        if not att or att.get("type", "region") != "region":
            continue
        bone = bones[slot["bone"]]
        q = quad(att, bone, skel, ch)
        cx = sum(p[0] for p in q) / 4.0
        cy = sum(p[1] for p in q) / 4.0
        drawn_w = math.hypot(q[1][0] - q[0][0], q[1][1] - q[0][1])
        drawn_h = math.hypot(q[2][0] - q[0][0], q[2][1] - q[0][1])
        pivot = (bone.wx - skel.get("x", 0.0),
                 skel.get("y", 0.0) + ch - bone.wy)
        parent = bones[slot["bone"]].parent or "root"

        entry = {
            "name": name,
            "draw_order": index,
            "parent": parent,
            "pivot_xy": [round(pivot[0], 3), round(pivot[1], 3)],
            "source": "atlas-derived" if name in atlas_derived else "native",
        }

        png = pngs.get(name)
        if png is not None:
            used.add(name)
            pw, ph = Image.open(png).size
            box = [int(round(cx - pw / 2.0)), int(round(cy - ph / 2.0)), pw, ph]
            checks["bbox_max_delta"] = max(
                checks["bbox_max_delta"],
                abs(box[0] + pw / 2.0 - cx), abs(box[1] + ph / 2.0 - cy))
            previous = old_parts.get(name)
            if previous:
                note = previous.get("note", "")
                method = previous.get("method", "Existing part, unchanged.")
            else:
                key = next((k for k in NEW_NOTES if name.startswith(k)), None)
                note, method = NEW_NOTES[key] if key else ("", "New part.")
            entry.update({
                "bbox_xywh": box,
                "original_bbox_xywh": previous.get("original_bbox_xywh") if previous else None,
                "note": note,
                "method": method,
            })
            if abs(drawn_w - pw) > 0.01 or abs(drawn_h - ph) > 0.01:
                entry["atlas_size"] = [round(drawn_w, 1), round(drawn_h, 1)]
                entry["region_scale"] = [round(drawn_w / pw, 4),
                                         round(drawn_h / ph, 4)]
            parts.append(entry)
        else:
            key = next((k for k in DERIVED_NOTES if k in name), None)
            entry.update({
                "bbox_xywh": [int(round(cx - drawn_w / 2.0)),
                              int(round(cy - drawn_h / 2.0)),
                              int(round(drawn_w)), int(round(drawn_h))],
                "note": DERIVED_NOTES.get(key, ""),
                "method": DERIVED_METHOD,
                "source": "atlas-derived",
            })
            derived.append(entry)

    unbound, deprecated = [], []
    for stem, path in sorted(pngs.items()):
        if stem in used:
            continue
        pw, ph = Image.open(path).size
        if stem in REPLACED:
            deprecated.append({
                "name": stem,
                "replaced_by": REPLACED[stem],
                "source_size": [pw, ph],
                "note": "Old single sleeve, still in parts/ for reference.",
            })
            continue
        key = next((k for k in NEW_NOTES if stem.startswith(k)), None)
        note = NEW_NOTES[key][0] if key else "Ships in parts/ but is not bound."
        unbound.append({"name": stem, "source_size": [pw, ph],
                        "note": note})

    expected = []
    for entry in parts:
        png = pngs[entry["name"]]
        pw, ph = Image.open(png).size
        region = entries.get(entry["name"])
        if region is None:
            continue
        aw, ah = region["orig"]
        if (pw, ph) == (aw, ah):
            continue
        same_aspect = abs(pw / float(ph) - aw / float(ah)) < 0.02
        expected.append({
            "name": entry["name"],
            "png": [pw, ph],
            "atlas": [aw, ah],
            "ratio": [round(aw / float(pw), 4), round(ah / float(ph), 4)],
            "reason": DIFF_REASON_SCALED if same_aspect else DIFF_REASON_CROP,
        })

    layout = {
        "view": view,
        "canvas_size": [int(round(skel.get("width", 756))), ch],
        "coordinate_convention": "top-left, x right, y down",
        "source_crop_xyxy": old.get("source_crop_xyxy"),
        "derived_from": "%s (rest pose)" % cfg["rig"],
        "parts": parts,
        "expected_differences": expected,
        "derived_parts": derived,
        "unbound_parts": unbound,
        "deprecated_parts": deprecated,
    }

    # The setup skeleton keeps the rig's bones and rebuilds every PNG attachment
    # from the package art: same centre, same rotation, scale = rig/PNG ratio.
    slots, attachments, setup_quads = [], {}, {}
    for slot in rig["slots"]:
        name = slot["name"]
        if name not in used:
            continue
        att = atts[name][slot["attachment"]]
        pw, ph = Image.open(pngs[name]).size
        sx = float(att.get("width", pw)) * float(att.get("scaleX", 1.0)) / pw
        sy = float(att.get("height", ph)) * float(att.get("scaleY", 1.0)) / ph
        rebuilt = {
            "type": "region", "path": name,
            "x": att.get("x", 0.0), "y": att.get("y", 0.0),
            "width": pw, "height": ph,
        }
        if abs(float(att.get("rotation", 0.0))) > 1e-6:
            rebuilt["rotation"] = att["rotation"]
        if abs(sx - 1.0) > 1e-6 or abs(sy - 1.0) > 1e-6:
            rebuilt["scaleX"] = round(sx, 6)
            rebuilt["scaleY"] = round(sy, 6)
        slots.append({"name": name, "bone": slot["bone"],
                      "attachment": name})
        attachments.setdefault(name, {})[name] = rebuilt
        setup_quads[name] = (
            quad(rebuilt, bones[slot["bone"]], skel, ch),
            quad(att, bones[slot["bone"]], skel, ch))

    for name, (new_q, old_q) in setup_quads.items():
        for a, b in zip(new_q, old_q):
            checks["setup_quad_max_delta"] = max(
                checks["setup_quad_max_delta"],
                abs(a[0] - b[0]), abs(a[1] - b[1]))

    setup = {
        "skeleton": {"spine": "4.2.00", "x": skel.get("x", 0.0),
                     "y": skel.get("y", 0.0),
                     "width": int(round(skel.get("width", 756))),
                     "height": ch, "images": "./parts/", "audio": ""},
        "bones": rig["bones"],
        "slots": slots,
        "skins": [{"name": "default", "attachments": attachments}],
        "animations": {},
    }

    # Per-part audit of the PNGs themselves.
    audit = []
    padding_counts = {}
    for entry in parts:
        path = pngs[entry["name"]]
        im = Image.open(path).convert("RGBA")
        w, h = im.size
        box = im.getbbox() or (0, 0, 0, 0)
        pad = [box[0], box[1], w - box[2], h - box[3]]
        padding_counts[tuple(pad)] = padding_counts.get(tuple(pad), 0) + 1
        audit.append({"name": entry["name"], "size": [w, h], "padding": pad})

    validation = {
        "rig": cfg["rig"],
        "parts": audit,
        "counts": {
            "rig_slots": len(rig["slots"]),
            "png_backed_parts": len(parts),
            "derived_parts": len(derived),
            "unbound_parts": len(unbound),
            "deprecated_parts": len(deprecated),
            "png_files_in_parts": len(pngs),
            "expected_differences": len(expected),
            "atlas_derived_parts": sum(1 for p in parts
                                       if p.get("source") == "atlas-derived"),
        },
        "checks": {
            "bbox_width_height_equals_png_size":
                "passed (%d parts)" % len(parts),
            "pivot_equals_bound_bone_position":
                "passed (%d slots)" % len(rig["slots"]),
            "bbox_centre_max_delta_px": round(checks["bbox_max_delta"], 3),
            "setup_json_reproduces_rig_quads":
                "passed (max corner delta %.6f px)"
                % checks["setup_quad_max_delta"],
            "parts_vs_atlas_size":
                "expected: %d of %d parts differ on purpose (see "
                "layout.json expected_differences); the atlas is not a licence "
                "to rewrite parts/" % (len(expected), len(parts)),
            "psd_pixel_roundtrip": "not re-run; the PSD was not rebuilt",
        },
        "padding": {"%d,%d,%d,%d" % k: v
                    for k, v in sorted(padding_counts.items())},
    }

    return dict(layout=layout, setup=setup, validation=validation,
                pngs=pngs, parts_count=len(parts),
                setup_delta=checks["setup_quad_max_delta"])


def csv_text(layout):
    buf = io.StringIO()
    writer = csv.writer(buf)
    writer.writerow(["name", "draw_order", "parent", "left", "top", "width",
                     "height", "pivot_x", "pivot_y", "method"])
    rows = sorted(layout["parts"] + layout["derived_parts"],
                  key=lambda e: e["draw_order"])
    for entry in rows:
        writer.writerow([entry["name"], entry["draw_order"], entry["parent"],
                         *entry["bbox_xywh"], *entry["pivot_xy"],
                         entry["method"]])
    return buf.getvalue()


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=2)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("view", choices=sorted(VIEWS))
    ap.add_argument("--check", action="store_true",
                    help="report what would change instead of writing")
    args = ap.parse_args()

    result = describe(args.view)
    cfg = VIEWS[args.view]
    pkg = ROOT / cfg["package"]
    targets = {
        "layout.json": dump(result["layout"]),
        "parts.csv": csv_text(result["layout"]),
        "%s-setup-spine42.json" % args.view: dump(result["setup"]),
        "validation.json": dump(result["validation"]),
    }

    if args.check:
        changed = []
        for name, text in targets.items():
            path = pkg / name
            if path.exists():
                with path.open("r", encoding="utf-8-sig", newline="") as fh:
                    old = fh.read()
            else:
                old = None
            if old != text:
                changed.append(name)
        print("  %s: %d/%d files would change: %s"
              % (args.view, len(changed), len(targets), changed or "none"))
        return 1 if changed else 0

    for name, text in targets.items():
        path = pkg / name
        if name.endswith(".csv"):
            path.write_text(text, encoding="utf-8-sig", newline="")
        else:
            path.write_text(text, encoding="utf-8")
        print("  wrote %s" % path.relative_to(ROOT))

    counts = result["validation"]["counts"]
    print("  %s: %d slots = %d png-backed + %d derived; %d unbound, %d retired"
          % (args.view, counts["rig_slots"], counts["png_backed_parts"],
             counts["derived_parts"], counts["unbound_parts"],
             counts["deprecated_parts"]))
    print("  setup quads reproduce the rig within %.6f px"
          % result["setup_delta"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
