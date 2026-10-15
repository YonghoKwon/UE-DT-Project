"""Export synthetic KITTI object data. Tilted boxes require explicit --upright-envelope."""
from __future__ import annotations
import itertools
import math
import shutil
import numpy as np
from export_common import (Dataset, OPTICAL, bounds, calibration, cancelled, cloud, destination,
                           pose, project, vector, write_json)


def box_camera(box, label, upright_envelope=False):
    if box.get('coordinates') != 'UE world meters; local XYZ dimensions; quaternion_xyzw':
        raise ValueError('unsupported 3D box coordinates')
    center = vector(box['center_m'], 3, 'box center')
    size = vector(box['size_m'], 3, 'box size')
    if np.any(size <= 0):
        raise ValueError('3D box dimensions must be positive')
    from export_common import rotation
    rb = rotation(box['quaternion_xyzw'])
    rc, tc = pose(label['camera_pose'])
    basis = OPTICAL @ rc.T @ rb
    c = OPTICAL @ rc.T @ (center-tc)
    corners = np.array(list(itertools.product(*[[-s/2, s/2] for s in size]))) @ basis.T+c
    tilt = math.degrees(math.acos(min(1., abs(float(basis[1, 2])))))
    if tilt > .01 and not upright_envelope:
        raise ValueError(f'KITTI yaw-only cannot represent {tilt:.4f} degree box tilt; use --upright-envelope explicitly')
    # KITTI length is object local X, height is camera Y, width object local Z.
    ry = math.atan2(-float(basis[2, 0]), float(basis[0, 0]))
    r = np.array([[math.cos(ry), 0, math.sin(ry)], [0, 1, 0], [-math.sin(ry), 0, math.cos(ry)]])
    local = corners @ r
    lo, hi = np.min(local, axis=0), np.max(local, axis=0)
    dims = hi-lo  # length, height, width of camera-upright enclosing box
    bottom = np.array([(lo[0]+hi[0])/2, hi[1], (lo[2]+hi[2])/2]) @ r.T
    alpha = (ry-math.atan2(bottom[0], bottom[2])+math.pi) % (2*math.pi)-math.pi
    return {'hwl': [float(dims[1]), float(dims[2]), float(dims[0])], 'location': bottom.tolist(),
            'rotation_y': ry, 'alpha': alpha, 'tilt_degrees': tilt,
            'volume_ratio': float(np.prod(dims)/np.prod(size)), 'truth_camera_corners': corners.tolist()}


def visible_bbox(semantic, instance, actor, category):
    y, x = np.nonzero((instance == actor) & (semantic == category))
    if not len(x):
        return None
    # KITTI bbox uses inclusive, 0-based endpoint coordinates.
    return [float(x.min()), float(y.min()), float(x.max()), float(y.max())]


def write_kitti(data, out, upright_envelope=False, semantic_kitti=False, cancel=None):
    for name in ('training/velodyne', 'training/image_2', 'training/calib', 'training/label_2', 'ImageSets'):
        (out/name).mkdir(parents=True)
    if semantic_kitti:
        (out/'sequences/00/velodyne').mkdir(parents=True)
        (out/'sequences/00/labels').mkdir()
    details, points_total = [], 0
    for i in range(data.count):
        cancelled(cancel)
        folder, label, semantic, instance = data.scene(i)
        if label.get('schema_minor', 0) < 1 or 'boxes_3d' not in label:
            raise ValueError('KITTI requires DATASET scene minor 1 boxes_3d; legacy v1 supports COCO only, regenerate for KITTI')
        name = f'{i:06d}'
        points, point_labels = cloud(folder)
        if any(int(v) not in data.instances for v in np.unique(point_labels['instance']) if v):
            raise ValueError('unknown instance ID in point labels')
        points.tofile(out/'training/velodyne'/f'{name}.bin')
        points_total += len(points)
        shutil.copyfile(folder/'rgb.png', out/'training/image_2'/f'{name}.png')
        k, tr = calibration(label)
        p = np.column_stack((k, np.zeros(3)))
        # Only P2 is a real camera. Zero placeholders carry no fabricated stereo/IMU baseline.
        matrices = {'P0': np.zeros((3, 4)), 'P1': np.zeros((3, 4)), 'P2': p, 'P3': np.zeros((3, 4)),
                    'R0_rect': np.eye(3), 'Tr_velo_to_cam': tr}
        (out/'training/calib'/f'{name}.txt').write_text(''.join(
            key+': '+' '.join(f'{v:.12g}' for v in m.flat)+'\n' for key, m in matrices.items()), encoding='ascii')
        lines, objects, seen = [], [], set()
        for box in label['boxes_3d']:
            actor, category = box['instance_id'], box['semantic_id']
            if actor not in data.instances or category != 2 or (actor, category) in seen:
                raise ValueError('boxes_3d must contain unique selected Slab instance/category IDs')
            seen.add((actor, category))
            bbox = visible_bbox(semantic, instance, actor, category)
            if bbox is None:
                continue  # KITTI object labels cover only objects visible in this image.
            b = box_camera(box, label, upright_envelope)
            corners = np.array(b['truth_camera_corners'])
            if np.any(corners[:, 2] <= 1e-6):
                raise ValueError('3D box crosses camera near plane; unsupported truncation geometry')
            projected, _ = project(corners, k)
            full = bounds(projected)
            w, h = label['camera_resolution']
            clipped = [max(0., full[0]), max(0., full[1]), min(w-1., full[2]), min(h-1., full[3])]
            full_area = (full[2]-full[0])*(full[3]-full[1])
            truncation = 1-max(0., clipped[2]-clipped[0])*max(0., clipped[3]-clipped[1])/full_area if full_area else 0.
            # Occlusion=3 means unknown: visible masks do not reveal occluded surface fraction.
            values = [max(0., min(1., truncation)), 3, b['alpha'], *bbox, *b['hwl'], *b['location'], b['rotation_y']]
            lines.append('Slab '+' '.join(str(int(v)) if j == 1 else f'{v:.12g}' for j, v in enumerate(values)))
            objects.append({'instance_id': actor, 'semantic_id': category, 'bbox': bbox, 'source_box': box, **b})
        (out/'training/label_2'/f'{name}.txt').write_text('\n'.join(lines)+ ('\n' if lines else ''), encoding='ascii')
        if semantic_kitti:
            if np.max(point_labels['instance'], initial=0) > 65535:
                raise ValueError('SemanticKITTI instance exceeds 16-bit limit')
            shutil.copyfile(out/'training/velodyne'/f'{name}.bin', out/'sequences/00/velodyne'/f'{name}.bin')
            packed = point_labels['semantic'].astype('<u4') | (point_labels['instance'].astype('<u4') << 16)
            packed.tofile(out/'sequences/00/labels'/f'{name}.label')
        details.append({'frame': name, 'source_scene': i, 'points': len(points), 'objects': objects})
    (out/'ImageSets/train.txt').write_text(''.join(f'{i:06d}\n' for i in range(data.count)), encoding='ascii')
    write_json(out/'class-map.json', {'classes': data.manifest['classes'], 'label_2': {'2': 'Slab'},
        'unboxed_categories': [0, 1, 3, 4, 5, 6], 'semantic_kitti_ids': 'custom canonical IDs 0..6, not road-class learning IDs',
        'intensity': 'uint16 modeled response / 65535; no radiometric calibration',
        'coordinates': 'velodyne RH X forward Y left Z up meters; camera x right y down z forward',
        'box_policy': 'upright enclosing box' if upright_envelope else 'reject tilt > 0.01 degrees',
        'camera': 'single ideal pinhole, zero distortion; R0_rect identity; P0/P1/P3 unavailable zero placeholders; no IMU',
        'detection_scope': 'only selected Slab has 3D truth; other visible classes are unannotated; filter scenes for detection training'})
    write_json(out/'source-boxes.json', details)
    return {'scenes': data.count, 'points': points_total, 'objects': sum(len(d['objects']) for d in details),
            'upright_envelope': upright_envelope, 'semantic_kitti': semantic_kitti}


def export_kitti(dataset, output, upright_envelope=False, semantic_kitti=False, cancel=None):
    from validate_export import validate_kitti
    data = Dataset(dataset)
    with destination(data.root, output) as out:
        report = write_kitti(data, out, upright_envelope, semantic_kitti, cancel)
        report['reload'] = validate_kitti(out, data.root)
        write_json(out/'export-report.json', report)
    return report


if __name__ == '__main__':
    from export_dataset import cli
    cli('kitti')
