import math
import datetime as dt
import json
import tempfile
from pathlib import Path
import unittest
from analyze_memory_acceptance import MIB, analyze, evaluate


def series(function, seconds=10800):
    return [dict(seconds=t, commit_bytes=(1000+function(t))*MIB) for t in range(0, seconds+1, 5)]


class MemoryAcceptanceTests(unittest.TestCase):
    def test_boundary_eight_is_pass(self):
        self.assertTrue(evaluate(series(lambda t: 8*t/3600), 10800, complete=True)["Pass"])

    def test_over_eight_fails(self):
        self.assertFalse(evaluate(series(lambda t: 8.01*t/3600), 10800, complete=True)["Pass"])

    def test_warmup_step_not_admitted_to_post30(self):
        result = evaluate(series(lambda t: 30*min(t, 1800)/1800), 10800, complete=True)
        self.assertTrue(result["Pass"])
        self.assertGreater(result["OldGate"]["Os10to30"]["slope_per_hour"], 8)
        self.assertAlmostEqual(result["Warmup10to30"]["first_last_delta"], 20)
        self.assertEqual(result["Plateau"]["Classification"], "plateau tendency")

    def test_post30_step_remains_visible_even_if_tail_flattens(self):
        result = evaluate(series(lambda t: 50 if t >= 5400 else 0), 10800, complete=True)
        self.assertFalse(result["Pass"])
        self.assertEqual(result["LastThird"]["slope_per_hour"], 0)

    def test_minimum_observation_and_complete_required(self):
        for end, complete in ((5399, True), (10800, False)):
            with self.assertRaises(ValueError):
                evaluate(series(lambda t: 0), end, complete=complete)
        self.assertTrue(evaluate(series(lambda t: 0, 5400), 5400, complete=True)["Pass"])

    def test_gaps_missing_end_and_pid_independent_numeric_validation(self):
        rows = series(lambda t: 0)
        for invalid in (rows[:-10], rows[:400]+rows[410:], list(reversed(rows))):
            with self.assertRaises(ValueError):
                evaluate(invalid, 10800, complete=True)
        rows[1000]["commit_bytes"] = math.nan
        with self.assertRaises(ValueError):
            evaluate(rows, 10800, complete=True)

    def test_legacy_analyzer_remains_callable_with_original_thresholds(self):
        from analyze_memory_trend import analyze
        rows = series(lambda t: 9*t/3600)
        for row in rows:
            row.update(rss_bytes=row["commit_bytes"], uobject_count=100,
                       llm_enabled=True, llm_default={"RHIMisc": 1})
        old = analyze(rows, complete=True)
        self.assertEqual(old["thresholds"]["mib_per_hour"], 8)
        self.assertIn("commit_bytes", old["growth_indicators"])


class AcceptanceEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.origin = dt.datetime(2026, 11, 1, tzinfo=dt.timezone.utc)
        self.runtime = [dict(type="metadata", requested_seconds=5400,
                             started_utc=self.origin.isoformat(), commit_diagnostics=False)]
        self.os, self.watch = [], []
        for t in range(0, 5401, 5):
            utc = (self.origin+dt.timedelta(seconds=t)).isoformat()
            self.runtime.append(dict(type="memory_curve", seconds=t, rss_bytes=MIB,
                commit_bytes=MIB, sensors={"a": dict(frame_id=t*20, running=True)},
                rhi_submission=dict(native_dispatches=t*20, native_fallbacks=0)))
            self.os.append(dict(utc=utc, pid=123, creation_filetime=456, commit_bytes=2*MIB,
                                private_bytes=2*MIB, working_set_bytes=MIB, private_working_set_bytes=MIB))
            self.watch.append(dict(Utc=utc, Foreign=[], Builds=[], VideoDecodePercent=0))
        self.runtime.append(dict(type="summary", complete=True, measurement_seconds=5400, sample_count=1081))

    def save(self):
        for name, rows in (("memory-trend.jsonl", self.runtime), ("os-counters.jsonl", self.os),
                           ("slot-watch.jsonl", self.watch)):
            (self.folder/name).write_text("\n".join(json.dumps(p) for p in rows), encoding="utf-8")
        (self.folder/"run-manifest.json").write_text('{"Valid":true}', encoding="utf-8")

    def test_valid_and_shutdown_decommit_excluded(self):
        self.os.append(dict(self.os[-1], utc=(self.origin+dt.timedelta(seconds=5401)).isoformat(), commit_bytes=0))
        self.save()
        result = analyze(self.folder)
        self.assertTrue(result["Pass"])
        self.assertEqual(result["Post30"]["slope_per_hour"], 0)

    def test_pid_reuse_rejected(self):
        self.os[-1]["creation_filetime"] = 999
        self.save()
        with self.assertRaisesRegex(ValueError, "identity"):
            analyze(self.folder)

    def test_overlap_and_build_and_decoder_rejected(self):
        for key, value in (("Foreign", [dict(Pid=9)]), ("Builds", [dict(Pid=10)]), ("VideoDecodePercent", 1)):
            previous = self.watch[100][key]
            self.watch[100][key] = value
            self.save()
            with self.assertRaisesRegex(ValueError, "Environment"):
                analyze(self.folder)
            self.watch[100][key] = previous

    def test_watch_gap_and_heavy_diagnostics_rejected(self):
        self.watch = self.watch[5:]
        self.save()
        with self.assertRaisesRegex(ValueError, "coverage"):
            analyze(self.folder)
        self.runtime[0]["commit_diagnostics"] = True
        self.save()
        with self.assertRaisesRegex(ValueError, "Heavy"):
            analyze(self.folder)

    def test_fallback_or_zero_rate_cannot_pass(self):
        self.runtime[1]["rhi_submission"]["native_fallbacks"] = 1
        self.save()
        self.assertFalse(analyze(self.folder)["Pass"])
        self.runtime[1]["rhi_submission"]["native_fallbacks"] = 0
        for p in self.runtime[1:-1]:
            p["sensors"]["a"]["frame_id"] = 1
        self.save()
        self.assertFalse(analyze(self.folder)["Pass"])


if __name__ == "__main__":
    unittest.main()
