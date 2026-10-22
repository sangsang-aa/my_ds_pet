#!/usr/bin/env python3
"""Checks for anim_diff.py.  No pytest needed:

    python3 spine/tools/test_anim_diff.py
"""
import copy
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import anim_diff  # noqa: E402


def rig(animations):
    return {
        "skeleton": {"spine": "4.2.0", "x": 0, "y": 0, "width": 100, "height": 100},
        "bones": [{"name": "root"}, {"name": "body", "parent": "root"}],
        "slots": [],
        "skins": [{"name": "default", "attachments": {}}],
        "animations": animations,
    }


def idle(keys, duration=1.0):
    return {"duration": duration, "bones": {"body": {"translate": keys}}}


BASE = rig({"idle": idle([{"time": 0, "y": 0}, {"time": 1, "y": 10}])})


def checked(name, got, want):
    return (name, got == want, "got=%r want=%r" % (got, want))


def main():
    checks = []

    same = anim_diff.diff(BASE, copy.deepcopy(BASE))
    checks.append(checked("identical rigs are equal", (same["ok"], same["changed_clips"]),
                          (True, [])))

    changed = copy.deepcopy(BASE)
    changed["animations"]["idle"]["bones"]["body"]["translate"][1]["y"] = 12.5
    rep = anim_diff.diff(BASE, changed)
    ch = rep["clips"]["idle"]["channels"][0]
    checks.append(checked("value change: clip flagged, delta and time reported",
                          (rep["ok"], rep["changed_clips"], ch["field"],
                           round(ch["max_delta"], 6), ch["at"]),
                          (False, ["idle"], "y", 2.5, 1.0)))

    changed = copy.deepcopy(BASE)
    changed["animations"]["idle"]["bones"]["body"]["translate"][0]["curve"] = [0.9, 0, 0.1, 1]
    rep = anim_diff.diff(BASE, changed)
    ch = rep["clips"]["idle"]["channels"][0]
    checks.append(checked("easing-only change is detected",
                          (rep["ok"], ch["curve_changed"]), (False, True)))

    # Spine omits "time" for the first key; that must not count as a change.
    no_time = copy.deepcopy(BASE)
    del no_time["animations"]["idle"]["bones"]["body"]["translate"][0]["time"]
    rep = anim_diff.diff(BASE, no_time)
    checks.append(checked("missing first-key time means t=0",
                          (rep["ok"], rep["changed_clips"]), (True, [])))

    added = copy.deepcopy(BASE)
    added["animations"]["wave"] = idle([{"time": 0, "y": 0}], duration=0.5)
    rep = anim_diff.diff(BASE, added)
    checks.append(checked("added clip is reported",
                          (rep["ok"], rep["changed_clips"]), (False, ["wave"])))

    removed = copy.deepcopy(added)
    del removed["animations"]["wave"]
    rep = anim_diff.diff(added, removed)
    checks.append(checked("removed clip is reported",
                          (rep["ok"], rep["changed_clips"]), (False, ["wave"])))

    edited = copy.deepcopy(added)
    edited["animations"]["idle"]["bones"]["body"]["translate"][1]["y"] = 11.0
    rep = anim_diff.diff(added, edited)
    checks.append(checked("only the edited clip is listed",
                          (rep["changed_clips"], rep["clips"]["wave"]["changed"]),
                          (["idle"], False)))

    rep = anim_diff.diff(BASE, BASE, clips=["idle"])
    checks.append(checked("--clip filters the comparison",
                          (rep["ok"], list(rep["clips"])), (True, ["idle"])))

    open_loop = rig({"idle": idle([{"time": 0, "y": 0}, {"time": 1, "y": 4}])})
    rep = anim_diff.diff(open_loop, open_loop)
    checks.append(checked("open loop produces a seam warning",
                          (rep["ok"], round(rep["clips"]["idle"]["loop_seam"]["gap"], 6)),
                          (True, 4.0)))

    duration = copy.deepcopy(BASE)
    duration["animations"]["idle"]["duration"] = 2.0
    rep = anim_diff.diff(BASE, duration)
    checks.append(checked("duration change is a difference",
                          (rep["ok"], rep["clips"]["idle"]["duration_changed"]),
                          (False, True)))

    width = max(len(c[0]) for c in checks)
    failed = 0
    for name, ok, detail in checks:
        print("  %-*s %s  %s" % (width, name, "PASS" if ok else "FAIL", detail))
        failed += 0 if ok else 1
    print("%d/%d passed" % (len(checks) - failed, len(checks)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
