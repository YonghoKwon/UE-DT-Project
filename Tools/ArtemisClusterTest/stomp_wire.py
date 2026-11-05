"""Small bounded STOMP 1.2 client for local broker functional checks.

No third-party package, WebSocket implementation, or product-side transport logic.
Binary body boundaries use content-length and preserve embedded NUL bytes.
"""
from __future__ import annotations

import socket
import time
import uuid

MAX_FRAME = 32 * 1024 * 1024


def escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace("\r", "\\r").replace("\n", "\\n").replace(":", "\\c")


def unescape(value: str) -> str:
    result = []
    offset = 0
    escapes = {"n": "\n", "r": "\r", "c": ":", "\\": "\\"}
    while offset < len(value):
        char = value[offset]
        offset += 1
        if char == "\\":
            if offset == len(value) or value[offset] not in escapes:
                raise ValueError("invalid STOMP escape")
            char = escapes[value[offset]]
            offset += 1
        result.append(char)
    return "".join(result)


def encode(command: str, headers: dict[str, str], body: bytes = b"") -> bytes:
    headers = dict(headers)
    if body or command == "SEND":
        headers["content-length"] = str(len(body))
    esc = command not in ("CONNECT", "CONNECTED")
    lines = [command, *(f"{escape(k) if esc else k}:{escape(str(v)) if esc else v}" for k, v in headers.items())]
    return ("\n".join(lines) + "\n\n").encode() + body + b"\0"


class Parser:
    def __init__(self, max_frame: int = MAX_FRAME):
        self.buffer = bytearray()
        self.max_frame = max_frame

    def feed(self, data: bytes) -> None:
        if len(self.buffer) + len(data) > self.max_frame:
            raise ValueError("bounded STOMP receive buffer exceeded")
        self.buffer.extend(data)

    def pop(self):
        while self.buffer and self.buffer[0] in (10, 13):
            del self.buffer[0]
        end = self.buffer.find(b"\n\n")
        delimiter = 2
        if end < 0:
            end = self.buffer.find(b"\r\n\r\n")
            delimiter = 4
        if end < 0:
            return None
        if end > 1024 * 1024:
            raise ValueError("STOMP header exceeds one MiB")
        lines = bytes(self.buffer[:end]).decode("utf-8").splitlines()
        if not lines:
            raise ValueError("empty STOMP command")
        command = lines[0]
        headers = {}
        for line in lines[1:]:
            if ":" not in line:
                raise ValueError("invalid STOMP header")
            key, value = line.split(":", 1)
            if command not in ("CONNECT", "CONNECTED"):
                key, value = unescape(key), unescape(value)
            headers.setdefault(key, value)
        start = end + delimiter
        if "content-length" in headers:
            size = int(headers["content-length"])
            if size < 0 or size > self.max_frame:
                raise ValueError("invalid STOMP body size")
            stop = start + size
            if len(self.buffer) <= stop:
                return None
            if self.buffer[stop] != 0:
                raise ValueError("STOMP content-length terminator mismatch")
        else:
            stop = self.buffer.find(b"\0", start)
            if stop < 0:
                return None
        body = bytes(self.buffer[start:stop])
        del self.buffer[:stop + 1]
        return command, headers, body


class Client:
    def __init__(self, port: int, user: str, password: str, *, timeout: float = 5.0):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout)
        self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.parser = Parser()
        self.send("CONNECT", {"accept-version": "1.2", "host": "localhost", "login": user,
                              "passcode": password, "heart-beat": "0,0"})
        response = self.receive(timeout)
        if response[0] != "CONNECTED":
            self.close()
            # Broker error text may echo a credential or body; never propagate it.
            raise ConnectionError("local broker refused STOMP connection")
        self.connected_headers = response[1]

    def send(self, command: str, headers: dict[str, str], body: bytes = b"") -> None:
        self.socket.sendall(encode(command, headers, body))

    def receive(self, timeout: float = 5.0):
        deadline = time.monotonic() + timeout
        while True:
            frame = self.parser.pop()
            if frame:
                return frame
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("local STOMP frame deadline")
            self.socket.settimeout(remaining)
            data = self.socket.recv(256 * 1024)
            if not data:
                raise ConnectionError("local STOMP connection closed")
            self.parser.feed(data)

    def subscribe(self, destination: str, *, ack: str = "auto", headers=None) -> str:
        key = uuid.uuid4().hex
        values = {"destination": destination, "id": key, "ack": ack,
                  "subscription-type": "MULTICAST", "receipt": "sub-" + key}
        values.update(headers or {})
        self.send("SUBSCRIBE", values)
        return key

    def close(self) -> None:
        if self.socket is not None:
            try:
                self.send("DISCONNECT", {})
            except (OSError, ConnectionError):
                pass
            self.socket.close()
            self.socket = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
