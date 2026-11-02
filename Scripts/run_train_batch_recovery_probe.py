"""Owned-process TRAIN-BATCH-01 recovery probe. Run only in an approved GPU window."""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
EDITOR = Path(r"C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")

def read(path):
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        return {}

def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
    temp.replace(path)

def terminate_recorded(pid, stamp):
    """Never terminate a PID without verifying its kernel creation time."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x1000 | 1, False, int(pid))
    if not handle:
        return False
    try:
        times = [wintypes.FILETIME() for _ in range(4)]
        if not kernel.GetProcessTimes(handle, *(ctypes.byref(t) for t in times)):
            return False
        actual = times[0].dwLowDateTime | (times[0].dwHighDateTime << 32)
        return actual == int(stamp) and bool(kernel.TerminateProcess(handle, 73))
    finally:
        kernel.CloseHandle(handle)

def environment(case, owner_pid=None):
    gpu = subprocess.run(["nvidia-smi", "--query-gpu=utilization.decoder", "--format=csv,noheader,nounits"], capture_output=True, text=True, check=True)
    if any(int(v.strip()) for v in gpu.stdout.splitlines()):
        raise RuntimeError("environment invalid: video decoding")
    command = "$p=@(Get-CimInstance Win32_Process|Select-Object Name,ProcessId,ParentProcessId,CommandLine);ConvertTo-Json -InputObject $p -Compress"
    result = subprocess.run(["powershell", "-NoProfile", "-Command", command], capture_output=True, text=True, check=True)
    processes = json.loads(result.stdout or "[]")
    if isinstance(processes, dict):
        processes = [processes]
    prefix = str(case).replace("\\", "/").lower()
    owned = {os.getpid(), owner_pid}
    owned.update(p["ProcessId"] for p in processes if prefix in (p.get("CommandLine") or "").replace("\\", "/").lower())
    for _ in range(5):
        owned.update(p["ProcessId"] for p in processes if p.get("ParentProcessId") in owned)
    def relevant(p):
        name = p.get("Name", "").lower()
        return name in {"unrealeditor.exe", "unrealeditor-cmd.exe", "ma0t10_dt.exe", "ma0t10_dt-win64-development.exe", "ma0t10_dt-win64-shipping.exe", "cl.exe"}
    external = [p for p in processes if relevant(p) and p["ProcessId"] not in owned]
    if external:
        raise RuntimeError("environment invalid: external Unreal/build")

def fixture(case, workers, count):
    job_id = str(uuid.uuid4())
    source = case / "input"
    source.mkdir(parents=True, exist_ok=True)
    descriptors = []
    for index in range(count):
        scenario = {"MESSAGE_ID": "IFactory-agent", "CREATE_TIMESTAMP": "20261102000000", "_meta": {"UUID": str(uuid.uuid4()), "duration_sec": .15}, "DATA_MAP": []}
        for row in range(2):
            scenario["DATA_MAP"].append({"frame_no": row + 10, "mtl_no": "TRAIN-RECOVERY", "slab_thk": 50, "slab_wth": 200, "slab_len": 800, "slab_wt": 1, "left_skew_angle": 0, "right_skew_angle": 0, "center_x": index + row, "elapsed_sec": row * .1})
        raw = json.dumps(scenario, separators=(",", ":")).encode()
        name = f"case-{index}.json"
        (source / name).write_bytes(raw)
        descriptors.append({"scenario_id": f"case-{index}", "duration_sec": .15, "input_sha256": hashlib.sha256(raw).hexdigest(), "input_bytes": len(raw), "input_file": name})
    manifest = {"schema": "ma0t10.train.job.v1", "MESSAGE_ID": "ITrain-agent-batch", "kind": "job", "job_id": job_id, "scenario_count": count, "worker_limit": workers, "preset_id": "train-recovery", "dimension_unit": "cm", "position_unit": "cm", "execution": {"mode": "sampled_timeline", "sample_hz": 20}, "scenarios": descriptors}
    write(source / "job.json", manifest)
    config = {"job_id": job_id, "workers": workers, "preset_id": "train-recovery", "manifest": str(source / "job.json"), "test_auto": True, "restore": False}
    write(case / "coordinator.json", config)
    return job_id, config

def launch(case, attempt):
    log = (case / f"console-{attempt}.log").open("wb")
    args = [str(EDITOR), str(ROOT / "ma0t10_dt.uproject"), "/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap", "-game", "-d3d12", "-RenderOffscreen", "-unattended", "-nosplash", "-nosound", "-nop4", "-ResX=1920", "-ResY=1080", f"-UserDir={case / 'user'}", f"-TrainBatchCoordinator={case / 'coordinator.json'}", f"-abslog={case / f'coordinator-{attempt}.log'}"]
    process = subprocess.Popen(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS)
    log.close()
    return process

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--case", required=True)
    parser.add_argument("--workers", type=int, choices=[1, 2, 4], default=2)
    parser.add_argument("--count", type=int, default=8)
    parser.add_argument("--kill-worker", action="store_true")
    parser.add_argument("--restart-coordinator", action="store_true")
    opts = parser.parse_args()
    case = (ROOT / "Saved/Reports/TRAIN-BATCH-01/r41b" / opts.case).resolve()
    approved = (ROOT / "Saved/Reports/TRAIN-BATCH-01/r41b").resolve()
    if not case.is_relative_to(approved) or case == approved or case.exists() or not 4 <= opts.count <= 20:
        raise ValueError("Fresh scoped case and count 4..20 required")
    environment(case)
    case.mkdir(parents=True)
    job_id, config = fixture(case, opts.workers, opts.count)
    result = {"job_id": job_id, "workers_requested": opts.workers, "worker_killed": False, "coordinator_restarted": False, "success": False}
    process = launch(case, 1)
    started = time.monotonic()
    jobs = case / "user/Saved/TrainBatch/Jobs"
    pool_file = jobs / job_id / "processes/pool.json"
    try:
        while time.monotonic() - started < 1500:
            time.sleep(2)
            environment(case, process.pid)
            state = read(case / "coordinator-status.json")
            pool = read(pool_file)
            slots = [s for s in pool.get("slots", []) if s.get("task", -1) >= 0 and s.get("pid", 0)]
            if opts.kill_worker and not result["worker_killed"] and len(slots) >= 2:
                slot = slots[-1]
                heartbeat = read(pool_file.parent / "leases" / slot["lease"] / "heartbeat.json")
                if heartbeat.get("native") and terminate_recorded(slot["pid"], slot["stamp"]):
                    result["worker_killed"] = True
                    result["killed_index"] = slot["task"]
            if opts.restart_coordinator and not result["coordinator_restarted"] and state.get("completed", 0) >= 1 and slots:
                process.kill()
                process.wait(timeout=30)
                config["restore"] = True
                write(case / "coordinator.json", config)
                time.sleep(2)
                process = launch(case, 2)
                result["coordinator_restarted"] = True
            result["actual_max"] = pool.get("actual_max", 0)
            write(case / "probe-status.json", dict(result, phase=state.get("phase", "booting"), completed=state.get("completed", 0)))
            if process.poll() is not None:
                result["exit_code"] = process.returncode
                result["success"] = process.returncode == 0 and state.get("success") is True
                break
        else:
            raise TimeoutError("Recovery probe deadline")
        if not result["success"]:
            result["error"] = read(case / "coordinator-status.json").get("error", "Coordinator failed")
    except Exception as exc:
        result["error"] = str(exc)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=30)
        # Children use the private coordinator lease and exit on orphan timeout.
        result["elapsed_seconds"] = round(time.monotonic() - started, 3)
        write(case / "probe-result.json", result)
    return 0 if result["success"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
