"""Summarize successful Windows VirtualAlloc/VirtualFree debugger records.

This is diagnostic attribution, never an RSS/Commit acceptance replacement.
The input is JSONL or CDB lines prefixed with ``R19JSON ``. Only observed
MEM_COMMIT intervals are accounted for; reserve bytes and allocation traffic
are not retained Commit. Missing/small/untraced calls remain a coverage limit.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re

MIB = 1024 * 1024
COMMIT, RESERVE, DECOMMIT, RELEASE = 0x1000, 0x2000, 0x4000, 0x8000


def stamp(value):
    return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()


def address(value):
    return int(value.replace("`", ""), 16) if isinstance(value, str) else int(value)


def load_records(path):
    records, errors = [], []
    with Path(path).open(encoding="utf-8-sig", errors="replace") as source:
        for number, line in enumerate(source, 1):
            if line.startswith("R19JSON "):
                payload = line[len("R19JSON "):]
            elif line.lstrip().startswith("{"):
                payload = line
            else:
                if line.startswith("Error:") or "Memory access error" in line:
                    errors.append({"line": number, "message": line.strip()})
                continue
            row = json.loads(payload)
            if row.get("kind") in ("alloc", "free"):
                for field in ("utc", "base", "size", "type", "status"):
                    if field not in row:
                        raise ValueError(f"line {number}: missing {field}")
                if int(row["size"]) < 0:
                    raise ValueError(f"line {number}: negative size")
                stamp(row["utc"])
            records.append(row)
    if not any(x.get("kind") == "alloc" for x in records):
        raise ValueError("No successful allocation records; empty capture is not evidence")
    return records, errors


def stack_key(row):
    frames = row.get("frames", [])
    # Preserve function spelling as supplied; do not invent a nearest export.
    # Strip stack/return-address columns to combine ASLR-independent call sites.
    calls = []
    for frame in frames:
        match = re.search(r"(?:[\w.-]+![^\r\n]+|[\w.-]+\+0x[0-9a-fA-F]+)\s*$", frame)
        if match:
            calls.append(match.group(0).strip())
    return tuple(calls) or ("<no captured stack>",)


def backing_module(calls):
    for call in calls:
        if call.startswith("<"):
            continue
        module = re.split(r"!|\+0x", call, maxsplit=1)[0]
        if module.lower() not in {"ntdll", "kernelbase", "kernel32"}:
            return module
    return "<unresolved>"


class CommitLedger:
    """Disjoint observed committed intervals, attributed to first committer."""
    def __init__(self):
        self.intervals = []
        self.reservations = {}
        self.unknown_releases = 0

    def commit(self, start, size, owner):
        end = start + size
        cursor = start
        added = []
        for low, high, _ in self.intervals:
            if high <= cursor:
                continue
            if low >= end:
                break
            if low > cursor:
                added.append((cursor, min(low, end), owner))
            cursor = max(cursor, high)
            if cursor >= end:
                break
        if cursor < end:
            added.append((cursor, end, owner))
        self.intervals = sorted(self.intervals + added)

    def free(self, start, size):
        end = start + size
        kept = []
        for low, high, owner in self.intervals:
            if high <= start or low >= end:
                kept.append((low, high, owner))
            else:
                if low < start:
                    kept.append((low, start, owner))
                if high > end:
                    kept.append((end, high, owner))
        self.intervals = kept

    def apply(self, row, owner):
        if row["status"] != 0:
            return
        base, size, flags = address(row["base"]), int(row["size"]), int(row["type"])
        if row["kind"] == "alloc":
            if flags & RESERVE:
                self.reservations[base] = size
            if flags & COMMIT:
                self.commit(base, size, owner)
        elif flags & RELEASE:
            size = size or self.reservations.get(base, 0)
            if not size:
                self.unknown_releases += 1
                return
            self.free(base, size)
            self.reservations.pop(base, None)
        elif flags & DECOMMIT:
            self.free(base, size)

    def totals(self):
        out = Counter()
        for low, high, owner in self.intervals:
            out[owner] += high - low
        return out


def analyze(records, start=None, end=None, top=20):
    for row in records:
        if row.get("kind") == "capture_error":
            raise ValueError("Collector reported a capture error; lifetime attribution rejected")
        if row.get("kind") == "summary" and any(
                row.get("counts", {}).get(key, 0) for key in ("missing", "invalid", "overwritten")):
            raise ValueError("Unpaired or invalid collector calls; lifetime attribution rejected")
    all_events = [x for x in records if x.get("kind") in ("alloc", "free")]
    remote = [x for x in all_events if x.get("process", "unknown") not in
              ("unknown", "0xffffffffffffffff", -1)]
    events = [x for x in all_events if x.get("process", "unknown") in
              ("unknown", "0xffffffffffffffff", -1)]
    times = [stamp(x["utc"]) for x in events]
    if any(a > b for a, b in zip(times, times[1:])):
        raise ValueError("Nonmonotonic capture timestamps")
    if not events:
        raise ValueError("No allocation/free events")
    begin, finish = (stamp(start) if start else times[0]), (stamp(end) if end else times[-1])
    if begin > finish:
        raise ValueError("Start is after end")
    if begin < times[0] or finish > times[-1]:
        raise ValueError("Requested window is not covered by capture")
    ledger, before = CommitLedger(), None
    traffic, counts, stackless = Counter(), Counter(), Counter()
    for row, when in zip(events, times):
        if when > finish:
            break
        if before is None and when >= begin:
            before = ledger.totals()
        owner = stack_key(row)
        ledger.apply(row, owner)
        if when >= begin:
            counts[row["kind"]] += 1
            if row["kind"] == "alloc" and row["status"] == 0 and row["type"] & COMMIT:
                traffic[owner] += row["size"]
                if not row.get("frames"):
                    stackless["commit_traffic_bytes"] += row["size"]
                    stackless["commit_calls"] += 1
    before = before or Counter()
    after = ledger.totals()
    deltas = {key: after[key] - before[key] for key in before.keys() | after.keys()}
    ordered = sorted(deltas, key=lambda key: deltas[key], reverse=True)
    stacks = [dict(BackingModule=backing_module(key), Stack=list(key),
                   ObservedCommitDeltaBytes=deltas[key],
                   ObservedCommitEndBytes=after[key], CommitTrafficBytes=traffic[key])
              for key in ordered[:top]]
    modules = Counter()
    for key, size in deltas.items():
        modules[backing_module(key)] += size
    return dict(StartUtc=start or events[0]["utc"], EndUtc=end or events[-1]["utc"],
                EventCounts=dict(counts), ObservedCommitBeforeBytes=sum(before.values()),
                ObservedCommitAfterBytes=sum(after.values()),
                ObservedCommitDeltaBytes=sum(deltas.values()),
                CommitTrafficBytes=sum(traffic.values()), Stackless=dict(stackless),
                ExcludedNonCurrentProcessCalls=len(remote),
                UnknownTargetProcessCalls=sum(x.get("process", "unknown") == "unknown" for x in events),
                UnknownReleases=ledger.unknown_releases,
                ModuleDeltasBytes=dict(modules.most_common()), TopStacks=stacks,
                Limits=["Observed VirtualAlloc backing intervals, not live Heap allocations or OS total Commit.",
                        "Recommit overlap is counted once; MEM_RESERVE alone is not Commit.",
                        "Uncaptured APIs, pre-attach activity, small-call filtering and debugger pauses can bias attribution.",
                        "Module offsets remain unresolved unless matching PDB evidence separately confirms symbols.",
                        "No RSS/Commit/150ms/Hz/performance acceptance verdict is inferred."])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--start")
    parser.add_argument("--end")
    parser.add_argument("--top", type=int, default=20)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    errors = []
    try:
        records, errors = load_records(args.capture)
        footers = [x for x in records if x.get("kind") == "summary"]
        if len(footers) != 1 or footers[0].get("pending", 0):
            raise ValueError("One complete collector footer with no pending calls is required")
        result = analyze(records, args.start, args.end, args.top)
    except ValueError as error:
        result = {"AttributionUsable": False, "Rejection": str(error)}
        errors.append({"message": str(error)})
    result["CaptureErrors"] = errors
    result["CaptureSha256"] = hashlib.sha256(args.capture.read_bytes()).hexdigest()
    result["AttributionUsable"] = not errors
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    return 0 if not errors else 2


if __name__ == "__main__":
    raise SystemExit(main())
