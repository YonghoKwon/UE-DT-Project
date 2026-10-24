"""Describe a memory_curve in wall time; never replace the original 8 MiB gates."""
import argparse
import json
import math
import statistics
from pathlib import Path

MIB = 1024 * 1024
INSUFFICIENT = "데이터 부족"
STABLE = "안정화"
SUSPECT = "누수 의심"


def number(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"{name} must be a finite number")
    return value


def load_curve(path):
    """Read legacy memory_curve JSON or incremental JSONL, retaining partial evidence."""
    raw = Path(path).read_text(encoding="utf-8-sig")
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        records, warnings = [], []
        lines = raw.splitlines()
        for index, line in enumerate(lines):
            if not line.strip():
                continue
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError:
                if index != len(lines) - 1:
                    raise ValueError(f"Malformed interior JSONL line {index + 1}") from None
                warnings.append("Truncated final JSONL record ignored; run is incomplete")
        if not records or any(not isinstance(r, dict) for r in records):
            raise ValueError("Expected JSONL objects")
        headers = [r for r in records if r.get("type") == "metadata"]
        summaries = [r for r in records if r.get("type") == "summary"]
        if len(headers) != 1 or records[0] is not headers[0] or len(summaries) > 1:
            raise ValueError("Expected one initial metadata record and at most one summary")
        if any(r.get("type") not in ("metadata", "memory_curve", "summary") for r in records):
            raise ValueError("Unknown JSONL record type")
        points = [r for r in records if r.get("type") == "memory_curve"]
        summary = summaries[0] if summaries else {}
        complete = summary.get("complete") is True and records[-1] is summary and not warnings
        if complete and summary.get("sample_count") != len(points):
            raise ValueError("Summary sample_count does not match memory_curve records")
        return points, {"metadata": headers[0], "summary": summary, "complete": complete}, warnings
    if not isinstance(data, dict) or not isinstance(data.get("memory_curve"), list):
        if isinstance(data, dict) and data.get("type") == "metadata":
            return [], {"complete": False, "metadata": data, "summary": {}}, ["No samples or completion marker"]
        raise ValueError("Expected a memory_curve JSON array")
    return data["memory_curve"], {"complete": False,
                                  "metadata": {}, "summary": {}}, [
        "Legacy curve has no verified completion marker; descriptive slopes only"]


def fit(points, field, scale=1):
    xs = [p["seconds"] / 3600 for p in points]
    ys = [p[field] / scale for p in points]
    mx, my = statistics.mean(xs), statistics.mean(ys)
    xx = sum((x - mx) ** 2 for x in xs)
    slope = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / xx
    # Median endpoint bands reduce sensitivity to an individual first/last page-in.
    band = max(1, len(points) // 5)
    return {"samples": len(points), "slope_per_hour": slope,
            "endpoint_median_delta": statistics.median(ys[-band:]) - statistics.median(ys[:band]),
            "first_last_delta": ys[-1] - ys[0], "range": max(ys) - min(ys)}


def analyze(points, *, complete=False, min_seconds=1800, min_tail_seconds=600,
            mib_per_hour=8, objects_per_hour=10, max_gap_seconds=60):
    for key, value in (("min_seconds", min_seconds), ("min_tail_seconds", min_tail_seconds),
                       ("mib_per_hour", mib_per_hour), ("objects_per_hour", objects_per_hour),
                       ("max_gap_seconds", max_gap_seconds)):
        if number(value, key) <= 0:
            raise ValueError(f"{key} must be positive")
    previous = None
    for p in points:
        if not isinstance(p, dict):
            raise ValueError("Each memory_curve row must be an object")
        for key in ("seconds", "rss_bytes", "commit_bytes", "uobject_count"):
            if number(p.get(key), key) < 0:
                raise ValueError(f"{key} must be nonnegative")
        if previous is not None and p["seconds"] <= previous:
            raise ValueError("seconds must be strictly increasing; do not merge runs")
        previous = p["seconds"]
        for field in ("llm_default", "llm_default_top", "llm_platform_top"):
            if not isinstance(p.get(field, {}), dict):
                raise ValueError(f"{field} must be an object")
            for tag, value in p.get(field, {}).items():
                number(value, f"LLM {tag}")
        if "previous_sample_total_gt_ms" in p:
            if number(p["previous_sample_total_gt_ms"], "previous_sample_total_gt_ms") < 0:
                raise ValueError("Negative sampler cost")
    result = {"verdict": INSUFFICIENT, "reasons": [], "samples": len(points),
              "thresholds": {"min_seconds": min_seconds, "min_tail_seconds": min_tail_seconds,
                             "mib_per_hour": mib_per_hour, "objects_per_hour": objects_per_hour,
                             "max_gap_seconds": max_gap_seconds},
              "metrics": {}, "llm_default_tail": {},
              "method": "OLS against actual elapsed hours, time-based last third; descriptive, correlated samples",
              "original_8mib_gate": "Unchanged; this diagnostic verdict is not acceptance or proof of no leak"}
    if len(points) < 3:
        result["reasons"].append("Fewer than three samples")
        return result
    duration = points[-1]["seconds"] - points[0]["seconds"]
    cut = points[0]["seconds"] + duration * 2 / 3
    tail = [p for p in points if p["seconds"] >= cut - 1e-6]
    gaps = [b["seconds"] - a["seconds"] for a, b in zip(points, points[1:])]
    result.update(duration_seconds=duration, tail_start_seconds=cut, tail_samples=len(tail),
                  max_gap_seconds=max(gaps), min_gap_seconds=min(gaps))
    if duration < min_seconds:
        result["reasons"].append("Observation shorter than minimum duration")
    if (len(tail) < 12 or duration / 3 < min_tail_seconds - 1e-6 or
            tail[-1]["seconds"] - tail[0]["seconds"] < min_tail_seconds - max_gap_seconds):
        result["reasons"].append("Last third needs at least twelve samples and minimum tail duration")
    if min(gaps) < 5 - 1e-6 or max(gaps) > max_gap_seconds:
        result["reasons"].append("Sampling interval outside [5, max_gap_seconds]")
    if not complete:
        result["reasons"].append("No complete run marker; partial/legacy evidence is descriptive only")
    if not all(p.get("llm_enabled") is True and p.get("llm_default") for p in tail):
        result["reasons"].append("LLM unavailable in some tail samples; zero is not a missing measurement")
    if len(tail) < 3:
        return result
    hours = (tail[-1]["seconds"] - tail[0]["seconds"]) / 3600
    growth = []
    def exceeds(value, threshold):
        return value > threshold and not math.isclose(value, threshold, rel_tol=1e-9, abs_tol=1e-9)
    for field, scale, threshold in (("rss_bytes", MIB, mib_per_hour),
                                     ("commit_bytes", MIB, mib_per_hour),
                                     ("uobject_count", 1, objects_per_hour)):
        full, last = fit(points, field, scale), fit(tail, field, scale)
        result["metrics"][field] = {"full": full, "last_third": last,
                                    "unit": "MiB/hour" if scale == MIB else "objects/hour"}
        if exceeds(last["slope_per_hour"], threshold) or exceeds(last["endpoint_median_delta"], threshold * hours):
            growth.append(field)
    tag_values = [{**p.get("llm_default_top", {}), **p.get("llm_default", {})} for p in tail]
    tags = sorted(set().union(*tag_values))
    for tag in tags:
        observed = [{"seconds": p["seconds"], "value": values[tag]}
                    for p, values in zip(tail, tag_values) if tag in values]
        row = {"coverage": len(observed) / len(tail), "last_third": None}
        if len(observed) == len(tail):
            row["last_third"] = fit(observed, "value", MIB)
            if (exceeds(row["last_third"]["slope_per_hour"], mib_per_hour) or
                    exceeds(row["last_third"]["endpoint_median_delta"], mib_per_hour * hours)):
                growth.append("LLM:" + tag)
        result["llm_default_tail"][tag] = row
    if not any(row["coverage"] == 1 for row in result["llm_default_tail"].values()):
        result["reasons"].append("No LLM tag observed throughout the last third")
    if not result["reasons"]:
        result["verdict"] = SUSPECT if growth else STABLE
        result["reasons"] = ["Sustained growth indicators: " + ", ".join(growth)] if growth else [
            "Last-third RSS/commit/UObject and continuously observed tags stay within diagnostic growth limits"]
    result["growth_indicators"] = growth
    result["limits"] = ["Top tags are censored and hierarchical; absent tags are never zero-filled or summed",
                         "RSS/commit growth may be allocator, renderer or driver retention; suspected is not confirmed",
                         "Stable describes this workload/window; it does not prove absence of leaks"]
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("curve", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--min-seconds", type=float, default=1800)
    parser.add_argument("--min-tail-seconds", type=float, default=600)
    parser.add_argument("--mib-per-hour", type=float, default=8)
    parser.add_argument("--objects-per-hour", type=float, default=10)
    parser.add_argument("--max-gap-seconds", type=float, default=60)
    args = parser.parse_args(argv)
    if args.output.resolve() == args.curve.resolve():
        parser.error("Output must not overwrite the input evidence")
    try:
        points, context, warnings = load_curve(args.curve)
        result = analyze(points, complete=context["complete"], min_seconds=args.min_seconds,
                         min_tail_seconds=args.min_tail_seconds, mib_per_hour=args.mib_per_hour,
                         objects_per_hour=args.objects_per_hour, max_gap_seconds=args.max_gap_seconds)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    result.update(source=str(args.curve.resolve()), context=context, warnings=warnings)
    costs = [p["previous_sample_total_gt_ms"] for p in points[1:] if "previous_sample_total_gt_ms" in p]
    last = context["summary"].get("last_sample_total_gt_ms")
    if last is not None:
        costs.append(number(last, "last_sample_total_gt_ms"))
    result["sampler_gt_ms"] = {"observed": len(costs), "mean": statistics.mean(costs) if costs else None,
                                "max": max(costs) if costs else None}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"{result['verdict']}: {args.output}")
    return {STABLE: 0, SUSPECT: 1, INSUFFICIENT: 2}[result["verdict"]]


if __name__ == "__main__":
    raise SystemExit(main())
