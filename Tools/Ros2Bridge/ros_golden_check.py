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
from sensor_msgs.msg import PointCloud2
from tf2_msgs.msg import TFMessage
import stomp
import wire

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / 'Source/ma0t10_dt/MA0T10/Sensor/Tests/Fixtures/ArchitectureWire'


class RosGolden(unittest.TestCase):
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
