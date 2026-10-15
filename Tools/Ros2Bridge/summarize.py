"""Summarize one UE/ROS acceptance cohort without reading credentials or bodies."""
import argparse
import json
from pathlib import Path
import re
import statistics


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def summarize(run):
    bridge = read(run.parent / (run.name+'-bridge.json'))
    ros = read(run.parent / (run.name+'-ros.json'))
    ledger = read(run/'ledger.json')
    automation = read(run/'automation/index.json')
    manifest = read(run/'manifest.json')
    log = (run/'native.log').read_text(encoding='utf8',errors='replace')
    source_matches = list(re.finditer(r'\[SensorPcdNoLossRhi\] input=(\d+) serialized=(\d+) submitted=(\d+) receipts=(\d+).*?gaps=(\d+) retries=(\d+) overload=(\d+)', log))
    if not source_matches:
        raise ValueError('No completed source summary')
    source = dict(zip(['input','serialized','submitted','receipts','gaps','retries','overload'],map(int,source_matches[-1].groups())))
    key, stream = next((k,v) for k,v in bridge['streams'].items() if k.endswith('/pcd'))
    topic = '/ma0t10/'+key.removesuffix('/pcd')+'/points'
    receiver = ros['streams'].get(topic)
    hz_log = (run/'ros2-topic-hz.log').read_text(encoding='utf8',errors='replace')
    bw_log = (run/'ros2-topic-bw.log').read_text(encoding='utf8',errors='replace')
    hz = list(map(float,re.findall(r'average rate: ([\d.]+)',hz_log)))
    bw = list(map(float,re.findall(r'([\d.]+) MB/s from',bw_log)))
    def samples(values):
        return {'samples':len(values),'last':values[-1] if values else None,
                'median':statistics.median(values) if values else None,
                'min':min(values) if values else None,'max':max(values) if values else None}
    frames = ledger['frames']
    sensor_ids = sorted(set(f['sensor_id'] for f in frames))
    measurement = {'seconds':ledger['duration_seconds'],'frames':len(frames),
                   'first_frame':min(f['frame_id'] for f in frames),'last_frame':max(f['frame_id'] for f in frames),
                   'sensor_ids':sensor_ids, 'viewport':[ledger['actual_viewport_x'],ledger['actual_viewport_y']],
                   'capacity_exceeded':ledger['record_capacity_exceeded']}
    return {'code_commit':manifest['Commit'],'dtcore':manifest['DTCore'],'dll_sha256':manifest['DllHash'],
            'interval':manifest['Interval'],'seconds':manifest['Seconds'],'rhi':manifest['Rhi'],
            'source_state':automation['tests'][0]['state'],'measurement':measurement,'source_total_including_warmup':source,
            'bridge_counts':bridge['counts'],'bridge_stream':stream,'bridge_queue_bytes':bridge['queue_bytes'],
            'ros_subscriber':receiver, 'ros_missing_from_total':stream['frames']-receiver['frames'] if receiver else None,
            'cli_hz':samples(hz),'cli_MBps':samples(bw),
            'notes':['Source totals and ROS counts include the ten-second warmup. Ledger is the 300-second measurement cohort.',
                     'ROS best effort subscriber missing count includes discovery and later DDS loss; exact phase attribution is unavailable.',
                     'Latency is raw WSL receive UTC minus Windows acquisition UTC; it includes clock offset.',
                     'Bridge latency is a bounded recent-6000 sample window, not necessarily every received frame.']}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('run',type=Path)
    args = p.parse_args()
    result = summarize(args.run)
    (args.run/'ros-summary.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    print(json.dumps({'source':result['source_state'],'source_frames':result['source_total_including_warmup']['submitted'],
                      'bridge_frames':result['bridge_stream']['frames'],'ros_frames':result['ros_subscriber']['frames'],
                      'ros_missing':result['ros_missing_from_total'],'cli_hz':result['cli_hz']['last']}))
