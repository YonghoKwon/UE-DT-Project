#!/usr/bin/env python3
"""Opt-in ROS2 -> AUTO-01 services/topics. Run separately from STOMP conversion."""
import argparse
from collections import OrderedDict
import hashlib
import json
import os
import uuid

import rclpy
from rclpy.clock import Clock, ClockType
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
from std_msgs.msg import String
from std_srvs.srv import Trigger, SetBool

from control import ApiClient, ControlError, parse_request

PREFIX = '/ma0t10/control/'


class ControlNode(Node):
    def __init__(self, client, lidar, capture_sensor):
        super().__init__('ma0t10_ros2_control')
        self.client, self.lidar, self.capture_sensor = client, lidar, capture_sensor
        self.cache = OrderedDict()
        self.result = self.create_publisher(String, PREFIX + 'result', 16)
        self.status = self.create_publisher(String, PREFIX + 'status', QoSProfile(
            depth=1, reliability=ReliabilityPolicy.RELIABLE, durability=DurabilityPolicy.TRANSIENT_LOCAL))
        self.create_subscription(String, PREFIX + 'request', self.request, 8)
        self.create_service(SetBool, PREFIX + 'pcd', self.pcd)
        self.create_service(Trigger, PREFIX + 'capture', self.capture)
        self.create_service(Trigger, PREFIX + 'get_status', self.get_status)
        # Polling/HTTP timeouts use wall/steady time even while consumers use /clock.
        self.steady = Clock(clock_type=ClockType.STEADY_TIME)
        self.create_timer(.5, self.poll, clock=self.steady)

    def execute(self, request_id, op, params):
        digest = hashlib.sha256(json.dumps([op, params],sort_keys=True).encode('utf8')).digest()
        if request_id in self.cache:
            previous_digest, feedback = self.cache[request_id]
            if digest != previous_digest:
                feedback = {'schema': 'ma0t10.ros2.result.v1', 'source': 'ros2_bridge',
                            'request_id': request_id, 'op': op, 'ok': False, 'error': 'request_id_conflict'}
        else:
            feedback = {'schema': 'ma0t10.ros2.result.v1', 'source': 'ros2_bridge',
                        'request_id': request_id, 'op': op}
            try:
                feedback.update(ok=True, api=self.client.call(op, params))
            except ControlError as exc:
                feedback.update(ok=False, error=str(exc))
            self.cache[request_id] = (digest, feedback)
            if len(self.cache) > 256:
                self.cache.popitem(last=False)
        self.result.publish(String(data=json.dumps(feedback, separators=(',', ':'))))
        return feedback

    def request(self, msg):
        try:
            obj = parse_request(msg.data)
        except ControlError as exc:
            self.result.publish(String(data=json.dumps({'schema': 'ma0t10.ros2.result.v1',
                                'source': 'ros2_bridge', 'ok': False, 'error': str(exc)})))
            return
        self.execute(obj['request_id'], obj['op'], obj['params'])

    def reply(self, response, op, params):
        result = self.execute(uuid.uuid4().hex, op, params)
        response.success, response.message = result['ok'], json.dumps(result, separators=(',', ':'))
        return response

    def pcd(self, request, response):
        return self.reply(response, 'pcd.start' if request.data else 'pcd.stop',
                          {'sensor_id': self.lidar} if request.data else {})

    def capture(self, request, response):
        return self.reply(response, 'capture', {'sensor_id': self.capture_sensor})

    def get_status(self, request, response):
        return self.reply(response, 'status', {})

    def poll(self):
        feedback = {'schema': 'ma0t10.ros2.status.v1', 'source': 'ros2_bridge'}
        try:
            feedback.update(ok=True, api=self.client.call('status', {}))
        except ControlError as exc:
            feedback.update(ok=False, error=str(exc))
        self.status.publish(String(data=json.dumps(feedback, separators=(',', ':'))))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--api-host', required=True, help='WSL NAT gateway / Windows WSL vEthernet IPv4')
    p.add_argument('--relay-port', type=int, default=18099)
    p.add_argument('--api-port', type=int, default=8099)
    p.add_argument('--lidar', required=True)
    p.add_argument('--capture-sensor', required=True)
    args = p.parse_args()
    try:
        client = ApiClient(args.api_host, args.relay_port, args.api_port,
                           os.environ.get('MA0T10_AUTOMATION_TOKEN', ''))
    except ValueError:
        p.error('Invalid endpoint or missing token (values never printed)')
    rclpy.init(args=[])
    node = ControlNode(client, args.lidar, args.capture_sensor)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, rclpy.executors.ExternalShutdownException):
        pass
    finally:
        # Stopping the bridge does not silently cancel UE work. Explicit stop commands own it.
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
