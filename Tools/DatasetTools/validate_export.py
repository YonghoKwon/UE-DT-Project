"""Reload COCO with its official API; parse KITTI and quantify projection against source masks."""
from __future__ import annotations
import argparse
from pathlib import Path
import math
import numpy as np
from PIL import Image
from pycocotools.coco import COCO
from export_common import Dataset, OPTICAL, PCD_TO_UE, bounds, cloud, iou, load_json, pose, project, rotation, write_json


def validate_coco(root, source=None):
    root = Path(root)
    api = COCO(str(root/'annotations/instances.json'))
    doc = load_json(root/'annotations/instances.json')
    for key in ('images', 'annotations', 'categories'):
        if len({v['id'] for v in doc[key]}) != len(doc[key]):
            raise ValueError('duplicate COCO IDs')
    data = Dataset(source) if source else None
    checked = 0
    for image in api.imgs.values():
        relative = Path(image['file_name'])
        if relative.is_absolute() or '..' in relative.parts or (root/relative).resolve().parent != (root/'images').resolve():
            raise ValueError('COCO image path escapes images')
        with Image.open(root/relative) as img:
            if img.size != (image['width'], image['height']):
                raise ValueError('COCO image size mismatch')
            img.verify()
        source_scene = data.scene(image['id']-1) if data else None
        expected_pairs = set()
        if source_scene:
            semantic, instance = source_scene[2:]
            expected_pairs = {(int(a), int(c)) for a in np.unique(instance) if a
                              for c in np.unique(semantic[instance == a]) if c}
        actual_pairs = set()
        for ann in api.loadAnns(api.getAnnIds(imgIds=[image['id']])):
            if ann['category_id'] not in api.cats or ann['iscrowd'] != 0:
                raise ValueError('COCO annotation category/iscrowd mismatch')
            mask = api.annToMask(ann)
            y, x = np.nonzero(mask)
            if not len(x) or int(mask.sum()) != ann['area'] or ann['bbox'] != [int(x.min()), int(y.min()), int(x.max()-x.min()+1), int(y.max()-y.min()+1)]:
                raise ValueError('COCO RLE area/bbox mismatch')
            if source_scene:
                pair = (ann['source_instance_id'], ann['category_id'])
                if pair in actual_pairs or not np.array_equal(mask, (instance == pair[0]) & (semantic == pair[1])):
                    raise ValueError('COCO decoded RLE differs from original mask')
                actual_pairs.add(pair)
            checked += 1
        if source_scene and actual_pairs != expected_pairs:
            raise ValueError('missing COCO source mask')
    if data and len(api.imgs) != data.count:
        raise ValueError('COCO missing source images')
    return {'reader': 'official pycocotools.COCO/annToMask', 'images': len(api.imgs),
            'masks_checked': checked, 'source_pixel_exact': bool(data)}


def read_calib(path):
    result = {}
    for line in path.read_text(encoding='ascii').splitlines():
        key, text = line.split(':', 1)
        if key in result:
            raise ValueError('duplicate calibration key')
        values = np.array([float(v) for v in text.split()])
        shape = (3, 3) if key == 'R0_rect' else (3, 4)
        if len(values) != math.prod(shape) or not np.isfinite(values).all():
            raise ValueError('invalid calibration')
        result[key] = values.reshape(shape)
    if not {'P2', 'R0_rect', 'Tr_velo_to_cam'} <= result.keys():
        raise ValueError('missing KITTI camera calibration')
    r = result['Tr_velo_to_cam'][:, :3]
    if not np.allclose(r@r.T, np.eye(3), atol=1e-6) or not np.isclose(np.linalg.det(r), 1, atol=1e-6):
        raise ValueError('KITTI extrinsic is not a proper RH rigid transform')
    return result


def read_objects(path):
    result = []
    for line in path.read_text(encoding='ascii').splitlines():
        f = line.split()
        if len(f) != 15 or f[0] != 'Slab':
            raise ValueError('invalid KITTI label_2 class/15 columns')
        v = np.array([float(x) for x in f[1:]])
        if not np.isfinite(v).all() or not 0 <= v[0] <= 1 or v[1] not in (0, 1, 2, 3) or np.any(v[7:10] <= 0) or abs(v[2]) > math.pi or abs(v[13]) > math.pi:
            raise ValueError('KITTI label range invalid')
        if v[5] < v[3] or v[6] < v[4]:
            raise ValueError('KITTI 2D bbox invalid')
        result.append(v)
    return result


def validate_kitti(root, source):
    root, data = Path(root), Dataset(source)
    details = load_json(root/'source-boxes.json')
    ids = (root/'ImageSets/train.txt').read_text(encoding='ascii').splitlines()
    if ids != [f'{i:06d}' for i in range(data.count)] or len(details) != data.count:
        raise ValueError('KITTI split/frame count mismatch')
    metrics, count = [], 0
    for index, name in enumerate(ids):
        raw = (root/'training/velodyne'/f'{name}.bin').read_bytes()
        if len(raw) % 16:
            raise ValueError('KITTI velodyne stride must be 16 bytes')
        points = np.frombuffer(raw, dtype='<f4').reshape(-1, 4)
        if not np.isfinite(points).all() or np.any(points[:, 3] < 0) or np.any(points[:, 3] > 1):
            raise ValueError('KITTI nonfinite/invalid intensity')
        folder, label, semantic, instance = data.scene(index)
        original, labels = cloud(folder)
        if points.shape != original.shape or not np.array_equal(points, original):
            raise ValueError('KITTI point count/axis/intensity changed unexpectedly')
        count += len(points)
        with Image.open(root/'training/image_2'/f'{name}.png') as img:
            if img.size != tuple(label['camera_resolution']):
                raise ValueError('KITTI image dimensions mismatch')
            if (folder/'rgb.png').read_bytes() != (root/'training/image_2'/f'{name}.png').read_bytes():
                raise ValueError('KITTI RGB copy mismatch')
        c = read_calib(root/'training/calib'/f'{name}.txt')
        tr = c['Tr_velo_to_cam']
        rl, tl = pose(label['lidar_pose'])
        rc, tc = pose(label['camera_pose'])
        if not np.allclose(c['P2'], np.column_stack((np.array(label['camera_K']).reshape(3,3), np.zeros(3))), atol=1e-8) or not np.allclose(c['R0_rect'], np.eye(3), atol=1e-8):
            raise ValueError('KITTI intrinsics/rectification differ from source')
        # Independent path through source world coordinates, versus reloaded KITTI matrices.
        world = (points[:, :3] @ PCD_TO_UE.T) @ rl.T+tl
        expected_camera = (world-tc) @ rc @ OPTICAL.T
        camera = (points[:, :3] @ tr[:, :3].T+tr[:, 3]) @ c['R0_rect'].T
        if not np.allclose(camera, expected_camera, rtol=1e-7, atol=1e-6):
            raise ValueError('KITTI extrinsic projection differs from source snapshot')
        uv, front = project(camera, c['P2'][:, :3])
        objects = read_objects(root/'training/label_2'/f'{name}.txt')
        if len(objects) != len(details[index]['objects']):
            raise ValueError('KITTI source/object count mismatch')
        for v, truth in zip(objects, details[index]['objects']):
            bbox = v[3:7]
            actor = truth['instance_id']
            subset = labels['instance'][front] == actor
            pixels = uv[subset]
            w, h = label['camera_resolution']
            pixels = pixels[(pixels[:, 0] >= 0) & (pixels[:, 0] < w) & (pixels[:, 1] >= 0) & (pixels[:, 1] < h)]
            inside = (pixels[:, 0] >= bbox[0]-1) & (pixels[:, 0] <= bbox[2]+1) & (pixels[:, 1] >= bbox[1]-1) & (pixels[:, 1] <= bbox[3]+1)
            source_box = next((b for b in label['boxes_3d'] if b['instance_id'] == actor), None)
            if source_box is None or truth['source_box'] != source_box:
                raise ValueError('KITTI source OBB metadata mismatch')
            import itertools
            local_corners = np.array(list(itertools.product(*[[-s/2,s/2] for s in source_box['size_m']])))
            world_corners = local_corners @ rotation(source_box['quaternion_xyzw']).T+np.array(source_box['center_m'])
            corners = (world_corners-tc) @ rc @ OPTICAL.T
            y, x = np.nonzero((instance == actor) & (semantic == source_box['semantic_id']))
            if not len(x) or not np.array_equal(bbox, [x.min(),y.min(),x.max(),y.max()]):
                raise ValueError('KITTI visible bbox differs from source mask')
            angle = v[13]
            r = np.array([[math.cos(angle), 0, math.sin(angle)], [0, 1, 0], [-math.sin(angle), 0, math.cos(angle)]])
            local = (corners-v[10:13]) @ r
            height, width, length = v[7:10]
            excess = float(max(0., np.max(np.abs(local[:, 0])-length/2), np.max(np.abs(local[:, 2])-width/2), np.max(local[:, 1]), np.max(-height-local[:, 1])))
            if excess > 1e-6:
                raise ValueError('KITTI 3D box does not enclose original truth')
            m = {'frame': name, 'instance_id': actor, 'projected_slab_points': len(pixels),
                 'point_bbox_iou': iou(bounds(pixels), bbox),
                 'points_inside_bbox_fraction_1px': float(inside.mean()) if len(pixels) else None,
                 'truth_box_projection_iou': iou(bounds(project(corners, c['P2'][:, :3])[0]), bbox),
                 'box_containment_error_m': excess, 'tilt_degrees': truth['tilt_degrees'], 'volume_ratio': truth['volume_ratio']}
            metrics.append(m)
        optional = root/'sequences/00/labels'/f'{name}.label'
        if optional.exists():
            packed = np.frombuffer(optional.read_bytes(), dtype='<u4')
            if len(packed) != len(labels) or not np.array_equal(packed & 65535, labels['semantic']) or not np.array_equal(packed >> 16, labels['instance']):
                raise ValueError('SemanticKITTI label reload mismatch')
    def aggregate(key):
        values = [m[key] for m in metrics if m[key] is not None]
        return {'n': len(values), 'mean': float(np.mean(values)) if values else None, 'min': min(values) if values else None}
    return {'reader': 'strict KITTI devkit-format parser + NumPy/Pillow; no official Python KITTI object API',
            'scenes': data.count, 'points': count, 'objects': len(metrics),
            'objects_without_projected_points': sum(m['projected_slab_points'] == 0 for m in metrics),
            'point_bbox_iou': aggregate('point_bbox_iou'),
            'points_inside_bbox_fraction_1px': aggregate('points_inside_bbox_fraction_1px'),
            'truth_box_projection_iou': aggregate('truth_box_projection_iou'), 'per_object': metrics}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('format', choices=['coco', 'kitti'])
    p.add_argument('output', type=Path)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    r = validate_coco(a.output, a.source) if a.format == 'coco' else validate_kitti(a.output, a.source)
    write_json(a.report, r)
    print('Export reload validation succeeded.')
