#!/usr/bin/env python3
"""Shared parsing for the animation tools (anim_diff.py, anim_lint.py).

Spine 4.2 timeline rules handled here:

  * the first key may omit ``time`` (it means 0);
  * a numeric channel is piecewise cubic; ``curve`` is either 4 numbers
    (one bezier) or 8 numbers (x curve then y curve, translate only);
  * ``curve: "stepped"`` is valid;
  * values are offsets/additive for translate/rotate/shear and multiplicative
    for scale, but both tools only compare the values inside one file pair, so
    the caller's defaults are enough.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import render  # noqa: E402

CHANNEL_FIELDS = {
    "translate": (("x", 0.0), ("y", 0.0)),
    "rotate": (("value", 0.0),),
    "scale": (("x", 1.0), ("y", 1.0)),
    "shear": (("x", 0.0), ("y", 0.0)),
}
CHANNEL_ORDER = ("translate", "rotate", "scale", "shear")
NON_NUMERIC = ("name", "color")


def time_of(key):
    """Key time, defaulting to 0 (Spine omits it on the first key)."""
    return float(key.get("time", 0.0))


def key_times(keys):
    return sorted(time_of(k) for k in keys if isinstance(k, dict))


def duration(anim):
    """Explicit duration when present, else the last key time."""
    if isinstance(anim.get("duration"), (int, float)):
        return float(anim["duration"])
    times = []
    for group in ("bones", "slots"):
        for channels in anim.get(group, {}).values():
            for keys in channels.values():
                if isinstance(keys, list):
                    times.extend(time_of(k) for k in keys if isinstance(k, dict))
    return max(times) if times else 0.0


def iter_channels(anim):
    """Yield (kind, item, channel, field, default, keys) for every timeline."""
    for bone, channels in sorted(anim.get("bones", {}).items()):
        for channel in CHANNEL_ORDER:
            if channel not in channels:
                continue
            for field, default in CHANNEL_FIELDS[channel]:
                yield ("bone", bone, channel, field, default, channels[channel])
    for slot, channels in sorted(anim.get("slots", {}).items()):
        if "attachment" in channels:
            yield ("slot", slot, "attachment", "name", None, channels["attachment"])
        if "color" in channels:
            yield ("slot", slot, "color", "color", None, channels["color"])


def sample(keys, time, field, default):
    """Sample a channel the way the Spine runtime does (bezier included)."""
    if not keys:
        return default
    if field in NON_NUMERIC:
        value = default
        for key in keys:
            if time_of(key) <= time:
                value = key.get(field, default)
            else:
                break
        return value
    return render.sample(keys, time, field, default, curves=True)


def curve_signature(keys):
    return [(round(time_of(k), 6),
             tuple(k["curve"]) if isinstance(k.get("curve"), list) else k.get("curve"))
            for k in keys]


def loop_seam(anim):
    """Largest |t=0 - t=duration| over numeric channels, and where it is."""
    total = duration(anim)
    if total <= 0:
        return {"gap": 0.0, "where": None}
    worst, where = 0.0, None
    for _, name, channel, field, default, keys in iter_channels(anim):
        if field in NON_NUMERIC or not keys:
            continue
        try:
            gap = abs(float(sample(keys, 0.0, field, default))
                      - float(sample(keys, total, field, default)))
        except (TypeError, ValueError):
            continue
        if gap > worst:
            worst, where = gap, "%s.%s.%s" % (name, channel, field)
    return {"gap": worst, "where": where}
