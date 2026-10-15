"""ROS-independent virtual-imu.v1 validation and sensor_msgs/Imu assignment."""
import math
import wire


def decode(body, headers):
    wire.checked_body(body, headers)
    obj = wire.json_payload(body)
    if obj.get('schemaVersion') != 'virtual-imu.v1' or obj.get('orientationSource') != 'simulator_truth':
        raise wire.WireError('imu_schema')
    sid = obj.get('sensorId')
    wire.sensor_key(sid)
    if ('x-sensor-id' in headers and headers['x-sensor-id'] != sid) or ('x-sensor-type' in headers and headers['x-sensor-type'] != 'imu'):
        raise wire.WireError('imu_identity')
    rate = obj.get('rateHz')
    if isinstance(rate, bool) or not isinstance(rate, (int, float)) or not math.isfinite(rate) or not 100 <= rate <= 400:
        raise wire.WireError('imu_rate')
    samples = obj.get('samples')
    if not isinstance(samples, list) or not 1 <= len(samples) <= 101:
        raise wire.WireError('imu_count')
    previous = None
    for s in samples:
        if not isinstance(s, dict):
            raise wire.WireError('imu_sample')
        seq = wire.integer(s.get('sequence'), 1, 2**53-1)
        utc = wire.stamp(s.get('timestampUtc'))
        for key, count in (('specificForceMps2', 3), ('angularVelocityRadps', 3),
                           ('positionWorldFluM', 3), ('orientationXyzw', 4)):
            v = s.get(key)
            if not isinstance(v, list) or len(v) != count or any(isinstance(x, bool) or not isinstance(x, (int, float)) or not math.isfinite(x) for x in v):
                raise wire.WireError('imu_vector')
        if abs(sum(x*x for x in s['orientationXyzw']) - 1) > 1e-5:
            raise wire.WireError('imu_quaternion')
        for key in ('accelVariance', 'gyroVariance', 'simulationSeconds'):
            v = s.get(key)
            if isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) or v < 0:
                raise wire.WireError('imu_variance_time')
        if previous and (seq != previous[0]+1 or abs(s['simulationSeconds']-previous[1]-1/rate) > 1e-7 or utc <= previous[2]):
            raise wire.WireError('imu_order')
        previous = seq, s['simulationSeconds'], utc
    return sid, samples


def assign(message, sample, frame):
    """Accepts a real ROS Imu or an equivalent test double; no ROS runtime required."""
    message.header.frame_id = frame
    message.header.stamp.sec, message.header.stamp.nanosec = wire.stamp(sample['timestampUtc'])
    for target, key in ((message.linear_acceleration, 'specificForceMps2'),
                        (message.angular_velocity, 'angularVelocityRadps'),
                        (message.orientation, 'orientationXyzw')):
        for axis, value in zip('xyzw', sample[key]):
            setattr(target, axis, float(value))
    message.linear_acceleration_covariance = [float(sample['accelVariance']) if i in (0, 4, 8) else 0. for i in range(9)]
    message.angular_velocity_covariance = [float(sample['gyroVariance']) if i in (0, 4, 8) else 0. for i in range(9)]
    # All-zero = unknown covariance per sensor_msgs. Orientation itself is simulator truth.
    message.orientation_covariance = [0.] * 9
    return message
