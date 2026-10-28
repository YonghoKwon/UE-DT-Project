"""Descriptive30min acquisition/output cuts; never promote them to normal gates."""
import argparse
import json
import math
from pathlib import Path
from analyze_memory_trend import load_curve, fit, MIB


def analyze_cut(points, context):
    meta = context['metadata']
    off = meta.get('diagnostic_acquisition_off') is True
    outputs = meta.get('diagnostic_outputs_off') is True
    if not context['complete'] or (off == outputs):
        raise ValueError('A complete explicit acquisition-off OR outputs-off cut is required')
    if not points or points[-1]['seconds'] < 1800:
        raise ValueError('Full30min cut required')
    ids = set(points[0].get('sensors', {}))
    if len(ids) != 3:
        raise ValueError('All3 existing sensors must be recorded, including zeroHz')
    previous = -1
    for x in points:
        if x['seconds'] <= previous or set(x['sensors']) != ids:
            raise ValueError('Nonmonotonic samples or changed sensor set')
        previous = x['seconds']
        for key in ['seconds', 'rss_bytes', 'commit_bytes', 'uobject_count']:
            if not math.isfinite(x[key]):
                raise ValueError('Nonfinite data')
        for s in x['sensors'].values():
            if s['running'] != (not off):
                raise ValueError('Scan running state differs from the cut')
            if outputs and s['visible_preview_points'] != 0:
                raise ValueError('Preview not disabled in outputs-off cut')
        for stream in x.get('pcd', {}).values():
            if any(stream.get(k, 0) != 0 for k in ['serialized_frames', 'submitted_frames', 'submitted_bytes']):
                raise ValueError('PCD serialization/submission progressed in disabled-output cut')
    post = [x for x in points if x['seconds'] >= 600]
    tail = [x for x in post if x['seconds'] >= points[-1]['seconds'] - 1200]
    if len(tail) < 12 or tail[-1]['seconds'] - tail[0]['seconds'] < 1140:
        raise ValueError('Common20min tail unavailable')
    hz = {sid: (post[-1]['sensors'][sid]['frame_id'] - post[0]['sensors'][sid]['frame_id']) /
          (post[-1]['seconds'] - post[0]['seconds']) for sid in sorted(ids)}
    if (off and any(v != 0 for v in hz.values())) or (not off and any(v <= 0 for v in hz.values())):
        raise ValueError('Unexpected frame progress; zeroHz is never omitted')
    copied = []
    for x in tail:
        tags = {**x.get('llm_default_top', {}), **x.get('llm_default', {})}
        if 'RHIMisc' not in tags:
            raise ValueError('RHIMisc unobserved')
        copied.append({**x, 'rhi_bytes': tags['RHIMisc']})
    metrics = {k: fit(copied, field, scale) for k, field, scale in
               [('RSS', 'rss_bytes', MIB), ('Commit', 'commit_bytes', MIB),
                ('RHIMisc', 'rhi_bytes', MIB), ('UObject', 'uobject_count', 1)]}
    tag_results = {}
    for tracker in ['llm_default', 'llm_platform']:
        keys = set().union(*(x.get(tracker, {}) for x in tail))
        result = {}
        for name in sorted(keys):
            observed = [{'seconds': x['seconds'], 'bytes': x[tracker][name]} for x in tail if name in x.get(tracker, {})]
            row = {'observed_samples': len(observed), 'continuous': len(observed) == len(tail)}
            if row['continuous']:
                row.update(fit(observed, 'bytes', MIB))
            result[name] = row
        tag_results[tracker] = result
    return {'Valid': True, 'Mode': 'acquisition-off' if off else 'outputs-off',
            'DurationSeconds': points[-1]['seconds'], 'ExcludedFirstSeconds': 600,
            'TailSeconds': 1200, 'ActualHz': hz, 'Metrics': metrics, 'LlmTags': tag_results,
            'RHISubmissionDeltas': {k: post[-1]['rhi_submission'][k] - post[0]['rhi_submission'][k]
                                    for k in post[-1].get('rhi_submission', {})},
            'Scope': 'Diagnostic cut only; same requested profile, changed activity/output. ZeroHz retained. LLM tag hierarchies are not summed. Missing tags are unobserved. Does not establish normal memory, accuracy, throughput, original8MiB orABAB acceptance.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('curve', type=Path)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    points, context, _ = load_curve(a.curve)
    result = analyze_cut(points, context)
    a.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'mode': result['Mode'], 'hz': result['ActualHz'],
                      'slopes': {k: v['slope_per_hour'] for k, v in result['Metrics'].items()}}))


if __name__ == '__main__':
    main()
