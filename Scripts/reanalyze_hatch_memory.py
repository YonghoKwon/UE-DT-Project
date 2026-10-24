"""Read preserved #26 JSON evidence without running Unreal or modifying input files."""
import argparse
import hashlib
import json
import statistics
from pathlib import Path

from analyze_memory_trend import MIB, analyze
from compare_hatch_memory import LIMIT, compare_pair

COHORTS = ("gpu-default-five-pairs", "gpu-five-pairs", "gpu-deep-dig",
           "cpu-default-reference", "gpu-tag-reuse-five-pairs", "gpu-cpu-scratch-five-pairs")


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def summarize_run(data):
    points = data.get("memory_curve", [])
    result = {"clicks": data["clicks"], "measurement_seconds": data["measurement_seconds"],
              "deltas_mib": {name: (data[after] - data[before]) / MIB for name, before, after in (
                  ("raw_rss", "raw_rss_before", "raw_rss_after"),
                  ("retained_rss", "rss_before", "rss_after"),
                  ("commit", "commit_before", "commit_after"))},
              "curve": analyze(points), "llm_boundary_deltas_mib": {}}
    if points:
        objects = [p["uobject_count"] for p in points]
        result["uobject_count"] = {"first": objects[0], "last": objects[-1],
                                   "min": min(objects), "max": max(objects)}
    else:
        result["uobject_count"] = None
    for tracker in ("default", "platform"):
        before, after = data.get("before_llm_" + tracker, {}), data.get("after_llm_" + tracker, {})
        result["llm_boundary_deltas_mib"][tracker] = {
            key: (after[key] - before[key]) / MIB for key in sorted(before.keys() & after.keys())}
    for key, before, after in (("rhi_resources_equal", "before_rhi_resources", "after_rhi_resources"),
                               ("owner_resources_equal_except_uploads", "resources_before", "resources_after")):
        def strip_uploads(value):
            if not isinstance(value, dict):
                return value
            return {k: strip_uploads(v) for k, v in value.items() if k != "view/fixed_point_uploads"}
        result[key] = (strip_uploads(data[before]) == strip_uploads(data[after])
                       if before in data and after in data else None)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("r2_root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    root = args.r2_root.resolve()
    if not root.is_dir():
        parser.error("r2_root must be an existing directory")
    if args.output.resolve().is_relative_to(root):
        parser.error("Write output outside the read-only evidence directory")
    result = {"evidence_root": str(root), "inputs_sha256": {}, "cohorts": {},
              "limits": ["Preserved JSON only; removed CSV/log files were not reconstructed",
                         "90-second curves cannot establish 30..60 minute stabilization",
                         "LLM tags are hierarchical; tag deltas must not be summed",
                         "No historical diagnostic is a new execution or a fix for the failed 8MiB gates"]}
    for name in COHORTS:
        folder = root / name
        paths = sorted(p for p in folder.rglob("dig-*.json")
                       if p.name in ("dig-runtime.json", "dig-idle-control.json"))
        runs = {}
        raw_runs = {}
        for path in paths:
            relative = str(path.relative_to(root))
            result["inputs_sha256"][relative] = hashlib.sha256(path.read_bytes()).hexdigest()
            data = read(path)
            runs[str(path.relative_to(folder))] = summarize_run(data)
            raw_runs[path.parent.name] = data
        pairs = []
        for index in range(1, 6):
            dig, control = raw_runs.get(f"pair-{index}-dig"), raw_runs.get(f"pair-{index}-control")
            if dig is not None and control is not None:
                pairs.append({"pair": index, "metrics": compare_pair(dig, control)})
        cohort = {"runs": runs, "pairs": pairs}
        if pairs:
            values = [p["metrics"]["raw_rss"] for p in pairs]
            cohort["raw_rss_summary"] = {
                "pair_count": len(pairs), "absolute_fail_pairs": [p["pair"] for p in pairs
                    if not p["metrics"]["raw_rss"]["original_dig_8mib_pass"]],
                "difference_fail_pairs": [p["pair"] for p in pairs
                    if not p["metrics"]["raw_rss"]["separate_difference_8mib_pass"]],
                "median_dig_mib": statistics.median(v["dig_bytes"] for v in values) / MIB,
                "worst_difference_mib": max(v["difference_bytes"] for v in values) / MIB,
                "unchanged_threshold_mib": LIMIT / MIB}
        result["cohorts"][name] = cohort
    if not any(c["runs"] for c in result["cohorts"].values()):
        parser.error("No preserved dig-runtime/dig-idle-control JSON evidence found")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Reanalyzed {len(result['inputs_sha256'])} preserved JSON files: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
