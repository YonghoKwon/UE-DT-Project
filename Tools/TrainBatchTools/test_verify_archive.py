import hashlib
import io
import os
import tarfile
import tempfile
import unittest
import zipfile

import verify_archive as va


class VerifyArchiveTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.files = {"job-1/a.json": b"{}", "job-1/scenes/b.pcd": os.urandom(4096), "job-1/empty.bin": b""}
        self.sums = os.path.join(self.dir.name, "checksums.sha256")
        with open(self.sums, "w", encoding="utf-8", newline="\n") as f:
            for name, data in sorted(self.files.items()):
                f.write(f"{hashlib.sha256(data).hexdigest()}  {name}\n")

    def tearDown(self):
        self.dir.cleanup()

    def make_zip(self, extra=None):
        path = os.path.join(self.dir.name, "x.zip")
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
            for name, data in {**self.files, **(extra or {})}.items():
                z.writestr(name, data)
        return path

    def make_targz(self, files):
        path = os.path.join(self.dir.name, "x.tar.gz")
        with tarfile.open(path, "w:gz") as t:
            for name, data in files.items():
                info = tarfile.TarInfo(name)
                info.size = len(data)
                t.addfile(info, io.BytesIO(data))
        return path

    def test_zip_ok(self):
        self.assertEqual(va.verify(self.make_zip(), self.sums, ""), [])

    def test_targz_ok(self):
        self.assertEqual(va.verify(self.make_targz(self.files), self.sums, ""), [])

    def test_mismatch_missing_extra_and_escape(self):
        changed = dict(self.files)
        changed["job-1/a.json"] = b"[]"
        del changed["job-1/empty.bin"]
        changed["../evil"] = b"x"
        errors = va.verify(self.make_targz(changed), self.sums, "")
        self.assertTrue(any(e.startswith("sha256 mismatch") for e in errors))
        self.assertTrue(any(e.startswith("missing") for e in errors))
        self.assertTrue(any("unsafe entry name" in e for e in errors))
        self.assertTrue(any(e.startswith("unexpected entry") for e in errors))

    def test_prefix_applied(self):
        with open(self.sums, "w", encoding="utf-8", newline="\n") as f:
            for name, data in sorted(self.files.items()):
                f.write(f"{hashlib.sha256(data).hexdigest()}  {name[len('job-1/'):]}\n")
        self.assertEqual(va.verify(self.make_zip(), self.sums, "job-1/"), [])


if __name__ == "__main__":
    unittest.main()
