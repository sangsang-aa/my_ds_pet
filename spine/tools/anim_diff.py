#!/usr/bin/env python3
"""Semantic diff for animation data in a Spine 4.2 rig.

The rig JSON is a single-line file, so `git diff` shows one enormous line and
cannot tell which keyframe moved.  This tool compares the *meaning* of two rig
files: per clip, per bone, per channel, the largest value change and where it
happens.  It also reports the loop seam (t=0 vs t=duration) of every clip.

    python3 spine/tools/anim_diff.py OLD.json NEW.json
    python3 spine/tools/anim_diff.py OLD.json NEW.json --clip idle --json

Exit status: 0 = no semantic difference, 1 = differences, 2 = usage/read error.
Only `animations` is compared; rig geometry, atlas and parts are out of scope.
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import anim_common as ac  # noqa: E402


def diff_channel(kind, name, channel, field, default, old_keys, new_keys, tol):
    old_keys = old_keys or []
    new_keys = new_keys or []
    times = sorted(set(ac.key_times(old_keys)) | set(ac.key_times(new_keys)))
    if times:
        # Midpoints catch easing-only edits, which agree at every key time.
        samples = times + [(a + b) / 2.0 for a, b in zip(times, times[1:])]
    else:
        samples = [0.0]

    max_delta = 0.0
    at = None
    old_at = new_at = None
    numeric = field not in ac.NON_NUMERIC
    for t in samples:
        a = ac.sample(old_keys, t, field, default)
        b = ac.sample(new_keys, t, field, default)
        if numeric:
            try:
                d = abs(float(a) - float(b))
            except (TypeError, ValueError):
                d = 0.0 if a == b else float("inf")
        else:
            d = 0.0 if a == b else 1.0
        if d > max_delta:
            max_delta, at, old_at, new_at = d, t, a, b

    curve_changed = ac.curve_signature(old_keys) != ac.curve_signature(new_keys)
    changed = (max_delta > tol or curve_changed
               or len(old_keys) != len(new_keys)
               or ac.key_times(old_keys) != ac.key_times(new_keys))
    if not changed:
        return None
    return {
        "kind": kind,
        "name": name,
        "channel": channel,
        "field": field,
        "keys_old": len(old_keys),
        "keys_new": len(new_keys),
        "max_delta": None if max_delta == float("inf") else max_delta,
        "at": at,
        "old": old_at,
        "new": new_at,
        "curve_changed": curve_changed,
    }


def diff(old, new, clips=None, tol=1e-6, seam_tol=0.01):
    old_anims = old.get("animations", {}) or {}
    new_anims = new.get("animations", {}) or {}
    names = list(old_anims)
    names += [n for n in new_anims if n not in old_anims]
    if clips:
        unknown = [c for c in clips if c not in old_anims and c not in new_anims]
        if unknown:
            raise SystemExit("unknown clip(s): %s (have: %s)"
                             % (", ".join(unknown), ", ".join(names)))
        names = [n for n in names if n in clips]

    report = {"clips": {}, "changed_clips": [], "seam_warnings": [], "ok": True}
    for clip in names:
        if clip not in old_anims:
            report["clips"][clip] = {"added": True, "changed": True}
            report["changed_clips"].append(clip)
            report["ok"] = False
            continue
        if clip not in new_anims:
            report["clips"][clip] = {"removed": True, "changed": True}
            report["changed_clips"].append(clip)
            report["ok"] = False
            continue

        a, b = old_anims[clip], new_anims[clip]
        old_ch = {(k, n, c, f): (d, keys) for k, n, c, f, d, keys in ac.iter_channels(a)}
        new_ch = {(k, n, c, f): (d, keys) for k, n, c, f, d, keys in ac.iter_channels(b)}
        channels = []
        for key in sorted(set(old_ch) | set(new_ch)):
            kind, name, channel, field = key
            default = old_ch.get(key, new_ch.get(key))[0]
            entry = diff_channel(kind, name, channel, field, default,
                                 old_ch.get(key, (default, []))[1],
                                 new_ch.get(key, (default, []))[1], tol)
            if entry:
                channels.append(entry)

        da, db = ac.duration(a), ac.duration(b)
        duration_changed = abs(da - db) > tol
        seam = ac.loop_seam(b)
        changed = bool(channels) or duration_changed
        report["clips"][clip] = {
            "duration_old": da,
            "duration_new": db,
            "duration_changed": duration_changed,
            "channels": channels,
            "loop_seam": seam,
            "changed": changed,
        }
        if changed:
            report["changed_clips"].append(clip)
            report["ok"] = False
        if seam["gap"] > seam_tol:
            report["seam_warnings"].append((clip, seam))
    return report


def print_report(report, out=sys.stdout):
    for clip, info in report["clips"].items():
        if info.get("added"):
            print("%s: added" % clip, file=out)
            continue
        if info.get("removed"):
            print("%s: removed" % clip, file=out)
            continue
        head = "%s:" % clip
        if not info["changed"]:
            print("%-22s no change   (dur %.3fs, seam %.3f)"
                  % (head, info["duration_new"], info["loop_seam"]["gap"]), file=out)
            continue
        print("%-22s CHANGED    (dur %.3fs -> %.3fs)" % (head, info["duration_old"],
                                                          info["duration_new"]), file=out)
        for ch in info["channels"]:
            delta = "n/a" if ch["max_delta"] is None else "%.4g" % ch["max_delta"]
            where = "" if ch["at"] is None else " at t=%.3fs (%s -> %s)" % (
                ch["at"], ch["old"], ch["new"])
            curves = " [curve]" if ch["curve_changed"] else ""
            print("    %-12s.%-9s.%-4s keys %d->%d  max delta %s%s%s"
                  % (ch["name"], ch["channel"], ch["field"], ch["keys_old"],
                     ch["keys_new"], delta, curves, where), file=out)
        if info["loop_seam"]["gap"]:
            print("    loop seam %.4f at %s" % (info["loop_seam"]["gap"],
                                                info["loop_seam"]["where"]), file=out)
    if report["ok"]:
        print("no semantic differences", file=out)
    else:
        print("changed clips: %s" % ", ".join(report["changed_clips"]), file=out)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Semantic animation diff for Spine 4.2 rigs.")
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("--clip", action="append", default=[],
                    help="only compare this clip (repeatable)")
    ap.add_argument("--tolerance", type=float, default=1e-6,
                    help="value tolerance (default 1e-6)")
    ap.add_argument("--seam-tolerance", type=float, default=0.01,
                    help="loop-seam warning threshold (default 0.01)")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    args = ap.parse_args(argv)

    try:
        old = json.loads(pathlib.Path(args.old).read_text(encoding="utf-8"))
        new = json.loads(pathlib.Path(args.new).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2

    report = diff(old, new, clips=args.clip or None,
                  tol=args.tolerance, seam_tol=args.seam_tolerance)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print_report(report)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
