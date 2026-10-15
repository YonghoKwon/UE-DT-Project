"""AUTO-01 loopback client. Tokens are read locally and never included in diagnostics."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, build_opener, ProxyHandler, HTTPRedirectHandler


class ControlError(RuntimeError):
    pass


class _NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None  # never forward local authorization to a different endpoint/process


class Client:
    def __init__(self, port: int = 8099, *, token: str | None = None, timeout: float = 10):
        if isinstance(port, bool) or not isinstance(port, int) or not 1024 <= port <= 65535:
            raise ValueError("port must be an integer in 1024..65535")
        self.base = f"http://127.0.0.1:{port}"
        self._token = os.environ.get("MA0T10_AUTOMATION_TOKEN", "") if token is None else token
        if len(self._token) > 256 or any(c in self._token for c in "\r\n"):
            raise ValueError("invalid authentication configuration")
        self.timeout = timeout
        self._http = build_opener(ProxyHandler({}), _NoRedirect())

    def request(self, path: str, body: dict | None = None) -> dict:
        if not path.startswith("/v1/") or "?" in path or "#" in path or "\r" in path or "\n" in path:
            raise ValueError("invalid API path")
        headers = {"Accept": "application/json"}
        if self._token:
            headers["Authorization"] = "Bearer " + self._token
        data = None if body is None else json.dumps(body, allow_nan=False).encode("utf-8")
        if data is not None:
            headers["Content-Type"] = "application/json"
        req = Request(self.base + path, data=data, headers=headers)
        try:
            with self._http.open(req, timeout=self.timeout) as response:
                result = json.load(response)
        except HTTPError as exc:
            # Do not echo request headers or untrusted response content.
            raise ControlError(f"API rejected request (HTTP {exc.code})") from None
        except (URLError, TimeoutError, OSError):
            raise ControlError("Local control API is unavailable") from None
        if not isinstance(result, dict) or not result.get("ok"):
            raise ControlError("API returned an unsuccessful result")
        return result

    def status(self) -> dict:
        return self.request("/v1/status")

    def sensors(self) -> list[dict]:
        return self.request("/v1/sensors")["sensors"]

    def wait(self, operation_id: str, timeout: float = 300) -> dict:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = self.status()
            if state["operation_id"] != operation_id:
                raise ControlError("Operation was superseded; completion is unknown")
            if state["terminal"]:
                if not state["success"]:
                    raise ControlError("Operation failed; inspect status error and detail")
                return state
            time.sleep(.2)
        raise ControlError("Operation wait timed out; operation may still be active")

    def capture(self, sensor_id: str) -> dict:
        result = self.request("/v1/capture", {"sensor_id": sensor_id})
        return self.wait(result["operation_id"])


def make_job(scenario: str, slab: str, lidar: str, camera: str, *, scenes: int = 20) -> dict:
    if not 1 <= scenes <= 10000:
        raise ValueError("scenes must be 1..10000")
    return {
        "schema": "ma0t10.job.v1", "result_path": "result.json", "timeout_sec": 600,
        "steps": [
            {"op": "sensor.stop", "sensor_id": camera},
            {"op": "sensor.stop", "sensor_id": lidar},
            {"op": "sensor.set", "sensor_id": camera, "profile": "IntelRealSenseD455", "quality": "FullSpec"},
            {"op": "sensor.set", "sensor_id": lidar, "profile": "IYOBOT_MLX80_NATIVE", "quality": "FullSpec"},
            {"op": "scenario.start", "json_path": str(Path(scenario).resolve()), "timeout_sec": 180},
            {"op": "dataset.start", "name": "AUTO01", "slab": slab, "lidar_id": lidar, "camera_id": camera,
             "scenes": scenes, "seed": 22015, "camera_truth": True, "lidar_fidelity": True,
             "ranges": {"yaw": [-10, 10], "sensor_position": [-1, 1]}, "timeout_sec": 180},
            {"op": "pcd.run", "sensor_id": lidar, "seconds": 30, "timeout_sec": 90},
        ],
    }


def check_result(path: Path) -> dict:
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ControlError("Result exceeds the reader limit")
    result = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(result, dict) or result.get("schema") != "ma0t10.job.result.v1" or result.get("ok") is not True:
        raise ControlError("Batch failed or result schema is invalid")
    steps = result.get("steps")
    if not isinstance(steps, list) or not steps or any(
        not isinstance(step, dict) or step.get("ok") is not True or not isinstance(step.get("op"), str)
        for step in steps
    ):
        raise ControlError("Batch contains failed or missing steps")
    return result


def score_generated(result_path: Path, output: Path) -> None:
    result = check_result(result_path)
    datasets = [s["detail"]["directory"] for s in result["steps"] if s["op"] == "dataset.start"]
    if len(datasets) != 1:
        raise ControlError("Expected exactly one completed dataset")
    scorer = Path(__file__).resolve().parents[1] / "DatasetTools" / "score_dataset.py"
    subprocess.run([sys.executable, str(scorer), datasets[0], "--output", str(output)], check=True)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", type=int, default=8099)
    p.add_argument("--config", type=Path, help="local JSON with port/token; contents are never printed")
    sub = p.add_subparsers(dest="command", required=True)
    sub.add_parser("status"); sub.add_parser("sensors")
    call = sub.add_parser("call"); call.add_argument("endpoint"); call.add_argument("json_file", type=Path)
    capture = sub.add_parser("capture"); capture.add_argument("sensor_id")
    job = sub.add_parser("make-job"); job.add_argument("output", type=Path)
    for field in ("scenario", "slab", "lidar", "camera"):
        job.add_argument("--" + field, required=True)
    job.add_argument("--scenes", type=int, default=20)
    check = sub.add_parser("check"); check.add_argument("result", type=Path)
    score = sub.add_parser("score"); score.add_argument("result", type=Path); score.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    try:
        if a.command == "make-job":
            a.output.write_text(json.dumps(make_job(a.scenario, a.slab, a.lidar, a.camera, scenes=a.scenes), indent=2), encoding="utf-8")
            return 0
        if a.command == "check":
            r = check_result(a.result); print(json.dumps({"ok": True, "elapsed_sec": r["elapsed_sec"]})); return 0
        if a.command == "score":
            score_generated(a.result, a.output); return 0
        cfg = json.loads(a.config.read_text(encoding="utf-8-sig")) if a.config else {}
        c = Client(cfg.get("port", a.port), token=cfg.get("token"))
        if a.command == "status": result = c.status()
        elif a.command == "sensors": result = c.sensors()
        elif a.command == "capture": result = c.capture(a.sensor_id)
        else: result = c.request(a.endpoint, json.loads(a.json_file.read_text(encoding="utf-8-sig")))
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    except (ControlError, ValueError, OSError, subprocess.CalledProcessError):
        print("Automation request failed; inspect local result/status and configuration.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
