"""Official AUTO-01 process entry point: graceful UE teardown, reliable runner exit status.

UE 5.3 on Windows can return zero after RequestExitWithStatus(false, 1).
Success therefore requires BOTH child exit zero and a fresh successful result document.
Never terminate/kill the UE child. A timeout writes its per-run cancellation marker.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

from client import ControlError, check_result


def run_job(executable: Path, job_path: Path, *, project: Path | None = None,
            map_name: str | None = None, ue_arguments: list[str] | None = None,
            timeout: float = 7200, shutdown_grace: float = 30,
            report_path: Path | None = None, working_directory: Path | None = None) -> int:
    started = time.monotonic()
    execution_id = uuid.uuid4().hex
    job_path = job_path.resolve()
    report_path = (report_path or job_path.with_name(job_path.stem + ".runner.json")).resolve()
    report = {"schema": "ma0t10.job.runner.v1", "execution_id": execution_id,
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "job": str(job_path), "child_exit_code": None, "child_running": False,
              "ok": False, "outcome": "invalid_input"}
    child = None
    engine_log = None
    lock_path = cancel_path = None
    lock_fd = None
    code = 2
    try:
        if not executable.is_file() or not math.isfinite(timeout) or not math.isfinite(shutdown_grace) or timeout <= 0 or shutdown_grace < 0:
            raise ValueError("invalid launch parameters")
        if job_path.stat().st_size > 256 * 1024:
            raise ValueError("job exceeds admission limit")
        job = json.loads(job_path.read_text(encoding="utf-8-sig"))
        if not isinstance(job, dict) or job.get("schema") != "ma0t10.job.v1":
            raise ValueError("invalid job schema")
        result_value = job.get("result_path")
        if not isinstance(result_value, str) or not result_value or "\x00" in result_value:
            raise ValueError("invalid result path")
        steps = job.get("steps")
        if not isinstance(steps, list) or not steps or any(
            not isinstance(s, dict) or not isinstance(s.get("op"), str) for s in steps
        ):
            raise ValueError("invalid job steps")
        result_path = (job_path.parent / result_value).resolve()
        if result_path.suffix != ".json" or result_path in (job_path, report_path):
            raise ValueError("result path overlaps input/runner report")
        args = list(ue_arguments or [])
        if any(a.lower().startswith(("-ma0t10job=", "-ma0t10jobcancelfile=")) for a in args):
            raise ValueError("reserved child arguments")
        result_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        lock_path = result_path.with_name(result_path.name + ".automation.lock")
        report["lock_file"] = str(lock_path)
        lock_fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        os.write(lock_fd, str(os.getpid()).encode("ascii"))
        # Preserve earlier results, and make it impossible to accept a stale success.
        if result_path.exists():
            previous = result_path.with_name(result_path.name + ".previous-" + execution_id)
            result_path.rename(previous)
            report["previous_result"] = str(previous)
        cancel_path = report_path.with_name(report_path.name + ".cancel-" + execution_id)
        command = [str(executable.resolve())]
        if project is not None:
            if not project.is_file():
                raise ValueError("project missing")
            command.append(str(project.resolve()))
        if map_name:
            command.append(map_name)
        configured_log = next((a.split("=", 1)[1] for a in args if a.lower().startswith("-abslog=")), None)
        engine_log = Path(configured_log) if configured_log else report_path.with_name(report_path.stem + ".engine.log")
        if not engine_log.is_absolute():
            engine_log = ((working_directory or Path.cwd()) / engine_log).resolve()
        if configured_log is None:
            args.append("-abslog=" + str(engine_log))
        report["engine_log"] = str(engine_log)
        command += args + ["-MA0T10Job=" + str(job_path), "-MA0T10JobCancelFile=" + str(cancel_path)]
        report["result"] = str(result_path)
        # Child output can include unrelated engine logs. Keep it in a local file;
        # never echo argv, environment, headers, tokens or the child's output.
        console_path = report_path.with_name(report_path.stem + ".child.log")
        flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
        with console_path.open("w", encoding="utf-8") as console:
            child = subprocess.Popen(command, cwd=working_directory, stdout=console,
                                     stderr=subprocess.STDOUT, creationflags=flags)
            report["child_pid"] = child.pid
            report["console_log"] = str(console_path)
            try:
                report["child_exit_code"] = child.wait(timeout=timeout)
            except (subprocess.TimeoutExpired, KeyboardInterrupt) as error:
                code = 124 if isinstance(error, subprocess.TimeoutExpired) else 130
                report["outcome"] = "timeout" if code == 124 else "interrupted"
                cancel_path.write_text(execution_id, encoding="ascii")
                try:
                    report["child_exit_code"] = child.wait(timeout=shutdown_grace)
                except subprocess.TimeoutExpired:
                    report["child_running"] = True
                    report["cancellation_marker"] = str(cancel_path)
                return code
        if report["child_exit_code"] != 0:
            report["outcome"] = "child_failed"
            return 1
        try:
            result = check_result(result_path)
            if [s["op"] for s in result["steps"]] != [s["op"] for s in steps]:
                raise ControlError("Result does not account for this job's steps")
        except (ControlError, ValueError, OSError):
            report["outcome"] = "result_failed_or_missing"
            return 1
        report["ok"] = True
        report["outcome"] = "succeeded"
        report["job_elapsed_sec"] = result.get("elapsed_sec")
        return 0
    except FileExistsError:
        report["outcome"] = "result_path_busy"
        return 2
    except (ValueError, OSError):
        report["outcome"] = "launch_failed" if child is None and code != 2 else "invalid_input"
        return 2
    finally:
        if lock_fd is not None:
            os.close(lock_fd)
            # Keep admission locked while an unresponsive child might still write.
            if child is None or child.poll() is not None:
                lock_path.unlink(missing_ok=True)
        if cancel_path is not None and (child is None or child.poll() is not None):
            cancel_path.unlink(missing_ok=True)
        report["elapsed_sec"] = time.monotonic() - started
        report["engine_log_complete"] = bool(engine_log and engine_log.is_file() and child is not None and child.poll() is not None)
        if report["engine_log_complete"]:
            # Includes startup handled-ensure lines, unlike world-owned job metrics.
            # Count lines, not unique events; never copy log contents into diagnostics.
            with engine_log.open(encoding="utf-8", errors="replace") as native_log:
                report["engine_error_log_lines"] = sum("Error:" in line or "Fatal error:" in line for line in native_log)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        temporary = report_path.with_name(report_path.name + ".tmp-" + execution_id)
        temporary.write_text(json.dumps(report, indent=2), encoding="utf-8")
        temporary.replace(report_path)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--executable", type=Path, required=True)
    p.add_argument("--job", type=Path, required=True)
    p.add_argument("--project", type=Path)
    p.add_argument("--map", dest="map_name")
    p.add_argument("--timeout", type=float, default=7200)
    p.add_argument("--shutdown-grace", type=float, default=30)
    p.add_argument("--report", type=Path)
    p.add_argument("--working-directory", type=Path)
    p.add_argument("ue_arguments", nargs=argparse.REMAINDER, help="arguments after -- are passed to UE")
    a = p.parse_args()
    args = a.ue_arguments[1:] if a.ue_arguments[:1] == ["--"] else a.ue_arguments
    code = run_job(a.executable, a.job, project=a.project, map_name=a.map_name,
                   ue_arguments=args, timeout=a.timeout, shutdown_grace=a.shutdown_grace,
                   report_path=a.report, working_directory=a.working_directory)
    print("Automation job succeeded." if code == 0 else "Automation job failed; inspect runner/result JSON.")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
