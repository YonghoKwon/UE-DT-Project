"""Opt-in live ROS2-02 acceptance against a disposable UE fixture, never a normal session.

Run in the named WSL with ROS sourced. Uses ROS service CLI and ROS topics only.
Writes metadata/paths/counts, never sensor bodies or credential environment.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time
import uuid

import rclpy
from rclpy.node import Node
from rclpy.parameter import Parameter
from rclpy.qos import qos_profile_sensor_data, QoSProfile, DurabilityPolicy
from rosgraph_msgs.msg import Clock
from sensor_msgs.msg import PointCloud2
from std_msgs.msg import String
from control_node import PREFIX
import wire


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--scenario', required=True, help='Windows absolute fixture JSON path')
    p.add_argument('--seconds', type=float, default=15)
    args = p.parse_args()
    rclpy.init(args=[])
    node = Node('ma0t10_control_acceptance', parameter_overrides=[Parameter('use_sim_time', value=True)])
    results, states, clocks, points, checks = [], [], [], [], []
    nano = lambda stamp: stamp.sec*10**9 + stamp.nanosec
    node.create_subscription(String, PREFIX+'result', lambda m: results.append(json.loads(m.data)), 100)
    node.create_subscription(String, PREFIX+'status', lambda m: states.append(json.loads(m.data)),
                             QoSProfile(depth=1, durability=DurabilityPolicy.TRANSIENT_LOCAL))
    node.create_subscription(Clock, '/clock', lambda m: clocks.append(nano(m.clock)), 100)
    node.create_subscription(PointCloud2, '/ma0t10/'+wire.sensor_key('SLAB-LIDAR-001')+'/points',
                             lambda m: points.append({'stamp_ns':nano(m.header.stamp), 'received':time.monotonic(),
                                                       'points':m.width, 'step':m.point_step}), qos_profile_sensor_data)
    pub = node.create_publisher(String, PREFIX+'request', 8)

    def until(test, seconds=20):
        end = time.monotonic() + seconds
        while not test():
            if time.monotonic() > end: raise AssertionError('Acceptance wait timed out')
            rclpy.spin_once(node, timeout_sec=.02)
        return test()

    def spin(seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end: rclpy.spin_once(node, timeout_sec=.02)

    def passed(name, detail=None):
        checks.append({'name':name, 'ok':True, 'detail':detail})
        print('PASS '+name, flush=True)

    def send(op, params, expect=True):
        rid = uuid.uuid4().hex
        pub.publish(String(data=json.dumps(dict(schema='ma0t10.ros2.control.v1', request_id=rid, op=op, params=params))))
        until(lambda: any(r.get('request_id') == rid for r in results))
        result = next(r for r in results if r.get('request_id') == rid)
        assert result['ok'] is expect, result.get('error', 'Unexpected rejection')
        return result

    def terminal(result, success=True):
        oid = result['api']['operation_id']
        until(lambda: any(s.get('ok') and s['api']['operation_id'] == oid and s['api']['terminal'] for s in states), 180)
        state = next(s['api'] for s in reversed(states) if s.get('ok') and s['api']['operation_id'] == oid and s['api']['terminal'])
        assert state['success'] is success
        return state

    def cli(service, typ, request):
        before = len(results)
        proc = subprocess.Popen(['ros2','service','call',PREFIX+service,typ,request], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        until(lambda: proc.poll() is not None, 25)
        text = proc.communicate()[0]
        assert proc.returncode == 0 and 'success=True' in text
        passed('ros2 service call '+service, {'output':text})
        until(lambda: len(results) > before)
        return results[-1]

    failure = None
    try:
        until(lambda: states and states[-1].get('ok'))
        spin(2)  # Discover point/result subscriptions before starting the producer.
        send('sensors', {})
        for sid in ('VCAM-001','SLAB-LIDAR-001'): send('sensor.stop', {'sensor_id':sid})
        send('sensor.set', {'sensor_id':'SLAB-LIDAR-001','profile':'IYOBOT_MLX80_NATIVE','quality':'FullSpec'})
        topic_rid = uuid.uuid4().hex
        payload = dict(schema='ma0t10.ros2.control.v1',request_id=topic_rid,op='sensor.set',
                       params={'sensor_id':'SLAB-LIDAR-001','interval_sec':.05})
        proc = subprocess.Popen(['ros2','topic','pub','--once',PREFIX+'request','std_msgs/msg/String',
                                 json.dumps({'data':json.dumps(payload)})], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        until(lambda: proc.poll() is not None,25)
        output = proc.communicate()[0]
        assert proc.returncode == 0
        until(lambda: any(r.get('request_id') == topic_rid and r['ok'] for r in results))
        passed('ros2 topic pub sensor.set', {'output':output})
        passed('ROS topic sensor settings and inventory')
        start = cli('pcd','std_srvs/srv/SetBool','{data: true}')
        until(lambda: len(points) >= 5)
        spin(args.seconds)
        cli('pcd','std_srvs/srv/SetBool','{data: false}')
        state = terminal(start)
        assert state['detail']['receipt_count'] > 0
        passed('PCD start -> PointCloud2 -> stop', state['detail'])
        spin(2); count = len(points); spin(1); assert len(points) == count
        assert points and all(x['step'] == 33 for x in points)
        assert clocks and all(b >= a for a,b in zip(clocks,clocks[1:]))
        missing = sorted({x['stamp_ns'] for x in points} - set(clocks))
        assert not missing
        assert node.get_clock().now().nanoseconds == clocks[-1]
        hz = (len(points)-1)/(points[-1]['received']-points[0]['received'])
        passed('/clock monotonic, exact PointCloud2 stamp set, use_sim_time, stopped stream',
               dict(point_frames=len(points), clock_frames=len(clocks), stamp_missing=len(missing), received_hz=hz))
        send('sensor.start', {'sensor_id':'VCAM-001'})
        capture = cli('capture','std_srvs/srv/Trigger','{}')
        state = terminal(capture)
        assert state['detail']['artifacts']
        passed('one-shot asynchronous capture terminal',state['detail'])
        send('sensor.stop', {'sensor_id':'VCAM-001'})
        dataset = send('dataset.start', dict(name='ROS202', scenes=3, slab='SlabActor_0',
                       lidar_id='SLAB-LIDAR-001', camera_id='VCAM-001', camera_truth=True, seed=22016))
        state = terminal(dataset)
        assert state['detail']['completed_scenes'] == 3
        passed('three-scene dataset terminal and progress',state['detail'])
        scenario = send('scenario.start', {'json_path':args.scenario})
        oid = scenario['api']['operation_id']
        until(lambda: states[-1].get('ok') and states[-1]['api']['operation_id'] == oid and states[-1]['api']['phase'] == 'playing')
        send('scenario.stop', {})
        state = terminal(scenario, success=False)
        passed('scenario start / playing / stop terminal',state['detail'])
        cli('get_status','std_srvs/srv/Trigger','{}')
        bad = send('sensor.set', {'sensor_id':'SLAB-LIDAR-001','profile':'unknown'}, expect=False)
        passed('UE rejects invalid sensor configuration', {'error':bad['error']})
        passed('source feedback and progress topic', {'status_messages':len(states), 'result_messages':len(results),
               'sources':sorted({s['source'] for s in states+results})})
    except Exception as exc:
        failure = type(exc).__name__  # never print arbitrary exception content
    finally:
        args.output.write_text(json.dumps({'ok':failure is None,'failure':failure, 'checks':checks,
            'results':results,'statuses':states,'clocks_ns':clocks,'point_metadata':points},indent=2))
        node.destroy_node(); rclpy.shutdown()
    if failure: raise SystemExit('ROS2 acceptance failed; inspect bounded metadata report')


if __name__ == '__main__': main()
