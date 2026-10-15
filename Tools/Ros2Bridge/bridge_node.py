#!/usr/bin/env python3
"""Artemis STOMP -> ROS2. Credentials only from inherited process environment."""
import argparse
from collections import Counter, deque
import json
import math
import os
from pathlib import Path
import queue
import signal
import threading
import time

import rclpy
from rclpy.node import Node
from rclpy.clock import Clock, ClockType
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy
from sensor_msgs.msg import PointCloud2, PointField, Image, CompressedImage, CameraInfo, Imu
from geometry_msgs.msg import TransformStamped
from std_msgs.msg import String
from rosgraph_msgs.msg import Clock as ClockMessage
from tf2_ros import TransformBroadcaster, StaticTransformBroadcaster
import stomp

import wire
import imu
from control import AcquisitionClock

DEFAULT_TOPICS = ('topic.virtual.sensor.export.0', 'topic.virtual.sensor.lidar.0', 'topic.virtual.sensor.camera.0')


class Inbox(stomp.ConnectionListener):
    def __init__(self, node, connection):
        self.node = node
        self.connection = connection

    def acknowledge(self, headers):
        try:
            self.connection.ack(id=headers['ack'])
            self.node.counts['stomp_acked'] += 1
        except Exception:
            self.node.counts['ack_error'] += 1

    def on_message(self, frame):
        n = self.node
        body = frame.body
        if not isinstance(body, bytes) or len(body) > wire.MAX_BODY:
            n.counts['body_rejected'] += 1
            self.acknowledge(frame.headers)
            return
        # Reserve bytes before enqueuing. Drop newest explicitly at the bounded limit.
        with n.inbox_lock:
            if n.inbox_bytes + len(body) > n.args.queue_bytes:
                n.counts['queue_overflow'] += 1
                self.acknowledge(frame.headers)
                return
            n.inbox_bytes += len(body)
        try:
            n.inbox.put_nowait((dict(frame.headers), body, time.time_ns(), self))
            n.counts['stomp_received'] += 1
            n.counts['stomp_bytes'] += len(body)
        except queue.Full:
            with n.inbox_lock:
                n.inbox_bytes -= len(body)
            n.counts['queue_overflow'] += 1
            self.acknowledge(frame.headers)

    def on_error(self, _frame):
        self.node.counts['broker_error'] += 1  # never print ERROR body/headers

    def on_disconnected(self):
        self.node.counts['disconnected'] += 1


class Bridge(Node):
    def __init__(self, args):
        super().__init__('ma0t10_artemis_bridge')
        self.args = args
        self.qos = QoSProfile(history=HistoryPolicy.KEEP_LAST, depth=args.qos_depth,
                              reliability=ReliabilityPolicy.RELIABLE if args.reliable else ReliabilityPolicy.BEST_EFFORT,
                              durability=DurabilityPolicy.VOLATILE)
        self.inbox = queue.Queue(maxsize=32)
        self.inbox_lock = threading.Lock()
        self.inbox_bytes = 0
        self.counts = Counter()
        self.acquisition_clock = AcquisitionClock()
        self.clock_publisher = self.create_publisher(ClockMessage, '/clock', 10) if getattr(args, 'publish_clock', False) else None
        self.streams = {}
        self.publishers_by_key = {}
        self.static_sent = set()
        self.tf = TransformBroadcaster(self)
        self.static_tf = StaticTransformBroadcaster(self)
        self.static_transforms = []
        self.started = time.time_ns()
        self.connection = None
        self.stopping = threading.Event()
        self.connector = threading.Thread(target=self.connect_loop, daemon=True)
        self.connector.start()
        self.steady = Clock(clock_type=ClockType.STEADY_TIME)
        self.create_timer(.001, self.drain, clock=self.steady)
        self.create_timer(5, self.metrics, clock=self.steady)

    def connect_loop(self):
        while not self.stopping.is_set():
            try:
                conn = stomp.Connection12([(os.environ.get('MA0T10_STOMP_HOST') or '127.0.0.1',
                                            int(os.environ.get('MA0T10_STOMP_PORT') or '61616'))],
                                           auto_decode=False, heartbeats=(1000, 1000),
                                           reconnect_attempts_max=1, timeout=5)
                conn.set_listener('bridge', Inbox(self, conn))
                self.connection = conn
                conn.connect(os.environ['MA0T10_STOMP_USER'], os.environ['MA0T10_STOMP_PASSWORD'], wait=True)
                topics = tuple(self.args.topic or DEFAULT_TOPICS) + (('topic.virtual.sensor.imu.0',) if getattr(self.args, 'imu', False) else ())
                for i, topic in enumerate(topics):
                    conn.subscribe(destination=topic, id=f'ros2-{i}', ack='client-individual',
                                   headers={'subscription-type': 'MULTICAST', 'consumer-window-size': '8388608'})
                self.counts['connections'] += 1
                while not self.stopping.wait(.25) and conn.is_connected():
                    pass
            except Exception as exc:
                self.counts['connection_error'] += 1
                self.counts['connection_error_' + type(exc).__name__] += 1
                # STOMP exception text may contain authentication headers. Never render it.
            finally:
                if self.connection:
                    try:
                        self.connection.disconnect()
                    except Exception:
                        pass
            self.stopping.wait(2)

    def publisher(self, sensor_id, suffix, cls):
        key = wire.sensor_key(sensor_id)
        if len(self.publishers_by_key) >= 512 and (key, suffix) not in self.publishers_by_key:
            raise wire.WireError('publisher_limit')
        return self.publishers_by_key.setdefault((key, suffix), None) or self.make_publisher(key, suffix, cls)

    def make_publisher(self, key, suffix, cls):
        pub = self.create_publisher(cls, f'/ma0t10/{key}/{suffix}', self.qos)
        self.publishers_by_key[key, suffix] = pub
        return pub

    def header(self, message, utc, frame):
        message.header.stamp.sec, message.header.stamp.nanosec = utc
        message.header.frame_id = frame

    def transform(self, utc, parent, child, pose):
        t = TransformStamped()
        self.header(t, utc, parent)
        t.child_frame_id = child
        # rosidl requires actual Python float, even when JSON numbers were integral.
        t.transform.translation.x, t.transform.translation.y, t.transform.translation.z = map(float, pose[0])
        t.transform.rotation.x, t.transform.rotation.y, t.transform.rotation.z, t.transform.rotation.w = map(float, pose[1])
        return t

    def optical_frame(self, utc, body_frame):
        optical = body_frame + '_optical'
        if optical not in self.static_sent:
            # Optical z=body x; optical x=-body y; optical y=-body z.
            t = self.transform(utc, body_frame, optical, ([0., 0., 0.], [-.5, .5, -.5, .5]))
            self.static_transforms.append(t)
            self.static_tf.sendTransform(self.static_transforms)
            self.static_sent.add(optical)
        return optical

    def publish_cloud(self, cloud):
        frame = f'ma0t10/{wire.sensor_key(cloud.sensor_id)}'
        msg = PointCloud2()
        self.header(msg, cloud.stamp, 'ma0t10_world' if cloud.world else frame)
        msg.height, msg.width, msg.point_step = 1, cloud.count, cloud.point_step
        msg.row_step, msg.is_bigendian, msg.is_dense = cloud.count*cloud.point_step, False, False
        msg.fields = [PointField(name=name, offset=offset, datatype=datatype, count=1) for name, offset, datatype in cloud.fields]
        msg.data = cloud.data
        if cloud.pose:
            self.tf.sendTransform(self.transform(cloud.stamp, 'ma0t10_world', frame, cloud.pose))
        self.publisher(cloud.sensor_id, 'points_legacy' if cloud.world else 'points', PointCloud2).publish(msg)
        return cloud.sensor_id, cloud.frame, cloud.stamp, 'legacy' if cloud.world else 'pcd', len(cloud.data)

    def publish_imu(self, body, headers):
        if not getattr(self.args, 'imu', False):
            raise wire.WireError('imu_disabled')
        sid, samples = imu.decode(body, headers)
        batch_frame = wire.integer(headers.get('x-frame-id'), 1, 2**53-1)
        frame = f'ma0t10/{wire.sensor_key(sid)}'
        for sample in samples:
            self.publisher(sid, 'imu', Imu).publish(imu.assign(Imu(), sample, frame))
            self.tf.sendTransform(self.transform(wire.stamp(sample['timestampUtc']), 'ma0t10_world', frame,
                                                (sample['positionWorldFluM'], sample['orientationXyzw'])))
            self.counts['imu_samples'] += 1
        return sid, batch_frame, wire.stamp(samples[-1]['timestampUtc']), 'imu', len(body)

    def publish_camera(self, body, headers):
        c = wire.camera(body, headers)
        if wire.jpeg_dimensions(c['jpeg']) != (c['width'],c['height']):
            raise wire.WireError('camera_dimensions')
        frame = f'ma0t10/{wire.sensor_key(c["sensor_id"])}'
        # Decode before publishing: malformed JPEG is discarded as one complete frame.
        try:
            rgb = wire.decode_rgb(c)
        except ImportError:
            rgb = None
            self.counts['image_decoder_unavailable'] += 1
        optical = self.optical_frame(c['stamp'], frame)
        if c['pose']:
            self.tf.sendTransform(self.transform(c['stamp'], 'ma0t10_world', frame, c['pose']))
        compressed = CompressedImage(format='jpeg', data=c['jpeg'])
        self.header(compressed, c['stamp'], optical)
        self.publisher(c['sensor_id'], 'image/compressed', CompressedImage).publish(compressed)
        if rgb is not None:
            image = Image(width=c['width'], height=c['height'], encoding='rgb8', step=c['width']*3, data=rgb)
            self.header(image, c['stamp'], optical)
            self.publisher(c['sensor_id'], 'image', Image).publish(image)
        info = CameraInfo(width=c['width'], height=c['height'], distortion_model='plumb_bob')
        self.header(info, c['stamp'], optical)
        if c['fov']:
            fx = c['width'] / (2*math.tan(math.radians(c['fov'][0])/2))
            fy = c['height'] / (2*math.tan(math.radians(c['fov'][1])/2))
            cx, cy = c['width']/2., c['height']/2.
            info.k = [fx, 0., cx, 0., fy, cy, 0., 0., 1.]
            info.r = [1., 0., 0., 0., 1., 0., 0., 0., 1.]
            info.p = [fx, 0., cx, 0., 0., fy, cy, 0., 0., 0., 1., 0.]
            info.d = [0.]*5
        # Binary JPEG carries no intrinsics: K[0]==0 explicitly marks uncalibrated.
        self.publisher(c['sensor_id'], 'camera_info', CameraInfo).publish(info)
        return c['sensor_id'], c['frame'], c['stamp'], 'camera', len(c['jpeg'])

    def drain(self):
        # Limit each executor turn; STOMP thread never blocks on DDS or conversion.
        for _ in range(4):
            try:
                headers, body, received, owner = self.inbox.get_nowait()
            except queue.Empty:
                return
            try:
                schema = headers.get('schema')
                if schema == wire.PCD_SCHEMA:
                    result = self.publish_cloud(wire.pcd(body, headers))
                elif schema == 'virtual-camera.jpeg.v1':
                    result = self.publish_camera(body, headers)
                else:
                    wire.checked_body(body, headers)
                    obj = wire.json_payload(body)
                    json_schema = obj.get('schemaVersion', obj.get('schema'))
                    if schema and schema != json_schema:
                        raise wire.WireError('schema_mismatch')
                    if json_schema == 'virtual-imu.v1':
                        result = self.publish_imu(body, headers)
                    elif json_schema == 'virtual-lidar.v1':
                        result = self.publish_cloud(wire.lidar_json(body, headers))
                    elif json_schema == 'virtual-camera.v1':
                        result = self.publish_camera(body, headers)
                    elif json_schema == 'virtual-lidar.telemetry.v1':
                        sid, frame, utc = wire.identity(headers)
                        if obj.get('sensorId') != sid or obj.get('timestampUtc') is None or wire.stamp(obj['timestampUtc']) != utc:
                            raise wire.WireError('identity_mismatch')
                        # Telemetry has no points or pose. It cannot become a PointCloud2.
                        self.publisher(sid, 'telemetry', String).publish(String(data=json.dumps(obj, separators=(',', ':'))))
                        result = sid, frame, utc, 'telemetry', len(body)
                    else:
                        raise wire.WireError('unknown_schema')
                self.record(result, received)
                self.counts['published_frames'] += 1
            except wire.WireError as exc:
                self.counts['invalid_or_unknown_schema'] += 1
                reason = str(exc)
                if reason in ('unknown_schema', 'unknown_context_schema', 'schema_mismatch'):
                    self.counts['unknown_schema'] += 1
                else:
                    self.counts['invalid_payload'] += 1
            except Exception:
                # Do not render exceptions from libraries: even malicious input stays private.
                self.counts['invalid_or_unknown_schema'] += 1
            finally:
                with self.inbox_lock:
                    self.inbox_bytes -= len(body)
                owner.acknowledge(headers)

    def record(self, result, received):
        sid, frame, utc, kind, size = result
        if self.clock_publisher:
            next_stamp = self.acquisition_clock.observe(utc)
            if next_stamp is not None:
                clock = ClockMessage()
                clock.clock.sec, clock.clock.nanosec = next_stamp
                self.clock_publisher.publish(clock)
                self.counts['clock_published'] += 1
            self.counts['clock_reordered'] = self.acquisition_clock.reordered
        key = wire.sensor_key(sid) + '/' + kind
        if key not in self.streams:
            self.streams[key] = {'sensor_id': sid, 'frames': 0, 'bytes': 0, 'source_frame_gaps': 0,
                                 'duplicates_or_reordered': 0, 'last_frame': None,
                                 'first_received_ns': received, 'last_received_ns': received,
                                 'latency_ms': deque(maxlen=6000)}
        s = self.streams[key]
        if s['last_frame'] is not None:
            delta = frame-s['last_frame']
            if delta > 1:
                s['source_frame_gaps'] += delta-1
            elif delta <= 0:
                s['duplicates_or_reordered'] += 1
        s['last_frame'] = frame
        s['frames'] += 1
        s['bytes'] += size
        s['last_received_ns'] = received
        s['latency_ms'].append((received-(utc[0]*10**9+utc[1]))/10**6)

    def metrics(self):
        report = {'started_ns': self.started, 'at_ns': time.time_ns(), 'counts': dict(self.counts),
                  'queue_bytes': self.inbox_bytes, 'streams': {}}
        for key, value in self.streams.items():
            s = {k: v for k, v in value.items() if k != 'latency_ms'}
            lat = sorted(value['latency_ms'])
            seconds = (s['last_received_ns']-s['first_received_ns'])/1e9
            s['received_hz'] = (s['frames']-1)/seconds if seconds else 0
            s['payload_bytes_per_sec'] = s['bytes']/seconds if seconds else 0
            s['latency_ms'] = {'mean': sum(lat)/len(lat), 'p95': lat[min(len(lat)-1, int(len(lat)*.95))],
                               'min': lat[0], 'max': lat[-1]} if lat else {}
            report['streams'][key] = s
        if self.args.metrics:
            path = Path(self.args.metrics)
            tmp = path.with_suffix('.tmp')
            tmp.write_text(json.dumps(report, indent=2), encoding='utf8')
            tmp.replace(path)
        if rclpy.ok():
            self.get_logger().info('counts=' + json.dumps(dict(self.counts), sort_keys=True))

    def close(self):
        self.stopping.set()
        self.connector.join(timeout=12)
        self.metrics()


def main():
    # stomp.py debug/error logging can render server headers. Suppress library output entirely.
    import logging
    logging.getLogger('stomp').disabled = True
    logging.getLogger('stomp').addHandler(logging.NullHandler())
    logging.getLogger('stomp').propagate = False
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--topic', action='append')
    parser.add_argument('--metrics')
    parser.add_argument('--imu', action='store_true', help='Opt-in virtual-imu.v1 to sensor_msgs/Imu')
    parser.add_argument('--reliable', action='store_true')
    parser.add_argument('--publish-clock', action='store_true', help='Opt-in acquisition UTC high-water /clock; frame stamps stay unchanged')
    parser.add_argument('--qos-depth', type=int, default=5)
    parser.add_argument('--queue-bytes', type=int, default=64*1024*1024)
    args = parser.parse_args()
    if not 1 <= args.qos_depth <= 100 or not 1 <= args.queue_bytes <= 128*1024*1024:
        parser.error('Invalid bounded QoS/queue limit')
    for name in ('MA0T10_STOMP_USER', 'MA0T10_STOMP_PASSWORD'):
        if not os.environ.get(name):
            parser.error('Missing credential environment (values are never printed)')
    rclpy.init()
    node = Bridge(args)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, rclpy.executors.ExternalShutdownException):
        pass
    finally:
        node.close()
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
