import json
import pathlib
import tempfile
import unittest

from score_dataset import LABEL_RECORD, PCD_RECORD, FIELDS, axis_error, percentile, score


class DatasetTests(unittest.TestCase):
    def test_axis_boundary_and_half_turn(self):
        self.assertEqual(axis_error(89, -89), 2)
        self.assertEqual(axis_error(180, 0), 0)
        self.assertEqual(axis_error(-179, 179), 2)
        with self.assertRaises(ValueError):
            axis_error(float("nan"), 0)

    def test_p95_is_interpolated(self):
        self.assertAlmostEqual(percentile([0, 20, 10], .95), 19)
        self.assertEqual(percentile([4], .95), 4)

    def fixture(self, root):
        manifest = {"schema_version": "ma0t10.dataset.v1", "completed_scenes": 2}
        (root / "dataset.json").write_text(json.dumps(manifest))
        for i in range(2):
            scene = root / f"scene_{i:05d}"
            scene.mkdir()
            label = {"schema_version": "ma0t10.dataset.scene.v1", "scene_index": i,
                     "analysis_yaw_deg": 89, "analysis_valid": i == 0,
                     "slab_pose": {"pitch_yaw_roll_deg": [0, -89, 0]}}
            (scene / "label.json").write_text(json.dumps(label))
            header = (f"# .PCD v0.7\nVERSION 0.7\nFIELDS {FIELDS}\n"
                      "SIZE 4 4 4 2 2 2 1 1 8 1 4\nTYPE F F F U U U U U I U F\n"
                      "COUNT 1 1 1 1 1 1 1 1 1 1 1\nWIDTH 1\nHEIGHT 1\nPOINTS 1\nDATA binary\n")
            (scene / "lidar.pcd").write_bytes(header.encode() + PCD_RECORD.pack(1, 2, 3, 100, 0, 0, 0, 1, 0, 0, 1))
            (scene / "lidar_labels.bin").write_bytes(LABEL_RECORD.pack(2, 1))

    def test_invalid_analysis_is_counted_and_not_scored(self):
        with tempfile.TemporaryDirectory(dir=pathlib.Path(__file__).resolve().parents[2] / "Saved/Reports/DATASET-01") as tmp:
            root = pathlib.Path(tmp)
            self.fixture(root)
            result = score(root)
            self.assertEqual(result["completed_scenes"], 2)
            self.assertEqual(result["invalid_analysis_scene_indices"], [1])
            self.assertEqual(result["mean_degrees"], 2)
            self.assertEqual(result["total_pcd_points"], 2)

    def test_corrupt_labels_and_truncated_pcd_fail(self):
        with tempfile.TemporaryDirectory(dir=pathlib.Path(__file__).resolve().parents[2] / "Saved/Reports/DATASET-01") as tmp:
            root = pathlib.Path(tmp)
            self.fixture(root)
            scene = root / "scene_00000"
            (scene / "lidar_labels.bin").write_bytes(b"")
            with self.assertRaisesRegex(ValueError, "count mismatch"):
                score(root)
            (scene / "lidar_labels.bin").write_bytes(LABEL_RECORD.pack(2, 1))
            data = (scene / "lidar.pcd").read_bytes()
            (scene / "lidar.pcd").write_bytes(data[:-1])
            with self.assertRaisesRegex(ValueError, "truncated"):
                score(root)


if __name__ == "__main__":
    unittest.main()
