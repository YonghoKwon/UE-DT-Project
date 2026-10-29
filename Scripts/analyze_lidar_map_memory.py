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
    if any(set(row['sensors']) != ids or row.get('lidar_actor_count', len(ids)) != len(ids) for row in points):
        raise ValueError('Sensor cohort changed/duplicate sensor identity')
    off = context['metadata'].get('all_lidar_off', False)
    if bool(ids) == off:
        raise ValueError('All-off/default sensor cohort invalid')
    hz = {name: (post[-1]['sensors'][name]['frame_id'] - post[0]['sensors'][name]['frame_id']) /
                (post[-1]['seconds'] - post[0]['seconds']) for name in sorted(ids)}
    metrics = {name: fit(tail, field, MIB) for name, field in [('RSS', 'rss_bytes'), ('Commit', 'commit_bytes')]}
    if context['metadata'].get('llm_enabled', True) and all('RHIMisc' in row['llm_default'] for row in tail):
        for row in tail:
            row['rhi_bytes'] = row['llm_default']['RHIMisc']
        metrics['RHIMisc'] = fit(tail, 'rhi_bytes', MIB)
    deltas = {key: post[-1]['rhi_submission'][key] - post[0]['rhi_submission'][key]
              for key in ('native_dispatches', 'native_fallbacks', 'dispatches', 'scene_builds')}
    if off and any(deltas.values()):
        raise ValueError('All-off run contained sensor RT work')
    os_rows = [json.loads(line) for line in (directory / 'os-counters.jsonl').read_text(encoding='utf-8').splitlines()]
    # Prefer the runtime's own UTC. The launcher's polling timestamp can be late,
    # which would otherwise admit orderly shutdown/decommit into the tail.
    timing = json.loads((directory / 'measurement-clock.json').read_text(encoding='utf-8-sig'))
    if context['metadata'].get('started_utc'):
        start = datetime.datetime.fromisoformat(context['metadata']['started_utc'])
        end_guard = 0
        clock_source = 'runtime metadata started_utc (written before monotonic start)'
    else:
        start = datetime.datetime.fromisoformat(timing['FirstCurveObservedUtc']) - datetime.timedelta(seconds=timing['FirstCurveSeconds'])
        end_guard = 2 * timing.get('PollingUncertaintySeconds', 5)
        clock_source = 'launcher watcher; endpoint guarded by twice recorded polling uncertainty'
    selected = []
    after_window = 0
    for row in os_rows:
        if 'monitor_error' in row:
            error_seconds = (datetime.datetime.fromisoformat(row['utc']) - start).total_seconds()
            if row is not os_rows[-1] or error_seconds < points[-1]['seconds'] - 5:
                raise ValueError('OS monitor failed inside measurement window')
            warnings.append('Terminal OS process-exit counter race retained; not converted to zero')
            continue
        row['seconds'] = (datetime.datetime.fromisoformat(row['utc']) - start).total_seconds()
        if row['seconds'] > points[-1]['seconds'] - end_guard:
            after_window += 1
        elif points[-1]['seconds'] - 1200 <= row['seconds']:
            selected.append(row)
    if len(selected) < 225 or selected[-1]['seconds'] - selected[0]['seconds'] < 1170:
        raise ValueError('OS tail counter coverage insufficient')
    identities = {(row.get('pid'), row.get('creation_filetime')) for row in os_rows if 'monitor_error' not in row}
    if len(identities) != 1:
        raise ValueError('OS counter process identity changed')
    os_metrics = {name: fit(selected, field, MIB) for name, field in (
        ('WorkingSet', 'working_set_bytes'), ('PrivateWorkingSet', 'private_working_set_bytes'),
        ('PrivateBytes', 'private_bytes'), ('Commit', 'commit_bytes'))}
    if (directory / 'environment-overlap.json').exists():
        raise ValueError('Environment overlap marker present; keep run as rejected evidence')
    environment = dict(Recorded=False, CleanObserved=None)
    watch_path = directory / 'slot-watch.jsonl'
    if watch_path.exists():
        watch = [json.loads(line) for line in watch_path.read_text(encoding='utf-8-sig').splitlines()]
        if not watch or any(not isinstance(row.get('Foreign'), list) for row in watch):
            raise ValueError('Environment watch is empty or malformed')
        if any(row['Foreign'] for row in watch):
            raise ValueError('External Editor/game environment overlap observed')
        times = [(datetime.datetime.fromisoformat(row['Utc']) - start).total_seconds() for row in watch]
        if any(b <= a for a, b in zip(times, times[1:])):
            raise ValueError('Environment watch time is not increasing')
        if times[0] > 5 or times[-1] < points[-1]['seconds'] - 5:
            raise ValueError('Environment watch does not cover measurement window')
        environment = dict(Recorded=True, CleanObserved=True, Samples=len(watch),
                           FirstSeconds=times[0], LastSeconds=times[-1],
                           MaximumPollGapSeconds=max((b-a for a,b in zip(times,times[1:])), default=0),
                           Scope='Observed process polls only; absence between polls is not proven')
    return dict(Valid=True, Complete=True, Configuration=context['metadata']['configuration'],
                Map=context['metadata'].get('map', 'ShipHatchTestMap (hatch runtime fixture)'), AllLidarOff=off, ActualHz=hz,
                Tail20=metrics, OsTail20=os_metrics, RhiSubmissionDeltas=deltas, Warnings=warnings,
                EnvironmentWindow=environment,
                OsWindow=dict(ClockSource=clock_source, EndGuardSeconds=end_guard,
                              ExcludedAfterMeasurementOrGuard=after_window,
                              FirstSeconds=selected[0]['seconds'], LastSeconds=selected[-1]['seconds']),
                Limits='Descriptive additional map/OS comparison, not original8MiB5pair or2h acceptance. PrivateBytes=private Commit per Windows; no caller attribution.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    result = analyze(args.directory)
    (args.directory / 'analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
