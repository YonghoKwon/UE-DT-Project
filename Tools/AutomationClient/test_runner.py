"""Real subprocess tests for the wrapper contract, independent of an installed UE."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from run_job import run_job

FAKE_CHILD = r'''
import json,os,pathlib,sys,time
job_path=pathlib.Path(next(a.split('=',1)[1] for a in sys.argv if a.startswith('-MA0T10Job=')))
cancel=pathlib.Path(next(a.split('=',1)[1] for a in sys.argv if a.startswith('-MA0T10JobCancelFile=')))
job=json.loads(job_path.read_text());result=job_path.parent/job['result_path']
mode=next((a.split('=',1)[1] for a in sys.argv if a.startswith('--test-mode=')),os.environ.get('AUTO01_TEST_MODE','success'))
if mode=='missing':sys.exit(0)
if mode=='timeout':
 while not cancel.exists():time.sleep(.005)
 result.write_text(json.dumps({'schema':'ma0t10.job.result.v1','ok':False,'steps':[]}));sys.exit(0)
if mode=='malformed':result.write_text('not json');sys.exit(0)
r={'schema':'ma0t10.job.result.v1','ok':mode!='failed','steps':[{'op':'wait','ok':True}],'elapsed_sec':.01}
if mode=='schema':r['schema']='other'
if mode=='truthy':r['ok']='true'
result.write_text(json.dumps(r));sys.exit(7 if mode=='child_failed' else 0)
'''


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.child = self.root / "fake_child.py"; self.child.write_text(FAKE_CHILD)
        self.job = self.root / "job.json"
        self.job.write_text(json.dumps({"schema": "ma0t10.job.v1", "result_path": "result.json",
                                       "steps": [{"op": "wait", "seconds": .1}]}))
        self.report = self.root / "runner.json"

    def tearDown(self): self.tmp.cleanup()

    def execute(self, mode, **options):
        return run_job(Path(sys.executable), self.job,
                       ue_arguments=[str(self.child), "--test-mode=" + mode],
                       report_path=self.report, **options)

    def test_success_requires_child_and_result(self):
        self.assertEqual(self.execute("success"), 0)
        self.assertTrue(json.loads(self.report.read_text())["ok"])
        self.assertEqual(self.execute("child_failed"), 1)
        self.assertEqual(json.loads(self.report.read_text())["child_exit_code"], 7)

    def test_zero_child_exit_does_not_mask_failed_or_invalid_json(self):
        for mode in ("failed", "missing", "malformed", "schema", "truthy"):
            with self.subTest(mode=mode):
                self.assertEqual(self.execute(mode), 1)
                report = json.loads(self.report.read_text())
                self.assertEqual(report["child_exit_code"], 0)
                self.assertFalse(report["ok"])

    def test_stale_success_is_preserved_and_rejected(self):
        self.assertEqual(self.execute("success"), 0)
        self.assertEqual(self.execute("missing"), 1)
        report = json.loads(self.report.read_text())
        self.assertTrue(Path(report["previous_result"]).is_file())
        self.assertFalse((self.root / "result.json").exists())

    def test_timeout_requests_graceful_cancellation(self):
        self.assertEqual(self.execute("timeout", timeout=.5, shutdown_grace=3), 124)
        report = json.loads(self.report.read_text())
        self.assertEqual(report["outcome"], "timeout")
        self.assertEqual(report["child_exit_code"], 0)
        self.assertFalse(report["child_running"])
        self.assertFalse(list(self.root.glob("*.cancel-*")))

    def test_invalid_input_and_result_lock(self):
        lock = self.root / "result.json.automation.lock"; lock.write_text("existing-owner")
        self.assertEqual(self.execute("success"), 2)
        self.assertEqual(lock.read_text(), "existing-owner")
        self.assertFalse((self.root / "result.json").exists()); lock.unlink()
        self.job.write_text("[]")
        self.assertEqual(self.execute("success"), 2)

    @unittest.skipUnless(os.name == "nt", "PowerShell wrapper is a Windows example")
    def test_powershell_wrapper_has_same_failure_meaning(self):
        script = Path(__file__).with_name("run_job.ps1")
        env = os.environ.copy(); env["AUTO01_TEST_MODE"] = "failed"
        child = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script),
                                "-Executable", sys.executable, "-Job", str(self.job),
                                "-Report", str(self.report), "-UeArguments", str(self.child)],
                               env=env, capture_output=True, text=True)
        self.assertEqual(child.returncode, 1)
        self.assertEqual(json.loads(self.report.read_text())["child_exit_code"], 0)


if __name__ == "__main__": unittest.main()
