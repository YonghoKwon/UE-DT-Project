"""Binary-pair/positive-fixture regression for the package semantic auditor."""
import json
from pathlib import Path
import shutil
import struct
import tempfile
import unittest

from audit_lidar_semantics import audit


class SemanticAuditTests(unittest.TestCase):
    def setUp(self):
        self.base = Path(__file__).resolve().parents[1] / 'Saved/Reports/ANALYSIS-24-22/serial/audit-fixtures'
        self.base.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(dir=self.base))
        self.assertTrue(self.root.resolve().is_relative_to(self.base.resolve()))
        self.manifest = {'requested_scenes': 1, 'completed_scenes': 1,
                         'project_commit': 'fixture',
                         'classes': [{'id': 1, 'name': 'Other'}, {'id': 2, 'name': 'Slab'}],
                         'instances': [{'id': 19, 'actor_name': 'FixtureSlab'}]}
        self.scene('scene_00000', [(2, 19), (1, 0)])
        self.save_manifest()

    def tearDown(self):
        # Check the exact absolute recursive-delete target before deleting owned fixtures.
        if not self.root.resolve().is_relative_to(self.base.resolve()):
            raise RuntimeError('fixture cleanup escaped its named report directory')
        shutil.rmtree(self.root)

    def save_manifest(self):
        (self.root / 'dataset.json').write_text(json.dumps(self.manifest), encoding='utf-8')

    def scene(self, name, pairs, backend=3):
        folder = self.root / name
        folder.mkdir(exist_ok=True)
        (folder / 'label.json').write_text(json.dumps({'lidar_backend': backend}), encoding='utf-8')
        (folder / 'lidar.pcd').write_bytes(
            f'POINTS {len(pairs)}\nDATA binary\n'.encode() + bytes(33 * len(pairs)))
        (folder / 'lidar_labels.bin').write_bytes(b''.join(struct.pack('<BI', *pair) for pair in pairs))

    def test_named_slab_and_little_endian_instance(self):
        result = audit(self.root, expected_backend=3, require_slab=True)
        self.assertTrue(result['Valid'])
        self.assertEqual(result['named_slab_points'], 1)
        self.assertEqual(result['scenes'][0]['instances'], {19: 1, 0: 1})
        self.assertEqual(result['classes']['1']['name'], 'Other')

    def test_all_other_is_structurally_valid_but_fails_positive_acceptance(self):
        self.scene('scene_00000', [(1, 0), (1, 0)])
        self.assertTrue(audit(self.root, 3, False)['Valid'])
        result = audit(self.root, 3, True)
        self.assertFalse(result['Valid'])
        self.assertEqual(result['instance_zero'], 2)

    def test_depth_backend_does_not_count_as_hwrt_acceptance(self):
        self.scene('scene_00000', [(2, 19)], backend=2)
        self.assertFalse(audit(self.root, 3, True)['Valid'])
        self.assertTrue(audit(self.root, 2, True)['Valid'])

    def test_truncated_label_and_unknown_instance_rejected(self):
        folder = self.root / 'scene_00000'
        (folder / 'lidar_labels.bin').write_bytes(bytes(9))
        with self.assertRaisesRegex(ValueError, 'cardinality'):
            audit(self.root)
        self.scene('scene_00000', [(2, 12345)])
        with self.assertRaisesRegex(ValueError, 'absent'):
            audit(self.root)

    def test_one_positive_scene_cannot_hide_an_all_other_scene(self):
        self.scene('scene_00001', [(1, 0)])
        self.manifest['requested_scenes'] = self.manifest['completed_scenes'] = 2
        self.save_manifest()
        self.assertFalse(audit(self.root, 3, True)['Valid'])


if __name__ == '__main__':
    unittest.main()
