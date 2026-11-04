"""r37 OS Commit acceptance; legacy trend/map analyzers remain unchanged.

User approved 2026-11-01 after r36/raw180-a1 demonstrated plateau.
Only the acceptance window changes: discard 30min, observe at least 60min.
"""
import argparse
import datetime as dt
import json
import math
from pathlib import Path

from analyze_memory_trend import MIB, fit, load_curve, number

POLICY = "r37-post30-commit-v1"
LIMIT = 8.0
WARMUP = 1800.0
MIN_OBSERVATION = 3600.0
MAX_GAP = 30.0
BOUNDARY_TOLERANCE = 15.0


def window(rows, start, end, field="commit_bytes"):
    selected = [p for p in rows if start <= p["seconds"] <= end]
    if (len(selected) < 12 or selected[0]["seconds"] > start + BOUNDARY_TOLERANCE
            or selected[-1]["seconds"] < end - BOUNDARY_TOLERANCE):
        raise ValueError(f"Insufficient coverage for {start}..{end}s")
    if any(b["seconds"] - a["seconds"] > MAX_GAP for a, b in zip(selected, selected[1:])):
        raise ValueError("Counter coverage gap")
    return dict(StartSeconds=start, EndSeconds=end, **fit(selected, field, MIB))


def evaluate(rows, end, *, complete):
    """Pure OS-counter policy. No WS trimming, clipping or detrending."""
    if not complete:
        raise ValueError("Incomplete run")
    if number(end, "end") - WARMUP < MIN_OBSERVATION:
        raise ValueError("Post-warmup observation must last at least60min")
    for i, row in enumerate(rows):
        for key in ("seconds", "commit_bytes"):
            if number(row.get(key), key) < 0:
                raise ValueError("Negative OS counter/time")
        if i and row["seconds"] <= rows[i-1]["seconds"]:
            raise ValueError("OS clock must increase strictly")
    accepted = window(rows, WARMUP, end)
    last_third = window(rows, end * 2 / 3, end)
    warmup = window(rows, 600, WARMUP)
    segments = {}
    for label, lo, hi in (("10-60", 600, 3600), ("60-120", 3600, 7200),
                          ("120-180", 7200, 10800)):
        if end >= hi:
            segments[label] = window(rows, lo, hi)
    plateau = dict(Classification="insufficient duration for r36 policy", DiagnosticOnly=True)
    if "120-180" in segments:
        first = segments["10-60"]["slope_per_hour"]
        last = segments["120-180"]["slope_per_hour"]
        yes = last <= first / 2 and last <= LIMIT
        plateau.update(Classification="plateau tendency" if yes else "sustained increase",
                       FirstSlope=first, LastSlope=last, HalfFirst=first/2)
    passed = accepted["slope_per_hour"] <= LIMIT
    return dict(Policy=POLICY, Pass=passed, Status="PASS" if passed else "FAIL",
                ThresholdMiBPerHour=LIMIT, Post30=accepted, LastThird=last_third,
                Warmup10to30=warmup, Segments=segments, Plateau=plateau,
                OldGate=dict(OsTailThird=last_third, Os10to30=warmup,
                             TailThirdCommitPass=last_third["slope_per_hour"] <= LIMIT,
                             TenToThirtyCommitPass=warmup["slope_per_hour"] <= LIMIT,
                             Note="Reported only; legacy analyzer functions were not changed"))


def approved_duration_end(directory, metadata, summary, points, warnings, manifest, approval):
    """r55c explicit 2h stop of an already-running 3h diagnostic.

    Preserve raw incomplete footer/exit2; only the user-selected observation
    endpoint changes. This is never an implicit acceptance of arbitrary partials.
    """
    directory = Path(directory)
    if (approval.get("ApprovalId") != "r55c-2026-11-04"
            or approval.get("Action") != "authorized-duration-stop"
            or approval.get("TargetSeconds") != 7200
            or metadata.get("requested_seconds") != 10800):
        raise ValueError("Unrecognized duration adjustment")
    if not directory.name.startswith(("hatch180-a", "slab-native180-a")):
        raise ValueError("Duration adjustment is not authorized for this scene")
    if warnings or not points or summary.get("sample_count") != len(points):
        raise ValueError("Adjusted run has incomplete/corrupt sample records")
    if (summary.get("complete") is not False
            or summary.get("reason") != "environment cancelled"
            or not 7200 <= summary.get("measurement_seconds", 0) <= 7230
            or points[-1]["seconds"] < 7200):
        raise ValueError("Adjusted run did not reach its approved endpoint normally")
    expected_cancellation = (manifest.get("GameExit"), manifest.get("Failure")) in (
        (2, "Packaged game exited 2"), (2, "Game exited 2"),
        # UE's orderly shutdown can return0 despite RequestExitWithStatus(2).
        # The old runner then rejects only the original10800s completion footer.
        (0, "No complete runtime memory summary"), (0, "No complete map footer"))
    if not expected_cancellation or manifest.get("VideoDecodeDetected"):
        raise ValueError("Adjusted run has a failure beyond the expected cancellation exit")
    owned = json.loads((directory / "owned-process.json").read_text(encoding="utf-8-sig"))
    if approval.get("OwnedPid") != owned.get("Pid"):
        raise ValueError("Adjusted stop process ownership mismatch")
    if (directory / "cancel.request").read_text(encoding="utf-8-sig").strip() != "r55c-approved-duration:7200":
        raise ValueError("Adjusted stop lacks its explicit cancellation token")
    if (directory / "probe-error.json").exists():
        raise ValueError("Probe write/validation error is not a duration adjustment")
    return 7200.0


def analyze(directory, approved_duration_file=None):
    directory = Path(directory)
    points, context, warnings = load_curve(directory / "memory-trend.jsonl")
    metadata, summary = context["metadata"], context["summary"]
    end = number(metadata.get("requested_seconds"), "requested_seconds")
    manifest = json.loads((directory / "run-manifest.json").read_text(encoding="utf-8-sig"))
    adjustment = None
    if approved_duration_file is not None:
        adjustment = json.loads(Path(approved_duration_file).read_text(encoding="utf-8-sig"))
        end = approved_duration_end(directory, metadata, summary, points, warnings, manifest, adjustment)
    if ((not context["complete"] and adjustment is None) or not points or summary.get("measurement_seconds", 0) < end
            or points[-1]["seconds"] < end):
        raise ValueError("Incomplete runtime measurement")
    if metadata.get("commit_diagnostics") is True or metadata.get("investigation") is True:
        raise ValueError("Heavy attribution diagnostics cannot certify acceptance")
    origin = dt.datetime.fromisoformat(metadata["started_utc"])
    if origin.tzinfo is None:
        origin = origin.replace(tzinfo=dt.timezone.utc)
    raw = [json.loads(line) for line in (directory / "os-counters.jsonl").read_text(
        encoding="utf-8-sig").splitlines() if line.strip()]
    rows, identities = [], set()
    for row in raw:
        elapsed = (dt.datetime.fromisoformat(row["utc"]) - origin).total_seconds()
        if "monitor_error" in row:
            if 0 <= elapsed <= end:
                raise ValueError("OS monitor failed during measurement")
            continue
        identities.add((row.get("pid"), row.get("creation_filetime")))
        if 0 <= elapsed <= end:
            rows.append(dict(row, seconds=elapsed))
    if len(identities) != 1 or next(iter(identities)) == (None, None):
        raise ValueError("Missing/changed OS process identity")
    result = evaluate(rows, end, complete=True)
    # Observe WS/Private without promoting these auxiliary values to Commit gate.
    result["OsAuxiliary"] = {name: window(rows, WARMUP, end, field) for name, field in (
        ("WorkingSet", "working_set_bytes"), ("PrivateBytes", "private_bytes"),
        ("PrivateWorkingSet", "private_working_set_bytes"))}
    ids = set(points[0]["sensors"])
    if not ids or any(set(p["sensors"]) != ids or p.get("lidar_actor_count", len(ids)) != len(ids)
                      for p in points):
        raise ValueError("Sensor cohort missing/changed")
    post = [p for p in points if WARMUP <= p["seconds"] <= end]
    rates = {key: (post[-1]["sensors"][key]["frame_id"] - post[0]["sensors"][key]["frame_id"])
             / (post[-1]["seconds"] - post[0]["seconds"]) for key in sorted(ids)}
    native = [p["rhi_submission"] for p in points]
    native_ok = (max(p["native_fallbacks"] for p in native) == 0
                 and native[-1]["native_dispatches"] > native[0]["native_dispatches"])
    reasons = []
    if not native_ok:
        reasons.append("Native dispatch absent or fallback nonzero")
    if any(hz < 19 or hz > 21 for hz in rates.values()):
        reasons.append("Sensor rate outside unchanged approximately20Hz band19..21")
    if any(not p["sensors"][key].get("running", True) for p in post for key in ids):
        reasons.append("Sensor stopped during observation")
    if not manifest.get("Valid") and adjustment is None:
        reasons.append("Run manifest invalid")
    watch = [json.loads(line) for line in (directory / "slot-watch.jsonl").read_text(
        encoding="utf-8-sig").splitlines() if line.strip()]
    if not watch or any(p.get("Foreign") or p.get("Builds") or p.get("VideoDecodePercent", -1) != 0
                        for p in watch):
        raise ValueError("Environment overlap/decoder state invalid")
    times = [(dt.datetime.fromisoformat(p["Utc"]) - origin).total_seconds() for p in watch]
    if (times[0] > 5 or times[-1] < end - 5
            or any(not 0 < b-a <= MAX_GAP for a, b in zip(times, times[1:]))):
        raise ValueError("Environment watch coverage insufficient")
    if (directory / "environment-overlap.json").exists():
        raise ValueError("Rejected environment marker present")
    if (directory / "pcd-validation.json").exists():
        pcd = json.loads((directory / "pcd-validation.json").read_text(encoding="utf-8-sig"))
        result["Pcd"] = pcd
        if not pcd.get("Pass"):
            reasons.append("External PCD validation failed")
    tail_points = [p for p in points if end*2/3 <= p["seconds"] <= end]
    result["OldGate"]["RuntimeTailThird"] = {
        field: fit(tail_points, field, MIB) for field in ("rss_bytes", "commit_bytes")}
    if all("RHIMisc" in p.get("llm_default", {}) for p in tail_points):
        result["OldGate"]["RHIMiscTailThird"] = fit(
            [dict(seconds=p["seconds"], value=p["llm_default"]["RHIMisc"]) for p in tail_points], "value", MIB)
    result.update(Valid=not reasons, Pass=result["Pass"] and not reasons, Reasons=reasons,
                  ActualHz=rates, NativeFallbacks=max(p["native_fallbacks"] for p in native),
                  NativeDispatchDelta=native[-1]["native_dispatches"]-native[0]["native_dispatches"],
                  RuntimeMetadata=metadata, RuntimeSummary=summary, Warnings=warnings,
                  Source=str(directory.resolve()), RequestedSeconds=end,
                  Scope="r37 memory revalidation only; original#26 absolute5pair, byte oracle and ABAB unchanged")
    if adjustment is not None:
        result["DurationAdjustment"] = dict(adjustment, OriginalRequestedSeconds=metadata["requested_seconds"],
            OriginalRuntimeComplete=summary["complete"], OriginalRunManifestValid=manifest.get("Valid"),
            OriginalGameExit=manifest["GameExit"], Note="User r55c selected 30..120min; raw footer/manifest retained unchanged. All coverage, environment, native, PCD and 8MiB/h checks remain.")
    result["Status"] = "PASS" if result["Pass"] else "FAIL"
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--approved-duration-file", type=Path,
                        help="Explicit r55c planned-stop proof; does not accept other incomplete runs")
    args = parser.parse_args()
    try:
        result = analyze(args.directory, args.approved_duration_file)
    except (OSError, ValueError, KeyError) as error:
        result = dict(Policy=POLICY, Valid=False, Pass=False, Status="INVALID", Reasons=[str(error)])
    (args.directory / "acceptance-r37.json").write_text(json.dumps(result, indent=2,
        ensure_ascii=False, allow_nan=False)+"\n", encoding="utf-8")
    print(json.dumps({key: result.get(key) for key in ("Status", "Reasons", "ActualHz", "NativeFallbacks")}))
    return 0 if result["Pass"] else 1 if result.get("Valid") else 2


if __name__ == "__main__":
    raise SystemExit(main())
