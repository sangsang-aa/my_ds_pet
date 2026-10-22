#!/usr/bin/env python3
"""Regenerate a view's verify/ set from its rig.

Each view gets:
  * still.png        the rest pose, straight RGBA at the rig's canvas size
  * sheet_<clip>.png a 2x2 contact sheet (t = 0 / 25 / 50 / 75 %) on a dark
                     background, labelled "<clip> t=<N>%"
  * <clip>_NNN.png   the 4 individual frames -- only for views whose verify/
                     directory already keeps them (currently just front)

Sample times are absolute seconds into the clip (0.0 / 0.25 / 0.5 / 0.75), which
is what the committed sets were built from; the frame names are percentages but
were never scaled by clip duration.  Keeping that convention means regenerating
a view leaves its images comparable with the committed baseline.

Usage:
  python3 spine/tools/make_verify.py front            # rewrite the set
  python3 spine/tools/make_verify.py side --check     # compare, write nothing
"""
import argparse
import io
import pathlib
import sys

TOOLS = pathlib.Path(__file__).resolve().parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import render  # noqa: E402
from PIL import Image, ImageChops, ImageDraw, ImageFont  # noqa: E402

VIEWS = {
    # view -> rig base, sheet size (px), whether the frames are kept on disk
    "front": dict(base="spine/front/whale_girl", sheet=(1512, 2016), frames=True),
    "side": dict(base="spine/side/whale_girl_side", sheet=(700, 1070), frames=False),
    "back": dict(base="spine/back/whale_girl_back", sheet=(700, 1094), frames=False),
}
CLIPS = ["idle", "walk", "wave", "shy"]
FRACS = [0.0, 0.25, 0.5, 0.75]
BG = (24, 24, 32)
LABEL_FILL = (208, 208, 214)
FONT_CANDIDATES = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
]


def label_font(size=20):
    for path in FONT_CANDIDATES:
        if pathlib.Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def build(view):
    """Return {relative path: PNG bytes} for the whole verify set."""
    cfg = VIEWS[view]
    base = ROOT / cfg["base"]
    if not base.with_suffix(".json").exists():
        raise SystemExit(f"missing rig: {base}.json")
    out = base.parent / "verify"
    sw, sh = cfg["sheet"]
    tw, th = sw // 2, sh // 2

    files = {}

    # rest pose
    buf = io.BytesIO()
    render.render(str(base) + ".json", str(base), None).save(buf, format="PNG")
    files["still.png"] = buf.getvalue()

    font = label_font()
    for clip in CLIPS:
        frames = []
        for frac in FRACS:
            pct = int(frac * 100)
            img = render.render(str(base) + ".json", str(base), None,
                                clip if frac else None, frac)
            frames.append((pct, img))
            if cfg["frames"]:
                buf = io.BytesIO()
                img.save(buf, format="PNG")
                files["%s_%03d.png" % (clip, pct)] = buf.getvalue()

        sheet = Image.new("RGB", (sw, sh), BG)
        draw = ImageDraw.Draw(sheet)
        for i, (pct, img) in enumerate(frames):
            cell = Image.new("RGBA", (tw, th), BG + (255,))
            tile = img if img.size == (tw, th) else img.resize((tw, th), Image.LANCZOS)
            cell.alpha_composite(tile)
            x, y = (i % 2) * tw, (i // 2) * th
            sheet.paste(cell.convert("RGB"), (x, y))
            draw.text((x + 10, y + 8), "%s t=%d%%" % (clip, pct),
                      font=font, fill=LABEL_FILL)
        buf = io.BytesIO()
        sheet.save(buf, format="PNG")
        files["sheet_%s.png" % clip] = buf.getvalue()

    return files


def check(view, files):
    out = ROOT / VIEWS[view]["base"]
    out = out.parent / "verify"
    bad = []
    for name, want in sorted(files.items()):
        path = out / name
        if not path.exists():
            bad.append((name, "missing"))
            continue
        have = Image.open(path).convert("RGBA")
        new = Image.open(io.BytesIO(want)).convert("RGBA")
        if have.size != new.size:
            bad.append((name, "size %s != %s" % (have.size, new.size)))
            continue
        diff = sum(1 for v in ImageChops.difference(have, new)
                   .convert("L").getdata() if v > 0)
        if diff:
            bad.append((name, "%d pixels differ" % diff))
    extra = sorted(p.name for p in out.glob("*.png") if p.name not in files)
    return bad, extra


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("view", choices=sorted(VIEWS))
    ap.add_argument("--check", action="store_true",
                    help="compare against the files on disk instead of writing")
    args = ap.parse_args()

    files = build(args.view)
    out = (ROOT / VIEWS[args.view]["base"]).parent / "verify"

    if args.check:
        bad, extra = check(args.view, files)
        for name, why in bad:
            print("  DIFF %-20s %s" % (name, why))
        for name in extra:
            print("  EXTRA %-19s not produced by this tool" % name)
        print("  %s: %d files, %d differing, %d extra"
              % (args.view, len(files), len(bad), len(extra)))
        return 1 if bad or extra else 0

    out.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (out / name).write_bytes(data)
    print("  %s: wrote %d files to %s/" % (args.view, len(files),
                                           out.relative_to(ROOT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
