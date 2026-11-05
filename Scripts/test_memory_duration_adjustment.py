"""r55c changes the endpoint, never the acceptance/coverage requirements."""
import json
from pathlib import Path
import tempfile
import unittest

from analyze_memory_acceptance import approved_duration_end, evaluate


class DurationAdjustmentTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name) / "hatch180-a1"
        self.directory.mkdir()
        (self.directory / "owned-process.json").write_text(json.dumps({"Pid": 1234}))
        (self.directory / "cancel.request").write_text("r55c-approved-duration:7200")
        self.metadata = {"requested_seconds": 10800}
        self.summary = {"complete": False, "reason": "environment cancelled", "sample_count": 2,
                        "measurement_seconds": 7202}
        self.points = [{"seconds": 0}, {"seconds": 7200.1}]
        self.manifest = {"Valid": False, "GameExit": 2, "VideoDecodeDetected": False,
                         "Failure": "Packaged game exited 2"}
        self.approval = {"ApprovalId": "r55c-2026-11-04", "Action": "authorized-duration-stop",
                         "TargetSeconds": 7200, "OwnedPid": 1234}

    def tearDown(self):
        self.temp.cleanup()

    def apply(self, **changes):
        values = dict(metadata=self.metadata, summary=self.summary, points=self.points,
                      warnings=[], manifest=self.manifest, approval=self.approval)
        values.update(changes)
        return approved_duration_end(self.directory, **values)

    def test_explicit_two_hour_stop_preserves_original_evidence(self):
        self.assertEqual(self.apply(), 7200)
        self.assertEqual(self.metadata["requested_seconds"], 10800)
        self.assertFalse(self.summary["complete"])
        self.assertFalse(self.manifest["Valid"])
        self.assertEqual(self.manifest["GameExit"], 2)

    def test_orderly_exit_zero_with_only_old_footer_rejection(self):
        for failure in ("No complete runtime memory summary", "No complete map footer"):
            self.assertEqual(self.apply(manifest=dict(self.manifest, GameExit=0, Failure=failure)), 7200)
        with self.assertRaises(ValueError):
            self.apply(manifest=dict(self.manifest, GameExit=0, Failure="another validation failure"))

    def test_short_or_corrupt_run_cannot_use_adjustment(self):
        for summary in [dict(self.summary, measurement_seconds=7199), dict(self.summary, sample_count=3),
                        dict(self.summary, reason="diagnostic validation or write failed")]:
            with self.assertRaises(ValueError):
                self.apply(summary=summary)
        with self.assertRaises(ValueError):
            self.apply(points=[{"seconds": 0}, {"seconds": 7199}])
        with self.assertRaises(ValueError):
            self.apply(warnings=["Truncated final record"])

    def test_crash_environment_or_ownership_failure_still_rejected(self):
        for manifest in [dict(self.manifest, GameExit=-1), dict(self.manifest, VideoDecodeDetected=True),
                         dict(self.manifest, Failure="Packaged game exited 2; External full PCD validation failed")]:
            with self.assertRaises(ValueError):
                self.apply(manifest=manifest)
        with self.assertRaises(ValueError):
            self.apply(approval=dict(self.approval, OwnedPid=555))
        (self.directory / "cancel.request").write_text("environment failure")
        with self.assertRaises(ValueError):
            self.apply()

    def test_only_authorized_scene_and_endpoint(self):
        for approval in [dict(self.approval, TargetSeconds=3600), dict(self.approval, ApprovalId="other")]:
            with self.assertRaises(ValueError):
                self.apply(approval=approval)
        raw = self.directory.with_name("raw180-a1")
        self.directory.rename(raw)
        self.directory = raw
        with self.assertRaises(ValueError):
            self.apply()

    def test_same_post30_ols_and_eight_mib_limit(self):
        for slope, passed in [(7.9, True), (8.1, False)]:
            rows = [{"seconds": second, "commit_bytes": (100 + slope * second / 3600) * 1048576}
                    for second in range(0, 7201, 5)]
            result = evaluate(rows, 7200, complete=True)
            self.assertEqual(result["Pass"], passed)
            self.assertAlmostEqual(result["Post30"]["slope_per_hour"], slope)
            self.assertEqual(result["Post30"]["StartSeconds"], 1800)
            self.assertEqual(result["Post30"]["EndSeconds"], 7200)
            self.assertEqual(result["ThresholdMiBPerHour"], 8)
            self.assertIn("OldGate", result)


if __name__ == "__main__":
    unittest.main()
