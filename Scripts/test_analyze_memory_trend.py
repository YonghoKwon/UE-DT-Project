import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

from analyze_memory_trend import INSUFFICIENT, MIB, STABLE, SUSPECT, analyze, load_curve, main


def curve():
    return [{"seconds": s, "cycle": s // 2, "rss_bytes": 1000 * MIB,
             "commit_bytes": 1100 * MIB, "uobject_count": 10000, "llm_enabled": True,
             "llm_default": {"UObject": 20 * MIB, "FMallocUnused": 10 * MIB},
             "previous_sample_total_gt_ms": 0.25}
            for s in range(0, 1801, 5)]


def jsonl(points, complete=True):
    rows = [{"type": "metadata", "schema_version": 1}]
    rows += [{"type": "memory_curve", **p} for p in points]
    if complete:
        rows.append({"type": "summary", "complete": True, "sample_count": len(points),
                     "last_sample_total_gt_ms": 0.5})
    return "\n".join(json.dumps(r) for r in rows) + "\n"


class MemoryTrendTests(unittest.TestCase):
    def test_constant_is_stable(self):
        self.assertEqual(analyze(curve(), complete=True)["verdict"], STABLE)

    def test_growth_in_actual_hours_not_cycles(self):
        points = curve()
        for p in points:
            p["rss_bytes"] += 24 * MIB * p["seconds"] / 3600
            p["cycle"] = 0
        result = analyze(points, complete=True)
        self.assertEqual(result["verdict"], SUSPECT)
        self.assertAlmostEqual(result["metrics"]["rss_bytes"]["last_third"]["slope_per_hour"], 24)

    def test_early_retention_then_plateau(self):
        points = curve()
        for p in points:
            p["rss_bytes"] += min(p["seconds"], 600) * MIB / 10
        result = analyze(points, complete=True)
        self.assertEqual(result["verdict"], STABLE)
        self.assertGreater(result["metrics"]["rss_bytes"]["full"]["slope_per_hour"], 8)

    def test_commit_growth_even_when_rss_constant(self):
        points = curve()
        for p in points:
            p["commit_bytes"] += 20 * MIB * p["seconds"] / 3600
        self.assertEqual(analyze(points, complete=True)["verdict"], SUSPECT)

    def test_uobjects_grow_inside_existing_committed_capacity(self):
        points = curve()
        for p in points:
            p["uobject_count"] += p["seconds"] // 5
        self.assertEqual(analyze(points, complete=True)["growth_indicators"], ["uobject_count"])

    def test_tracked_growth_inside_allocator_capacity(self):
        points = curve()
        for p in points:
            p["llm_default"]["UObject"] += 20 * MIB * p["seconds"] / 3600
        self.assertEqual(analyze(points, complete=True)["growth_indicators"], ["LLM:UObject"])

    def test_top_tag_missing_is_censored_not_zero(self):
        points = curve()
        points[-1]["llm_default_top"] = {"Transient": 400 * MIB}
        result = analyze(points, complete=True)
        self.assertEqual(result["verdict"], STABLE)
        self.assertLess(result["llm_default_tail"]["Transient"]["coverage"], 1)
        self.assertIsNone(result["llm_default_tail"]["Transient"]["last_third"])

    def test_full_coverage_top_tag_is_analyzed(self):
        points = curve()
        for p in points:
            p["llm_default_top"] = {"Other": 24 * MIB * p["seconds"] / 3600}
        self.assertEqual(analyze(points, complete=True)["verdict"], SUSPECT)

    def test_short_incomplete_and_llm_disabled_are_insufficient(self):
        short, disabled = curve()[:20], curve()
        for p in disabled:
            p["llm_enabled"] = False
        for points, complete in ((short, True), (curve(), False), (disabled, True), ([], True)):
            with self.subTest(samples=len(points), complete=complete):
                self.assertEqual(analyze(points, complete=complete)["verdict"], INSUFFICIENT)

    def test_long_gap_or_oversampling_is_insufficient(self):
        points = curve()
        for malformed in (points[:30] + points[45:], [{**p, "seconds": p["seconds"] / 2} for p in points]):
            self.assertEqual(analyze(malformed, complete=True)["verdict"], INSUFFICIENT)

    def test_jitter_uses_time_based_last_third(self):
        points = curve()
        for index, p in enumerate(points):
            p["seconds"] += index * 0.01
        result = analyze(points, complete=True)
        self.assertEqual(result["verdict"], STABLE)
        self.assertAlmostEqual(result["tail_start_seconds"], points[-1]["seconds"] * 2 / 3)

    def test_invalid_time_missing_and_nonfinite_are_rejected(self):
        for field, value in (("seconds", 0), ("rss_bytes", float("nan")),
                             ("commit_bytes", -1), ("uobject_count", True), ("rss_bytes", None)):
            points = curve()
            points[-1][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                analyze(points, complete=True)

    def test_threshold_boundary_and_invalid_threshold(self):
        points = curve()
        for p in points:
            p["rss_bytes"] += 8 * MIB * p["seconds"] / 3600
        self.assertAlmostEqual(analyze(points, complete=True)["metrics"]["rss_bytes"]["last_third"]["slope_per_hour"], 8)
        self.assertEqual(analyze(points, complete=True)["verdict"], STABLE)
        with self.assertRaises(ValueError):
            analyze(points, mib_per_hour=0)

    def test_jsonl_complete_and_partial_and_legacy(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "curve.jsonl"
            path.write_text(jsonl(curve()), encoding="utf-8")
            points, context, _ = load_curve(path)
            self.assertTrue(context["complete"])
            self.assertEqual(len(points), 361)
            path.write_text(jsonl(curve(), False) + '{"type":', encoding="utf-8")
            points, context, warnings = load_curve(path)
            self.assertFalse(context["complete"])
            self.assertTrue(warnings)
            path.write_text(json.dumps({"memory_curve": curve()}), encoding="utf-8-sig")
            self.assertFalse(load_curve(path)[1]["complete"])

    def test_historical_gate_failure_is_preserved_with_short_curve(self):
        from reanalyze_hatch_memory import summarize_run
        points = curve()[:19]
        data = {"clicks": 50, "measurement_seconds": 90, "memory_curve": points,
                "raw_rss_before": 100 * MIB, "raw_rss_after": 118 * MIB,
                "rss_before": 100 * MIB, "rss_after": 118 * MIB,
                "commit_before": 100 * MIB, "commit_after": 113 * MIB}
        result = summarize_run(data)
        self.assertEqual(result["curve"]["verdict"], INSUFFICIENT)
        self.assertEqual(result["deltas_mib"]["raw_rss"], 18)
        self.assertEqual(result["uobject_count"]["max"], result["uobject_count"]["min"])

    def test_thirty_second_sampling_with_frame_jitter(self):
        points = curve()[::6]
        for index, p in enumerate(points):
            p["seconds"] += index * 0.01
        self.assertEqual(analyze(points, complete=True)["verdict"], STABLE)

    def test_corrupt_interior_or_count_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "curve.jsonl"
            for text in (jsonl(curve()).replace('"sample_count": 361', '"sample_count": 362'),
                         jsonl(curve()).replace('"cycle": 2', '"cycle": INVALID', 1)):
                path.write_text(text, encoding="utf-8")
                with self.assertRaises(ValueError):
                    load_curve(path)

    def test_cli_exit_codes_and_costs(self):
        with tempfile.TemporaryDirectory() as temp, contextlib.redirect_stdout(io.StringIO()):
            source, output = Path(temp) / "in.jsonl", Path(temp) / "out.json"
            source.write_text(jsonl(curve()), encoding="utf-8")
            self.assertEqual(main([str(source), "--output", str(output)]), 0)
            data = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(data["sampler_gt_ms"]["observed"], 361)
            self.assertEqual(data["sampler_gt_ms"]["max"], 0.5)
            source.write_text(jsonl(curve(), False), encoding="utf-8")
            self.assertEqual(main([str(source), "--output", str(output)]), 2)
            points = curve()
            for p in points:
                p["rss_bytes"] += p["seconds"] * MIB
            source.write_text(jsonl(points), encoding="utf-8")
            self.assertEqual(main([str(source), "--output", str(output)]), 1)


if __name__ == "__main__":
    unittest.main()
