import unittest
from analyze_memory_cut import analyze_cut


def fixture(off):
    rows = [{'seconds': t, 'rss_bytes': 1000000, 'commit_bytes': 2000000, 'uobject_count': 100,
             'llm_default': {'RHIMisc': 100, 'Untracked': t * 100}, 'llm_platform': {},
             'sensors': {s: {'frame_id': 0 if off else t * 20, 'running': not off,
                             'visible_preview_points': 12000 if off else 0} for s in ['a', 'b', 'c']},
             'rhi_submission': {'dispatches': 0 if off else t * 20}} for t in range(0, 1801, 5)]
    context = {'complete': True, 'metadata': {'diagnostic_acquisition_off': off, 'diagnostic_outputs_off': not off}}
    return rows, context


class CutTests(unittest.TestCase):
    def test_zero_hz_is_recorded_for_all_sensors(self):
        rows, context = fixture(True)
        self.assertEqual(analyze_cut(rows, context)['ActualHz'], {'a': 0, 'b': 0, 'c': 0})

    def test_on_has_three20hz_sensors_and_tag_growth(self):
        rows, context = fixture(False)
        x = analyze_cut(rows, context)
        self.assertEqual(x['ActualHz'], {'a': 20, 'b': 20, 'c': 20})
        self.assertGreater(x['LlmTags']['llm_default']['Untracked']['slope_per_hour'], 0)

    def test_frame_progress_in_stopped_cut_rejected(self):
        rows, context = fixture(True)
        rows[-1]['sensors']['c']['frame_id'] = 1
        with self.assertRaises(ValueError):
            analyze_cut(rows, context)

    def test_missing_sensor_not_averaged_away(self):
        rows, context = fixture(False)
        del rows[-1]['sensors']['c']
        with self.assertRaises(ValueError):
            analyze_cut(rows, context)

    def test_incomplete_run_rejected(self):
        rows, context = fixture(False)
        context['complete'] = False
        with self.assertRaises(ValueError):
            analyze_cut(rows, context)

    def test_output_stop_must_be_effective(self):
        rows, context = fixture(False)
        rows[-1]['pcd'] = {'a': {'serialized_frames': 1, 'submitted_bytes': 33}}
        with self.assertRaises(ValueError):
            analyze_cut(rows, context)


if __name__ == '__main__':
    unittest.main()
