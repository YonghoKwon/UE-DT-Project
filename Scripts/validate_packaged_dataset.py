"""Validate the real three-scene output produced by the packaged UI smoke test."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def validate(root, scenes):
    manifest = json.loads((root / 'dataset.json').read_text(encoding='utf-8-sig'))
    assert manifest['requested_scenes'] == manifest['completed_scenes'] == scenes, 'incomplete manifest'
    commit = manifest['project_commit']
    source = manifest.get('project_commit_source', 'build')
    # TESTSET-04: unavailable provenance is explicit metadata, not failed capture.
    assert (re.fullmatch('[0-9a-f]{40}', commit) and source == 'build') or (
        commit == 'unknown' and source == 'unavailable'), 'invalid build provenance'
    assert manifest['camera_truth'] and manifest['lidar_fidelity'], 'required opt-ins absent'
    folders = sorted(p for p in root.glob('scene_*') if p.is_dir())
    assert len(folders) == scenes, 'scene directory count mismatch'
    expected = {'depth.exr', 'instance.png', 'label.json', 'lidar.pcd', 'lidar_labels.bin', 'rgb.png', 'seg.png'}
    details = []
    for index, folder in enumerate(folders):
        assert folder.name == f'scene_{index:05d}'
        assert {p.name for p in folder.iterdir()} == expected, 'partial or missing scene files'
        data = (folder / 'lidar.pcd').read_bytes()
        header, body = data.split(b'DATA binary\n', 1)
        points = int(re.search(rb'^POINTS (\d+)', header, re.MULTILINE)[1])
        assert len(body) == points * 33, 'PCD record size/count mismatch'
        assert (folder / 'lidar_labels.bin').stat().st_size == points * 5, 'label count mismatch'
        label = json.loads((folder / 'label.json').read_text(encoding='utf-8-sig'))
        resolution = tuple(int(n) for n in label['camera_resolution'])
        for name in ('instance.png', 'rgb.png', 'seg.png'):
            png = (folder / name).read_bytes()
            assert png[:8] == b'\x89PNG\r\n\x1a\n', 'invalid PNG'
            assert struct.unpack('>II', png[16:24]) == resolution, 'image/metadata resolution mismatch'
        assert (folder / 'depth.exr').read_bytes()[:4] == b'\x76\x2f\x31\x01', 'invalid EXR'
        details.append({'scene': index, 'points': points, 'resolution': resolution})
    files = [{'path': str(p.relative_to(root)), 'bytes': p.stat().st_size,
              'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(root.rglob('*')) if p.is_file()]
    return {'valid': True, 'root': str(root.resolve()), 'project_commit': commit, 'project_commit_source': source,
            'completed_scenes': scenes, 'scenes': details, 'files': files, 'bytes': sum(p['bytes'] for p in files)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dataset', type=Path)
    parser.add_argument('--scenes', type=int, default=3)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    try:
        result = validate(args.dataset, args.scenes)
    except (AssertionError, OSError, ValueError, KeyError, TypeError) as error:
        result = {'valid': False, 'root': str(args.dataset), 'error': str(error)}
    if args.report:
        args.report.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('files', 'scenes')}))
    return 0 if result['valid'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
