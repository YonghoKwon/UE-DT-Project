import unittest
from compare_hatch_memory import LIMIT, compare_pair


def run(clicks, delta):
    return {"clicks": clicks, "measurement_seconds": 90.5, "matched_window_seconds": 90,
            **{key: value for before, after in (("raw_rss_before", "raw_rss_after"), ("rss_before", "rss_after"), ("commit_before", "commit_after"))
               for key, value in ((before, 100_000_000), (after, 100_000_000 + delta))}}


class MatchedMemoryTests(unittest.TestCase):
    def test_absolute_failure_stays_failure_when_difference_passes(self):
        row = compare_pair(run(50, LIMIT * 3), run(0, LIMIT * 3))["raw_rss"]
        self.assertFalse(row["original_dig_8mib_pass"])
        self.assertTrue(row["separate_difference_8mib_pass"])

    def test_difference_keeps_exact_eight_mib_boundary(self):
        self.assertTrue(compare_pair(run(50, LIMIT), run(0, 0))["commit"]["separate_difference_8mib_pass"])
        self.assertFalse(compare_pair(run(50, LIMIT + 1), run(0, 0))["commit"]["separate_difference_8mib_pass"])

    def test_time_mismatch_is_rejected(self):
        control = run(0, 0)
        control["measurement_seconds"] += 3
        with self.assertRaises(ValueError):
            compare_pair(run(50, 0), control)

    def test_incorrect_excavation_count_is_rejected(self):
        with self.assertRaises(ValueError):
            compare_pair(run(49, 0), run(0, 0))


if __name__ == "__main__":
    unittest.main()
