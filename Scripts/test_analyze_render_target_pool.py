import unittest
from analyze_render_target_pool import analyze

def element(name='SceneDepth',address='A',width=1920,flags='1',slot=0):
    return dict(name=name,element=address,rhi='R'+address,width=width,height=1080,format='PF_DepthStencil',flags=flags,slot=slot,bytes=1024,free=True)
def row(stage,qpc,elements):
    return dict(type='pool',stage=stage,qpc=qpc,cycle=0,elements=elements,count=len(elements),allocated_kib=len(elements),used_kib=0,commit_bytes=1000,collect_rt_ms=.1)

class PoolAnalysisTests(unittest.TestCase):
    def test_descriptor_name_and_slot_are_separate(self):
        a=element();b=element('HZB',slot=8)
        out=analyze([row('before',1,[a]),row('after',2,[b])])
        self.assertTrue(out['NameChangesOnObservedIdentity'][0]['DescriptorUnchanged'])
        self.assertEqual(len(out['FirstObservations']),1)
    def test_dimension_and_flags_variation_is_reported(self):
        out=analyze([row('before',1,[element()]),row('after',2,[element(address='B',width=1280,flags='2')])])
        self.assertEqual(set(out['DescriptorChanges'][0]['ChangedFields']),{'width','flags'})
    def test_disappearance_is_not_called_gpu_free(self):
        out=analyze([row('before',1,[element()]),row('after',2,[])])
        self.assertEqual(len(out['DeparturesFromPool']),1)
        self.assertNotIn('FreedGpuBytes',out)
    def test_creation_window_excludes_warmup_and_shutdown(self):
        creates=[dict(type='pool_create',qpc=n,name='X') for n in (1,3,5,7)]+[dict(type='summary',complete=True)]
        out=analyze([row('before',2,[]),row('after',6,[])],creates)
        self.assertEqual(out['CreationEventCountDuringMeasurement'],2)
    def test_bad_boundaries_and_incomplete_cpu_trace_rejected(self):
        with self.assertRaises(ValueError):analyze([row('before',2,[]),row('after',1,[])])
        with self.assertRaises(ValueError):analyze([row('before',1,[]),row('after',2,[])],[dict(type='summary',complete=False)])
    def test_duplicate_identity_rejected(self):
        with self.assertRaises(ValueError):analyze([row('before',1,[element(),element()]),row('after',2,[])])

if __name__=='__main__':unittest.main()
