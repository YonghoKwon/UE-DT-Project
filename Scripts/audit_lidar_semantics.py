"""Audit one real dataset's binary PCD/semantic pairs without Unreal or a GPU."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct


def audit(dataset, expected_backend=None, require_slab=False):
    dataset = Path(dataset)
    manifest_path = dataset / 'dataset.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    classes = {int(row['id']): row['name'] for row in manifest['classes']}
    instances = {int(row['id']) for row in manifest['instances']}
    requested, completed = manifest['requested_scenes'], manifest['completed_scenes']
    if not isinstance(completed, int) or completed <= 0 or requested != completed:
        raise ValueError('incomplete dataset manifest')
    folders = sorted(p for p in dataset.glob('scene_*') if p.is_dir())
    if len(folders) != completed:
        raise ValueError('scene count differs from manifest')
    total, instance_counts, backends = Counter(), Counter(), Counter()
    scenes = []
    for folder in folders:
        metadata = json.loads((folder / 'label.json').read_text(encoding='utf-8-sig'))
        pcd = (folder / 'lidar.pcd').read_bytes()
        if b'DATA binary\n' not in pcd:
            raise ValueError('expected binary PCD, not compressed/ascii')
        header, body = pcd.split(b'DATA binary\n', 1)
        match = re.search(rb'^POINTS (\d+)', header, re.MULTILINE)
        if not match:
            raise ValueError('PCD POINTS missing')
        points = int(match[1])
        labels = (folder / 'lidar_labels.bin').read_bytes()
        if len(body) != points * 33 or len(labels) != points * 5:
            raise ValueError('PCD 33B/label 5B cardinality mismatch')
        counts, ids = Counter(), Counter()
        named_slab = 0
        for semantic, instance in struct.iter_unpack('<BI', labels):
            if semantic not in classes or (instance != 0 and instance not in instances):
                raise ValueError('semantic/instance ID absent from this dataset manifest')
            counts[semantic] += 1
            ids[instance] += 1
            named_slab += classes[semantic] == 'Slab' and instance != 0
        backend = metadata.get('lidar_backend', 'unknown')
        backends[backend] += 1
        total.update(counts)
        instance_counts.update(ids)
        scenes.append({
            'scene': folder.name, 'points': points, 'backend': backend,
            'frame_id': metadata.get('lidar_frame_id'),
            'classes': {str(k): {'name': classes[k], 'points': v} for k, v in counts.items()},
            'instances': dict(ids), 'named_slab_points': named_slab,
            'pcd_sha256': hashlib.sha256(pcd).hexdigest(),
            'labels_sha256': hashlib.sha256(labels).hexdigest(),
            'slab_pose': metadata.get('slab_pose'), 'slab_size_m': metadata.get('slab_size_m'),
            'lidar_pose': metadata.get('lidar_pose'),
        })
    slab_points = sum(v for k, v in total.items() if classes[k] == 'Slab')
    named_slab_points = sum(row['named_slab_points'] for row in scenes)
    backend_ok = expected_backend is None or set(backends) == {expected_backend}
    positive_ok = not require_slab or all(row['named_slab_points'] > 0 for row in scenes)
    return {
        'schema': 'ma0t10.lidar-semantic-audit.v1', 'Valid': backend_ok and positive_ok,
        'dataset': str(dataset.resolve()), 'project_commit': manifest.get('project_commit'),
        'manifest_sha256': hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        'completed_scenes': completed, 'points': sum(total.values()),
        'classes': {str(k): {'name': classes[k], 'points': v} for k, v in total.items()},
        'instance_zero': instance_counts[0], 'slab_points': slab_points,
        'named_slab_points': named_slab_points, 'backends': dict(backends),
        'expected_backend': expected_backend, 'backend_matches': backend_ok,
        'positive_slab_required': require_slab, 'positive_slab_each_scene': positive_ok,
        'scenes': scenes,
        'note': 'Positive fixture acceptance only; not semantic accuracy across every surface or consumer ACK.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dataset', type=Path)
    parser.add_argument('--expect-backend', type=int, choices=(1, 2, 3))
    parser.add_argument('--require-slab', action='store_true')
    parser.add_argument('--summary', type=Path)
    args = parser.parse_args()
    try:
        result = audit(args.dataset, args.expect_backend, args.require_slab)
    except (OSError, ValueError, KeyError, TypeError) as error:
        result = {'Valid': False, 'dataset': str(args.dataset), 'error': str(error)}
    if args.summary:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('scenes',)}, ensure_ascii=False))
    return 0 if result['Valid'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
