"""Read-only Windows process VAD/stack totals. No heap/VirtualAlloc attribution claim."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import datetime
import json
import time
from pathlib import Path


def snapshot(pid, expected_creation=None, include_regions=False):
    kernel = c.WinDLL('kernel32', use_last_error=True)
    psapi = c.WinDLL('psapi', use_last_error=True)
    ntdll = c.WinDLL('ntdll')
    class MBI(c.Structure):
        _fields_ = [('BaseAddress', c.c_void_p), ('AllocationBase', c.c_void_p),
                    ('AllocationProtect', w.DWORD), ('PartitionId', w.WORD),
                    ('RegionSize', c.c_size_t), ('State', w.DWORD),
                    ('Protect', w.DWORD), ('Type', w.DWORD)]
    class PMC(c.Structure):
        _fields_ = [('cb', w.DWORD), ('PageFaultCount', w.DWORD)] + [(x, c.c_size_t) for x in
            ['PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage',
             'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage',
             'PagefileUsage', 'PeakPagefileUsage', 'PrivateUsage']]
    class THREAD(c.Structure):
        _fields_ = [('dwSize', w.DWORD), ('cntUsage', w.DWORD), ('th32ThreadID', w.DWORD),
                    ('th32OwnerProcessID', w.DWORD), ('tpBasePri', w.LONG),
                    ('tpDeltaPri', w.LONG), ('dwFlags', w.DWORD)]
    class TBI(c.Structure):
        _fields_ = [('ExitStatus', w.LONG), ('TebBaseAddress', c.c_void_p),
                    ('ProcessId', c.c_void_p), ('ThreadId', c.c_void_p),
                    ('AffinityMask', c.c_size_t), ('Priority', w.LONG), ('BasePriority', w.LONG)]
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]; kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.GetProcessTimes.argtypes = [w.HANDLE, c.POINTER(w.FILETIME), c.POINTER(w.FILETIME), c.POINTER(w.FILETIME), c.POINTER(w.FILETIME)]
    kernel.VirtualQueryEx.argtypes = [w.HANDLE, c.c_void_p, c.POINTER(MBI), c.c_size_t]
    kernel.VirtualQueryEx.restype = c.c_size_t
    kernel.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
    kernel.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]; kernel.CreateToolhelp32Snapshot.restype = w.HANDLE
    kernel.Thread32First.argtypes = [w.HANDLE, c.POINTER(THREAD)]
    kernel.Thread32Next.argtypes = [w.HANDLE, c.POINTER(THREAD)]
    kernel.OpenThread.argtypes = [w.DWORD, w.BOOL, w.DWORD]; kernel.OpenThread.restype = w.HANDLE
    ntdll.NtQueryInformationThread.argtypes = [w.HANDLE, w.ULONG, c.c_void_p, w.ULONG, c.c_void_p]
    ntdll.NtQueryInformationThread.restype = w.LONG
    psapi.GetProcessMemoryInfo.argtypes = [w.HANDLE, c.POINTER(PMC), w.DWORD]
    process = kernel.OpenProcess(0x0400 | 0x0010, False, pid)
    if not process:
        raise c.WinError(c.get_last_error())
    started = time.perf_counter()
    try:
        created, exited, kernel_time, user_time = w.FILETIME(), w.FILETIME(), w.FILETIME(), w.FILETIME()
        if not kernel.GetProcessTimes(process, c.byref(created), c.byref(exited), c.byref(kernel_time), c.byref(user_time)):
            raise c.WinError(c.get_last_error())
        creation = (int(created.dwHighDateTime) << 32) | int(created.dwLowDateTime)
        if expected_creation is not None and creation != expected_creation:
            raise OSError('PID was reused; stop without querying the replacement process')
        counters = PMC(); counters.cb = c.sizeof(counters)
        if not psapi.GetProcessMemoryInfo(process, c.byref(counters), counters.cb):
            raise c.WinError(c.get_last_error())
        totals = dict(private_commit_bytes=0, mapped_commit_bytes=0, image_commit_bytes=0,
                      reserve_bytes=0, guard_commit_bytes=0, committed_regions=0)
        allocations = {}
        stack_bases = []
        address = 0
        while address < 0x7FFFFFFF0000:
            mbi = MBI()
            if not kernel.VirtualQueryEx(process, address, c.byref(mbi), c.sizeof(mbi)):
                break
            size = int(mbi.RegionSize)
            if size <= 0:
                raise ValueError('Non-progressing VirtualQueryEx')
            if mbi.State == 0x1000:
                key = {0x20000: 'private_commit_bytes', 0x40000: 'mapped_commit_bytes',
                       0x1000000: 'image_commit_bytes'}.get(mbi.Type)
                if key:
                    totals[key] += size
                totals['committed_regions'] += 1
                if mbi.Protect & 0x100:
                    totals['guard_commit_bytes'] += size
            elif mbi.State == 0x2000:
                totals['reserve_bytes'] += size
            if include_regions and mbi.State in (0x1000, 0x2000):
                base = int(mbi.AllocationBase or 0)
                if base:
                    item = allocations.setdefault(base, dict(allocation_base=base,
                        end=base, type=int(mbi.Type), committed_bytes=0, reserved_only_bytes=0,
                        guard_bytes=0, region_count=0))
                    item['end'] = max(item['end'], int(mbi.BaseAddress or 0) + size)
                    item['region_count'] += 1
                    item['committed_bytes' if mbi.State == 0x1000 else 'reserved_only_bytes'] += size
                    if mbi.State == 0x1000 and mbi.Protect & 0x100:
                        item['guard_bytes'] += size
            address = int(mbi.BaseAddress or 0) + size
        stacks = dict(thread_count=0, stack_query_ok=0, stack_query_failed=0,
                      stack_committed_bytes=0, stack_reserved_bytes=0)
        threads = kernel.CreateToolhelp32Snapshot(0x00000004, 0)
        if threads and threads != c.c_void_p(-1).value:
            try:
                entry = THREAD(); entry.dwSize = c.sizeof(entry)
                more = kernel.Thread32First(threads, c.byref(entry))
                while more:
                    if entry.th32OwnerProcessID == pid:
                        stacks['thread_count'] += 1
                        thread = kernel.OpenThread(0x0040, False, entry.th32ThreadID)
                        ok = False
                        if thread:
                            try:
                                info = TBI()
                                if ntdll.NtQueryInformationThread(thread, 0, c.byref(info), c.sizeof(info), None) >= 0:
                                    tib = (c.c_size_t * 3)(); read = c.c_size_t()
                                    if kernel.ReadProcessMemory(process, info.TebBaseAddress, tib, c.sizeof(tib), c.byref(read)) and read.value == c.sizeof(tib):
                                        base, limit = int(tib[1]), int(tib[2])
                                        mbi = MBI()
                                        if base >= limit > 0 and kernel.VirtualQueryEx(process, limit, c.byref(mbi), c.sizeof(mbi)):
                                            alloc = int(mbi.AllocationBase or 0)
                                            if base >= alloc > 0:
                                                stacks['stack_committed_bytes'] += base - limit
                                                stacks['stack_reserved_bytes'] += base - alloc
                                                if include_regions:
                                                    stack_bases.append(alloc)
                                                ok = True
                            finally:
                                kernel.CloseHandle(thread)
                        stacks['stack_query_ok' if ok else 'stack_query_failed'] += 1
                    more = kernel.Thread32Next(threads, c.byref(entry))
            finally:
                kernel.CloseHandle(threads)
        result = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), pid=pid, creation_filetime=creation,
                    working_set_bytes=int(counters.WorkingSetSize), private_usage_bytes=int(counters.PrivateUsage),
                    page_fault_count=int(counters.PageFaultCount), **totals, **stacks,
                    monitor_wall_ms=(time.perf_counter() - started) * 1000,
                    scope='Read-only VirtualQueryEx totals; MEM_PRIVATE is not synonymous with OS heap. Stack totals use x64 TEB NT_TIB and validate bounds; failed thread queries stay explicit. No raw memory or addresses exported.')
        if include_regions:
            result['allocation_regions'] = list(allocations.values())
            result['stack_allocation_bases'] = sorted(set(stack_bases))
            result['scope'] = result['scope'].replace('No raw memory or addresses exported.',
                'Opt-in allocation-base addresses/sizes only; no memory contents exported.')
        return result
    finally:
        kernel.CloseHandle(process)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pid', type=int, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--stop-file', type=Path)
    p.add_argument('--interval', type=float, default=60)
    p.add_argument('--include-regions', action='store_true', help='Record allocation-base addresses/sizes, never memory contents')
    p.add_argument('--runtime-clock', type=Path, help='Align samples to memory-trend metadata started_utc')
    args = p.parse_args()
    if args.interval < 60:
        p.error('External VM monitor interval must be at least60s')
    expected_creation = None
    runtime_start = None
    next_seconds = 0.0
    if args.runtime_clock:
        deadline = time.monotonic() + 300
        while time.monotonic() < deadline:
            if args.stop_file and args.stop_file.exists():
                return
            if args.runtime_clock.exists():
                try:
                    with args.runtime_clock.open(encoding='utf-8-sig') as f:
                        metadata = json.loads(next(f))
                    runtime_start = datetime.datetime.fromisoformat(metadata['started_utc'].replace('Z', '+00:00'))
                    break
                except (OSError, ValueError, KeyError, StopIteration):
                    pass
            time.sleep(1)
        if runtime_start is None:
            p.error('Runtime metadata clock unavailable after300s')
    while True:
        if runtime_start:
            elapsed = (datetime.datetime.now(datetime.timezone.utc) - runtime_start).total_seconds()
            if elapsed < next_seconds:
                if args.stop_file and args.stop_file.exists():
                    return
                time.sleep(min(1.0, next_seconds-elapsed))
                continue
        try:
            row = snapshot(args.pid, expected_creation, args.include_regions)
            expected_creation = row['creation_filetime']
            if runtime_start:
                row['runtime_seconds'] = (datetime.datetime.fromisoformat(row['utc']) - runtime_start).total_seconds()
        except OSError as e:
            row = dict(pid=args.pid, monitor_error=str(e), utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        with args.output.open('a', encoding='utf-8') as f:
            f.write(json.dumps(row) + '\n')
        if not args.stop_file or args.stop_file.exists() or 'monitor_error' in row:
            break
        if runtime_start:
            next_seconds += args.interval
            continue
        for _ in range(int(args.interval)):
            if args.stop_file.exists():
                return
            time.sleep(1)


if __name__ == '__main__':
    main()
