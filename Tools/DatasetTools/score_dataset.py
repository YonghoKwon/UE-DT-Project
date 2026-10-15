"""Validate DATASET-01 files and score the existing world-XY Slab PCA yaw.

No third-party packages. Usage: python Tools/DatasetTools/score_dataset.py DATASET --output REPORT.json
Yaw is an unoriented major axis (period 180 degrees), not pitch/roll or heading.
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import statistics
import struct

PCD_RECORD = struct.Struct("<fffHHHBBqBf")
LABEL_RECORD = struct.Struct("<BI")
FIELDS = "x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence"


def read_pcd(path: pathlib.Path) -> int:
    """Read a standard PCD 0.7 binary header and its explicit packed records."""
    with path.open("rb") as stream:
        header: dict[str, str] = {}
        for _ in range(128):
            line = stream.readline(1024 * 1024)
            if not line or not line.endswith(b"\n"):
                raise ValueError(f"{path}: truncated PCD header")
            text = line.decode("ascii").strip()
            if text.startswith("#") or not text:
                continue
            key, _, value = text.partition(" ")
            header[key] = value.strip()
            if key == "DATA":
                break
        else:
            raise ValueError(f"{path}: missing DATA header")
        expected = {
            "VERSION": "0.7", "FIELDS": FIELDS,
            "SIZE": "4 4 4 2 2 2 1 1 8 1 4", "TYPE": "F F F U U U U U I U F",
            "COUNT": "1 1 1 1 1 1 1 1 1 1 1", "DATA": "binary",
        }
        if any(header.get(k) != v for k, v in expected.items()):
            raise ValueError(f"{path}: unsupported or inconsistent PCD schema")
        count = int(header["POINTS"])
        if count < 0 or int(header["WIDTH"]) * int(header["HEIGHT"]) != count:
            raise ValueError(f"{path}: inconsistent PCD dimensions")
        for _ in range(count):
            record = stream.read(PCD_RECORD.size)
            if len(record) != PCD_RECORD.size:
                raise ValueError(f"{path}: truncated PCD point data")
            values = PCD_RECORD.unpack(record)
            if not all(math.isfinite(v) for v in (*values[:3], values[-1])):
                raise ValueError(f"{path}: nonfinite point")
        if stream.read(1):
            raise ValueError(f"{path}: excess PCD point data")
        return count


def axis_error(predicted: float, truth: float) -> float:
    if not math.isfinite(predicted) or not math.isfinite(truth):
        raise ValueError("nonfinite angle")
    return abs((predicted - truth + 90.0) % 180.0 - 90.0)


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered or not 0 <= fraction <= 1:
        raise ValueError("percentile requires samples and a fraction in [0, 1]")
    index = (len(ordered) - 1) * fraction
    low = math.floor(index)
    high = math.ceil(index)
    return ordered[low] + (ordered[high] - ordered[low]) * (index - low)


def score(root: pathlib.Path) -> dict:
    manifest = json.loads((root / "dataset.json").read_text(encoding="utf-8"))
    if manifest.get("schema_version") != "ma0t10.dataset.v1":
        raise ValueError("unsupported dataset schema")
    completed = manifest["completed_scenes"]
    if type(completed) is not int or completed < 1:
        raise ValueError("dataset contains no completed scenes")
    errors: list[float] = []
    invalid: list[int] = []
    points = 0
    for index in range(completed):
        scene = root / f"scene_{index:05d}"
        label = json.loads((scene / "label.json").read_text(encoding="utf-8"))
        if label.get("schema_version") != "ma0t10.dataset.scene.v1" or label.get("scene_index") != index:
            raise ValueError(f"{scene}: scene identity/schema mismatch")
        count = read_pcd(scene / "lidar.pcd")
        labels = (scene / "lidar_labels.bin").read_bytes()
        if len(labels) != count * LABEL_RECORD.size:
            raise ValueError(f"{scene}: PCD/label count mismatch")
        if any(semantic > 6 for semantic, _ in LABEL_RECORD.iter_unpack(labels)):
            raise ValueError(f"{scene}: unknown semantic class")
        points += count
        truth = float(label["slab_pose"]["pitch_yaw_roll_deg"][1])
        if not math.isfinite(truth):
            raise ValueError(f"{scene}: nonfinite truth")
        if label["analysis_valid"] is True:
            errors.append(axis_error(float(label["analysis_yaw_deg"]), truth))
        else:
            invalid.append(index)
    return {
        "schema_version": "ma0t10.dataset.score.v1", "dataset": str(root.resolve()),
        "project_commit": manifest.get("project_commit"), "completed_scenes": completed,
        "valid_analysis_scenes": len(errors), "invalid_analysis_scene_indices": invalid,
        "total_pcd_points": points, "metric": "absolute world-XY major-axis yaw error, period 180 degrees",
        "p95_definition": "linear interpolation at 0.95*(n-1) in sorted valid errors",
        "mean_degrees": statistics.fmean(errors) if errors else None,
        "p95_degrees": percentile(errors, .95) if errors else None,
        "max_degrees": max(errors) if errors else None,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        report = score(args.dataset)
    except (ValueError, KeyError, OSError, struct.error) as exc:
        parser.exit(2, f"Dataset validation failed: {exc}\n")
    text = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
