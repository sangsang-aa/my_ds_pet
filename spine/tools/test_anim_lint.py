#!/usr/bin/env python3
"""Checks for anim_lint.py.  No pytest needed:

    python3 spine/tools/test_anim_lint.py
"""
import copy
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import anim_lint  # noqa: E402


def rig(animations=None, bones=None, slots=None):
    return {
        "skeleton": {"spine": "4.2.0", "x": 0, "y": 0, "width": 100, "height": 100},
        "bones": bones if bones is not None else [
            {"name": "root"},
            {"name": "body", "parent": "root"},
        ],
        "slots": slots if slots is not None else [
            {"name": "sq", "bone": "body", "attachment": "sq"},
        ],
        "skins": [{"name": "default", "attachments": {"sq": {"sq": {
            "type": "region", "width": 10, "height": 10}}}}],
        "animations": animations if animations is not None else {
            "idle": {"bones": {"body": {"translate": [
                {"time": 0, "y": 0}, {"time": 1, "y": 10}]}}},
        },
    }


def codes(result, level):
    return {entry["code"] for entry in result[level]}


def main():
    checks = []

    def check(name, ok, detail=""):
        checks.append((name, bool(ok), detail))

    base = rig()
    result = anim_lint.lint(base)
    check("clean rig has no errors or warnings",
          not result["errors"] and not result["warnings"],
          "errors=%s warnings=%s" % (codes(result, "errors"), codes(result, "warnings")))

    duplicate = rig(bones=[{"name": "root"}, {"name": "root"}])
    result = anim_lint.lint(duplicate)
    check("duplicate bone name is an error", "bone-duplicate" in codes(result, "errors"))

    missing_parent = rig(bones=[{"name": "root", "parent": "nope"}])
    result = anim_lint.lint(missing_parent)
    check("missing parent is an error", "bone-parent" in codes(result, "errors"))

    late_parent = rig(bones=[{"name": "body", "parent": "root"}, {"name": "root"}])
    result = anim_lint.lint(late_parent)
    check("parent defined later is an error", "bone-parent" in codes(result, "errors"))

    missing_att = rig(slots=[{"name": "sq", "bone": "body", "attachment": "nope"}])
    result = anim_lint.lint(missing_att)
    check("missing slot attachment is an error",
          "slot-attachment" in codes(result, "errors"))

    bad_bone = rig(animations={"idle": {"bones": {"ghost": {"rotate": [
        {"time": 0, "value": 0}]}}}})
    result = anim_lint.lint(bad_bone)
    check("animation on an unknown bone is an error",
          "clip-bone" in codes(result, "errors"))

    unsorted = rig(animations={"idle": {"bones": {"body": {"translate": [
        {"time": 0, "y": 0}, {"time": 1, "y": 1}, {"time": 0.5, "y": 2}]}}}})
    result = anim_lint.lint(unsorted)
    check("descending key times are an error", "key-order" in codes(result, "errors"))

    duplicate_time = rig(animations={"idle": {"bones": {"body": {"translate": [
        {"time": 0, "y": 0}, {"time": 1, "y": 1}, {"time": 1, "y": 2}]}}}})
    result = anim_lint.lint(duplicate_time)
    check("duplicate key time is a warning, not an error",
          "key-duplicate" in codes(result, "warnings")
          and "key-duplicate" not in codes(result, "errors"))

    late_start = rig(animations={"idle": {"bones": {"body": {"translate": [
        {"time": 0.2, "y": 0}, {"time": 1, "y": 1}]}}}})
    result = anim_lint.lint(late_start)
    check("first key after 0 is a warning", "key-start" in codes(result, "warnings"))

    long_key = rig(animations={"idle": {"duration": 0.5, "bones": {"body": {"translate": [
        {"time": 0, "y": 0}, {"time": 1, "y": 1}]}}}})
    result = anim_lint.lint(long_key)
    check("key after duration is an error", "key-duration" in codes(result, "errors"))

    nan = rig(animations={"idle": {"bones": {"body": {"translate": [
        {"time": 0, "y": float("nan")}, {"time": 1, "y": 1}]}}}})
    result = anim_lint.lint(nan)
    check("NaN value is an error", "key-value" in codes(result, "errors"))

    bad_curve = rig(animations={"idle": {"bones": {"body": {"rotate": [
        {"time": 0, "value": 0, "curve": [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8]},
        {"time": 1, "value": 10}]}}}})
    result = anim_lint.lint(bad_curve)
    check("8-value curve on rotate is an error", "key-curve" in codes(result, "errors"))

    open_loop = rig(animations={"idle": {"bones": {"body": {"translate": [
        {"y": 0}, {"time": 1, "y": 5}]}}}})
    result = anim_lint.lint(open_loop, loop_clips=["idle"], seam_tol=0.01)
    check("open loop is an error only when --loop asks for it",
          "loop-seam" in codes(result, "errors")
          and "key-start" not in codes(result, "warnings"))

    single = rig(animations={"idle": {"bones": {"body": {"rotate": [
        {"time": 0, "value": 1}]}}}})
    result = anim_lint.lint(single)
    check("single-key timeline is a warning", "key-single" in codes(result, "warnings"))

    clean_second = copy.deepcopy(base)
    clean_second["animations"]["idle"]["bones"]["body"]["translate"][0].pop("time")
    result = anim_lint.lint(clean_second)
    check("implicit first-key time is accepted",
          not result["errors"] and not result["warnings"],
          "errors=%s warnings=%s" % (codes(result, "errors"), codes(result, "warnings")))

    width = max(len(c[0]) for c in checks)
    failed = 0
    for name, ok, detail in checks:
        print("  %-*s %s  %s" % (width, name, "PASS" if ok else "FAIL", detail))
        failed += 0 if ok else 1
    print("%d/%d passed" % (len(checks) - failed, len(checks)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
