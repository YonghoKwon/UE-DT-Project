#!/usr/bin/env python3
"""Opt-in integration: real Artemis -> running bridge -> actual ROS messages.

Run in the disposable WSL with ROS sourced and credential environment inherited.
Synthetic golden evidence is separate from UE PIE acceptance.
"""
import json
import os
from pathlib import Path
import time
import unittest
import logging

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2, CompressedImage, CameraInfo
from std_msgs.msg import String
from tf2_msgs.msg import TFMessage
import stomp
import wire
from test_wire import headers as golden_headers

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / 'Source/ma0t10_dt/MA0T10/Sensor/Tests/Fixtures/ArchitectureWire'


class RosGolden(unittest.TestCase):
    def exchange(self, sid, outputs, body, headers):
        rclpy.init()
        node = Node('ma0t10_adapter_probe')
        received = {}
        subscriptions = [node.create_subscription(cls, '/ma0t10/'+wire.sensor_key(sid)+'/'+suffix,
                         lambda msg,k=suffix: received.update({k:msg}), qos_profile_sensor_data)
                         for suffix,cls in outputs.items()]
        conn = stomp.Connection12([(os.environ['MA0T10_STOMP_HOST'],61616)],auto_decode=False)
        headers = dict(headers, **{'destination-type':'MULTICAST','persistent':'false'})
        try:
            try:
                conn.connect(os.environ['MA0T10_STOMP_USER'],os.environ['MA0T10_STOMP_PASSWORD'],wait=True)
            except Exception:
                raise RuntimeError('Synthetic adapter connection failed; credential details suppressed') from None
            deadline, last = time.monotonic()+15, 0
            while time.monotonic() < deadline and set(received) != set(outputs):
                if time.monotonic()-last > .5:
                    conn.send('topic.virtual.sensor.camera.0' if 'image/compressed' in outputs else 'topic.virtual.sensor.lidar.0',body,headers=headers)
                    last = time.monotonic()
                rclpy.spin_once(node,timeout_sec=.05)
            self.assertEqual(set(received),set(outputs))
            return received
        finally:
            conn.disconnect()
            node.destroy_node()
            rclpy.shutdown()

    def test_actual_binary_camera_compressed_and_info(self):
        path = ROOT/'Saved/Reports/ROS2-01/camera-fixture.jpg'
        if not path.exists():
            self.skipTest('Generate an optional valid 4x2 JPEG in the report directory first')
        data = path.read_bytes()
        sid = 'ros2-synthetic-camera'
        h = {'schema':'virtual-camera.jpeg.v1','sensor-id':sid,'frame-id':'1','timestamp-utc':'2026-10-15T00:00:00.123456789Z',
             'width':'4','height':'2','content-type':'image/jpeg'}
        out = self.exchange(sid,{'image/compressed':CompressedImage,'camera_info':CameraInfo},data,h)
        self.assertTrue(bytes(out['image/compressed'].data) == data)
        self.assertEqual(out['image/compressed'].header.stamp.nanosec,123456789)
        self.assertEqual(out['camera_info'].k[0],0.)
        self.assertEqual((out['camera_info'].width,out['camera_info'].height),(4,2))

    def test_actual_telemetry_golden(self):
        data = (GOLDEN/'with-context.telemetry.json').read_bytes()
        h = golden_headers('with-context.telemetry.headers')
        out = self.exchange('sensor-fixed-007',{'telemetry':String},data,h)
        self.assertEqual(json.loads(out['telemetry'].data),json.loads(data))

    def test_actual_legacy_lidar_golden(self):
        body = (GOLDEN/'with-context.lidar.json').read_bytes()
        h = {k:v for k,v in golden_headers('with-context.pcd.headers').items() if k in ('sensor-id','frame-id','timestamp-utc')}
        h['schema'] = 'virtual-lidar.v1'
        out = self.exchange('sensor-fixed-007', {'points_legacy':PointCloud2}, body, h)
        self.assertEqual(out['points_legacy'].width,2)
        self.assertEqual(out['points_legacy'].point_step,12)
        self.assertEqual(out['points_legacy'].header.frame_id,'ma0t10_world')

    def test_actual_stomp_ros_cloud_and_tf(self):
        rclpy.init()
        node = Node('ma0t10_golden_probe')
        messages, transforms = [], []
        topic = '/ma0t10/' + wire.sensor_key('sensor-fixed-007') + '/points'
        sub = node.create_subscription(PointCloud2, topic, messages.append, qos_profile_sensor_data)
        tf_sub = node.create_subscription(TFMessage, '/tf', lambda msg: transforms.extend(msg.transforms), qos_profile_sensor_data)
        conn = stomp.Connection12([(os.environ['MA0T10_STOMP_HOST'], 61616)], auto_decode=False)
        body = (GOLDEN/'with-context.pcd').read_bytes()
        h = {'schema':wire.PCD_SCHEMA, 'destination-type':'MULTICAST', 'content-type':'application/vnd.pcd', 'persistent':'false'}
        try:
            conn.connect(os.environ['MA0T10_STOMP_USER'], os.environ['MA0T10_STOMP_PASSWORD'], wait=True)
            deadline, last_send = time.monotonic()+15, 0
            while time.monotonic() < deadline and (not messages or not transforms):
                if time.monotonic()-last_send > .5:
                    conn.send('topic.virtual.sensor.export.0', body, headers=h)
                    last_send = time.monotonic()
                rclpy.spin_once(node, timeout_sec=.05)
            self.assertTrue(messages, 'No PointCloud2 from actual Artemis')
            self.assertTrue(transforms, 'No acquisition TF')
            msg = messages[-1]
            self.assertEqual((msg.width,msg.height,msg.point_step,msg.row_step), (2,1,33,66))
            self.assertEqual(bytes(msg.data), body.split(b'DATA binary\n')[1])
            self.assertEqual(msg.header.frame_id, 'ma0t10/'+wire.sensor_key('sensor-fixed-007'))
            self.assertEqual(msg.header.stamp.nanosec, 456000000)
            t = next(t for t in transforms if t.child_frame_id == msg.header.frame_id)
            self.assertEqual(t.header.frame_id, 'ma0t10_world')
            self.assertAlmostEqual(t.transform.translation.y, 4.56)
            self.assertAlmostEqual(t.transform.rotation.w, 1.)
        finally:
            conn.disconnect()
            node.destroy_node()
            rclpy.shutdown()


if __name__ == '__main__':
    logging.getLogger('stomp').addHandler(logging.NullHandler())
    logging.getLogger('stomp').propagate = False
    unittest.main()
