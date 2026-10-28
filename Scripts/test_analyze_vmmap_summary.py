import unittest
import xml.etree.ElementTree as ET
from analyze_vmmap_summary import summarize


class SummaryTests(unittest.TestCase):
    def test_nested_blocks_are_not_counted_twice(self):
        r = ET.fromstring('<root PID="42"><Snapshots><Snapshot Timestamp="7"><MemoryRegions>'
            '<Region Type="Heap" Commit="100" Size="200"><Region Type="Heap" Commit="100" Size="200"/></Region>'
            '<Region Type="Heap" Commit="50" Size="60"/>'
            '<Region Type="Free" Commit="0" Size="99999"/>'
            '</MemoryRegions></Snapshot></Snapshots></root>')
        x = summarize(r)
        self.assertEqual(x['Snapshots'][0]['Types']['Heap']['Commit'], 150)
        self.assertEqual(x['Snapshots'][0]['Types']['Heap']['RegionCount'], 2)
        self.assertNotIn('Free', x['Snapshots'][0]['Types'])

    def test_missing_snapshot_is_not_zero_heap(self):
        with self.assertRaises(ValueError):
            summarize(ET.fromstring('<root/>'))

    def test_shared_fields_remain_separate(self):
        r = ET.fromstring('<root><Snapshots><Snapshot><MemoryRegions>'
            '<Region Type="Image" PrivateWS="4" ShareableWS="8" SharedWS="6"/>'
            '</MemoryRegions></Snapshot></Snapshots></root>')
        x = summarize(r)['Snapshots'][0]['Types']['Image']
        self.assertEqual(x['PrivateWS'], 4)
        self.assertEqual(x['ShareableWS'], 8)
        self.assertEqual(x['SharedWS'], 6)


if __name__ == '__main__':
    unittest.main()
