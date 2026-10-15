"""python -m unittest discover -s Tools/Ros2Bridge -p 'test_*.py' -v"""
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import unittest
from unittest.mock import patch
import io

import wire

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / 'Source/ma0t10_dt/MA0T10/Sensor/Tests/Fixtures/ArchitectureWire'


def headers(name):
    out = {}
    for line in (GOLDEN / name).read_text().splitlines()[1:]:
        if ':' in line:
            k, v = line.split(':', 1)
            out[k] = v.replace('\\c', ':').replace('\\n', '\n').replace('\\r', '\r').replace('\\\\', '\\')
    return out


class WireTests(unittest.TestCase):
    def setUp(self):
        self.body = (GOLDEN/'with-context.pcd').read_bytes()
        self.h = headers('with-context.pcd.headers')

    def test_both_golden_clouds_exact_layout(self):
        for prefix in ('with-context', 'without-context'):
            body = (GOLDEN/f'{prefix}.pcd').read_bytes()
            cloud = wire.pcd(body, headers(f'{prefix}.pcd.headers'))
            self.assertEqual(cloud.count, 2)
            self.assertEqual(cloud.point_step, 33)
            self.assertEqual(cloud.data, body.split(b'DATA binary\n', 1)[1])
            self.assertEqual(struct.unpack_from('<fff', cloud.data), (1.25, -2.5, .75))
            self.assertEqual(struct.unpack_from('<H', cloud.data, 12)[0], 12345)
            self.assertEqual(dict((n, o) for n, o, _ in cloud.fields)['confidence'], 29)

    def test_stdlib_fallback_same_as_numpy(self):
        normal = wire.pcd(self.body, self.h)
        with patch.object(wire, 'np', None):
            fallback = wire.pcd(self.body, self.h)
        self.assertEqual(normal, fallback)

    def test_int64_offset_preserved_without_float_rounding(self):
        cloud = wire.pcd(self.body, self.h)
        lo, hi = struct.unpack_from('<Ii', cloud.data, 20)
        self.assertEqual((hi << 32) | lo, 1234567890123)
        data = bytearray(cloud.data)
        struct.pack_into('<q', data, 20, -1234567890123)
        lo, hi = struct.unpack_from('<Ii', data, 20)
        self.assertEqual((hi << 32) | lo, -1234567890123)

    def test_golden_tf_reflects_world_only(self):
        cloud = wire.pcd(self.body, self.h)
        t, q = cloud.pose
        for actual, expected in zip(t, [1.23, 4.56, 7.89]):
            self.assertAlmostEqual(actual, expected)
        self.assertEqual(q, [0., 0., 0., 1.])
        # ROS world = S * T_wire * local, not a second reflection of local Y.
        local = struct.unpack_from('<fff', cloud.data)
        self.assertEqual(local[1], -2.5)
        world = [t[i]+local[i] for i in range(3)]
        self.assertAlmostEqual(world[1], 2.06)

    def test_zero_points(self):
        header = self.body.split(b'DATA binary\n')[0].replace(b'WIDTH 2', b'WIDTH 0').replace(b'POINTS 2', b'POINTS 0')
        body = header+b'DATA binary\n'
        h = {k: v for k, v in self.h.items() if k not in ('checksum','x-checksum-sha1','content-length','point-count')}
        self.assertEqual(wire.pcd(body, h).count, 0)

    def test_no_snapshot_does_not_invent_tf(self):
        lines = self.body.split(b'\n')
        body = b'\n'.join(x for x in lines if not x.startswith(b'# MA0T10_META '))
        h = {k: v for k, v in self.h.items() if k not in ('checksum','x-checksum-sha1','content-length')}
        self.assertIsNone(wire.pcd(body, h).pose)

    def test_corrupted_checksum(self):
        with self.assertRaises(wire.WireError):
            wire.pcd(self.body[:-1]+bytes([self.body[-1]^1]), self.h)

    def test_truncated_body(self):
        h = {k: v for k, v in self.h.items() if k not in ('checksum','x-checksum-sha1','content-length')}
        with self.assertRaises(wire.WireError):
            wire.pcd(self.body[:-1], h)

    def test_unknown_version_and_layout(self):
        for h in [dict(self.h, schema='virtual-pointcloud.pcd.v2'), dict(self.h, **{'frame-id': '42'})]:
            with self.assertRaises(wire.WireError):
                wire.pcd(self.body, h)
        h = {k: v for k, v in self.h.items() if k not in ('checksum','x-checksum-sha1','content-length')}
        for old, new in [(b'virtual-pointcloud.context.v1', b'virtual-pointcloud.context.v2'), (b'SIZE 4 4 4',b'SIZE 8 4 4')]:
            with self.assertRaises(wire.WireError):
                wire.pcd(self.body.replace(old, new), h)

    def test_nonfinite_xyz(self):
        body = bytearray(self.body)
        struct.pack_into('<f', body, len(body)-66, math.nan)
        h = {k: v for k, v in self.h.items() if k not in ('checksum','x-checksum-sha1')}
        with self.assertRaises(wire.WireError):
            wire.pcd(bytes(body), h)

    def test_nonidentity_rotation(self):
        # UE yaw +90 = ROS yaw -90, with local wire Y reflection already in T.
        m = {'coordinate_frame':'sensor_local','coordinate_units':'m','coordinate_axes':'x_forward_y_left_z_up',
             'world_coordinate_axes':'ue_x_forward_y_right_z_up',
             'transform_convention':'row_major_4x4_column_vector_local_m_to_ue_world_m',
             'sensor_to_world_m':[0,1,0,1,1,0,0,2,0,0,1,3,0,0,0,1]}
        t, q = wire.pcd_pose(m)
        self.assertEqual(t, [1,-2,3])
        self.assertAlmostEqual(q[2], -math.sqrt(.5))
        self.assertAlmostEqual(q[3], math.sqrt(.5))

    def test_reflection_not_a_ros_rotation(self):
        with self.assertRaises(wire.WireError):
            wire.quaternion([[1,0,0],[0,-1,0],[0,0,1]])

    def test_180_degree_quaternion(self):
        self.assertEqual(wire.quaternion([[-1,0,0],[0,1,0],[0,0,-1]]), [0.,1.,0.,0.])

    def test_legacy_fixture_cm_world_conversion(self):
        body = (ROOT/'Samples/payload_fixtures/virtual_lidar_v1_sample.json').read_bytes()
        cloud = wire.lidar_json(body, {})
        self.assertEqual(cloud.count, 4)
        self.assertTrue(cloud.world)
        xyz = struct.unpack_from('<fff', cloud.data)
        for actual, expected in zip(xyz, [9,2.6,0]):
            self.assertAlmostEqual(actual, expected, places=5)

    def test_legacy_miss_is_not_a_hit(self):
        obj = json.loads((ROOT/'Samples/payload_fixtures/virtual_lidar_v1_sample.json').read_bytes())
        obj['points'][0]['hit'] = False
        self.assertEqual(wire.lidar_json(json.dumps(obj).encode(), {}).count, 3)

    def test_camera_fixture_metadata_only(self):
        # Existing contract fixture is a 13-byte JPEG prefix, not a decodable image.
        body = (ROOT/'Samples/payload_fixtures/virtual_camera_v1_sample.json').read_bytes()
        c = wire.camera(body, {})
        self.assertEqual((c['width'],c['height']), (640,360))
        self.assertEqual(c['pose'][0], [0.,4.5,3.5])
        self.assertEqual(c['fov'], [90.,58.7])

    def test_binary_camera_no_invented_pose_intrinsics(self):
        h = headers('without-context.camera.headers')
        # Golden only freezes the JPEG envelope; bytes are deliberately a short fake.
        body = bytes([0xff,0xd8,0,1,0xff,0xd9])
        h = {k:v for k,v in h.items() if k not in ('checksum','content-length')}
        c = wire.camera(body, h)
        self.assertIsNone(c['pose'])
        self.assertIsNone(c['fov'])

    def test_camera_fake_jpeg_cannot_decode(self):
        try:
            import PIL
        except ImportError:
            self.skipTest('Optional Pillow decoder is not installed')
        c = wire.camera((ROOT/'Samples/payload_fixtures/virtual_camera_v1_sample.json').read_bytes(), {})
        with self.assertRaises(Exception):
            wire.decode_rgb(c)

    def test_marker_validation_rejects_contract_fake_jpeg(self):
        body = (ROOT/'Samples/payload_fixtures/virtual_camera_v1_sample.json').read_bytes()
        with self.assertRaises(wire.WireError):
            wire.jpeg_dimensions(wire.camera(body,{})['jpeg'])

    def test_jpeg_marker_dimensions_and_truncation(self):
        data = b'\xff\xd8\xff\xc0\x00\x0b\x08\x00\x02\x00\x04\x01\x01\x11\x00\xff\xda\x00\x08\x01\x01\x00\x00\x3f\x00\x12\xff\x00\x34\xff\xd9'
        self.assertEqual(wire.jpeg_dimensions(data),(4,2))
        for bad in [data[:-1], data[:6], data+b'bad']:
            with self.assertRaises(wire.WireError):
                wire.jpeg_dimensions(bad)

    def test_valid_jpeg_rgb_decode_if_available(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest('Optional Pillow decoder is not installed')
        buf = io.BytesIO()
        Image.new('RGB',(4,2),(120,80,40)).save(buf,format='JPEG')
        data = buf.getvalue()
        self.assertEqual(wire.jpeg_dimensions(data),(4,2))
        c = wire.camera(data, {'schema':'virtual-camera.jpeg.v1','sensor-id':'cam', 'frame-id':'1',
                              'timestamp-utc':'2026-10-15T00:00:00Z','width':'4','height':'2'})
        self.assertEqual(len(wire.decode_rgb(c)),24)

    def test_timestamp_nanoseconds_and_large_frame_identity(self):
        self.assertEqual(wire.stamp('2026-10-12T01:02:03.123456789Z')[1], 123456789)
        self.assertEqual(wire.pcd(self.body, self.h).frame, 9007199254740993)
        for value in ['2026-10-12T01:02:03+09:00','garbage','2026-99-12T01:02:03Z']:
            with self.assertRaises(wire.WireError):
                wire.stamp(value)

    def test_sensor_mapping_is_injective_and_legal(self):
        keys = [wire.sensor_key(x) for x in ['a-b','a_b','A','a','1/a','센서']]
        self.assertEqual(len(set(keys)), len(keys))
        for key in keys:
            self.assertRegex(key, r'^s_[0-9a-f]+$')

    def test_oversized_header_and_unknown_json(self):
        h = {'schema': wire.PCD_SCHEMA}
        with self.assertRaises(wire.WireError):
            wire.pcd(b'#'+b'a'*65536+b'\n', h)
        with self.assertRaises(wire.WireError):
            wire.lidar_json(b'{"schemaVersion":"virtual-lidar.v2"}', {})


if __name__ == '__main__':
    unittest.main()
