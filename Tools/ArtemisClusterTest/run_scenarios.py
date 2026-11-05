"""Owned local Artemis cluster scenarios, after all Unreal perf/memory gates.

Uses the opt-in NullRHI automation probe for real product channels. No native/GPU
performance claim is made by this tool. Run under the worktree whose Editor was
built; output is restricted to its Saved/Reports tree.
"""
from __future__ import annotations

import argparse
import base64
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import threading
import time
import urllib.request

import psutil

from configure import NODES, ROOT, configure, read_env, haproxy_config
from stomp_wire import Client

PROJECT = ROOT.parents[1]
PCD_TOPIC = "topic.virtual.sensor.export.0"
JSON_TOPIC = "topic.virtual.sensor.lidar.0"
PORTS = {"live-a": 28161, "live-b": 28162, "backup-a": 28163, "backup-b": 28164}


def dump(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, indent=2), encoding="utf-8")
    temp.replace(path)


def complete_suite(results: list[dict], expected: list[str], failure, cleanup_errors) -> bool:
    return ([row["name"] for row in results] == expected
            and all(row["pass"] for row in results) and failure is None and not cleanup_errors)


class Cluster:
    def __init__(self, report: Path):
        self.report = report
        self.env = read_env(ROOT / ".local/test.env")
        self.command = ["docker", "compose", "--project-name", "dtpcd-if10", "--env-file",
                        str(ROOT / ".local/test.env"), "-f", str(ROOT / "compose.yaml")]

    def scrub(self, text: str) -> str:
        for key in ("ARTEMIS_PASSWORD", "CLUSTER_PASSWORD"):
            text = text.replace(self.env[key], "[redacted]")
        auth = base64.b64encode(f"{self.env['ARTEMIS_USER']}:{self.env['ARTEMIS_PASSWORD']}".encode()).decode()
        text = text.replace(auth, "[redacted]")
        return re.sub(r"(tcp-check send-binary\s+)[0-9a-fA-F]+", r"\1[redacted]", text)

    def call(self, args: list[str], *, check=True, timeout=300) -> str:
        result = subprocess.run(self.command + args, cwd=ROOT, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=timeout)
        output = self.scrub(result.stdout + result.stderr)
        with (self.report / "docker-driver.log").open("a", encoding="utf-8") as log:
            log.write(json.dumps({"args": args, "exit": result.returncode}) + "\n" + output + "\n")
        if check and result.returncode:
            raise RuntimeError(f"owned compose command failed (exit={result.returncode}); see sanitized driver log")
        return self.scrub(result.stdout)

    def jolokia(self, node: str, request: dict):
        auth = base64.b64encode(f"{self.env['ARTEMIS_USER']}:{self.env['ARTEMIS_PASSWORD']}".encode()).decode()
        req = urllib.request.Request(f"http://127.0.0.1:{PORTS[node]}/console/jolokia/",
                                     data=json.dumps(request).encode(), headers={
                                         "Content-Type": "application/json", "Authorization": "Basic " + auth,
                                         "Origin": f"http://127.0.0.1:{PORTS[node]}"})
        with urllib.request.urlopen(req, timeout=5) as response:
            result = json.load(response)
        if result.get("status") != 200:
            raise RuntimeError("local management request unavailable")
        return result.get("value")

    def broker(self, node: str) -> str:
        return f'org.apache.activemq.artemis:broker="{node}"'

    def snapshot(self) -> dict:
        output = {"time": time.time(), "nodes": {}}
        for node in NODES:
            row = {"available": False}
            try:
                row["active"] = self.jolokia(node, {"type": "read", "mbean": self.broker(node), "attribute": "Active"})
                row["heap"] = self.jolokia(node, {"type": "read", "mbean": "java.lang:type=Memory", "attribute": "HeapMemoryUsage"})
                address = self.broker(node) + f',component=addresses,address="{PCD_TOPIC}"'
                try:
                    row["address_bytes"] = self.jolokia(node, {"type": "read", "mbean": address, "attribute": "AddressSize"})
                except Exception:
                    row["address_bytes"] = None
                try:
                    row["pcd_queue_names"] = self.jolokia(node, {"type": "read", "mbean": address, "attribute": "QueueNames"})
                    row["pcd_messages"] = self.jolokia(node, {"type": "read", "mbean": address, "attribute": "NumberOfMessages"})
                except Exception:
                    row["pcd_messages"] = None
                camera = self.broker(node) + ',component=addresses,address="topic.virtual.sensor.camera.0"'
                try:
                    row["diagnostic_camera_queues"] = self.jolokia(node, {"type": "read", "mbean": camera, "attribute": "QueueNames"})
                except Exception:
                    row["diagnostic_camera_queues"] = None
                row["available"] = True
                try:
                    row["connections"] = self.jolokia(node, {"type": "read", "mbean": self.broker(node), "attribute": "ConnectionCount"})
                except Exception:
                    row["connections"] = None
            except Exception as error:
                row["error_type"] = type(error).__name__
            output["nodes"][node] = row
        with (self.report / "broker-snapshots.jsonl").open("a", encoding="utf-8") as file:
            file.write(json.dumps(output) + "\n")
        return output

    def wait_ready(self, timeout=180) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = self.snapshot()
            if all(state["nodes"][node].get("active") for node in ("live-a", "live-b")):
                try:
                    with Client(27616, self.env["ARTEMIS_USER"], self.env["ARTEMIS_PASSWORD"]):
                        time.sleep(10)
                        return
                except (ConnectionError, OSError, TimeoutError):
                    # Broker Active precedes HAProxy's required two readiness
                    # successes. Keep the same bounded startup deadline.
                    pass
            time.sleep(3)
        raise RuntimeError("two live brokers/VIP not ready; inspect sanitized logs")

    def wait_vip(self, timeout=60) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                with Client(27616, self.env["ARTEMIS_USER"], self.env["ARTEMIS_PASSWORD"]):
                    return
            except (ConnectionError, OSError, TimeoutError):
                time.sleep(2)
        raise RuntimeError("VIP did not become ready after owned proxy restart")

    def disk(self) -> dict:
        result = {}
        for node in NODES:
            result[node] = self.call(["exec", "-T", node, "sh", "-c",
                                     "du -sk ./data/paging ./data/journal ./data/large-messages"], check=False)
        return result

    def logs(self) -> None:
        self.call(["logs", "--no-color", "--tail", "400"], check=False)


def check_pcd(headers: dict[str, str], body: bytes) -> int:
    if headers.get("schema") != "virtual-pointcloud.pcd.v1":
        raise ValueError("unexpected PCD schema")
    if hashlib.sha1(body).hexdigest() != headers.get("checksum", "").lower():
        raise ValueError("PCD SHA1 mismatch")
    marker = b"DATA binary\n"
    offset = body.find(marker)
    if offset < 0 or offset > 1024 * 1024:
        raise ValueError("synthetic probe must use uncompressed binary PCD")
    header = body[:offset].decode("utf-8")
    values = {line.split()[0]: line.split()[1:] for line in header.splitlines()
              if line and not line.startswith("#")}
    count = int(values["POINTS"][0])
    sizes = list(map(int, values["SIZE"]))
    counts = list(map(int, values.get("COUNT", ["1"] * len(sizes))))
    stride = sum(a * b for a, b in zip(sizes, counts))
    records = body[offset + len(marker):]
    if stride != 33 or len(records) != count * stride or int(headers["point-count"]) != count:
        raise ValueError("PCD 33-byte/count mismatch")
    # Probe fixture repeats one known point. This compares every record byte,
    # without a Python per-point loop becoming the receiver's bottleneck.
    record = records[:stride]
    if records != record * count or struct.unpack_from("<fff", record) != (1.0, 2.0, 3.0):
        raise ValueError("synthetic point oracle mismatch")
    return count


class Consumer:
    def __init__(self, cluster: Cluster, *, port=27616, pause=False):
        self.cluster = cluster
        self.port = port
        self.pause = pause
        self.stop = threading.Event()
        self.ready = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.result = {"frames": 0, "points": 0, "invalid": 0, "duplicates": 0,
                       "out_of_order": 0, "gap_frames": 0, "connections": 0,
                       "first": {}, "last": {}, "errors": [], "json_messages": 0,
                       "json_interarrival_ms": [], "max_frame_age_ms": 0}

    def run(self):
        previous = {}
        while not self.stop.is_set():
            try:
                with Client(self.port, self.cluster.env["ARTEMIS_USER"], self.cluster.env["ARTEMIS_PASSWORD"]) as client:
                    self.result["connections"] += 1
                    # Explicit ACK replenishes Artemis' byte window, including
                    # frames larger than one MiB. Pause mode deliberately omits
                    # the first PCD ACK to exercise DROP without unbounded RAM.
                    client.subscribe(PCD_TOPIC, ack="client-individual",
                                     headers={"consumer-window-size": "4194304"})
                    client.subscribe(JSON_TOPIC, ack="client-individual")
                    ready_receipts = 0
                    last_json = None
                    while not self.stop.is_set():
                        try:
                            command, headers, body = client.receive(1)
                        except (TimeoutError, socket.timeout):
                            continue
                        if command == "RECEIPT":
                            ready_receipts += 1
                            if ready_receipts == 2:
                                self.ready.set()
                            continue
                        if command == "ERROR":
                            raise ConnectionError("local broker ERROR (body suppressed)")
                        if command != "MESSAGE":
                            continue
                        if headers.get("destination") == JSON_TOPIC:
                            self.result["json_messages"] += 1
                            stamp = time.monotonic()
                            if last_json is not None:
                                self.result["json_interarrival_ms"].append((stamp - last_json) * 1000)
                            last_json = stamp
                            if headers.get("ack"):
                                client.send("ACK", {"id": headers["ack"]})
                            continue
                        self.result["frames"] += 1
                        try:
                            self.result["points"] += check_pcd(headers, body)
                            sensor, frame = headers["sensor-id"], int(headers["frame-id"])
                            self.result["first"].setdefault(sensor, frame)
                            self.result["last"][sensor] = frame
                            utc = datetime.datetime.fromisoformat(headers["timestamp-utc"].replace("Z", "+00:00"))
                            age = (datetime.datetime.now(datetime.timezone.utc) - utc).total_seconds() * 1000
                            self.result["max_frame_age_ms"] = max(self.result["max_frame_age_ms"], age)
                            if sensor in previous:
                                delta = frame - previous[sensor]
                                self.result["duplicates"] += delta == 0
                                self.result["out_of_order"] += delta < 0
                                self.result["gap_frames"] += max(0, delta - 1)
                            previous[sensor] = frame
                        except (ValueError, KeyError, struct.error):
                            self.result["invalid"] += 1
                        if self.pause:
                            # Keep the subscription alive without consuming or
                            # acknowledging. Otherwise broker TTL deletes the
                            # temporary queue and hides the intended backlog.
                            while not self.stop.wait(5):
                                client.socket.sendall(b"\n")
                        elif headers.get("ack"):
                            client.send("ACK", {"id": headers["ack"]})
                        else:
                            raise ValueError("client-individual delivery lacks ACK id")
            except Exception as error:
                self.result["errors"].append(type(error).__name__)
                self.stop.wait(1)

    def start(self):
        self.thread.start()
        if not self.ready.wait(20):
            self.close()
            raise RuntimeError("local consumer subscription did not become ready")
        return self

    def close(self):
        self.stop.set()
        self.thread.join(timeout=8)


def assert_unreal_slot() -> None:
    names = {"unrealeditor.exe", "unrealeditor-cmd.exe", "ma0t10_dt.exe", "dtdevicetesthost.exe",
             "ma0t10_dt-win64-shipping.exe", "cl.exe", "shadercompileworker.exe"}
    if any((proc.info["name"] or "").lower() in names for proc in psutil.process_iter(["name"])):
        raise RuntimeError("Unreal/build slot is busy; no external process was stopped")


def process_broker_connections(process, ports: tuple[int, int]) -> int:
    # psutil 5 uses Process.connections; newer releases renamed that method.
    query = getattr(process, "net_connections", None) or process.connections
    return sum(c.status == psutil.CONN_ESTABLISHED and bool(c.raddr)
               and c.raddr.ip in ("127.0.0.1", "::1") and c.raddr.port in ports
               for c in query(kind="tcp"))


def native_case(cluster: Cluster, name: str, *, pcd=True, seconds=90, points=40000,
                tcp=27616, ws=27616, consumer_port=27616, kill_live=False, pause=False, shared_json=False) -> dict:
    assert_unreal_slot()
    case = cluster.report / name
    case.mkdir()
    consumer = Consumer(cluster, port=consumer_port, pause=pause).start()
    env = os.environ.copy()
    env.update(DTDEVICE_ARTEMIS_RUN="1", DTDEVICE_ARTEMIS_TCP_URL=f"tcp://127.0.0.1:{tcp}",
               DTDEVICE_ARTEMIS_WS_URL=f"ws://127.0.0.1:{ws}",
               DTDEVICE_ARTEMIS_USER=cluster.env["ARTEMIS_USER"], DTDEVICE_ARTEMIS_PASSCODE=cluster.env["ARTEMIS_PASSWORD"],
               DTDEVICE_ARTEMIS_SECONDS=str(seconds), DTDEVICE_ARTEMIS_POINTS=str(points), DTDEVICE_ARTEMIS_PCD="1" if pcd else "0")
    env["DTDEVICE_ARTEMIS_JSON_MODE"] = "Default" if shared_json else "sensor"
    editor = Path("C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe")
    command = [str(editor), str(PROJECT / "ma0t10_dt.uproject"), "/Engine/Maps/Entry", "-NullRHI", "-unattended",
               "-nosound", "-nosplash", "-nop4", "-RenderOffscreen", f"-UserDir={case / 'user'}",
               f"-abslog={case / 'automation.log'}", f"-ReportExportPath={case / 'automation'}",
               "-ExecCmds=t.MaxFPS 60,t.IdleWhenNotForeground 0,Automation RunTests MA0T10.DataIf10.BrokerRuntime; Quit", "-TestExit=Automation Test Queue Empty"]
    started = time.monotonic()
    killed = False
    snapshots = []
    socket_samples = []
    disk_samples = []
    with (case / "console.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=PROJECT, env=env, stdout=log, stderr=subprocess.STDOUT,
                                   stdin=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            while process.poll() is None:
                try:
                    count = process_broker_connections(psutil.Process(process.pid), (tcp, ws))
                    socket_samples.append({"time": time.time(), "pid": process.pid, "broker_established": count})
                except (psutil.AccessDenied, psutil.NoSuchProcess):
                    socket_samples.append({"time": time.time(), "pid": process.pid, "broker_established": None})
                if time.monotonic() - started > seconds + 150:
                    process.terminate()
                    raise TimeoutError("owned broker probe deadline")
                if kill_live and not killed and time.monotonic() - started > 40:
                    cluster.call(["kill", "-s", "KILL", "live-a"])
                    killed = True
                snapshots.append(cluster.snapshot())
                disk_samples.append({"time": time.time(), "nodes": cluster.disk()})
                time.sleep(5)
        finally:
            consumer.close()
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=15)
    summary_path = case / "user/Saved/Reports/DATA-IF-10/BrokerRuntime/summary.json"
    if not summary_path.exists():
        raise RuntimeError(f"real-channel functional summary missing: {name}")
    summary = json.loads(summary_path.read_text(encoding="utf-8-sig"))
    tests_path = case / "automation/index.json"
    tests = json.loads(tests_path.read_text(encoding="utf-8-sig"))["tests"] if tests_path.exists() else []
    result = {"name": name, "exit": process.returncode, "scope": "NullRHI real channels / synthetic PCD / not performance gate",
              "sender": summary, "consumer": consumer.result, "forced_live_kill": killed,
              "snapshots": snapshots, "process_socket_samples": socket_samples, "disk": cluster.disk(), "disk_samples_while_running": disk_samples,
              "pass": process.returncode == 0 and len(tests) == 1 and tests[0]["state"] == "Success" and consumer.result["invalid"] == 0}
    result["pass"] &= 19.8 <= summary["scheduled_hz"] <= 20.2
    if pcd and not pause:
        result["pass"] &= set(consumer.result["first"]) == {"DATAIF-1", "DATAIF-2", "DATAIF-3"}
        result["delivered_frames_per_second_all_streams"] = consumer.result["frames"] / summary["seconds"]
    if not pause:
        result["pass"] &= consumer.result["json_messages"] > 0
    if "order" in name:
        result["pass"] &= consumer.result["out_of_order"] == 0 and consumer.result["duplicates"] == 0
    if kill_live:
        result["pass"] &= summary["json_connections"] >= 2 and summary["bulk_connection_revision"] >= 4
        result["pass"] &= summary.get("json_subscription_receipts", 0) >= 2 and summary.get("json_messages", 0) > 0
        camera = [(snap["time"], node, row.get("diagnostic_camera_queues")) for snap in snapshots
                  for node, row in snap["nodes"].items() if row.get("active") and row.get("diagnostic_camera_queues")]
        result["raw_resubscription_broker_evidence"] = camera
        result["pass"] &= bool(camera) and any(node == "backup-a" for _, node, _ in camera)
    if name == "d-paused-consumer-drop":
        samples = [row["address_bytes"] for snap in snapshots for row in snap["nodes"].values()
                   if row.get("active") and isinstance(row.get("address_bytes"), (int, float))]
        limit = 64 * 1024 * 1024
        # Address accounting admits whole frames. Retain the observed overshoot,
        # and cap it independently rather than claiming total JVM RSS <= address limit.
        result["address_bound"] = {"limit_bytes": limit, "observed_peak_bytes": max(samples) if samples else None,
                                   "whole_frame_margin_bytes": 4 * summary["frame_bytes"]}
        result["pass"] &= bool(samples) and limit * .8 <= max(samples) <= limit + 4 * summary["frame_bytes"]
    dump(case / "result.json", result)
    # Raw Unreal output is local-only; defensively redact test credentials before retaining it.
    for file in case.rglob("*.log"):
        text = file.read_text(encoding="utf-8", errors="replace")
        file.write_text(cluster.scrub(text), encoding="utf-8")
    return result


def run(args) -> None:
    memory = json.loads(args.memory_state.read_text(encoding="utf-8-sig"))
    if memory.get("State") not in ("Completed", "completed", "measurements-complete") or len(memory.get("Selected", memory.get("Completed", []))) < 3:
        raise RuntimeError("three memory scenes must finish before Docker scenarios")
    followup = args.memory_state.parent / "r55d-followup-plan.json"
    if followup.exists():
        finished = json.loads((followup.parent / "r55d-followup-status.json").read_text(encoding="utf-8-sig"))
        hatch = memory.get("Selected", {}).get("hatch180", {})
        if (finished.get("Status") != "Completed" or hatch.get("Name") != "hatch180-a2"
                or hatch.get("TargetSeconds") != 10800):
            raise RuntimeError("user r55d hatch3h retry must finish before Docker scenarios")
    report = args.report.resolve()
    report.relative_to((PROJECT / "Saved/Reports").resolve())
    report.mkdir(parents=True, exist_ok=False)
    assert_unreal_slot()
    configure()
    cluster = Cluster(report)
    results = []
    failure = None
    expected_cases = ["a-json-only", "a-pcd3-json", "b-cross-live-order", "c-live-failover",
                      "d-paused-consumer-drop", "d-fresh-subscription", "e-under-large-threshold",
                      "e-default-frame", "e-large-message-enabled"]
    try:
        cluster.call(["config", "--quiet"])
        cluster.call(["up", "-d"], timeout=600)
        cluster.wait_ready()
        image_metadata = {}
        for image in (cluster.env["ARTEMIS_IMAGE"], "haproxy:3.0-alpine"):
            inspected = subprocess.run(["docker", "image", "inspect", image, "--format", "{{json .RepoDigests}}"],
                                       capture_output=True, text=True, timeout=30)
            image_metadata[image] = json.loads(inspected.stdout) if inspected.returncode == 0 else {"available": False}
        dump(report / "image-digests.json", image_metadata)
        for name, pcd in (("a-json-only", False), ("a-pcd3-json", True)):
            results.append(native_case(cluster, name, pcd=pcd))
        results.append(native_case(cluster, "b-cross-live-order", tcp=27617, ws=27618, consumer_port=27618))
        # Route both product connections through pair A for this one failover case;
        # pair B stays in the cluster/quorum. Restore the ordinary VIP afterwards.
        config = haproxy_config(cluster.env["ARTEMIS_USER"], cluster.env["ARTEMIS_PASSWORD"])
        paired = config.replace("server live-b live-b:61616 check", "server live-b live-b:61616 disabled check")
        paired = paired.replace("server backup-b backup-b:61616 check", "server backup-b backup-b:61616 disabled check")
        (ROOT / ".local/haproxy.cfg").write_text(paired, encoding="utf-8")
        cluster.call(["up", "-d", "--no-deps", "--force-recreate", "vip"])
        cluster.wait_vip()
        results.append(native_case(cluster, "c-live-failover", seconds=150, kill_live=True))
        (ROOT / ".local/haproxy.cfg").write_text(config, encoding="utf-8")
        cluster.call(["up", "-d", "--no-deps", "--force-recreate", "vip"])
        cluster.wait_vip()
        # Keep the promoted backup active for remaining probes. No failback race is hidden.
        results.append(native_case(cluster, "d-paused-consumer-drop", seconds=60, points=2000, pause=True,
                                   tcp=27618, ws=27618, consumer_port=27618))
        results.append(native_case(cluster, "d-fresh-subscription", seconds=30))
        results.append(native_case(cluster, "e-under-large-threshold", seconds=30, points=2000))
        results.append(native_case(cluster, "e-default-frame", seconds=30, points=40000))
        # Separate cold broker instance for a lower large-message threshold; never change the game payload.
        cluster.call(["down", "--volumes", "--remove-orphans"])
        configure(threshold=64 * 1024)
        cluster.call(["up", "-d"])
        cluster.wait_ready()
        results.append(native_case(cluster, "e-large-message-enabled", seconds=15, points=40000, pause=True))
    except Exception as error:
        failure = {"type": type(error).__name__, "message": cluster.scrub(str(error))}
    finally:
        cleanup_errors = []
        for action in (("logs", "--no-color", "--tail", "400"), ("down", "--volumes", "--remove-orphans")):
            try:
                cluster.call(list(action), check=False)
            except Exception as error:
                cleanup_errors.append(type(error).__name__)
        overall = complete_suite(results, expected_cases, failure, cleanup_errors)
        comparison = None
        if len(results) >= 2:
            before, loaded = results[0]["sender"], results[1]["sender"]
            comparison = {"scope": "ordinary JSON broker receipt; not consumer processing ACK",
                          "pcd_off_mean_ms": before.get("json_receipt_mean_ms"),
                          "pcd_on_mean_ms": loaded.get("json_receipt_mean_ms"),
                          "pcd_off_p95_ms": before.get("json_receipt_p95_ms"),
                          "pcd_on_p95_ms": loaded.get("json_receipt_p95_ms")}
        dump(report / "summary.json", {"cases": results, "expected_cases": expected_cases, "pass": overall,
                                       "failure": failure, "cleanup_errors": cleanup_errors, "ordinary_latency": comparison,
                                       "note": "DROP retains old queued messages; fresh-frame recovery requires a new non-durable subscription, not durable backlog replay."})
    if not overall:
        raise RuntimeError("broker scenarios incomplete or failed; see sanitized summary")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--memory-state", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    run(parser.parse_args())
