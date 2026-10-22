#!/usr/bin/env python3
"""Checks for render.py's quad rules. No pytest needed:

    python3 spine/tools/test_render.py

The rig relies on two facts that are easy to break:

  * when an attachment's width/height equal the region size, the region is drawn
    1:1 (every existing slot is in this state, so this is the no-op guarantee);
  * when they differ, the region is stretched across the attachment quad.  Spine
    does exactly that at runtime -- confirmed against the official web player
    with a 20x20 region and an 80x20 attachment, which renders 4:1.

The scene is deterministic: one opaque red square on a 200x200 canvas, so the
alpha bounding box is the quad and nothing else.  Centres sit half a pixel below
and right of the bone because the renderer keeps the half-pixel nudge that the
committed verify images were produced with; sizes are exact.
"""
import contextlib
import io
import json
import pathlib
import sys
import tempfile

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import render  # noqa: E402

CANVAS = 200
BONE_AT = (100, 100)


def build(tmp, att_w, att_h, att_x=0.0, att_y=0.0, region=(20, 20),
          rotation=0.0):
    tmp = pathlib.Path(tmp)
    rw, rh = region
    Image.new("RGBA", (rw, rh), (255, 0, 0, 255)).save(tmp / "page.png")
    (tmp / "page.atlas").write_text(
        "page.png\nsize: %d,%d\nformat: RGBA8888\nfilter: Linear,Linear\n"
        "repeat: none\nsq\n  rotate: false\n  xy: 0, 0\n  size: %d, %d\n"
        "  orig: %d, %d\n  offset: 0, 0\n  index: -1\n"
        % (rw, rh, rw, rh, rw, rh), encoding="utf-8")
    rig = {
        "skeleton": {"spine": "4.2.00", "x": 0, "y": 0,
                     "width": CANVAS, "height": CANVAS,
                     "images": "./", "audio": ""},
        "bones": [{"name": "root"},
                  {"name": "b", "parent": "root",
                   "x": BONE_AT[0], "y": BONE_AT[1]}],
        "slots": [{"name": "sq", "bone": "b", "attachment": "sq"}],
        "skins": [{"name": "default", "attachments": {"sq": {"sq": {
            "type": "region", "path": "sq", "x": att_x, "y": att_y,
            "width": att_w, "height": att_h, "rotation": rotation}}}}],
        "animations": {},
    }
    (tmp / "rig.json").write_text(json.dumps(rig), encoding="utf-8")
    return str(tmp / "rig.json"), str(tmp / "page")


def size_and_centre(img):
    box = img.getbbox()
    assert box, "nothing was drawn"
    return ((box[2] - box[0], box[3] - box[1]),
            ((box[0] + box[2]) / 2.0, (box[1] + box[3]) / 2.0))


def close(a, b, tol=0.75):
    """Centres may sit up to 0.5 px off the bone (the historical nudge)."""
    return abs(a - b) <= tol


def main():
    checks = []

    with tempfile.TemporaryDirectory() as tmp:
        rig, base = build(tmp, 20, 20)
        (w, h), c = size_and_centre(render.render(rig, base))
        checks.append(("region size == attachment size draws 1:1",
                       (w, h) == (20, 20) and close(c[0], 100) and close(c[1], 100),
                       "size=%s centre=%s" % ((w, h), c)))

    with tempfile.TemporaryDirectory() as tmp:
        rig, base = build(tmp, 80, 20)
        (w, h), c = size_and_centre(render.render(rig, base))
        checks.append(("attachment wider than region stretches it",
                       (w, h) == (80, 20) and close(c[0], 100) and close(c[1], 100),
                       "size=%s centre=%s" % ((w, h), c)))

    with tempfile.TemporaryDirectory() as tmp:
        rig, base = build(tmp, 80, 40)
        (w, h), c = size_and_centre(render.render(rig, base))
        checks.append(("both axes scale independently",
                       (w, h) == (80, 40) and close(c[0], 100) and close(c[1], 100),
                       "size=%s centre=%s" % ((w, h), c)))

    with tempfile.TemporaryDirectory() as tmp:
        rig, base = build(tmp, 20, 20, att_x=10, att_y=-5)
        (w, h), c = size_and_centre(render.render(rig, base))
        checks.append(("x/y move the quad's centre",
                       (w, h) == (20, 20) and close(c[0], 110) and close(c[1], 105),
                       "size=%s centre=%s" % ((w, h), c)))

    with tempfile.TemporaryDirectory() as tmp:
        rig, base = build(tmp, 80, 20)
        (w, h), c = size_and_centre(
            render.render(rig, base, resample=Image.BILINEAR))
        checks.append(("resample is honoured (bilinear, within 2 px)",
                       abs(w - 80) <= 2 and abs(h - 20) <= 2 and close(c[0], 100, 1),
                       "size=%s centre=%s" % ((w, h), c)))

    # lookup must be strict: no borrowing a same-named attachment from another
    # group, and a miss has to warn instead of disappearing.
    slot = {"name": "sq", "bone": "b", "attachment": "sq"}
    atts = {"sq": {"other": {"type": "region", "width": 1, "height": 1}},
            "b": {"sq": {"type": "region", "width": 1, "height": 1}}}
    err = io.StringIO()
    with contextlib.redirect_stderr(err):
        got = render.find_attachment(atts, slot)
    ok = got is None and "warning" in err.getvalue()
    direct = render.find_attachment({"sq": {"sq": {"type": "region"}}}, slot)
    checks.append(("missing attachment returns None and warns (no fallback)",
                   ok and direct is not None,
                   "returned=%r warning=%r direct=%r"
                   % (got, err.getvalue().strip(), direct)))

    with tempfile.TemporaryDirectory() as tmp:
        rig, base = build(tmp, 20, 10, region=(20, 10), rotation=90)
        (w, h), c = size_and_centre(render.render(rig, base))
        checks.append(("rotation 90 swaps the drawn bbox",
                       (w, h) == (10, 20) and close(c[0], 100) and close(c[1], 100),
                       "size=%s centre=%s" % ((w, h), c)))

    width = max(len(c[0]) for c in checks)
    failed = 0
    for name, ok, detail in checks:
        print("  %-*s %s  %s" % (width, name, "PASS" if ok else "FAIL", detail))
        failed += 0 if ok else 1
    print("%d/%d passed" % (len(checks) - failed, len(checks)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
