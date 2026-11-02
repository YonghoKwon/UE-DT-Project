import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import run_train_batch_recovery_probe as probe


class RecoveryProbeTests(unittest.TestCase):
    def test_fixture_uses_exact_utf8_hashes_and_twenty_hz_duration(self):
        with tempfile.TemporaryDirectory(prefix="train-batch-probe-") as directory:
            case = Path(directory)
            job_id, config = probe.fixture(case, 2, 4)
            manifest = probe.read(case / "input/job.json")
            self.assertEqual(manifest["job_id"], job_id)
            self.assertEqual(manifest["scenario_count"], 4)
            self.assertEqual(manifest["execution"], {"mode": "sampled_timeline", "sample_hz": 20})
            self.assertFalse(config["restore"])
            for descriptor in manifest["scenarios"]:
                raw = (case / "input" / descriptor["input_file"]).read_bytes()
                self.assertEqual(descriptor["input_bytes"], len(raw))
                self.assertEqual(descriptor["input_sha256"], hashlib.sha256(raw).hexdigest())
                self.assertEqual(json.loads(raw)["_meta"]["duration_sec"], descriptor["duration_sec"])

    def test_atomic_status_roundtrip_and_invalid_input(self):
        with tempfile.TemporaryDirectory(prefix="train-batch-status-") as directory:
            path = Path(directory) / "state.json"
            self.assertEqual(probe.read(path), {})
            probe.write(path, {"phase": "running", "stamp": "134380815887182186"})
            self.assertEqual(probe.read(path)["stamp"], "134380815887182186")
            self.assertFalse(path.with_suffix(".json.tmp").exists())
            path.write_text("{truncated", encoding="utf-8")
            self.assertEqual(probe.read(path), {})

    @unittest.skipUnless(sys.platform == "win32", "Windows creation-time ownership")
    def test_wrong_creation_stamp_never_terminates_owned_dummy(self):
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"], creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.OpenProcess.restype = wintypes.HANDLE
            kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
            kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
            kernel.CloseHandle.argtypes = [wintypes.HANDLE]
            handle = kernel.OpenProcess(0x1000, False, child.pid)
            self.assertTrue(handle)
            try:
                values = [wintypes.FILETIME() for _ in range(4)]
                self.assertTrue(kernel.GetProcessTimes(handle, *(ctypes.byref(v) for v in values)))
                stamp = values[0].dwLowDateTime | (values[0].dwHighDateTime << 32)
            finally:
                kernel.CloseHandle(handle)
            self.assertFalse(probe.terminate_recorded(child.pid, stamp + 1))
            self.assertIsNone(child.poll())
            self.assertTrue(probe.terminate_recorded(child.pid, stamp))
            child.wait(timeout=10)
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)


if __name__ == "__main__":
    unittest.main()
