"""Keep absolute RSS gates and matched dig-minus-control gates independent."""
import argparse
import json
import statistics
from pathlib import Path

LIMIT = 8 * 1024 * 1024
METRICS = {"raw_rss": ("raw_rss_before", "raw_rss_after"),
           "retained_rss": ("rss_before", "rss_after"),
           "commit": ("commit_before", "commit_after")}
MEMORY_ERRORS = ("50 clicks raw RSS grows <=8MiB", "50 clicks retained RSS grows <=8MiB",
                 "50 clicks committed memory grows <=8MiB")


def compare_pair(dig, control):
    if dig["clicks"] != 50 or control["clicks"] != 0:
        raise ValueError("Expected 50 production digs and zero measured control digs")
    if abs(dig["measurement_seconds"] - control["measurement_seconds"]) > 2:
        raise ValueError("Matched measurement durations differ by more than2 seconds")
    if dig.get("matched_window_seconds", 0) <= 0 or dig["matched_window_seconds"] != control.get("matched_window_seconds"):
        raise ValueError("A common explicit measurement window is required")
    rows = {}
    for name, (before, after) in METRICS.items():
        a, b = dig[after] - dig[before], control[after] - control[before]
        rows[name] = {"dig_bytes": a, "control_bytes": b, "difference_bytes": a - b,
                      "original_dig_8mib_pass": a <= LIMIT,
                      "control_8mib_pass": b <= LIMIT, "separate_difference_8mib_pass": a - b <= LIMIT}
    return rows


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report_root", type=Path)
    args = parser.parse_args()
    root = args.report_root.resolve()
    pairs, manifests, unexpected = [], [], []
    for index in range(1, 4):
        runs = {}
        for role, filename in (("dig", "dig-runtime.json"), ("control", "dig-idle-control.json")):
            folder = root / f"pair-{index}-{role}"
            data, manifest = read(folder / filename), read(folder / "source-manifest.json")
            if not manifest.get("MemoryAudit") or manifest.get("MemoryWarmupSeconds") != 35:
                raise ValueError("Every comparison run must use LLM/RHI audit and original35s preparation")
            if manifest.get("MatchedMemorySeconds") != data.get("matched_window_seconds"):
                raise ValueError("Manifest and actual window disagree")
            if data.get("viewport_point_budget") != 12000:
                raise ValueError("Original12000-point viewport budget is required")
            manifest.pop("Mode", None)
            manifests.append(manifest)
            automation = read(folder / "automation/index.json")
            errors = [entry["event"]["message"] for test in automation["tests"]
                      for entry in test["entries"] if entry["event"]["type"] == "Error"]
            unexpected += [f"{folder.name}: {message}" for message in errors
                           if not any(token in message for token in MEMORY_ERRORS)]
            runs[role] = data
        pairs.append({"pair": index, "metrics": compare_pair(runs["dig"], runs["control"]),
                      "runs": runs})
    if any(item != manifests[0] for item in manifests[1:]):
        raise ValueError("Commit/source hashes/dependencies/display/preparation/audit/window differ")
    separate_pass = not unexpected and all(row["separate_difference_8mib_pass"]
                                            for pair in pairs for row in pair["metrics"].values())
    result = {"commit": manifests[0]["Commit"], "pairs": pairs,
              "original_absolute_gates_pass": all(row["original_dig_8mib_pass"] for p in pairs for row in p["metrics"].values()),
              "separate_difference_gates_pass": separate_pass, "unexpected_errors": unexpected,
              "median_difference_bytes": {name: statistics.median(p["metrics"][name]["difference_bytes"] for p in pairs) for name in METRICS},
              "interpretation": "Difference gates supplement, never replace, original8MiB gates. Three pairs are descriptive evidence; Claude reviews final attribution. LLM runs are not Hz/FPS acceptance."}
    (root / "comparison.json").write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    lines = ["# HATCH-06 matched memory comparison", "", f"Commit: {result['commit']}", "",
             "Original absolute8MiB and dig-minus-control8MiB are separate checks. Original failures remain failures.", "",
             "| Pair | Metric |50dig MiB|0dig MiB|difference MiB|absolute gate|separate difference gate|",
             "|---|---|---:|---:|---:|---|---|"]
    for pair in pairs:
        for name, row in pair["metrics"].items():
            lines.append(f"|{pair['pair']}|{name}|{row['dig_bytes']/1048576:.3f}|{row['control_bytes']/1048576:.3f}|{row['difference_bytes']/1048576:.3f}|{'PASS' if row['original_dig_8mib_pass'] else 'FAIL'}|{'PASS' if row['separate_difference_8mib_pass'] else 'FAIL'}|")
    lines += ["", result["interpretation"], "", f"Unexpected functional errors: {unexpected}"]
    (root / "COMPARISON.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k != "pairs"}, indent=2))
    return 0 if separate_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
