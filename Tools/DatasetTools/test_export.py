"""Small generated fixtures exercise format contracts; actual RHI evidence is separate."""
import copy
import json
import math
from pathlib import Path
import struct
import tempfile
import unittest

import numpy as np
from PIL import Image
from pycocotools import mask as masks

from export_common import (Dataset, PALETTE, calibration, cloud, load_json, source_hash, write_json)
from export_coco import annotations, export_coco
from export_kitti import box_camera, export_kitti
from export_dataset import export
from score_dataset import PCD_RECORD
from validate_export import validate_coco, validate_kitti


def fixture(root, count=1, empty=False):
    root.mkdir()
    pose = {'xyz_m': [0, 0, 0], 'quaternion_xyzw': [0, 0, 0, 1]}
    manifest = {'schema_version': 'ma0t10.dataset.v1', 'schema_minor': 1, 'completed_scenes': count,
                'camera_truth': True, 'project_commit': 'synthetic-test-only',
                'classes': [{'id': k, 'name': v} for k, v in PALETTE.items()],
                'instances': [{'id': 1, 'actor_name': 'Slab', 'actor_class': 'SlabActor'}]}
    write_json(root/'dataset.json', manifest)
    for i in range(count):
        folder = root/f'scene_{i:05d}'
        folder.mkdir()
        semantic = np.zeros((48, 64), dtype='u1')
        if not empty:
            semantic[19:29, 27:37] = 2
        instance = np.zeros((48, 64, 3), dtype='u1')
        instance[:, :, 0] = semantic != 0
        Image.fromarray(semantic).save(folder/'seg.png')
        Image.fromarray(instance).save(folder/'instance.png')
        Image.new('RGB', (64, 48)).save(folder/'rgb.png')
        box = {'instance_id': 1, 'semantic_id': 2, 'center_m': [10, 0, 0], 'size_m': [4, 2, 2],
               'quaternion_xyzw': [0, 0, 0, 1], 'yaw_deg': 0,
               'coordinates': 'UE world meters; local XYZ dimensions; quaternion_xyzw'}
        label = {'schema_version': 'ma0t10.dataset.scene.v1', 'schema_minor': 1, 'scene_index': i,
                 'camera_resolution': [64, 48], 'camera_K': [40, 0, 31.5, 0, 40, 23.5, 0, 0, 1],
                 'camera_pose': pose, 'lidar_pose': pose, 'boxes_3d': [] if empty else [box]}
        write_json(folder/'label.json', label)
        points = [] if empty else [(8., y, z) for y in np.linspace(-.8, .8, 9) for z in np.linspace(-.8, .8, 9)]
        header = ('VERSION 0.7\nFIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence\n'
                  'SIZE 4 4 4 2 2 2 1 1 8 1 4\nTYPE F F F U U U U U I U F\nCOUNT 1 1 1 1 1 1 1 1 1 1 1\n'
                  f'WIDTH {len(points)}\nHEIGHT 1\nPOINTS {len(points)}\nDATA binary\n').encode()
        (folder/'lidar.pcd').write_bytes(header+b''.join(PCD_RECORD.pack(x,y,z,65535,0,0,0,1,0,1,1.) for x,y,z in points))
        (folder/'lidar_labels.bin').write_bytes(struct.pack('<BI', 2, 1)*len(points))


class ExportTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.base = Path(self.tmp.name)
        self.src = self.base/'source'
        fixture(self.src)
        self.scene = self.src/'scene_00000'

    def tearDown(self):
        self.tmp.cleanup()

    def change_label(self, **values):
        label = load_json(self.scene/'label.json')
        label.update(values)
        write_json(self.scene/'label.json', label)

    def test_frame_axes_and_translation(self):
        label = load_json(self.scene/'label.json')
        k, tr = calibration(label)
        np.testing.assert_allclose(tr[:, :3], [[0,-1,0], [0,0,-1], [1,0,0]])
        np.testing.assert_allclose(tr[:, :3] @ [10, 2, 3], [-2, -3, 10])
        label['lidar_pose']['xyz_m'] = [2, 3, 4]
        np.testing.assert_allclose(calibration(label)[1][:, 3], [3,-4,2])

    def test_rotated_extrinsics_known_90_degree_yaw(self):
        label = load_json(self.scene/'label.json')
        label['lidar_pose']['quaternion_xyzw'] = [0,0,math.sqrt(.5),math.sqrt(.5)]
        tr = calibration(label)[1]
        np.testing.assert_allclose(tr[:, :3] @ [10, 0, 0], [10,0,0], atol=1e-10)
        self.assertAlmostEqual(np.linalg.det(tr[:, :3]), 1)

    def test_intrinsics_and_nonfinite_quaternion_rejected(self):
        label = load_json(self.scene/'label.json')
        for bad in ([0,0,0,0], [0,0,0,float('nan')], [0,0,0,2]):
            label['camera_pose']['quaternion_xyzw'] = bad
            with self.assertRaises(ValueError): calibration(label)
        label = load_json(self.scene/'label.json'); label['camera_K'][0] = -1
        with self.assertRaises(ValueError): calibration(label)

    def test_exact_rle_holes_disconnected_and_multiple_classes(self):
        sem = np.ones((6,7), dtype='u1')*2
        ids = np.ones((6,7), dtype='u4')
        ids[1:5,2:5] = 0; ids[:,5] = 0; sem[5,0] = 3
        result = annotations(sem, ids, 1)
        self.assertEqual([a['category_id'] for a in result], [2,3])
        for a in result:
            decoded = masks.decode(a['segmentation'])
            np.testing.assert_array_equal(decoded, (ids == 1) & (sem == a['category_id']))
        self.assertEqual(sum(a['area'] for a in result), int((ids != 0).sum()))

    def test_coco_standard_reload_and_source_immutable(self):
        before = source_hash(self.src)
        r = export_coco(self.src, self.base/'coco')
        self.assertEqual(r['annotations'], 1)
        self.assertTrue(r['reload']['source_pixel_exact'])
        self.assertEqual(before, source_hash(self.src))
        doc = load_json(self.base/'coco/annotations/instances.json')
        self.assertEqual(doc['annotations'][0]['bbox'], [27,19,10,10])
        self.assertEqual({c['id']:c['name'] for c in doc['categories']}, {k:v for k,v in PALETTE.items() if k})

    def test_kitti_box_dimensions_bottom_center_and_yaw(self):
        label = load_json(self.scene/'label.json')
        box = box_camera(label['boxes_3d'][0], label)
        np.testing.assert_allclose(box['hwl'], [2,2,4])
        np.testing.assert_allclose(box['location'], [0,1,10], atol=1e-10)
        self.assertAlmostEqual(box['rotation_y'], -math.pi/2)
        self.assertAlmostEqual(box['volume_ratio'], 1)

    def test_tilt_requires_explicit_policy(self):
        label = load_json(self.scene/'label.json')
        label['boxes_3d'][0]['quaternion_xyzw'] = [math.sin(.1),0,0,math.cos(.1)]
        with self.assertRaisesRegex(ValueError, 'yaw-only'): box_camera(label['boxes_3d'][0], label)
        box = box_camera(label['boxes_3d'][0], label, True)
        self.assertGreater(box['tilt_degrees'], 10)
        self.assertGreater(box['volume_ratio'], 1)

    def test_kitti_projection_and_semantic_point_reload(self):
        r = export_kitti(self.src, self.base/'kitti', semantic_kitti=True)
        self.assertEqual(r['points'], 81)
        self.assertEqual(r['reload']['objects_without_projected_points'], 0)
        self.assertEqual(r['reload']['points_inside_bbox_fraction_1px']['min'], 1)
        self.assertGreater(r['reload']['point_bbox_iou']['min'], .7)
        points = np.fromfile(self.base/'kitti/training/velodyne/000000.bin', dtype='<f4').reshape(-1,4)
        np.testing.assert_array_equal(points[:,3], 1)
        self.assertEqual((self.base/'kitti/training/label_2/000000.txt').read_text().split()[0], 'Slab')

    def test_empty_dataset_and_empty_scene(self):
        for count in (0,1):
            root = self.base/f'empty{count}'; fixture(root, count, empty=True)
            r = export(root, self.base/f'out{count}', ('coco','kitti'))
            self.assertEqual(r['formats']['coco']['annotations'], 0)
            self.assertEqual(r['formats']['kitti']['points'], 0)

    def test_legacy_coco_supported_kitti_explicitly_rejected(self):
        self.change_label(schema_minor=0, boxes_3d=[])
        export_coco(self.src, self.base/'coco')
        with self.assertRaisesRegex(ValueError, 'legacy v1'): export_kitti(self.src, self.base/'bad')
        self.assertFalse((self.base/'bad').exists())
        self.assertFalse(list(self.base.glob('bad.pending-*')))

    def test_overlap_existing_output_and_invalid_format(self):
        for out in (self.src, self.src/'out', self.base):
            with self.assertRaises(ValueError): export(self.src, out)
        existing = self.base/'existing'; existing.mkdir(); (existing/'keep').write_text('keep')
        with self.assertRaises(ValueError): export(self.src, existing)
        self.assertEqual((existing/'keep').read_text(), 'keep')
        with self.assertRaises(ValueError): export(self.src, self.base/'out', ('bogus',))

    def test_unknown_class_instance_gt_off_and_future_schema(self):
        doc = load_json(self.src/'dataset.json')
        for key, value in [('schema_minor',2), ('camera_truth',False), ('report_sample_only',True),
                           ('classes',[{'id':2,'name':'Car'}])]:
            bad = copy.deepcopy(doc); bad[key] = value; write_json(self.src/'dataset.json', bad)
            with self.assertRaises((ValueError, KeyError)): export_coco(self.src, self.base/'bad')
        write_json(self.src/'dataset.json', doc)
        Image.new('L', (64,48), 99).save(self.scene/'seg.png')
        with self.assertRaises(ValueError): export_coco(self.src, self.base/'bad')

    def test_corrupt_point_count_and_image(self):
        p = self.scene/'lidar_labels.bin'; original = p.read_bytes(); p.write_bytes(original[:-1])
        with self.assertRaises(ValueError): cloud(self.scene)
        p.write_bytes(original)
        Image.new('RGB', (2,2)).save(self.scene/'rgb.png')
        with self.assertRaises(ValueError): Dataset(self.src).scene(0)

    def test_cancelled_export_leaves_no_output(self):
        marker = self.base/'cancel'; marker.write_text('cancel')
        with self.assertRaisesRegex(ValueError, 'cancelled'): export(self.src, self.base/'bad', cancel=marker)
        self.assertFalse((self.base/'bad').exists())
        self.assertFalse(list(self.base.glob('bad.pending-*')))

    def test_corrupted_export_rejected_on_reload(self):
        out = self.base/'out'; export_kitti(self.src, out)
        (out/'training/velodyne/000000.bin').write_bytes(b'broken')
        with self.assertRaises(ValueError): validate_kitti(out, self.src)
        coco = self.base/'coco'; export_coco(self.src, coco)
        doc = load_json(coco/'annotations/instances.json'); doc['annotations'][0]['area'] = 1
        write_json(coco/'annotations/instances.json', doc)
        with self.assertRaises(ValueError): validate_coco(coco, self.src)


if __name__ == '__main__': unittest.main()
