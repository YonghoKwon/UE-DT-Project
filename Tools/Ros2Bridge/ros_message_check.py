"""ROS-only serialization checks; no broker or credential required."""
import argparse
import unittest
from unittest.mock import patch
from types import SimpleNamespace

import rclpy
from rclpy.serialization import serialize_message
from bridge_node import Bridge, Inbox


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


if __name__ == '__main__':
    unittest.main()
