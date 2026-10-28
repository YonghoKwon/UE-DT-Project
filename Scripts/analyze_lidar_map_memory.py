"""Map-default/all-off and Shipping OS counter evidence; no substitution for original#26."""
import argparse
import datetime
import json
from pathlib import Path
from analyze_memory_trend import load_curve, fit, MIB


def analyze(directory):
    directory = Path(directory)
    points, context, warnings = load_curve(directory / 'memory-trend.jsonl')
    if not context['complete'] or not points or points[-1]['seconds'] < 3600:
        raise ValueError('Incomplete60min map run; preserve as partial evidence')
    tail = [dict(row) for row in points if row['seconds'] >= points[-1]['seconds'] - 1200]
    post = [row for row in points if row['seconds'] >= 600]
    if len(tail) < 230 or tail[-1]['seconds'] - tail[0]['seconds'] < 1180:
        raise ValueError('Insufficient tail coverage')
    ids = set(points[0]['sensors'])
    if any(set(row['sensors']) != ids or row['lidar_actor_count'] != len(ids) for row in points):
        raise ValueError('Sensor cohort changed/duplicate sensor identity')
    off = context['metadata']['all_lidar_off']
    if bool(ids) == off:
        raise ValueError('All-off/default sensor cohort invalid')
    hz = {name: (post[-1]['sensors'][name]['frame_id'] - post[0]['sensors'][name]['frame_id']) /
                (post[-1]['seconds'] - post[0]['seconds']) for name in sorted(ids)}
    metrics = {name: fit(tail, field, MIB) for name, field in [('RSS', 'rss_bytes'), ('Commit', 'commit_bytes')]}
    if all('RHIMisc' in row['llm_default'] for row in tail):
        for row in tail:
            row['rhi_bytes'] = row['llm_default']['RHIMisc']
        metrics['RHIMisc'] = fit(tail, 'rhi_bytes', MIB)
    deltas = {key: post[-1]['rhi_submission'][key] - post[0]['rhi_submission'][key]
              for key in ('native_dispatches', 'native_fallbacks', 'dispatches', 'scene_builds')}
    if off and any(deltas.values()):
        raise ValueError('All-off run contained sensor RT work')
    os_rows = [json.loads(line) for line in (directory / 'os-counters.jsonl').read_text(encoding='utf-8').splitlines()]
    start = datetime.datetime.fromisoformat(context['metadata']['external_start_utc']) if 'external_start_utc' in context['metadata'] else None
    # Runtime header file creation time is not a timing oracle. Align external
    # samples with the UTC explicitly recorded by the launcher's measurement watcher.
    timing = json.loads((directory / 'measurement-clock.json').read_text(encoding='utf-8-sig'))
    start = datetime.datetime.fromisoformat(timing['FirstCurveObservedUtc']) - datetime.timedelta(seconds=timing['FirstCurveSeconds'])
    selected = []
    for row in os_rows:
        if 'monitor_error' in row:
            continue  # Last process-exit race is retained in raw data, never a zero.
        row['seconds'] = (datetime.datetime.fromisoformat(row['utc']) - start).total_seconds()
        if points[-1]['seconds'] - 1200 <= row['seconds'] <= points[-1]['seconds']:
            selected.append(row)
    if len(selected) < 225 or selected[-1]['seconds'] - selected[0]['seconds'] < 1170:
        raise ValueError('OS tail counter coverage insufficient')
    os_metrics = {name: fit(selected, field, MIB) for name, field in (
        ('WorkingSet', 'working_set_bytes'), ('PrivateWorkingSet', 'private_working_set_bytes'),
        ('PrivateBytes', 'private_bytes'), ('Commit', 'commit_bytes'))}
    return dict(Valid=True, Complete=True, Configuration=context['metadata']['configuration'],
                Map=context['metadata']['map'], AllLidarOff=off, ActualHz=hz,
                Tail20=metrics, OsTail20=os_metrics, RhiSubmissionDeltas=deltas, Warnings=warnings,
                Limits='Descriptive additional map/OS comparison, not original8MiB5pair or2h acceptance. PrivateBytes=private Commit per Windows; no caller attribution.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    result = analyze(args.directory)
    (args.directory / 'analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
