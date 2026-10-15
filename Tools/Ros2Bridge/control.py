"""ROS2-02 bounded AUTO-01 adapter. No ROS dependency, retries, proxies or redirects."""
import http.client
import ipaddress
import json
import math
import re
import socket

MAX_BODY = 256 * 1024
# This is an adapter allowlist, never an arbitrary URL/UPROPERTY execution interface.
COMMANDS = {
    'status': ('GET', '/v1/status', set()),
    'sensors': ('GET', '/v1/sensors', set()),
    'sensor.set': ('POST', '/v1/sensor', {'sensor_id', 'interval_sec', 'profile', 'quality'}),
    'sensor.start': ('POST', '/v1/sensor/start', {'sensor_id'}),
    'sensor.stop': ('POST', '/v1/sensor/stop', {'sensor_id'}),
    'capture': ('POST', '/v1/capture', {'sensor_id'}),
    'pcd.start': ('POST', '/v1/pcd/start', {'sensor_id'}),
    'pcd.stop': ('POST', '/v1/pcd/stop', set()),
    'scenario.start': ('POST', '/v1/scenario/start', {'json_path', 'uuid', 'send_pcd', 'sensor_id'}),
    'scenario.stop': ('POST', '/v1/scenario/stop', set()),
    'dataset.start': ('POST', '/v1/dataset/start', {'name', 'scenes', 'slab', 'lidar_id', 'camera_id',
                      'seed', 'settle_frames', 'camera_truth', 'lidar_fidelity', 'light', 'ranges'}),
    'dataset.stop': ('POST', '/v1/dataset/stop', set()),
}


class ControlError(Exception):
    """Only fixed, non-sensitive codes cross the ROS/log boundary."""


def token_checked(token):
    if not isinstance(token, str) or not token or len(token) > 256 or any(not 33 <= ord(c) <= 126 for c in token):
        raise ValueError('authentication_required')
    return token


def strict_json(raw):
    def finite_float(value):
        result = float(value)
        if not math.isfinite(result):
            raise ValueError('finite')
        return result
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('duplicate_key')
            result[key] = value
        return result
    try:
        if len(raw.encode('utf8') if isinstance(raw, str) else raw) > MAX_BODY:
            raise ValueError('size')
        return json.loads(raw, object_pairs_hook=pairs, parse_float=finite_float,
                          parse_constant=lambda _: (_ for _ in ()).throw(ValueError('finite')))
    except (ValueError, UnicodeError, RecursionError):
        raise ControlError('invalid_json') from None


def command(op, params):
    if not isinstance(op, str) or op not in COMMANDS or not isinstance(params, dict):
        raise ControlError('invalid_command')
    method, path, fields = COMMANDS[op]
    if set(params) - fields:
        raise ControlError('unknown_parameter')
    # Detailed enum/range/admission validation remains owned by AUTO-01.
    strings = {'sensor_id', 'json_path', 'uuid', 'name', 'slab', 'lidar_id', 'camera_id', 'light', 'profile', 'quality'}
    for key, value in params.items():
        if key in strings and (not isinstance(value, str) or not value or len(value) > 4096):
            raise ControlError('invalid_parameter')
        if key in {'send_pcd', 'camera_truth', 'lidar_fidelity'} and type(value) is not bool:
            raise ControlError('invalid_parameter')
        if key in {'scenes', 'seed', 'settle_frames'} and type(value) is not int:
            raise ControlError('invalid_parameter')
        if key == 'interval_sec' and (type(value) not in (float, int) or not math.isfinite(value)):
            raise ControlError('invalid_parameter')
        if key == 'ranges' and not isinstance(value, dict):
            raise ControlError('invalid_parameter')
    required = {'sensor_id'} if op in {'sensor.set', 'sensor.start', 'sensor.stop', 'capture', 'pcd.start'} else set()
    if op == 'dataset.start':
        required = {'name', 'scenes', 'slab', 'lidar_id', 'camera_id'}
    if not required <= params.keys():
        raise ControlError('missing_parameter')
    if op == 'scenario.start' and (('json_path' in params) == ('uuid' in params)):
        raise ControlError('scenario_source_required')
    return method, path, None if method == 'GET' else params


def parse_request(raw):
    obj = strict_json(raw)
    if not isinstance(obj, dict) or set(obj) != {'schema', 'request_id', 'op', 'params'} or obj['schema'] != 'ma0t10.ros2.control.v1':
        raise ControlError('invalid_schema')
    if not isinstance(obj['request_id'], str) or not re.fullmatch(r'[A-Za-z0-9_.-]{1,64}', obj['request_id']):
        raise ControlError('invalid_request_id')
    command(obj['op'], obj['params'])
    return obj


class ApiClient:
    def __init__(self, host, port, api_port, token, timeout=2):
        ip = ipaddress.IPv4Address(host)
        if not (ip.is_private or ip.is_loopback) or ip.is_unspecified or ip.is_multicast or ip.is_link_local:
            raise ValueError('private_ipv4_required')
        if any(type(p) is not int or not 1024 <= p <= 65535 for p in (port, api_port)):
            raise ValueError('invalid_port')
        if not 0 < timeout <= 10:
            raise ValueError('invalid_timeout')
        self.host, self.port, self.api_port = str(ip), port, api_port
        self._token, self.timeout = token_checked(token), timeout

    def call(self, op, params):
        method, path, body = command(op, params)
        data = None if body is None else json.dumps(body, allow_nan=False, separators=(',', ':')).encode('utf8')
        if data is not None and len(data) > MAX_BODY:
            raise ControlError('request_too_large')
        conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        try:
            # Connect to the NAT relay, preserve AUTO-01's literal loopback Host contract.
            conn.request(method, path, data, {'Host': f'127.0.0.1:{self.api_port}',
                         'Authorization': 'Bearer ' + self._token, 'Content-Type': 'application/json'})
            response = conn.getresponse()
            if response.status != 200 and response.status != 202:
                raise ControlError('authentication_failed' if response.status == 401 else f'http_{response.status}')
            raw = response.read(MAX_BODY + 1)
            result = strict_json(raw)
            if not isinstance(result, dict) or result.get('ok') is not True:
                raise ControlError('api_unsuccessful')
            # Never expose bearer values even if an untrusted upstream reflects one.
            if self._token in json.dumps(result, ensure_ascii=False):
                raise ControlError('unsafe_response')
            return result
        except (socket.timeout, TimeoutError):
            raise ControlError('api_timeout_outcome_unknown') from None
        except (OSError, http.client.HTTPException):
            raise ControlError('api_unavailable_outcome_unknown') from None
        finally:
            conn.close()


class AcquisitionClock:
    """Monotonic high-water mark of valid acquisition UTC; never alters frame stamps."""
    def __init__(self):
        self.last_ns = 0
        self.reordered = 0

    def observe(self, stamp):
        ns = stamp[0] * 10**9 + stamp[1]
        if ns <= self.last_ns:
            self.reordered += ns < self.last_ns
            return None
        self.last_ns = ns
        return stamp
