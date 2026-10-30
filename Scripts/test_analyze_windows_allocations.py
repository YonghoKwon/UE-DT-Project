import unittest
from analyze_windows_allocations import CommitLedger, analyze


def event(second, kind, base, size, flags, frame="Driver+0x123"):
    return dict(utc=f"2026-10-30T00:00:{second:02d}Z", kind=kind, base=base,
                size=size, type=flags, status=0, frames=[frame])


class WindowsAllocationTests(unittest.TestCase):
    def test_reserve_and_recommit_do_not_double_count(self):
        rows = [event(0,"alloc",0x10000,0x40000,0x2000),
                event(1,"alloc",0x10000,0x20000,0x1000),
                event(2,"alloc",0x20000,0x20000,0x1000)]
        result = analyze(rows)
        self.assertEqual(result["ObservedCommitAfterBytes"],0x30000)
        self.assertEqual(result["CommitTrafficBytes"],0x40000)

    def test_partial_decommit_preserves_other_owners(self):
        ledger=CommitLedger();ledger.commit(0,100,"A");ledger.commit(100,100,"B")
        ledger.free(50,100)
        self.assertEqual(dict(ledger.totals()),{"A":50,"B":50})

    def test_zero_size_release_uses_reservation(self):
        rows=[event(0,"alloc",0x10000,0x30000,0x3000),
              event(1,"free",0x10000,0,0x8000)]
        self.assertEqual(analyze(rows)["ObservedCommitAfterBytes"],0)

    def test_window_uses_existing_baseline_and_excludes_later_shutdown(self):
        rows=[event(0,"alloc",0x10000,0x10000,0x3000),
              event(1,"alloc",0x20000,0x20000,0x3000),
              event(2,"free",0x10000,0x10000,0x8000),
              event(3,"free",0x20000,0x20000,0x8000)]
        result=analyze(rows,"2026-10-30T00:00:01Z","2026-10-30T00:00:02Z")
        self.assertEqual(result["ObservedCommitBeforeBytes"],0x10000)
        self.assertEqual(result["ObservedCommitDeltaBytes"],0x10000)

    def test_bad_timestamp_order_and_uncovered_window_fail(self):
        rows=[event(1,"alloc",1,1,0x1000),event(0,"alloc",2,1,0x1000)]
        with self.assertRaises(ValueError):analyze(rows)
        with self.assertRaises(ValueError):analyze(list(reversed(rows)),end="2026-10-30T00:00:03Z")

    def test_unknown_release_is_visible(self):
        rows=[event(0,"alloc",1,1,0x1000),event(1,"free",99,0,0x8000)]
        self.assertEqual(analyze(rows)["UnknownReleases"],1)

    def test_reset_does_not_free_committed_memory(self):
        rows=[event(0,"alloc",0x10000,0x10000,0x3000),event(1,"alloc",0x10000,0x10000,0x80000)]
        self.assertEqual(analyze(rows)["ObservedCommitAfterBytes"],0x10000)

    def test_unresolved_stack_is_never_invented(self):
        row=event(0,"alloc",1,100,0x1000);row["frames"]=[]
        result=analyze([row])
        self.assertEqual(result["Stackless"]["commit_traffic_bytes"],100)
        self.assertEqual(result["TopStacks"][0]["Stack"],["<no captured stack>"])

    def test_remote_process_allocations_are_excluded(self):
        local=event(0,"alloc",0x10000,0x10000,0x3000)
        local["process"]="0xffffffffffffffff"
        remote=event(0,"alloc",0x20000,0x20000,0x3000)
        remote["process"]="0x44"
        result=analyze([local,remote])
        self.assertEqual(result["ObservedCommitAfterBytes"],0x10000)
        self.assertEqual(result["ExcludedNonCurrentProcessCalls"],1)

    def test_unpaired_collector_events_reject_lifetime_claim(self):
        row=event(0,"alloc",0x10000,0x10000,0x3000)
        with self.assertRaises(ValueError):
            analyze([row,dict(kind="summary",counts={"missing":1})])
        with self.assertRaises(ValueError):
            analyze([row,dict(kind="capture_error",message="bad pointer")])


if __name__ == "__main__":unittest.main()
