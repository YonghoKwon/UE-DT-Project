"""Strict Shipping gate: both 10..60 minutes and the final 20 minutes, WS/Commit <=8 MiB/h."""
import argparse
import datetime as dt
import json
from pathlib import Path

from analyze_memory_trend import MIB, fit

LIMIT = 8.0


def utc(value):
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def evaluate(rows, actual_seconds, complete=True, pcd_valid=True):
    """Rows already use runtime-relative seconds; never substitute a flat tail for the full window."""
    reasons = []
    if not complete or actual_seconds < 3590:
        reasons.append("Incomplete 60-minute runtime")
    if not pcd_valid:
        reasons.append("External PCD validation is missing or failed")
    if any(b["seconds"] <= a["seconds"] for a, b in zip(rows, rows[1:])):
        reasons.append("OS counter timestamps are not strictly increasing")
    metrics = {}
    end = min(3600.0, actual_seconds)
    for name, begin, minimum in (("PostWarmup10To60", 600.0, 590), ("Tail20", end - 1200.0, 235)):
        selected = [r for r in rows if begin <= r["seconds"] <= end]
        gaps = [b["seconds"] - a["seconds"] for a, b in zip(selected, selected[1:])]
        if len(selected) < minimum or not gaps or max(gaps) > 10:
            reasons.append(name + ": insufficient coverage or OS sampling gap >10s")
            continue
        metrics[name] = {}
        for metric, field in (("WorkingSet", "working_set_bytes"),
                              ("PrivateWorkingSet", "private_working_set_bytes"),
                              ("Commit", "commit_bytes")):
            stats = fit(selected, field, MIB)
            if metric != "PrivateWorkingSet":
                stats["limit_mib_h"] = LIMIT
                stats["pass"] = stats["slope_per_hour"] <= LIMIT
            metrics[name][metric] = stats
    valid = not reasons and len(metrics) == 2
    passed = valid and all(m[k]["pass"] for m in metrics.values() for k in ("WorkingSet", "Commit"))
    return {"Valid": valid, "Pass": passed, "Verdict": "PASS" if passed else "FAIL" if valid else "INSUFFICIENT",
            "Reasons": reasons, "Metrics": metrics, "LimitMiBPerHour": LIMIT,
            "Method": "OLS of external OS samples; full10..60 and final20 independently required. No trimming, endpoint substitution, or tail-only override."}


def analyze(folder):
    read = lambda p: json.loads(p.read_text(encoding="utf-8-sig"))
    curve = [json.loads(l) for l in (folder / "memory-trend.jsonl").read_text(encoding="utf-8-sig").splitlines() if l.strip()]
    metadata, summary = curve[0], curve[-1]
    start = utc(metadata["started_utc"])
    raw = [json.loads(l) for l in (folder / "os-counters.jsonl").read_text(encoding="utf-8-sig").splitlines() if l.strip()]
    errors = [r for r in raw if "monitor_error" in r]
    raw = [r for r in raw if "monitor_error" not in r]
    rows = [dict(r, seconds=(utc(r["utc"]) - start).total_seconds()) for r in raw]
    owner = read(folder / "owned-process.json")
    identities = {(r["pid"], r["creation_filetime"]) for r in raw}
    manifest = read(folder / "run-manifest.json")
    pcd = read(folder / "pcd-validation.json")
    complete = (summary.get("type") == "summary" and summary.get("complete") and manifest.get("Valid")
                and not manifest.get("VideoDecodeDetected") and not errors
                and len(identities) == 1 and next(iter(identities))[0] == owner["Pid"]
                and metadata.get("requested_seconds") == 3600 and not (folder / "environment-overlap.json").exists())
    pcd_valid = (pcd.get("Pass", False) and pcd.get("NativeRecordsEnabled", False)
                 and pcd.get("SemanticValidatedFrames", 0) > 0
                 and pcd.get("SemanticValidatedFrames") == pcd.get("ValidatedFrames"))
    result = evaluate(rows, summary.get("measurement_seconds", 0), bool(complete), pcd_valid)
    result.update(Source=str(folder), BuiltCommit=manifest.get("BuiltCommit"),
                  InternalReceiveObservation=metadata.get("internal_receive_observation"),
                  PcdValidation=pcd, PidIdentities=[list(x) for x in identities])
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = analyze(args.report.resolve())
    output = args.output or args.report / "os-strict-analysis.json"
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({"Valid": result["Valid"], "Pass": result["Pass"], "Verdict": result["Verdict"]}))
    raise SystemExit(0 if result["Pass"] else 1)
