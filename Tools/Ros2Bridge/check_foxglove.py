"""Windows localhost WebSocket protocol check, distinct from browser 3D acceptance.

Uses only stdlib. Does not subscribe to or save point/image payloads.
"""
import base64
import hashlib
import json
import os
import socket
import struct
import time


def check():
    key = base64.b64encode(os.urandom(16)).decode('ascii')
    with socket.create_connection(('127.0.0.1',8765), timeout=10) as conn:
        conn.sendall(('GET / HTTP/1.1\r\nHost: localhost:8765\r\nUpgrade: websocket\r\n'
                      'Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n'
                      'Sec-WebSocket-Protocol: foxglove.sdk.v1\r\n'
                      f'Sec-WebSocket-Key: {key}\r\n\r\n').encode('ascii'))
        buffer = bytearray()
        while b'\r\n\r\n' not in buffer:
            block = conn.recv(4096)
            if not block or len(buffer) > 16384:
                raise ValueError('WebSocket handshake failed')
            buffer.extend(block)
        response, rest = bytes(buffer).split(b'\r\n\r\n',1)
        expected = base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
        if not response.startswith(b'HTTP/1.1 101') or expected.lower() not in response.lower():
            raise ValueError('WebSocket handshake validation failed')
        buffer = bytearray(rest)

        def read(n):
            while len(buffer) < n:
                block = conn.recv(65536)
                if not block:
                    raise ValueError('WebSocket closed')
                buffer.extend(block)
            data = bytes(buffer[:n])
            del buffer[:n]
            return data

        topics = []
        server_info = False
        deadline = time.monotonic()+10
        while time.monotonic() < deadline:
            first, second = read(2)
            length = second & 127
            if length == 126:
                length = struct.unpack('>H',read(2))[0]
            elif length == 127:
                length = struct.unpack('>Q',read(8))[0]
            if length > 1024*1024 or second & 128 or not first & 128:
                raise ValueError('Unexpected WebSocket frame')
            body = read(length)
            if first & 15 != 1:
                continue
            obj = json.loads(body)
            if obj.get('op') == 'serverInfo':
                server_info = True
            if obj.get('op') == 'advertise':
                topics.extend({'topic':c['topic'],'schemaName':c['schemaName']} for c in obj.get('channels',[]))
                if any(t['schemaName'] == 'sensor_msgs/msg/PointCloud2' for t in topics):
                    break
        return {'http_upgrade':101, 'server_info':server_info, 'channels':topics,
                'pointcloud_advertised':any(t['schemaName'] == 'sensor_msgs/msg/PointCloud2' for t in topics),
                'browser_3d_verified':False}


if __name__ == '__main__':
    print(json.dumps(check(),indent=2))
