import datetime
import json
import tempfile
import unittest
from pathlib import Path
from analyze_lidar_map_memory import analyze


class MapMemoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.origin = datetime.datetime(2026, 10, 28, tzinfo=datetime.timezone.utc)
        self.rows = [dict(type='metadata', configuration='Shipping', map='fixture', all_lidar_off=True)]
        self.os = []
        for second in range(0, 3601, 5):
            self.rows.append(dict(type='memory_curve', seconds=second, rss_bytes=1024**2,
                                  commit_bytes=2*1024**2, lidar_actor_count=0, sensors={}, llm_default={},
                                  rhi_submission=dict(native_dispatches=0, native_fallbacks=0, dispatches=0, scene_builds=0)))
            self.os.append(dict(utc=(self.origin + datetime.timedelta(seconds=second)).isoformat(),
                                working_set_bytes=1024**2, private_working_set_bytes=512*1024,
                                private_bytes=2*1024**2 + second*100, commit_bytes=2*1024**2 + second*100))
        self.rows.append(dict(type='summary', complete=True, sample_count=721))
        (self.path/'measurement-clock.json').write_text(json.dumps(dict(FirstCurveObservedUtc=self.origin.isoformat(), FirstCurveSeconds=0)))

    def save(self):
        (self.path/'memory-trend.jsonl').write_text('\n'.join(json.dumps(row) for row in self.rows))
        (self.path/'os-counters.jsonl').write_text('\n'.join(json.dumps(row) for row in self.os))

    def test_shipping_os_private_working_set_and_commit_are_distinct(self):
        self.save(); result = analyze(self.path)
        self.assertNotIn('RHIMisc', result['Tail20'])
        self.assertEqual(result['OsTail20']['PrivateWorkingSet']['slope_per_hour'], 0)
        self.assertGreater(result['OsTail20']['Commit']['slope_per_hour'], 0)
        self.assertEqual(result['OsTail20']['Commit'], result['OsTail20']['PrivateBytes'])

    def test_interrupted_run_is_never_passed(self):
        self.rows.pop(); self.save()
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            analyze(self.path)

    def test_disabled_llm_fixed_zero_is_not_rhi_evidence(self):
        self.rows[0]['llm_enabled'] = False
        for row in self.rows[1:-1]:
            row['llm_default']['RHIMisc'] = 0
        self.save()
        self.assertNotIn('RHIMisc', analyze(self.path)['Tail20'])

    def test_os_counter_pid_reuse_rejected(self):
        self.os[-1]['pid'] = 123
        self.save()
        with self.assertRaisesRegex(ValueError, 'identity'):
            analyze(self.path)

    def test_interior_os_failure_is_not_silently_skipped(self):
        self.os[200] = dict(utc=self.os[200]['utc'], monitor_error='access denied')
        self.save()
        with self.assertRaisesRegex(ValueError, 'inside'):
            analyze(self.path)

    def test_all_off_rejects_sensor_rt_work(self):
        self.rows[-2]['rhi_submission']['dispatches'] = 1; self.save()
        with self.assertRaisesRegex(ValueError, 'RT work'):
            analyze(self.path)

    def test_missing_os_tail_rejected(self):
        self.os = self.os[:100]; self.save()
        with self.assertRaisesRegex(ValueError, 'OS tail'):
            analyze(self.path)

    def test_runtime_utc_excludes_shutdown_even_with_late_watcher(self):
        self.rows[0]['started_utc'] = self.origin.isoformat()
        (self.path/'measurement-clock.json').write_text(json.dumps(dict(FirstCurveObservedUtc=(self.origin+datetime.timedelta(seconds=2)).isoformat(), FirstCurveSeconds=0, PollingUncertaintySeconds=2)))
        teardown = dict(self.os[-1]);teardown.update(utc=(self.origin+datetime.timedelta(seconds=3601)).isoformat(), private_bytes=0, commit_bytes=0)
        self.os.append(teardown);self.save();result=analyze(self.path)
        self.assertGreater(result['OsTail20']['Commit']['slope_per_hour'], 0)
        self.assertEqual(result['OsWindow']['ExcludedAfterMeasurementOrGuard'], 1)
        self.assertEqual(result['OsWindow']['EndGuardSeconds'], 0)

    def test_watcher_only_clock_guards_uncertain_shutdown_boundary(self):
        (self.path/'measurement-clock.json').write_text(json.dumps(dict(FirstCurveObservedUtc=(self.origin+datetime.timedelta(seconds=5)).isoformat(), FirstCurveSeconds=0, PollingUncertaintySeconds=5)))
        teardown=dict(self.os[-1]);teardown.update(utc=(self.origin+datetime.timedelta(seconds=3602)).isoformat(),private_bytes=0,commit_bytes=0)
        self.os.append(teardown);self.save();result=analyze(self.path)
        self.assertGreater(result['OsTail20']['Commit']['slope_per_hour'], 0)
        self.assertEqual(result['OsWindow']['EndGuardSeconds'], 10)

    def test_actors_cannot_disappear_from_averages(self):
        self.rows[-2]['lidar_actor_count'] = 1; self.save()
        with self.assertRaisesRegex(ValueError, 'cohort'):
            analyze(self.path)

    def watch(self, points):
        (self.path/'slot-watch.jsonl').write_text('\n'.join(json.dumps(dict(
            Utc=(self.origin+datetime.timedelta(seconds=t)).isoformat(), Foreign=foreign))
            for t,foreign in points), encoding='utf-8')

    def test_overlap_rejected_even_with_complete_curve(self):
        self.save(); self.watch([(0,[]),(2400,[dict(Pid=99,Name='UnrealEditor-Cmd')]),(3600,[])])
        with self.assertRaisesRegex(ValueError, 'overlap'):
            analyze(self.path)

    def test_rejected_overlap_marker_cannot_be_hidden_by_clean_watch(self):
        self.save(); self.watch([(0,[]),(3600,[])])
        (self.path/'environment-overlap.json').write_text('{}')
        with self.assertRaisesRegex(ValueError, 'overlap'):
            analyze(self.path)

    def test_late_or_empty_watch_is_not_clean_environment_proof(self):
        self.save(); self.watch([(100,[]),(3600,[])])
        with self.assertRaisesRegex(ValueError, 'cover'):
            analyze(self.path)
        self.watch([])
        with self.assertRaisesRegex(ValueError, 'empty'):
            analyze(self.path)

    def test_clean_watch_and_legacy_no_watch_are_distinguished(self):
        self.save()
        self.assertIsNone(analyze(self.path)['EnvironmentWindow']['CleanObserved'])
        self.watch([(second,[]) for second in range(0,3601,5)])
        result=analyze(self.path)['EnvironmentWindow']
        self.assertTrue(result['CleanObserved'])
        self.assertEqual(result['Samples'],721)

    def test_endpoint_only_watch_cannot_certify_clean_interval(self):
        self.save(); self.watch([(0,[]),(3600,[])])
        with self.assertRaisesRegex(ValueError,'polling gap'):
            analyze(self.path)


if __name__ == '__main__':
    unittest.main()
