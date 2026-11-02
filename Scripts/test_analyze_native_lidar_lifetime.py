import unittest
from analyze_native_lidar_lifetime import analyze


def create(va='1000', size=64, role='B.Vertices'):
    return f'[SensorNativeLedger] create VA=0x{va} bytes={size} heap=2 state=2755 role={role}\n'


def release(va='1000', used=7, done=7):
    return f'[SensorNativeLedger] release VA=0x{va} bytes=64 role=B.Vertices last_use={used} completed={done} device=0x00000000\n'


class LedgerAnalysisTests(unittest.TestCase):
    def test_missing_ledger_is_not_evidence_of_zero_errors(self):
        self.assertFalse(analyze('DRED: No PageFault data.\n')['Valid'])

    def test_freed_fault_matches_same_process_range_and_dred_name(self):
        log = create() + release() + 'DXGI_ERROR_DEVICE_REMOVED\nDRED: PageFault at VA GPUAddress "0x101F"\nDRED: Recent freed objects with VA ranges that match the faulting VA:\nName: SensorOwned/B.Vertices/0x1000/64 (Type: Resource)\n'
        x = analyze(log)
        self.assertFalse(x['Valid'])
        f = x['Faults'][0]
        self.assertTrue(f['native_range_generations'][0]['released_before_fault_report'])
        self.assertEqual(f['native_range_generations'][0]['recorded_completed_fence'], 7)
        self.assertTrue(f['matching_dred_names'][0]['recently_freed'])

    def test_reused_address_preserves_both_generations_and_ambiguity(self):
        x = analyze(create() + release() + create(role='S.Scratch') + '[SensorNativeDRED] faultVA=0x1010\n')
        self.assertEqual(len(x['Faults'][0]['native_range_generations']), 2)
        self.assertTrue(x['Faults'][0]['address_reused_or_ambiguous'])

    def test_range_end_is_exclusive(self):
        x = analyze(create() + '[SensorNativeDRED] faultVA=0x1040\n')
        self.assertEqual(x['Faults'][0]['native_range_generations'], [])

    def test_logs_never_share_address_state(self):
        analyze(create())
        x = analyze('[SensorNativeDRED] faultVA=0x1010\n')
        self.assertEqual(x['Faults'][0]['native_range_generations'], [])

    def test_concatenated_process_sessions_rejected(self):
        with self.assertRaises(ValueError):
            analyze('LogInit: Command Line: first\n' + create() + 'LogInit: Command Line: second\n')

    def test_automation_reprints_are_not_second_allocations(self):
        x = analyze(create() + 'LogAutomationController: ' + create())
        self.assertEqual(x['Creates'], 1)
        self.assertEqual(x['RecordedReferenceErrors'], [])

    def test_fence_violation_and_stale_reference_are_separate_errors(self):
        x = analyze(create() + release(used=9, done=8) + '[SensorNativeBinding] surface=0x1000\n')
        self.assertEqual({e['kind'] for e in x['RecordedReferenceErrors']},
                         {'release_before_recorded_fence', 'recorded_reference_not_live'})

    def test_device_failure_without_fault_va_is_not_a_pass(self):
        x = analyze('DXGI_ERROR_DEVICE_REMOVED\nDRED: No PageFault data.\n')
        self.assertFalse(x['Valid'])
        self.assertTrue(x['DeviceFailureObserved'])
        self.assertEqual(x['Faults'], [])

    def test_zero_unavailable_fault_va_not_joined(self):
        x = analyze('[SensorNativeDRED] result=0x887a0004 faultVA=0x0\n')
        self.assertEqual(x['Faults'], [])

    def test_duplicate_fault_messages_are_one_address_incident(self):
        x = analyze('Faulting address: 0x1010\nDRED: PageFault at VA GPUAddress "0x1010"\n')
        self.assertEqual(len(x['Faults']), 1)
        self.assertEqual(x['Faults'][0]['lines'], [1, 2])

    def test_nonselected_metadata_oob_candidate_is_not_mislabeled_uaf(self):
        log = create(size=64) + create(va='5000', size=16384)
        log += '[SensorNativeBlasBuild] vertex_count=3000 result=0x9000\n'
        log += '[SensorNativeBlasBuild] vertex_count=3 result=0xa000\n'
        log += '[SensorNativeSubmit] fence=8 completed=7\n'
        log += '[SensorNativeBinding] fence=8 mesh=0 surface=0x5000 blas=0x9000\n'
        log += '[SensorNativeBinding] fence=8 mesh=3 surface=0x1000 blas=0xa000\n'
        log += 'DRED: PageFault at VA GPUAddress "0x1100"\n'
        x = analyze(log)
        f = x['Faults'][0]
        self.assertEqual(f['native_range_generations'], [])
        self.assertEqual(f['UnboundedRootSrvOobCandidates'][0]['mesh_slot'], 3)
        self.assertEqual(f['UnboundedRootSrvOobCandidates'][0]['derived_triangle_index_at_fault_va'], 16)

    def test_completed_submit_is_not_in_flight_oob_candidate(self):
        log = create(size=64) + '[SensorNativeBlasBuild] vertex_count=3000 result=0x9000\n'
        log += '[SensorNativeSubmit] fence=8 completed=8\n[SensorNativeBinding] fence=8 mesh=0 surface=0x1000 blas=0x9000\n'
        log += 'Faulting address: 0x1100\n'
        self.assertEqual(analyze(log)['Faults'][0]['UnboundedRootSrvOobCandidates'], [])

    def test_removed_device_fence_sentinel_is_not_safe_completion(self):
        log = create(size=64) + '[SensorNativeBlasBuild] vertex_count=3000 result=0x9000\n'
        log += '[SensorNativeSubmit] fence=8 completed=18446744073709551615\n'
        log += '[SensorNativeBinding] fence=8 mesh=0 surface=0x1000 blas=0x9000\nFaulting address: 0x1100\n'
        self.assertEqual(len(analyze(log)['Faults'][0]['UnboundedRootSrvOobCandidates']), 1)


if __name__ == '__main__':
    unittest.main()
