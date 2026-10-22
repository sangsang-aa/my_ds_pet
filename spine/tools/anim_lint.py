#!/usr/bin/env python3
"""Structure and keyframe lint for a Spine 4.2 rig.

Checks things that render.py would otherwise turn into a crash, a silent skip
or a wrong pose:

  * bone names unique, parents defined before children, finite transforms;
  * slot bones and attachments exist;
  * animation bones/slots exist, key times increase, values are finite,
    curves are 4/8 numbers (8 only for translate) or "stepped";
  * keys after the first must carry "time";
  * duration is not shorter than the last key;
  * optional loop-seam check for the clips named with --loop.

    python3 spine/tools/anim_lint.py spine/front/whale_girl.json
    python3 spine/tools/anim_lint.py spine/front/whale_girl.json --loop idle,walk

Exit status: 0 = no errors (warnings allowed), 1 = errors, 2 = usage/read error.
"""
import argparse
import json
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import anim_common as ac  # noqa: E402

BONE_CHANNELS = set(ac.CHANNEL_ORDER) | {"inherit"}
SLOT_CHANNELS = {"attachment", "color", "twoColor", "drawOrder"}


def _finite(value):
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def lint(data, loop_clips=(), seam_tol=0.01):
    errors, warnings, infos = [], [], []
    stats = {"bones": 0, "slots": 0, "skins": 0, "clips": 0, "keys": 0}

    def err(code, where, message):
        errors.append({"code": code, "where": where, "message": message})

    def warn(code, where, message):
        warnings.append({"code": code, "where": where, "message": message})

    version = (data.get("skeleton") or {}).get("spine")
    if not version:
        warn("skeleton-version-missing", "skeleton.spine",
             "no skeleton.spine version; expected 4.2")
    elif not str(version).startswith("4.2"):
        warn("skeleton-version", "skeleton.spine",
             "version is %r; this pipeline expects 4.2" % version)

    # ---- bones ---------------------------------------------------------
    bones = data.get("bones") or []
    stats["bones"] = len(bones)
    seen = {}
    for i, bone in enumerate(bones):
        name = bone.get("name")
        if not name:
            err("bone-name-missing", "bones[%d]" % i, "bone has no name")
            continue
        if name in seen:
            err("bone-duplicate", "bones[%d]" % i, "duplicate bone %r" % name)
        seen[name] = i
        parent = bone.get("parent")
        if parent is not None:
            if parent not in seen:
                err("bone-parent", "bones[%d] %s" % (i, name),
                    "parent %r is missing or defined later" % parent)
        for field in ("x", "y", "rotation", "scaleX", "scaleY",
                      "shearX", "shearY", "length"):
            if field in bone and not _finite(bone[field]):
                err("bone-value", "bones[%d] %s.%s" % (i, name, field),
                    "not a finite number: %r" % (bone[field],))

    # ---- slots / skins -------------------------------------------------
    slots = data.get("slots") or []
    stats["slots"] = len(slots)
    slot_names = {}
    for i, slot in enumerate(slots):
        name = slot.get("name")
        if not name:
            err("slot-name-missing", "slots[%d]" % i, "slot has no name")
            continue
        if name in slot_names:
            err("slot-duplicate", "slots[%d]" % i, "duplicate slot %r" % name)
        slot_names[name] = i
        bone = slot.get("bone")
        if bone and bone not in seen:
            err("slot-bone", "slots[%d] %s" % (i, name),
                "bone %r does not exist" % bone)

    skins = data.get("skins") or []
    stats["skins"] = len(skins)

    def attachment_exists(slot, attachment):
        for skin in skins:
            group = (skin.get("attachments") or {}).get(slot)
            if isinstance(group, dict) and attachment in group:
                return True
        return False

    for i, slot in enumerate(slots):
        name, attachment = slot.get("name"), slot.get("attachment")
        if name and attachment and not attachment_exists(name, attachment):
            err("slot-attachment", "slots[%d] %s" % (i, name),
                "attachment %r is not in any skin" % attachment)

    # ---- animations ----------------------------------------------------
    animations = data.get("animations") or {}
    stats["clips"] = len(animations)
    for clip_name, anim in animations.items():
        where = "animations.%s" % clip_name
        if not isinstance(anim, dict):
            err("clip-type", where, "animation is not an object")
            continue
        if "duration" in anim and (not _finite(anim["duration"]) or anim["duration"] < 0):
            err("clip-duration", where, "invalid duration %r" % (anim["duration"],))

        for group, known in (("bones", BONE_CHANNELS), ("slots", SLOT_CHANNELS)):
            for item, channels in (anim.get(group) or {}).items():
                item_where = "%s.%s" % (where, item)
                if group == "bones" and item not in seen:
                    err("clip-bone", item_where, "bone does not exist")
                if group == "slots" and item not in slot_names:
                    err("clip-slot", item_where, "slot does not exist")
                for channel, keys in (channels or {}).items():
                    cell = "%s.%s" % (item_where, channel)
                    if channel not in known:
                        warn("clip-channel", cell, "unknown channel %r" % channel)
                        continue
                    if not isinstance(keys, list) or not keys:
                        err("clip-keys", cell, "timeline is empty or not a list")
                        continue
                    stats["keys"] += len(keys)
                    _lint_keys(err, warn, cell, channel, keys, anim, group, item,
                               attachment_exists)

        total = ac.duration(anim)
        if total <= 0:
            warn("clip-empty", where, "clip has no keys with a duration")
        seam = ac.loop_seam(anim)
        infos.append({"where": where, "code": "loop-seam",
                      "message": "t=0 vs t=%.3fs: %.4f%s"
                                  % (total, seam["gap"],
                                     " at %s" % seam["where"] if seam["where"] else "")})
        if clip_name in loop_clips and seam["gap"] > seam_tol:
            err("loop-seam", where,
                "loop gap %.4f > %.4f at %s" % (seam["gap"], seam_tol, seam["where"]))

    return {"errors": errors, "warnings": warnings, "infos": infos, "stats": stats}


def _lint_keys(err, warn, where, channel, keys, anim, group, item,
               attachment_exists):
    times = []
    for i, key in enumerate(keys):
        if not isinstance(key, dict):
            err("key-type", "%s[%d]" % (where, i), "key is not an object")
            continue
        if "time" not in key:
            if i == 0:
                times.append(0.0)          # Spine's implicit first key
            else:
                warn("key-time-missing", "%s[%d]" % (where, i),
                     "key after the first has no time; treated as 0")
                times.append(0.0)
        else:
            if not _finite(key["time"]) or key["time"] < 0:
                err("key-time", "%s[%d]" % (where, i),
                    "invalid time %r" % (key["time"],))
                times.append(0.0)
            else:
                times.append(float(key["time"]))

        if group == "bones" and channel in ac.CHANNEL_FIELDS:
            for field, _default in ac.CHANNEL_FIELDS[channel]:
                if field in key and not _finite(key[field]):
                    err("key-value", "%s[%d].%s" % (where, i, field),
                        "not a finite number: %r" % (key[field],))
        if group == "slots" and channel == "attachment":
            name = key.get("name")
            if name is not None and item and not attachment_exists(item, name):
                warn("key-attachment", "%s[%d]" % (where, i),
                     "attachment %r is not in any skin for slot %r" % (name, item))

        curve = key.get("curve")
        if curve is None or curve == "stepped":
            pass
        elif not isinstance(curve, list):
            err("key-curve", "%s[%d]" % (where, i),
                "curve must be a list or \"stepped\"")
        else:
            if len(curve) not in (4, 8):
                err("key-curve", "%s[%d]" % (where, i),
                    "curve has %d values; expected 4 or 8" % len(curve))
            elif len(curve) == 8 and channel != "translate":
                err("key-curve", "%s[%d]" % (where, i),
                    "8-value curves are only valid for translate")
            if not all(_finite(v) for v in curve):
                err("key-curve", "%s[%d]" % (where, i),
                    "curve contains a non-finite value")

    if len(times) != len(keys):
        return
    for a, b in zip(times, times[1:]):
        if b < a - 1e-9:
            err("key-order", where, "key time %.4f follows %.4f" % (b, a))
            break
    for a, b in zip(times, times[1:]):
        if abs(b - a) <= 1e-9:
            warn("key-duplicate", where, "duplicate key time %.4f" % a)
            break
    if times and abs(times[0]) > 1e-9:
        warn("key-start", where,
             "first key is at %.4fs, not 0; the setup pose is held before it"
             % times[0])
    if len(keys) == 1:
        warn("key-single", where, "timeline has a single key; it never moves")
    total = ac.duration(anim)
    if total and times and times[-1] > total + 1e-9:
        err("key-duration", where,
            "last key %.4fs is after duration %.4fs" % (times[-1], total))


def print_report(path, result, out=sys.stdout):
    s = result["stats"]
    print("%s: %d bones, %d slots, %d skins, %d clips, %d keys"
          % (path, s["bones"], s["slots"], s["skins"], s["clips"], s["keys"]), file=out)
    for entry in result["errors"]:
        print("  ERROR %-18s %s  %s" % (entry["code"], entry["where"],
                                        entry["message"]), file=out)
    for entry in result["warnings"]:
        print("  WARN  %-18s %s  %s" % (entry["code"], entry["where"],
                                        entry["message"]), file=out)
    for entry in result["infos"]:
        print("  INFO  %-18s %s  %s" % (entry["code"], entry["where"],
                                        entry["message"]), file=out)
    print("%d error(s), %d warning(s)" % (len(result["errors"]),
                                          len(result["warnings"])), file=out)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Lint a Spine 4.2 rig.")
    ap.add_argument("rig")
    ap.add_argument("--loop", default="",
                    help="comma-separated clips that must close (e.g. idle,walk)")
    ap.add_argument("--seam-tolerance", type=float, default=0.01)
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    args = ap.parse_args(argv)

    try:
        data = json.loads(pathlib.Path(args.rig).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2

    loops = [c.strip() for c in args.loop.split(",") if c.strip()]
    unknown = [c for c in loops if c not in (data.get("animations") or {})]
    if unknown:
        print("error: unknown loop clip(s): %s" % ", ".join(unknown), file=sys.stderr)
        return 2

    result = lint(data, loops, args.seam_tolerance)
    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print_report(args.rig, result)
    return 1 if result["errors"] else 0


if __name__ == "__main__":
    sys.exit(main())
