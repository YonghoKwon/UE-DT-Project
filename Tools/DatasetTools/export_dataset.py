"""Explicit offline export dispatcher. All outputs are new, source dataset remains immutable."""
from __future__ import annotations
import argparse
from pathlib import Path
import sys
from export_common import Dataset, cancelled, destination, source_hash, write_json


def export(dataset, output, formats=('coco',), upright_envelope=False, semantic_kitti=False, cancel=None):
    from export_coco import write_coco
    from export_kitti import write_kitti
    from validate_export import validate_coco, validate_kitti
    if not formats or len(set(formats)) != len(formats) or any(f not in ('coco', 'kitti') for f in formats):
        raise ValueError('select coco and/or kitti explicitly')
    if semantic_kitti and 'kitti' not in formats:
        raise ValueError('SemanticKITTI requires kitti')
    data = Dataset(dataset)
    digest = source_hash(data.root)
    report = {'schema': 'ma0t10.dataset.export.v1', 'source': str(data.root),
              'project_commit': data.manifest.get('project_commit'), 'source_sha256': digest, 'formats': {}}
    with destination(data.root, output) as out:
        for fmt in formats:
            cancelled(cancel)
            target = out/fmt
            if fmt == 'coco':
                r = write_coco(data, target, cancel)
                r['reload'] = validate_coco(target, data.root)
            else:
                r = write_kitti(data, target, upright_envelope, semantic_kitti, cancel)
                r['reload'] = validate_kitti(target, data.root)
            report['formats'][fmt] = r
            write_json(target/'export-report.json', r)
        cancelled(cancel)
        if source_hash(data.root) != digest:
            raise ValueError('source dataset changed during export')
        write_json(out/'export-report.json', report)
    return report


def cli(single=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dataset', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--formats', default='coco', help='coco,kitti; only for export_dataset.py')
    p.add_argument('--upright-envelope', action='store_true', help='explicitly approximate tilted boxes by camera-upright enclosing boxes')
    p.add_argument('--semantic-kitti', action='store_true', help='also write custom semantic-ID point labels')
    p.add_argument('--cancel-file', type=Path, help=argparse.SUPPRESS)
    a = p.parse_args()
    try:
        if single == 'coco':
            from export_coco import export_coco
            export_coco(a.dataset, a.output, a.cancel_file)
        elif single == 'kitti':
            from export_kitti import export_kitti
            export_kitti(a.dataset, a.output, a.upright_envelope, a.semantic_kitti, a.cancel_file)
        else:
            export(a.dataset, a.output, a.formats.split(','), a.upright_envelope, a.semantic_kitti, a.cancel_file)
    except (ValueError, KeyError, TypeError, OSError, OverflowError) as exc:
        p.exit(2, f'Export failed: {exc}\n')
    print('Export and reload validation succeeded.')


if __name__ == '__main__':
    cli()
