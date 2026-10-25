import unittest
from copy import deepcopy
from analyze_memory_fix_variation import describe,baseline_stats,compare_cut,MIB

def curve(duration,slope=12,warm_spike=False):
    rows=[]
    for t in range(0,duration+1,5):
        value=1000*MIB+t/3600*slope*MIB
        if warm_spike and t<600:value+=t*10*MIB
        rows.append({'seconds':t,'rss_bytes':value,'commit_bytes':value,'uobject_count':25000,'llm_enabled':True,'llm_default':{'RHIMisc':value},
                     'sensors':{k:{'frame_id':t*20} for k in ['a','b','c']}})
    ctx={'complete':True,'metadata':{'source_commit':'test','allocator':'test'},'summary':{'pcd':{}}}
    return rows,ctx

def result(slope,hz=20):
    return {'Metrics':{k:{'tail20':{'slope_per_hour':slope}} for k in ['RSS','Commit','RHIMisc']},'ActualHzPostwarmup':{'a':hz,'b':hz,'c':hz}}

class VariationTests(unittest.TestCase):
    def test_excludes_ten_minute_spike(self):
        d=describe(*curve(3600,12,True));self.assertAlmostEqual(d['Metrics']['RSS']['postwarmup']['slope_per_hour'],12);self.assertAlmostEqual(d['Metrics']['RSS']['tail20']['slope_per_hour'],12)
    def test_same_tail_horizon_thirty_and_sixty(self):
        for duration in [1800,3600]:self.assertEqual(describe(*curve(duration))['TailSamples'],241)
    def test_partial_data_rejected(self):
        rows,ctx=curve(3600);ctx['complete']=False
        with self.assertRaises(ValueError):describe(rows,ctx)
    def test_missing_tag_is_not_zero(self):
        rows,ctx=curve(3600);rows[-1]['llm_default']={}
        with self.assertRaises(ValueError):describe(rows,ctx)
    def test_zero_sensor_not_omitted(self):
        rows,ctx=curve(3600)
        for x in rows:x['sensors']['a']['frame_id']=0
        with self.assertRaises(ValueError):describe(rows,ctx)
    def test_exactly_three_references(self):
        with self.assertRaises(ValueError):baseline_stats([result(1),result(2)])
    def test_noise_and_reduction(self):
        base=[result(x) for x in [10,14,12]];self.assertEqual(baseline_stats(base)['RSS']['range'],4)
        self.assertTrue(compare_cut(base,result(7))['Candidate']);self.assertFalse(compare_cut(base,result(9))['Candidate'])
    def test_slowdown_and_tradeoff_excluded(self):
        base=[result(x) for x in [10,14,12]];self.assertFalse(compare_cut(base,result(1,10))['Candidate'])
        cut=result(1);cut['Metrics']['Commit']['tail20']['slope_per_hour']=100;self.assertFalse(compare_cut(base,cut)['Candidate'])
    def test_minimum_effect_prevents_tiny_candidate(self):
        self.assertFalse(compare_cut([result(x) for x in [10,10.01,10.02]],result(9.5))['Candidate'])

if __name__=='__main__':unittest.main()
