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

    def test_all_off_rejects_sensor_rt_work(self):
        self.rows[-2]['rhi_submission']['dispatches'] = 1; self.save()
        with self.assertRaisesRegex(ValueError, 'RT work'):
            analyze(self.path)

    def test_missing_os_tail_rejected(self):
        self.os = self.os[:100]; self.save()
        with self.assertRaisesRegex(ValueError, 'OS tail'):
            analyze(self.path)

    def test_actors_cannot_disappear_from_averages(self):
        self.rows[-2]['lidar_actor_count'] = 1; self.save()
        with self.assertRaisesRegex(ValueError, 'cohort'):
            analyze(self.path)


if __name__ == '__main__':
    unittest.main()
