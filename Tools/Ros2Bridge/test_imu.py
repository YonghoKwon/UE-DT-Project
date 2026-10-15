import copy
import json
from types import SimpleNamespace as NS
import unittest
import ast
from collections import Counter
from pathlib import Path
import imu
import wire


def batch():
    return {'schemaVersion': 'virtual-imu.v1', 'sensorId': 'lidar.imu', 'rateHz': 200,
            'orientationSource': 'simulator_truth', 'samples': [
                {'sequence': n+1, 'simulationSeconds': n*.005,
                 'timestampUtc': f'2026-10-16T00:00:00.{n*50000:07d}Z',
                 'specificForceMps2': [0, 0, 9.81], 'angularVelocityRadps': [0, 0, 1],
                 'orientationXyzw': [0, 0, 0, 1], 'positionWorldFluM': [0, 0, 0],
                 'accelVariance': .0008, 'gyroVariance': .00000392} for n in range(3)]}


class ImuTests(unittest.TestCase):
    def decode(self, obj):
        return imu.decode(json.dumps(obj).encode(), {})

    def test_batch_preserves_fixed_step_utc_and_flu(self):
        sid, samples = self.decode(batch())
        self.assertEqual(sid, 'lidar.imu')
        msg = NS(header=NS(stamp=NS()), linear_acceleration=NS(), angular_velocity=NS(), orientation=NS())
        imu.assign(msg, samples[1], 'imu_frame')
        self.assertEqual(msg.header.stamp.nanosec, 5000000)
        self.assertEqual(msg.header.frame_id, 'imu_frame')
        self.assertEqual(msg.linear_acceleration.z, 9.81)
        self.assertEqual(msg.angular_velocity.z, 1.)
        self.assertEqual(msg.orientation.w, 1.)
        self.assertEqual(msg.linear_acceleration_covariance[4], .0008)
        self.assertEqual(msg.orientation_covariance, [0.]*9)

    def test_rejects_bad_shapes_nan_rates_order_and_quaternion(self):
        mutations = [lambda b: b.update(rateHz=99), lambda b: b.update(samples=[]),
                     lambda b: b['samples'][0].update(specificForceMps2=[1, 2]),
                     lambda b: b['samples'][0].update(gyroVariance=-1),
                     lambda b: b['samples'][0].update(orientationXyzw=[0, 0, 0, 0]),
                     lambda b: b['samples'][1].update(sequence=8),
                     lambda b: b['samples'][1].update(simulationSeconds=.006),
                     lambda b: b['samples'][1].update(timestampUtc=b['samples'][0]['timestampUtc']),
                     lambda b: b['samples'][0].update(angularVelocityRadps=[float('nan'), 0, 0])]
        for mutate in mutations:
            b=copy.deepcopy(batch());mutate(b)
            with self.subTest(obj=b), self.assertRaises(wire.WireError):self.decode(b)

    def test_checksum_and_unknown_schema_rejected(self):
        with self.assertRaises(wire.WireError):imu.decode(json.dumps(batch()).encode(), {'checksum': 'bad'})
        b=batch();b['schemaVersion']='virtual-imu.v2'
        with self.assertRaises(wire.WireError):self.decode(b)

    def test_actual_bridge_publish_method_with_ros_message_double(self):
        # Execute the production method with message/publisher doubles, without installing ROS/WSL.
        source=ast.parse(Path(__file__).with_name('bridge_node.py').read_text(encoding='utf-8'))
        cls=next(n for n in source.body if isinstance(n, ast.ClassDef) and n.name=='Bridge')
        method=next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name=='publish_imu')
        def message():return NS(header=NS(stamp=NS()),linear_acceleration=NS(),angular_velocity=NS(),orientation=NS())
        env={'wire':wire,'imu':imu,'Imu':message}
        exec(compile(ast.Module(body=[method],type_ignores=[]),'<Bridge.publish_imu>','exec'),env)
        sent=[];transforms=[]
        fake=NS(publisher=lambda *args:NS(publish=sent.append),tf=NS(sendTransform=transforms.append),
                transform=lambda *args:args,counts=Counter())
        result=env['publish_imu'](fake,json.dumps(batch()).encode(),{})
        self.assertEqual(len(sent),3);self.assertEqual(len(transforms),3)
        self.assertEqual(fake.counts['imu_samples'],3)
        self.assertEqual(result[3],'imu')


if __name__ == '__main__':unittest.main()
