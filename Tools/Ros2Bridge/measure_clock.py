"""Measure guest-minus-Windows UTC using a named WSL pipe; never adjust either clock.

RTT is steady-clock wall time, offset uses precise Windows UTC and the round-trip
midpoint. Half RTT is an uncertainty bound under the midpoint assumption, not an
NTP/PTP synchronization guarantee. Runs only in Windows against ma0t10-ros2.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import time


def windows_utc_ns():
    value = ctypes.c_uint64()
    ctypes.windll.kernel32.GetSystemTimePreciseAsFileTime(ctypes.byref(value))
    return (value.value - 116444736000000000) * 100


def measure(samples=40, interval=.05):
    if os.name != 'nt': raise ValueError('Run the host probe in Windows')
    child = subprocess.Popen(['wsl.exe', '-d', 'ma0t10-ros2', '-u', 'root', '--', 'python3', '-u', '-c',
        'import sys,time\nfor line in sys.stdin: print(time.time_ns(),flush=True)'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    rows = []
    try:
        for _ in range(samples):
            before = windows_utc_ns(); steady = time.perf_counter_ns()
            child.stdin.write('sample\n'); child.stdin.flush()
            guest = int(child.stdout.readline())
            rtt = time.perf_counter_ns() - steady; after = windows_utc_ns()
            rows.append({'guest_minus_host_ns': guest - (before+after)//2, 'rtt_ns':rtt,
                         'host_mid_ns':(before+after)//2, 'host_clock_step_ns':(after-before)-rtt})
            time.sleep(interval)
    finally:
        child.stdin.close(); child.wait(timeout=5)
        child.stdout.close()
    best = min(rows, key=lambda x:x['rtt_ns'])
    return {'schema':'ma0t10.clock-offset.v1','method':'precise Windows UTC / WSL UTC pipe midpoint',
            'best':best,'midpoint_uncertainty_ns':best['rtt_ns']//2,'samples':rows}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    report = measure()
    args.output.write_text(json.dumps(report,indent=2),encoding='utf8')
    print(json.dumps({'guest_minus_host_ms':report['best']['guest_minus_host_ns']/1e6,
                      'min_rtt_ms':report['best']['rtt_ns']/1e6}))


if __name__ == '__main__': main()
