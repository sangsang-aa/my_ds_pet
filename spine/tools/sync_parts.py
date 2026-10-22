#!/usr/bin/env python3
"""Re-cut PNGs in a parts package straight from the view's atlas.

The atlas is what every render uses (preview, verify, assembled.png), so when a
part is re-exported at a new size the copy under parts/ can end up as an older
version of the same drawing.  This tool rewrites that copy from the atlas:

  * extract the region from the page (un-rotating if needed)
  * restore the untrimmed canvas if the region was packed with offsets
  * give it the package's 2 px transparent margin when the art leaves room for
    it; when the art fills the region, the region size is kept so the rig still
    draws it 1:1

The rig and every rendered image are untouched; only parts/<name>.png changes.

Usage:
  python3 spine/tools/sync_parts.py front --select differ,missing --skip collar
  python3 spine/tools/sync_parts.py front --select differ --check
"""
import argparse
import json
import pathlib
import sys

TOOLS = pathlib.Path(__file__).resolve().parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import render  # noqa: E402
from PIL import Image  # noqa: E402

VIEWS = {
    "front": dict(rig="spine/front/whale_girl", package="spine-parts-v2-20261020/front"),
    "side": dict(rig="spine/side/whale_girl_side", package="spine-parts-v2-20261020/side"),
    "back": dict(rig="spine/back/whale_girl_back", package="spine-parts-v2-20261020/back"),
}
PAD = 2


def region_image(entry):
    """The region's art, untrimmed, as RGBA."""
    img = render.region_image(entry)
    orig = tuple(entry.get("orig", img.size))
    off = tuple(entry.get("offset", (0, 0)))
    if orig != img.size:
        canvas = Image.new("RGBA", orig, (0, 0, 0, 0))
        canvas.paste(img, (max(0, -off[0]), max(0, -off[1])))
        img = canvas
    return img


def pack(img):
    """Apply the 2 px margin, or keep the region size if the art fills it."""
    box = img.getbbox()
    if box is None:
        raise SystemExit("region is fully transparent")
    art = img.crop(box)
    if art.width + 2 * PAD <= img.width and art.height + 2 * PAD <= img.height:
        out = Image.new("RGBA", (art.width + 2 * PAD, art.height + 2 * PAD),
                        (0, 0, 0, 0))
        out.paste(art, (PAD, PAD))
        return out, "2px-margin"
    return img, "region-size"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("view", choices=sorted(VIEWS))
    ap.add_argument("--select", default="differ,missing",
                    help="comma list of: differ, missing, all")
    ap.add_argument("--skip", default="",
                    help="comma list of region names to leave alone")
    ap.add_argument("--check", action="store_true",
                    help="report what would change instead of writing")
    args = ap.parse_args()

    cfg = VIEWS[args.view]
    base = ROOT / cfg["rig"]
    parts = ROOT / cfg["package"] / "parts"
    entries = render.load_atlas(str(base))
    rig = json.loads(base.with_suffix(".json").read_text(encoding="utf-8"))
    skip = {s.strip() for s in args.skip.split(",") if s.strip()}
    select = {s.strip() for s in args.select.split(",") if s.strip()}

    # Only regions the rig binds; the atlas may hold spares.
    wanted = [s["name"] for s in rig["slots"]
              if s.get("attachment") in rig["skins"][0]["attachments"].get(s["name"], {})]

    todo = []
    for name in wanted:
        if name in skip:
            continue
        entry = entries.get(name)
        if entry is None:
            print("  no atlas region for slot %s" % name)
            continue
        path = parts / (name + ".png")
        if not path.exists():
            why = "missing"
        elif Image.open(path).size != tuple(entry["orig"]):
            why = "differ"
        else:
            why = None
        if why and (why in select or "all" in select):
            todo.append((name, why, path, entry))

    if not todo:
        print("  %s: nothing to do" % args.view)
        return 0

    for name, why, path, entry in todo:
        img, strategy = pack(region_image(entry))
        old = str(Image.open(path).size) if path.exists() else "none"
        if args.check:
            print("  WOULD %-26s %-8s %-11s %s -> %dx%d"
                  % (name, why, strategy, old, img.width, img.height))
            continue
        img.save(path)
        saved = Image.open(path).convert("RGBA")
        box = saved.getbbox()
        pad = [box[0], box[1], saved.width - box[2], saved.height - box[3]]
        assert saved.size == img.size and min(pad) >= 1, (name, saved.size, pad)
        print("  %-26s %-8s %-11s %s -> %dx%d  margins %s"
              % (name, why, strategy, old, img.width, img.height, pad))

    return 0


if __name__ == "__main__":
    sys.exit(main())
