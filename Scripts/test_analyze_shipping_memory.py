import unittest
import datetime as dt
import json
import tempfile
from pathlib import Path

from analyze_shipping_memory import evaluate, analyze, MIB


def samples(ws_slope=0, commit_slope=0, step=0):
    return [dict(seconds=t, working_set_bytes=(1000 + ws_slope*t/3600 + (step if t >= 900 else 0))*MIB,
                 private_working_set_bytes=(800 + ws_slope*t/3600 + (step if t >= 900 else 0))*MIB,
                 commit_bytes=(2000 + commit_slope*t/3600 + (step if t >= 900 else 0))*MIB)
            for t in range(0, 3601, 5)]


class ShippingMemoryTests(unittest.TestCase):
    def test_both_windows_stable(self):
        self.assertTrue(evaluate(samples(2, 3), 3600)["Pass"])

    def test_flat_tail_cannot_hide_early_step(self):
        result = evaluate(samples(step=30), 3600)
        self.assertEqual(result["Verdict"], "FAIL")
        self.assertTrue(result["Metrics"]["Tail20"]["Commit"]["pass"])
        self.assertFalse(result["Metrics"]["PostWarmup10To60"]["Commit"]["pass"])

    def test_ws_alone_can_fail(self):
        self.assertFalse(evaluate(samples(9, 0), 3600)["Pass"])

    def test_commit_alone_can_fail(self):
        self.assertFalse(evaluate(samples(0, 9), 3600)["Pass"])

    def test_incomplete_run_never_passes(self):
        self.assertEqual(evaluate(samples(), 1200)["Verdict"], "INSUFFICIENT")

    def test_missing_pcd_proof_never_passes(self):
        self.assertFalse(evaluate(samples(), 3600, pcd_valid=False)["Valid"])

    def test_sampling_gap_never_passes(self):
        rows = [r for r in samples() if not 1800 < r["seconds"] < 1850]
        self.assertFalse(evaluate(rows, 3600)["Valid"])

    def test_duplicate_timestamp_never_passes(self):
        rows = samples(); rows.insert(20, rows[20])
        self.assertFalse(evaluate(rows, 3600)["Valid"])

    def test_structural_only_receiver_proof_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix='shipping-memory-test-') as directory:
            folder=Path(directory);start=dt.datetime(2026,1,1,tzinfo=dt.timezone.utc)
            curve=[dict(type='metadata',started_utc=start.isoformat(),requested_seconds=3600),
                   dict(type='summary',complete=True,measurement_seconds=3600)]
            (folder/'memory-trend.jsonl').write_text('\n'.join(map(json.dumps,curve)),encoding='utf-8')
            rows=[dict(r,utc=(start+dt.timedelta(seconds=r['seconds'])).isoformat(),pid=999,creation_filetime=123) for r in samples()]
            (folder/'os-counters.jsonl').write_text('\n'.join(map(json.dumps,rows)),encoding='utf-8')
            (folder/'owned-process.json').write_text(json.dumps({'Pid':999}),encoding='utf-8')
            (folder/'run-manifest.json').write_text(json.dumps({'Valid':True,'VideoDecodeDetected':False}),encoding='utf-8')
            proof=folder/'pcd-validation.json'
            proof.write_text(json.dumps({'Pass':True,'ValidatedFrames':1}),encoding='utf-8')
            self.assertFalse(analyze(folder)['Valid'])
            proof.write_text(json.dumps({'Pass':True,'ValidatedFrames':1,'SemanticValidatedFrames':1,'NativeRecordsEnabled':True}),encoding='utf-8')
            self.assertTrue(analyze(folder)['Pass'])


if __name__ == "__main__":
    unittest.main()
