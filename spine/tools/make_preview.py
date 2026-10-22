#!/usr/bin/env python3
"""Rebuild a view's preview.html as one fully offline page.

The rig JSON, the atlas text and the atlas page image are embedded as base64
data URIs, and so are the official Spine Web Player bundle (JS + CSS).  The
result opens straight from disk with no network access.

The player bundle is third-party code, so it is not committed: it is cached in
spine/tools/.cache/ and downloaded on first use from the npm mirror
(registry.npmmirror.com).  Every later run works offline.

Usage:
  python3 spine/tools/make_preview.py front
  python3 spine/tools/make_preview.py all
"""
import argparse
import base64
import pathlib
import sys
import tarfile
import tempfile
import urllib.request

TOOLS = pathlib.Path(__file__).resolve().parent
ROOT = TOOLS.parents[1]

VIEWS = {
    "front": "spine/front/whale_girl",
    "side": "spine/side/whale_girl_side",
    "back": "spine/back/whale_girl_back",
}

# The previews were built with this bundle; pin it so rebuilds are reproducible.
PLAYER_VERSION = "4.2.120"
PLAYER_URL = ("https://registry.npmmirror.com/@esotericsoftware/spine-player/-/"
              "spine-player-%s.tgz" % PLAYER_VERSION)
PLAYER_MEMBERS = {
    "package/dist/iife/spine-player.js": "spine-player.js",
    "package/dist/spine-player.css": "spine-player.css",
}

HTML = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Spine Animation Preview</title>

<!-- Official Spine Web Player, inlined so the page works offline -->
<script src="{player_js}"></script>
<link rel="stylesheet" href="{player_css}">

<style>
  * {{ margin: 0; padding: 0; box-sizing: border-box; }}
  body {{
    background: #0f0f1a;
    color: #e0e0e0;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    min-height: 100vh;
    padding: 20px;
  }}
  h1 {{
    font-size: 1.5em;
    margin-bottom: 10px;
    color: #a8b2d1;
    letter-spacing: 0.04em;
  }}
  #status {{
    font-size: 0.85em;
    margin-bottom: 15px;
    color: #6b7da0;
    transition: color 0.3s;
  }}
  #player-container {{
    width: 700px;
    height: 600px;
    max-width: 95vw;
    border-radius: 12px;
    overflow: hidden;
    box-shadow: 0 8px 32px rgba(0,0,0,0.5);
  }}
  .info {{
    margin-top: 15px;
    font-size: 0.8em;
    color: #4a5568;
    text-align: center;
    max-width: 600px;
    line-height: 1.5;
  }}
  .info a {{ color: #6b8aad; text-decoration: none; }}
  .info a:hover {{ text-decoration: underline; }}
</style>
</head>
<body>

<h1>Spine Animation Preview</h1>
<div id="status">Loading Spine Player...</div>
<div id="player-container"></div>

<div class="info">
  Rendered with the official
  <a href="https://en.esotericsoftware.com/spine-player" target="_blank">Spine Web Player</a>.
  Use the controls to switch animations, adjust speed, and toggle debug views.
</div>

<script>
  new spine.SpinePlayer("player-container", {{
      skeleton: "{skeleton}",
      atlas: "{atlas}",
      animation: "{animation}",
      backgroundColor: "#1a1a2eff",
      showControls: true,
      premultipliedAlpha: false,
      rawDataURIs: {{
{raw_data}
      }},
      success: function(player) {{
        document.getElementById('status').textContent = 'Loaded successfully';
        document.getElementById('status').style.color = '#4ade80';
        var anims = player.skeleton.data.animations.map(function(a) {{ return a.name; }});
        console.log('Available animations:', anims);
        var skins = player.skeleton.data.skins.map(function(s) {{ return s.name; }});
        console.log('Available skins:', skins);
      }},
      error: function(player, reason) {{
        document.getElementById('status').textContent = 'Error: ' + reason;
        document.getElementById('status').style.color = '#ef4444';
        console.error('Spine Player error:', reason);
      }}
  }});
</script>

</body>
</html>
"""

MIME = {".json": "application/json", ".atlas": "application/octet-stream",
        ".png": "image/png"}


def data_uri(path):
    mime = MIME.get(path.suffix.lower())
    if mime is None:
        raise SystemExit("unsupported asset: %s" % path)
    blob = base64.b64encode(path.read_bytes()).decode("ascii")
    return "data:%s;base64,%s" % (mime, blob)


def player_bundle():
    """Return (js_data_uri, css_data_uri), downloading the bundle if needed."""
    cache = TOOLS / ".cache" / ("spine-player-%s" % PLAYER_VERSION)
    js = cache / "spine-player.js"
    css = cache / "spine-player.css"
    if not (js.exists() and css.exists()):
        print("  downloading Spine Web Player %s ..." % PLAYER_VERSION)
        cache.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory() as tmp:
            tgz = pathlib.Path(tmp) / "player.tgz"
            with urllib.request.urlopen(PLAYER_URL, timeout=120) as resp:
                tgz.write_bytes(resp.read())
            with tarfile.open(tgz) as tar:
                for member, name in PLAYER_MEMBERS.items():
                    src = tar.extractfile(member)
                    if src is None:
                        raise SystemExit("bundle member missing: %s" % member)
                    (cache / name).write_bytes(src.read())
    js_uri = "data:text/javascript;base64," + base64.b64encode(js.read_bytes()).decode("ascii")
    css_uri = "data:text/css;base64," + base64.b64encode(css.read_bytes()).decode("ascii")
    return js_uri, css_uri


def build(view, animation="idle"):
    base = ROOT / VIEWS[view]
    assets = [base.with_suffix(".json"), base.with_suffix(".atlas"),
              base.with_suffix(".png")]
    for path in assets:
        if not path.exists():
            raise SystemExit("missing asset: %s" % path)

    raw = ",\n".join(
        '        "%s": "%s"' % (p.name, data_uri(p)) for p in assets)
    js_uri, css_uri = player_bundle()
    html = HTML.format(
        player_js=js_uri, player_css=css_uri,
        skeleton=assets[0].name, atlas=assets[1].name,
        animation=animation, raw_data=raw)

    out = base.parent / "preview.html"
    out.write_text(html, encoding="utf-8")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("view", choices=sorted(VIEWS) + ["all"])
    ap.add_argument("--animation", default="idle")
    args = ap.parse_args()

    views = sorted(VIEWS) if args.view == "all" else [args.view]
    for view in views:
        out = build(view, args.animation)
        print("  %-14s %.2f MB (offline)"
              % (out.relative_to(ROOT), out.stat().st_size / 1e6))
    return 0


if __name__ == "__main__":
    sys.exit(main())
