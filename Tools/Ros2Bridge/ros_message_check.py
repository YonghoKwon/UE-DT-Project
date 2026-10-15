"""ROS-only serialization checks; no broker or credential required."""
import argparse
import unittest
from unittest.mock import patch
from types import SimpleNamespace

import rclpy
from rclpy.serialization import serialize_message
from bridge_node import Bridge, Inbox
import wire


class RosMessageTests(unittest.TestCase):
    def test_integral_json_pose_serializes_as_ros_float(self):
        rclpy.init()
        args = argparse.Namespace(qos_depth=5, reliable=False, queue_bytes=1024, metrics=None, topic=None)
        with patch.object(Bridge, 'connect_loop', lambda self: None):
            node = Bridge(args)
        try:
            msg = node.transform((1792000000,123456789), 'ma0t10_world', 'sensor', ([0,-2,3],[0,0,0,1]))
            self.assertIsInstance(msg.transform.translation.x, float)
            self.assertTrue(serialize_message(msg))
            optical = node.optical_frame((1792000000,0),'sensor')
            self.assertEqual(optical, 'sensor_optical')
            self.assertTrue(serialize_message(node.static_transforms[0]))
        finally:
            node.close()
            node.destroy_node()
            rclpy.shutdown()

    def test_bounded_queue_ack_and_unknown_schema_counters(self):
        rclpy.init()
        args = argparse.Namespace(qos_depth=5, reliable=False, queue_bytes=1024, metrics=None, topic=None)
        with patch.object(Bridge, 'connect_loop', lambda self: None):
            node = Bridge(args)
        acks = []
        owner = Inbox(node, SimpleNamespace(ack=lambda **kwargs: acks.append(kwargs['id'])))
        try:
            owner.on_message(SimpleNamespace(body=b'{}', headers={'schema':'unsupported.v9', 'ack':'one'}))
            node.drain()
            self.assertEqual(node.counts['unknown_schema'], 1)
            self.assertEqual(node.counts['published_frames'], 0)
            self.assertEqual(acks, ['one'])
            owner.on_message(SimpleNamespace(body=b'x'*1025, headers={'ack':'two'}))
            self.assertEqual(node.counts['queue_overflow'], 1)
            self.assertEqual(node.inbox_bytes, 0)
            self.assertEqual(acks, ['one','two'])
        finally:
            node.close()
            node.destroy_node()
            rclpy.shutdown()

    def test_camera_messages_serialize_without_invented_calibration(self):
        rclpy.init()
        args = argparse.Namespace(qos_depth=5, reliable=False, queue_bytes=1024, metrics=None, topic=None)
        with patch.object(Bridge, 'connect_loop', lambda self: None):
            node = Bridge(args)
        captured = {}
        # Synthetic marker frame + injected decoded bytes: tests ROS fields/serialization,
        # not JPEG codec correctness or actual UE Camera transport acceptance.
        data = b'\xff\xd8\xff\xc0\x00\x0b\x08\x00\x02\x00\x04\x01\x01\x11\x00\xff\xda\x00\x08\x01\x01\x00\x00\x3f\x00\x12\xff\x00\x34\xff\xd9'
        headers = {'schema':'virtual-camera.jpeg.v1','sensor-id':'cam','frame-id':'1',
                   'timestamp-utc':'2026-10-15T00:00:00Z','width':'4','height':'2'}
        try:
            with patch.object(wire, 'decode_rgb', lambda frame: bytes(24)), \
                 patch.object(node, 'publisher', lambda sid,suffix,cls: SimpleNamespace(publish=lambda msg:captured.update({suffix:msg}))):
                node.publish_camera(data,headers)
            self.assertEqual(set(captured), {'image','image/compressed','camera_info'})
            self.assertEqual(captured['image'].encoding,'rgb8')
            self.assertEqual(captured['image'].step,12)
            self.assertEqual(captured['camera_info'].k[0],0.)
            self.assertTrue(captured['image'].header.frame_id.endswith('_optical'))
            for msg in captured.values():
                self.assertTrue(serialize_message(msg))
        finally:
            node.close()
            node.destroy_node()
            rclpy.shutdown()


if __name__ == '__main__':
    unittest.main()
