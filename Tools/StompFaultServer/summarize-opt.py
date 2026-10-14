"""Summarize PCD-OPT evidence without reading configuration or payloads."""
import argparse
import csv
import json
import re
from pathlib import Path


def summarize(folder):
    log = (folder / 'native.log').read_text(encoding='utf-8-sig', errors='replace')
    # AutomationController replays LogTemp entries at test end. Count source
    # observations once, retaining the original log as evidence.
    log = '\n'.join(line for line in log.splitlines() if 'LogAutomationController:' not in line)
    samples = []
    for line in log.splitlines():
        if '[PCD-OPT-SAMPLE]' in line or '[PCD20HZNativeStream]' in line:
            row = dict(re.findall(r'(\w+)=([^\s]+)', line.split('] ', 2)[-1]))
            row['kind'] = 'publisher' if '[PCD-OPT-SAMPLE]' in line else 'worker'
            samples.append(row)
    if samples:
        fields = sorted(set().union(*(r.keys() for r in samples)))
        with (folder / 'timeseries.csv').open('w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(samples)
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8-sig'))
    result = {'folder': str(folder), 'interval': manifest['Interval'], 'broker': manifest['Broker'],
              'commit': manifest['Commit'], 'seconds': manifest['Seconds'],
              'suspensions': log.count('Automatic PCD resume eligible at'),
              'resumes': len(re.findall(r'\[PCD-RESUME\].*resumed count=', log)),
              'raw_disconnects': len(re.findall(r'\[SensorRawConnection\] connected=0', log)),
              'hitches_over_100ms': log.count('[PCD-OPT-HITCH]')}
    ledger_path = folder / 'ledger.json'
    if ledger_path.exists():
        ledger = json.loads(ledger_path.read_text(encoding='utf-8-sig'))
        rows = [r for r in ledger['frames'] if r['kind'] == 2]
        begin, duration = ledger['measurement_start_monotonic'], ledger['duration_seconds']
        receipts = sorted(r['receipt_monotonic'] for r in rows if r['receipt_monotonic'] > 0)
        result.update(accepted=len(rows), receipt=sum(r['receipt_monotonic'] > 0 for r in rows),
                      consumed=sum(r['consumer_monotonic'] > 0 for r in rows),
                      accepted_hz=len(rows) / duration,
                      receipt_hz=sum(begin <= r < begin+duration for r in receipts) / duration,
                      max_receipt_gap_s=max((b-a for a, b in zip(receipts, receipts[1:])), default=0),
                      max_receipt_ms=max((r['receipt_ms'] for r in rows), default=0))
    worker = [r for r in samples if r['kind'] == 'worker']
    if worker:
        result['max_outstanding_bytes'] = max(int(r['outstandingBytes']) for r in worker)
        sizes = [int(r['frameBytes']) for r in worker if int(r['frameBytes']) > 0]
        result['mean_frame_bytes'] = sum(sizes)/len(sizes) if sizes else 0
        result['MBps'] = result.get('receipt_hz', 0) * result['mean_frame_bytes'] / 1e6
    pub = [r for r in samples if r['kind'] == 'publisher']
    if pub:
        for name in ('discarded', 'stale', 'motionStalls', 'maxWallMs', 'skipped'):
            values = [float(r[name]) for r in pub if name in r]
            if values:
                result[name] = max(values)
    index_path = folder / 'automation/index.json'
    if index_path.exists():
        index = json.loads(index_path.read_text(encoding='utf-8-sig'))
        result['automation'] = [t['state'] for t in index['tests']]
    (folder / 'summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('folders', nargs='+', type=Path)
    args = parser.parse_args()
    print(json.dumps([summarize(f) for f in args.folders], indent=2))
