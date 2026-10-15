"""Exercise a disposable running simulator fixture; writes no simulator settings files.

Stop/start and generation deliberately change the specified runtime sensors. Use a test
map, keep the instance otherwise idle, and provide its exact IDs and Slab actor name.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import time

from client import Client, ControlError


def exercise(c, lidar, camera, slab, scenario=None):
    checks = []

    def passed(name, detail=None): checks.append({"name": name, "ok": True, "detail": detail})
    def rejected(path, body):
        try: c.request(path, body)
        except ControlError: passed("reject " + path); return
        raise AssertionError("invalid request accepted: " + path)

    assert c.status()["api_enabled"]
    assert {lidar, camera} <= {s["sensor_id"] for s in c.sensors()}
    passed("status and inventory")
    for path in ("sensor", "sensor/start", "sensor/stop", "capture", "pcd/start", "scenario/start", "dataset/start"):
        rejected("/v1/" + path, {})
    for path in ("pcd/stop", "scenario/stop", "dataset/stop"):
        rejected("/v1/" + path, {})
    rejected("/v1/sensor", {"sensor_id": lidar, "interval_sec": "0.1"})
    rejected("/v1/sensor", {"sensor_id": lidar, "profile": "unknown"})
    for target in (camera, lidar): c.request("/v1/sensor/stop", {"sensor_id": target})
    before = next(s for s in c.sensors() if s["sensor_id"] == camera)["revision"]
    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(lambda _: c.request("/v1/sensor", {"sensor_id": camera, "interval_sec": .1}), range(16)))
    after = next(s for s in c.sensors() if s["sensor_id"] == camera)
    assert abs(after["interval_sec"] - .1) < 1e-5 and after["revision"] >= before
    passed("16 concurrent settings requests serialized")
    c.request("/v1/sensor/start", {"sensor_id": camera})
    c.request("/v1/sensor/stop", {"sensor_id": camera})
    passed("sensor start and stop")
    for target in (camera, lidar):
        c.request("/v1/sensor/start", {"sensor_id": target})
        state = c.capture(target)
        assert state["detail"]["artifacts"] and all(Path(p).is_file() for p in state["detail"]["artifacts"])
        c.request("/v1/sensor/stop", {"sensor_id": target})
        passed("capture " + target, state["detail"])
    if scenario:
        state = c.request("/v1/scenario/start", {"json_path": str(Path(scenario).resolve())})
        state = c.wait(state["operation_id"])
        passed("scenario start and completion", state["detail"])
        first_run = state["detail"]["run_id"]
        state = c.request("/v1/scenario/start", {"json_path": str(Path(scenario).resolve())})
        state = c.wait(state["operation_id"])
        assert state["detail"]["run_id"] != first_run
        passed("same scenario file replays with a new run", state["detail"])
        state = c.request("/v1/scenario/start", {"json_path": str(Path(scenario).resolve())})
        deadline = time.monotonic() + 3
        while state["phase"] == "loading" and time.monotonic() < deadline:
            time.sleep(.05); state = c.status()
        assert state["phase"] == "playing" and not state["terminal"]
        c.request("/v1/scenario/stop", {})
        assert c.status()["terminal"] and not c.status()["success"]
        passed("scenario cancellation")
        # Existing replay subsystem owns its drain; leave time for its final status.
        time.sleep(.5)
    body = {"name": "AUTO01_api", "scenes": 3, "slab": slab, "lidar_id": lidar, "camera_id": camera,
            "camera_truth": True, "lidar_fidelity": True}
    state = c.request("/v1/dataset/start", body)
    rejected("/v1/dataset/start", body)
    state = c.wait(state["operation_id"])
    assert state["detail"]["completed_scenes"] == 3
    passed("dataset progress and completion", state["detail"])
    body["scenes"] = 100
    c.request("/v1/dataset/start", body); c.request("/v1/dataset/stop", {})
    assert c.status()["terminal"] and not c.status()["success"]
    passed("dataset cancellation")
    # Work is drained by the dataset service, even after cancellation was acknowledged.
    deadline = time.monotonic() + 15
    while c.status()["dataset_active"] and time.monotonic() < deadline: time.sleep(.2)
    return checks


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", type=int, default=8099)
    for arg in ("lidar", "camera", "slab", "output"): p.add_argument("--" + arg, required=True)
    p.add_argument("--scenario")
    a = p.parse_args()
    checks = exercise(Client(a.port), a.lidar, a.camera, a.slab, a.scenario)
    Path(a.output).write_text(json.dumps({"ok": True, "checks": checks}, indent=2), encoding="utf-8")
    print(f"PASS: {len(checks)} integration checks")


if __name__ == "__main__": main()
