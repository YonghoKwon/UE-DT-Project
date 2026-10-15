"""Pure-Python mapping/auth/timeout/relay/clock regression. No UE/WSL required."""
import json
import socket
import threading
import time
import unittest
from unittest.mock import patch

from control import AcquisitionClock, ApiClient, COMMANDS, ControlError, command, parse_request, strict_json
from control_relay import receive_request, forward, wsl_interface


class ControlTests(unittest.TestCase):
    def test_every_command_mapping(self):
        params = {'sensor_id': 'lidar'}
        for op, (method, path, _) in COMMANDS.items():
            p = params if op in {'sensor.set', 'sensor.start', 'sensor.stop', 'capture', 'pcd.start'} else {}
            if op == 'scenario.start': p = {'uuid': 'scenario'}
            if op == 'dataset.start': p = dict(name='test', scenes=3, slab='s', lidar_id='l', camera_id='c')
            self.assertEqual(command(op, p), (method, path, None if method == 'GET' else p))

    def test_invalid_requests(self):
        good = dict(schema='ma0t10.ros2.control.v1', request_id='a-1', op='status', params={})
        self.assertEqual(parse_request(json.dumps(good)), good)
        for change in [dict(op='shutdown'), dict(op='/v1/status'), dict(params={'token': 'x'}),
                       dict(request_id='\n'), dict(schema='v2'), dict(op=[]), dict(params=[]), dict(extra=True)]:
            with self.subTest(change=change), self.assertRaises(ControlError): parse_request(json.dumps(good | change))
        for raw in ['null', '{}', '{', '{"a":1,"a":2}', '{"a":NaN}', '{"a":1e9999}', ' '*262145]:
            with self.assertRaises(ControlError): parse_request(raw)
        for op, params in [('capture', {}), ('sensor.set', {'sensor_id': 'a', 'interval_sec': True}),
                           ('dataset.start', {'scenes': '3'}), ('scenario.start', {}),
                           ('scenario.start', {'uuid': 'a', 'json_path': 'b'})]:
            with self.assertRaises(ControlError): command(op, params)

    def test_token_and_endpoint_required(self):
        for token in ['', 'a\nb', 'a b', 'x'*257]:
            with self.assertRaises(ValueError): ApiClient('127.0.0.1', 8099, 8099, token)
        for host in ['0.0.0.0', 'example.com', '8.8.8.8', '224.0.0.1', '169.254.1.1']:
            with self.assertRaises(ValueError): ApiClient(host, 8099, 8099, 'unit-secret')

    def test_http_auth_failure_no_secret_or_redirect(self):
        response = unittest.mock.Mock(status=401)
        with patch('http.client.HTTPConnection') as conn:
            conn.return_value.getresponse.return_value = response
            with self.assertRaisesRegex(ControlError, '^authentication_failed$'):
                ApiClient('127.0.0.1', 18099, 8099, 'unit-secret').call('status', {})
            args = conn.return_value.request.call_args
            self.assertEqual(args.args[3]['Host'], '127.0.0.1:8099')
            self.assertEqual(args.args[3]['Authorization'], 'Bearer unit-secret')
            response.status = 302
            with self.assertRaisesRegex(ControlError, '^http_302$'):
                ApiClient('127.0.0.1', 18099, 8099, 'unit-secret').call('status', {})
            self.assertEqual(conn.call_count, 2)

    def test_timeout_no_retry_outcome_unknown(self):
        with patch('http.client.HTTPConnection') as conn:
            conn.return_value.getresponse.side_effect = socket.timeout()
            with self.assertRaisesRegex(ControlError, '^api_timeout_outcome_unknown$'):
                ApiClient('127.0.0.1', 18099, 8099, 'unit-secret').call('pcd.stop', {})
            self.assertEqual(conn.return_value.request.call_count, 1)
            conn.return_value.close.assert_called_once()

    def test_monotonic_clock_multi_sensor_reordering(self):
        c = AcquisitionClock()
        samples = [(100, 100), (100, 100), (100, 99), (101, 0), (99, 1), (101, 5)]
        self.assertEqual([s for s in (c.observe(x) for x in samples) if s], [(100,100), (101,0), (101,5)])
        self.assertEqual(c.reordered, 2)

    def relay_request(self, headers='', body=b'', method='GET', path='/v1/status'):
        sender, receiver = socket.socketpair()
        self.addCleanup(sender.close); self.addCleanup(receiver.close)
        raw = f'{method} {path} HTTP/1.1\r\nHost: 127.0.0.1:8099\r\n{headers}\r\n'.encode('ascii') + body
        sender.sendall(raw)
        sender.shutdown(socket.SHUT_WR)
        return receive_request(receiver, 'unit-secret', 8099)

    def test_relay_authenticates_before_any_upstream(self):
        for auth in ['', 'Authorization: Bearer wrong\r\n']:
            with self.assertRaises(PermissionError): self.relay_request(auth)
        request = self.relay_request('Authorization: Bearer unit-secret\r\n')
        self.assertIn(b'Host: 127.0.0.1:8099\r\n', request)
        sender, receiver = socket.socketpair()
        with sender:
            sender.sendall(b'GET /v1/status HTTP/1.1\r\nHost: 127.0.0.1:8099\r\n\r\n')
            with patch('socket.create_connection') as upstream:
                forward(receiver, 'unit-secret', 8099, threading.Event())
                upstream.assert_not_called()
            self.assertIn(b'401', sender.recv(4096))

    def test_relay_rejects_origin_duplicates_pipeline_and_routes(self):
        auth = 'Authorization: Bearer unit-secret\r\n'
        for headers in [auth+'Origin: http://other\r\n', auth+auth, auth+'Content-Length: 262145\r\n', auth+'Transfer-Encoding: chunked\r\n']:
            with self.assertRaises(ValueError): self.relay_request(headers)
        with self.assertRaises(ValueError): self.relay_request(auth, b'GET /v1/status HTTP/1.1\r\n\r\n')
        with self.assertRaises(ValueError): self.relay_request(auth, path='/v1/shutdown')

    def test_relay_checks_live_interface(self):
        with patch('control_relay.os.name', 'nt'), patch('subprocess.check_output', return_value=b'{"IPAddress":"172.19.144.1","PrefixLength":20}'):
            self.assertEqual(str(wsl_interface('172.19.144.1', '172.19.144.0/20')), '172.19.144.0/20')
            for bind in ['0.0.0.0', '192.168.1.10', '127.0.0.1']:
                with self.assertRaises(ValueError): wsl_interface(bind, '172.19.144.0/20')


if __name__ == '__main__': unittest.main()
