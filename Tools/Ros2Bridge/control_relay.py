"""Temporary authenticated Windows WSL-interface -> AUTO-01 loopback relay.

No system network/firewall changes. Stop with Ctrl+C or an owned --stop-file.
"""
import argparse
import hmac
import ipaddress
import json
import os
from pathlib import Path
import select
import socket
import subprocess
import threading
import time

from control import COMMANDS, MAX_BODY, token_checked


def wsl_interface(bind, subnet):
    """Validate against the live Windows interface inventory, never caller claims alone."""
    if os.name != 'nt':
        raise ValueError('windows_required')
    script = "Get-NetIPAddress -AddressFamily IPv4 | Where-Object InterfaceAlias -Like 'vEthernet (WSL*' | Select-Object IPAddress,PrefixLength | ConvertTo-Json -Compress"
    output = subprocess.check_output(['powershell.exe', '-NoProfile', '-Command', script], timeout=10)
    entries = json.loads(output)
    if isinstance(entries, dict):
        entries = [entries]
    expected = ipaddress.IPv4Network(subnet, strict=False)
    for entry in entries:
        if entry['IPAddress'] == bind and ipaddress.IPv4Network(f"{bind}/{entry['PrefixLength']}", strict=False) == expected:
            return expected
    raise ValueError('actual_wsl_interface_required')


def receive_request(client, token, api_port):
    """One bounded HTTP request; authenticate before opening any upstream socket."""
    raw = b''
    deadline = time.monotonic() + 2
    def read(size):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise socket.timeout()
        client.settimeout(remaining)
        return client.recv(size)
    while b'\r\n\r\n' not in raw:
        block = read(4096)
        if not block:
            raise ValueError('incomplete')
        raw += block
        if len(raw.split(b'\r\n\r\n', 1)[0]) > 8192:
            raise ValueError('header_limit')
    head, body = raw.split(b'\r\n\r\n', 1)
    lines = head.decode('ascii').split('\r\n')
    method, path, version = lines.pop(0).split(' ')
    if version != 'HTTP/1.1' or (method, path) not in {(m, p) for m, p, _ in COMMANDS.values()}:
        raise ValueError('route')
    headers = {}
    for line in lines:
        key, value = line.split(':', 1)
        key, value = key.lower(), value.strip()
        if key in headers or key not in {'host', 'authorization', 'content-type', 'content-length', 'accept', 'accept-encoding', 'connection'}:
            raise ValueError('headers')
        headers[key] = value
    if not hmac.compare_digest(headers.get('authorization', ''), 'Bearer ' + token):
        raise PermissionError('authentication')
    if headers.get('host') != f'127.0.0.1:{api_port}':
        raise ValueError('host')
    size_text = headers.get('content-length', '0')
    if not size_text.isascii() or not size_text.isdigit() or len(size_text) > 9:
        raise ValueError('length')
    size = int(size_text)
    if size > MAX_BODY or (method == 'GET' and size) or (method == 'POST' and headers.get('content-type') != 'application/json'):
        raise ValueError('body')
    while len(body) < size:
        block = read(min(65536, size - len(body)))
        if not block:
            raise ValueError('incomplete')
        body += block
    if len(body) != size:
        raise ValueError('pipeline')
    # Preserve API Host/auth contracts and strip all unneeded inbound headers.
    auth = 'Bearer ' + token
    return (f'{method} {path} HTTP/1.1\r\nHost: 127.0.0.1:{api_port}\r\nAuthorization: {auth}\r\n'
            f'Content-Type: application/json\r\nContent-Length: {size}\r\nConnection: close\r\n\r\n').encode('ascii') + body


def forward(client, token, api_port, stopping):
    with client:
        client.settimeout(2)
        try:
            request = receive_request(client, token, api_port)
            with socket.create_connection(('127.0.0.1', api_port), timeout=2) as upstream:
                upstream.sendall(request)
                total = 0
                while not stopping.is_set():
                    data = upstream.recv(65536)
                    if not data:
                        return
                    total += len(data)
                    if total > MAX_BODY + 8192:
                        return
                    client.sendall(data)
        except PermissionError:
            client.sendall(b'HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
        except ValueError:
            client.sendall(b'HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
        except OSError:
            pass  # No packet contents, headers or exception text are logged.


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bind', required=True)
    p.add_argument('--wsl-subnet', required=True)
    p.add_argument('--port', type=int, default=18099)
    p.add_argument('--api-port', type=int, default=8099)
    p.add_argument('--stop-file', type=Path)
    args = p.parse_args()
    try:
        token = token_checked(os.environ.get('MA0T10_AUTOMATION_TOKEN', ''))
        subnet = wsl_interface(args.bind, args.wsl_subnet)
        ip = ipaddress.IPv4Address(args.bind)
        if not ip.is_private or ip.is_loopback or ip.is_unspecified or any(not 1024 <= x <= 65535 for x in (args.port, args.api_port)):
            raise ValueError('endpoint')
    except (ValueError, OSError, subprocess.SubprocessError):
        p.error('Actual Windows WSL interface/subnet, user ports and a nonempty token are required')
    stopping = threading.Event()
    slots = threading.BoundedSemaphore(8)
    threads = []

    def serve(client):
        try:
            forward(client, token, args.api_port, stopping)
        finally:
            slots.release()

    with socket.socket() as listener:
        listener.bind((args.bind, args.port))
        listener.listen(8)
        listener.settimeout(.25)
        print('Authenticated temporary WSL API relay ready; no bodies logged.', flush=True)
        try:
            while not args.stop_file or not args.stop_file.exists():
                try:
                    client, address = listener.accept()
                except socket.timeout:
                    continue
                if ipaddress.IPv4Address(address[0]) not in subnet or not slots.acquire(blocking=False):
                    client.close()
                    continue
                threads = [t for t in threads if t.is_alive()]
                worker = threading.Thread(target=serve, args=(client,), daemon=True)
                threads.append(worker)
                worker.start()
        except KeyboardInterrupt:
            pass
        finally:
            stopping.set()
    for worker in threads:
        worker.join(timeout=3)
    print('Owned relay listener and connections closed.', flush=True)


if __name__ == '__main__':
    main()
