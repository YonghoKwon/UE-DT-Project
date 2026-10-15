#!/usr/bin/env python3
"""Independent ROS subscriber: rate/bandwidth/UTC latency. Never stores point/image bodies."""
import argparse
from collections import defaultdict, deque
import json
from pathlib import Path
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2, Image, CameraInfo
from tf2_msgs.msg import TFMessage


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--seconds', type=float, default=300)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    rclpy.init()
    node = Node('ma0t10_ros_probe')
    subscriptions, streams = {}, {}
    started = time.monotonic()

    def receive(topic, msg):
        now = time.time_ns()
        if topic not in streams:
            streams[topic] = {'frames': 0, 'bytes': 0, 'first_ns': now, 'last_ns': now,
                              'frame_id': msg.header.frame_id, 'samples': deque(maxlen=12000),
                              'stamps': set(), 'duplicate_stamps': 0}
        s = streams[topic]
        stamp = msg.header.stamp.sec*10**9 + msg.header.stamp.nanosec
        if stamp in s['stamps']:
            s['duplicate_stamps'] += 1
        if len(s['stamps']) < 20000:
            s['stamps'].add(stamp)
        s['frames'] += 1
        s['bytes'] += len(msg.data) if hasattr(msg, 'data') else 0
        s['samples'].append({'latency_ms': (now-stamp)/1e6, 'gap_ms': (now-s['last_ns'])/1e6})
        s['last_ns'] = now
        if isinstance(msg, PointCloud2):
            s['last_points'], s['point_step'], s['row_step'] = msg.width*msg.height, msg.point_step, msg.row_step
            s['fields'] = [(f.name, f.offset, f.datatype) for f in msg.fields]
            s['invalid_layout'] = s.get('invalid_layout', 0) + int(len(msg.data) != msg.row_step*msg.height)

    try:
        while time.monotonic()-started < args.seconds:
            for topic, types in node.get_topic_names_and_types():
                cls = {'sensor_msgs/msg/PointCloud2':PointCloud2, 'sensor_msgs/msg/Image':Image,
                       'sensor_msgs/msg/CameraInfo':CameraInfo}.get(types[0])
                if cls and topic.startswith('/ma0t10/') and topic not in subscriptions:
                    subscriptions[topic] = node.create_subscription(cls, topic, lambda m, t=topic: receive(t,m), qos_profile_sensor_data)
            rclpy.spin_once(node, timeout_sec=.02)
    except KeyboardInterrupt:
        pass
    finally:
        out = {'elapsed_sec': time.monotonic()-started, 'streams':{}}
        for topic, s in streams.items():
            samples = s.pop('samples')
            s.pop('stamps')
            duration = (s['last_ns']-s['first_ns'])/1e9
            s['hz'] = (s['frames']-1)/duration if duration else 0
            s['payload_bytes_per_sec'] = s['bytes']/duration if duration else 0
            for field in ('latency_ms','gap_ms'):
                values = sorted(x[field] for x in samples)
                s[field] = {'mean':sum(values)/len(values), 'p95':values[min(len(values)-1,int(len(values)*.95))], 'max':values[-1]}
            out['streams'][topic] = s
        args.output.write_text(json.dumps(out,indent=2))
        print(json.dumps({'ros_streams':len(streams),'frames':sum(s['frames'] for s in streams.values())}))
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
