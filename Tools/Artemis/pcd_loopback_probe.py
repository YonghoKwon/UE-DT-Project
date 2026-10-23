"""Credential-free local STOMP receipt/echo fixture for real PCD throughput tests.

python Tools/Artemis/pcd_loopback_probe.py --port 18625 --report <json> --stop-file <path>
Run MA0T10.GpuLidar.ScheduledThree+MA0T10.GpuLidar.ScheduledOS1Three with
-GpuLidarPcdLoopbackPort=18625. Create the stop file after Editor exits.
Binds only 127.0.0.1; no payload or CONNECT headers are persisted.
This is a bounded test receiver, not an Artemis or deployment acceptance test.
"""
import argparse
import asyncio
import hashlib
import json
import time
from pathlib import Path

MAX_BODY = 8 * 1024 * 1024
# PCD 0.7/PCL field layout. Full remains the default; optional stream profiles
# select a subset with mandatory XYZ. Storage writers continue using Full.
FIELD_TYPES = dict(zip(
    "x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence".split(),
    [(4,"F"),(4,"F"),(4,"F"),(2,"U"),(2,"U"),(2,"U"),(1,"U"),(1,"U"),(8,"I"),(1,"U"),(4,"F")]))


def decode_lzf(data, expected):
    if not 0 <= expected <= MAX_BODY:
        raise ValueError("LZF output exceeds fixture bound")
    out = bytearray()
    i = 0
    while i < len(data):
        control = data[i]
        i += 1
        if control < 32:
            n = control + 1
            if i+n > len(data) or len(out)+n > expected:
                raise ValueError("truncated/overflow LZF literal")
            out.extend(data[i:i+n])
            i += n
        else:
            n = control >> 5
            if n == 7:
                if i >= len(data):
                    raise ValueError("truncated LZF match length")
                n += data[i]
                i += 1
            if i >= len(data):
                raise ValueError("truncated LZF distance")
            distance = ((control & 31) << 8) + data[i] + 1
            i += 1
            n += 2
            if distance > len(out) or len(out)+n > expected:
                raise ValueError("invalid LZF reference/output size")
            block = bytes(out[-distance:])
            out.extend((block * ((n+distance-1)//distance))[:n])
    if len(out) != expected:
        raise ValueError("LZF size mismatch")
    return out


def validate_pcd(body, headers):
    import struct
    start = 0
    fields = {}
    for line in body[:65536].splitlines(keepends=True):
        start += len(line)
        if line.startswith(b"#"):
            continue
        if not line.endswith(b"\n"):
            raise ValueError("truncated PCD header")
        key, _, value = line.decode("utf-8").strip().partition(" ")
        if key in fields:
            raise ValueError("duplicate PCD header")
        fields[key] = value
        if key == "DATA":
            break
    if fields.get("VERSION") != "0.7" or fields.get("DATA") not in ("binary", "binary_compressed"):
        raise ValueError("unsupported PCD version/data")
    names = fields["FIELDS"].split()
    sizes = [int(v) for v in fields["SIZE"].split()]
    types = fields["TYPE"].split()
    counts = [int(v) for v in fields["COUNT"].split()]
    if len(names) != len(set(names)) or not {"x", "y", "z"} <= set(names):
        raise ValueError("PCD requires unique fields and XYZ")
    if not len(names) == len(sizes) == len(types) == len(counts):
        raise ValueError("PCD field descriptor lengths differ")
    for name, size, kind, count in zip(names, sizes, types, counts):
        if FIELD_TYPES.get(name) != (size, kind) or count != 1:
            raise ValueError("PCD field type differs")
    points = int(fields["POINTS"])
    expected = points * sum(sizes)
    if points < 0 or expected > MAX_BODY or int(fields["WIDTH"]) * int(fields["HEIGHT"]) != points:
        raise ValueError("PCD dimensions/size differ")
    data = body[start:]
    if fields["DATA"] == "binary_compressed":
        if len(data) < 8:
            raise ValueError("missing LZF size prefix")
        compressed, uncompressed = struct.unpack_from("<II", data)
        if uncompressed != expected or compressed != len(data)-8:
            raise ValueError("LZF size prefix differs")
        decode_lzf(data[8:], uncompressed)
    elif len(data) != expected:
        raise ValueError("PCD record length differs")
    if int(headers["point-count"]) != points:
        raise ValueError("point count header differs")
    if hashlib.sha1(body).hexdigest() != headers["checksum"]:
        raise ValueError("PCD checksum differs")
    return points


async def main(args):
    report = {"scope": "127.0.0.1 credential-free STOMP receipt + echo fixture",
              "started_unix": time.time(), "connections": 0, "invalid": 0,
              "schema_record_bytes": 33, "sensors": {}, "errors": []}
    clients = set()

    async def serve(reader, writer):
        clients.add(writer)
        report["connections"] += 1
        subscriptions = {}
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                command = line.strip().decode("ascii")
                if not command:
                    continue
                headers = {}
                while True:
                    line = await reader.readline()
                    if line in (b"\n", b"\r\n"):
                        break
                    if not line:
                        raise ValueError("incomplete STOMP header")
                    key, value = line.rstrip(b"\r\n").decode().split(":", 1)
                    headers[key] = value
                size = int(headers.get("content-length", 0))
                if size < 0 or size > MAX_BODY:
                    raise ValueError("body exceeds bounded fixture limit")
                body = await reader.readexactly(size)
                if await reader.readexactly(1) != b"\0":
                    raise ValueError("invalid STOMP terminator")
                if command in ("CONNECT", "STOMP"):
                    if headers.get("login") or headers.get("passcode"):
                        raise ValueError("fixture requires empty credentials")
                    writer.write(b"CONNECTED\nversion:1.2\nheart-beat:0,0\n\n\0")
                elif command == "SUBSCRIBE":
                    subscriptions[headers["id"]] = headers["destination"]
                elif command == "UNSUBSCRIBE":
                    subscriptions.pop(headers["id"], None)
                elif command == "SEND":
                    try:
                        points = validate_pcd(body, headers)
                    except (ValueError, KeyError) as error:
                        report["invalid"] += 1
                        raise ValueError(str(error)) from error
                    sensor, frame = headers["sensor-id"], int(headers["frame-id"])
                    row = report["sensors"].setdefault(sensor, {"frames": 0, "bytes": 0,
                        "gaps": 0, "duplicates": 0, "max_gap_ms": 0,
                        "first_frame": frame, "first_unix": time.time(), "samples": []})
                    now = time.time()
                    if row["frames"]:
                        row["gaps"] += max(0, frame - row["last_frame"] - 1)
                        row["duplicates"] += frame <= row["last_frame"]
                        row["max_gap_ms"] = max(row["max_gap_ms"], 1000 * (now-row["last_unix"]))
                    row["frames"] += 1
                    row["bytes"] += size
                    row.update(last_frame=frame, last_unix=now, last_points=points)
                    # Bounded metadata only; enough for the warmup + 30 s windows.
                    if len(row["samples"]) < 8192:
                        row["samples"].append([now, frame, size])
                    receipt = headers.get("receipt", "")
                    if receipt:
                        writer.write(f"RECEIPT\nreceipt-id:{receipt}\n\n\0".encode())
                    for sub, destination in subscriptions.items():
                        if destination != headers["destination"]:
                            continue
                        echo = dict(headers, subscription=sub, **{"message-id": receipt})
                        echo.pop("receipt", None)
                        writer.write(("MESSAGE\n" + "".join(f"{k}:{v}\n" for k, v in echo.items()) + "\n").encode())
                        writer.write(body)
                        writer.write(b"\0")
                elif command == "DISCONNECT":
                    break
                await writer.drain()
        except (ConnectionError, asyncio.IncompleteReadError):
            pass  # Normal Editor teardown can close an idle/in-flight socket.
        except Exception as error:
            # Persist only bounded diagnostics; never headers or bodies.
            report["errors"].append(type(error).__name__ + ": " + str(error)[:160])
        finally:
            clients.discard(writer)
            writer.close()
            try:
                await writer.wait_closed()
            except ConnectionError:
                pass

    def save():
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")

    if args.stop_file.exists():
        raise RuntimeError("stop file already exists; use a fresh run folder")
    server = await asyncio.start_server(serve, "127.0.0.1", args.port)
    print(f"PCD loopback ready on 127.0.0.1:{args.port}", flush=True)
    async with server:
        save()
        while not args.stop_file.exists():
            await asyncio.sleep(1)
            save()
    for client in tuple(clients):
        client.close()
    report["finished_unix"] = time.time()
    save()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=18625)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--stop-file", type=Path, required=True)
    options = parser.parse_args()
    if not 1024 <= options.port <= 65535:
        parser.error("use an unprivileged local port")
    options.report.parent.mkdir(parents=True, exist_ok=True)
    asyncio.run(main(options))
