"""Summarize top-level VMMap regions without exporting addresses or file paths."""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def summarize(root):
    snapshots = []
    for snap in root.findall('./Snapshots/Snapshot'):
        types = {}
        for row in snap.findall('./MemoryRegions/Region'):
            # Child Region rows are subdivisions already included in the parent.
            kind = row.get('Type', 'Unknown')
            if kind in ('Free', 'Unusable'):
                continue
            item = types.setdefault(kind, {k: 0 for k in
                ['Size', 'Commit', 'PrivateBytes', 'PrivateWS', 'ShareableWS', 'SharedWS', 'RegionCount']})
            item['RegionCount'] += 1
            for key in item:
                if key != 'RegionCount':
                    item[key] += int(row.get(key, '0'))
        snapshots.append({'timestamp_filetime': int(snap.get('Timestamp', '0')), 'Types': types})
    if not snapshots:
        raise ValueError('VMMap snapshot is missing')
    return {'Valid': True, 'Pid': int(root.get('PID', '0')), 'Snapshots': snapshots,
            'Scope': 'VMMap classifications; top-level regions only, no nested double counting. Heap commit is not proof of outstanding heap leaks. SharedWS is a subset of ShareableWS; never sum both. Addresses, raw memory, file paths omitted.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('xml', type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    raw = args.xml.read_bytes()
    data = summarize(ET.fromstring(raw))
    data['InputSHA256'] = hashlib.sha256(raw).hexdigest()
    args.output.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'Pid': data['Pid'], 'Types': list(data['Snapshots'][-1]['Types'])}))


if __name__ == '__main__':
    main()
