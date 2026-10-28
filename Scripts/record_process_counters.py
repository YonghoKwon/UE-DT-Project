"""Read-only Windows Working Set, private Working Set, and private Commit counters."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import datetime
import json
import time
from pathlib import Path


def snapshot(pid, expected_creation=None):
    kernel = c.WinDLL('kernel32', use_last_error=True)
    psapi = c.WinDLL('psapi', use_last_error=True)
    class Counters(c.Structure):
        _fields_ = [('cb', w.DWORD), ('PageFaultCount', w.DWORD)] + [(name, c.c_size_t) for name in (
            'PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage',
            'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage',
            'PagefileUsage', 'PeakPagefileUsage', 'PrivateUsage', 'PrivateWorkingSetSize')
        ] + [('SharedCommitUsage', c.c_uint64)]
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4
    psapi.GetProcessMemoryInfo.argtypes = [w.HANDLE, c.c_void_p, w.DWORD]
    handle = kernel.OpenProcess(0x0400 | 0x0010, False, pid)
    if not handle:
        raise c.WinError(c.get_last_error())
    started = time.perf_counter()
    try:
        times = [w.FILETIME() for _ in range(4)]
        if not kernel.GetProcessTimes(handle, *(c.byref(t) for t in times)):
            raise c.WinError(c.get_last_error())
        creation = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        if expected_creation is not None and creation != expected_creation:
            raise OSError('PID reused; replacement process was not sampled')
        counters = Counters(); counters.cb = c.sizeof(counters)
        if not psapi.GetProcessMemoryInfo(handle, c.byref(counters), counters.cb):
            raise c.WinError(c.get_last_error())
        return dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    pid=pid, creation_filetime=creation,
                    working_set_bytes=counters.WorkingSetSize,
                    private_working_set_bytes=counters.PrivateWorkingSetSize,
                    private_bytes=counters.PrivateUsage, commit_bytes=counters.PagefileUsage,
                    shared_commit_bytes=counters.SharedCommitUsage,
                    monitor_wall_ms=(time.perf_counter() - started) * 1000,
                    counter_contract='PROCESS_MEMORY_COUNTERS_EX2; PrivateUsage and PagefileUsage are the same private Commit, not independent metrics; private Working Set is separate')
    finally:
        kernel.CloseHandle(handle)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--stop-file', type=Path, required=True)
    parser.add_argument('--interval', type=float, default=5)
    args = parser.parse_args()
    if args.interval < 5:
        parser.error('Interval must be at least5seconds')
    expected = None
    while not args.stop_file.exists():
        try:
            row = snapshot(args.pid, expected)
            expected = row['creation_filetime']
        except OSError as error:
            row = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), monitor_error=str(error))
        with args.output.open('a', encoding='utf-8') as file:
            file.write(json.dumps(row) + '\n')
        if 'monitor_error' in row:
            break
        deadline = time.monotonic() + args.interval
        while not args.stop_file.exists() and time.monotonic() < deadline:
            time.sleep(.25)


if __name__ == '__main__':
    main()
