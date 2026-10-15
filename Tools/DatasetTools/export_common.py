"""Strict DATASET-01 readers and frame mathematics shared by offline exporters."""
from __future__ import annotations

import contextlib
import hashlib
import json
import math
from pathlib import Path
import shutil
import tempfile

import numpy as np
from PIL import Image

from score_dataset import read_pcd, FIELDS

PALETTE = {0: 'Background', 1: 'Other', 2: 'Slab', 3: 'Roller',
           4: 'Conveyor', 5: 'Floor', 6: 'Sensor'}
PCD_DTYPE = np.dtype([(name, dtype) for name, dtype in zip(FIELDS.split(),
    ['<f4', '<f4', '<f4', '<u2', '<u2', '<u2', 'u1', 'u1', '<i8', 'u1', '<f4'])])
LABEL_DTYPE = np.dtype([('semantic', 'u1'), ('instance', '<u4')])
OPTICAL = np.array([[0., 1, 0], [0, 0, -1], [1, 0, 0]])
PCD_TO_UE = np.diag([1., -1, 1])


def load_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False)+'\n', encoding='utf-8')


def vector(value, size, name):
    a = np.asarray(value, dtype=np.float64)
    if a.shape != (size,) or not np.isfinite(a).all():
        raise ValueError(f'invalid {name}: expected {size} finite numbers')
    return a


def rotation(q):
    x, y, z, w = vector(q, 4, 'quaternion_xyzw')
    if abs(x*x+y*y+z*z+w*w-1) > 1e-5:
        raise ValueError('quaternion must have unit norm')
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])


def pose(p):
    return rotation(p['quaternion_xyzw']), vector(p['xyz_m'], 3, 'xyz_m')


def calibration(label):
    """PCD RH FLU -> UE world -> camera UE local -> optical RDF, column vectors."""
    rl, tl = pose(label['lidar_pose'])
    rc, tc = pose(label['camera_pose'])
    r = OPTICAL @ rc.T @ rl @ PCD_TO_UE
    t = OPTICAL @ rc.T @ (tl-tc)
    k = vector(label['camera_K'], 9, 'camera_K').reshape(3, 3)
    if k[0, 0] <= 0 or k[1, 1] <= 0 or not np.allclose(k[2], [0, 0, 1]) or k[0, 1] != 0 or k[1, 0] != 0:
        raise ValueError('unsupported camera intrinsics')
    return k, np.column_stack((r, t))


def project(points, k):
    a = np.asarray(points) @ k.T
    visible = a[:, 2] > 1e-6
    return a[visible, :2]/a[visible, 2:3], visible


def bounds(pixels):
    if not len(pixels):
        return None
    lo, hi = np.min(pixels, axis=0), np.max(pixels, axis=0)
    return [float(lo[0]), float(lo[1]), float(hi[0]), float(hi[1])]


def iou(a, b):
    if a is None or b is None:
        return None
    iw, ih = max(0, min(a[2], b[2])-max(a[0], b[0])), max(0, min(a[3], b[3])-max(a[1], b[1]))
    intersection = iw*ih
    union = max(0, a[2]-a[0])*max(0, a[3]-a[1])+max(0, b[2]-b[0])*max(0, b[3]-b[1])-intersection
    return float(intersection/union) if union else 0.


def check_schema(doc, expected):
    if doc.get('schema_version') != expected or type(doc.get('schema_minor', 0)) is not int or not 0 <= doc.get('schema_minor', 0) <= 1:
        raise ValueError(f'unsupported schema; require {expected} minor 0 or 1')


class Dataset:
    def __init__(self, root):
        self.root = Path(root).resolve(strict=True)
        self.manifest = load_json(self.root/'dataset.json')
        check_schema(self.manifest, 'ma0t10.dataset.v1')
        if self.manifest.get('report_sample_only'):
            raise ValueError('report sample is not a complete dataset')
        self.count = self.manifest['completed_scenes']
        if type(self.count) is not int or not 0 <= self.count <= 10000:
            raise ValueError('invalid completed_scenes')
        classes = self.manifest['classes']
        if len(classes) != 7 or {c['id']: c['name'] for c in classes} != PALETTE:
            raise ValueError('class palette differs from DATASET-01 canonical semantic palette')
        items = self.manifest['instances']
        self.instances = {a['id']: a for a in items}
        if len(self.instances) != len(items) or any(type(i) is not int or not 0 < i <= 0xffffff for i in self.instances):
            raise ValueError('invalid/duplicate instance IDs')

    def scene(self, index, truth=True):
        folder = self.root/f'scene_{index:05d}'
        # Never follow a dataset scene symlink outside the admitted input root.
        if folder.resolve().parent != self.root:
            raise ValueError('scene escapes dataset root')
        label = load_json(folder/'label.json')
        check_schema(label, 'ma0t10.dataset.scene.v1')
        if type(label['scene_index']) is not int or label['scene_index'] != index:
            raise ValueError('scene index mismatch')
        resolution = vector(label['camera_resolution'], 2, 'camera_resolution')
        if np.any(resolution != np.floor(resolution)) or np.any(resolution <= 0) or np.prod(resolution) > 1920*1080:
            raise ValueError('invalid camera resolution')
        with Image.open(folder/'rgb.png') as img:
            if img.format != 'PNG' or img.size != tuple(resolution):
                raise ValueError('RGB dimensions/format mismatch')
            img.verify()
        if not truth:
            return folder, label
        if self.manifest.get('camera_truth') is not True:
            raise ValueError('camera_truth is required; generate with Camera GT enabled')
        with Image.open(folder/'seg.png') as img:
            if img.mode != 'L' or img.size != tuple(resolution):
                raise ValueError('semantic mask must be uint8 L with camera dimensions')
            semantic = np.array(img)
        with Image.open(folder/'instance.png') as img:
            if img.mode not in ('RGB', 'RGBA') or img.size != tuple(resolution):
                raise ValueError('instance mask must be RGB/RGBA with camera dimensions')
            rgb = np.asarray(img).astype(np.uint32)
            instance = rgb[:, :, 0]+256*rgb[:, :, 1]+65536*rgb[:, :, 2]
        if np.max(semantic, initial=0) > 6 or any(int(i) not in self.instances for i in np.unique(instance) if i):
            raise ValueError('unknown semantic/instance ID in masks')
        return folder, label, semantic, instance


def cloud(folder):
    count = read_pcd(folder/'lidar.pcd')  # strict packed PCD schema/count/finite validation
    with (folder/'lidar.pcd').open('rb') as f:
        while not f.readline().startswith(b'DATA '):
            pass
        points = np.frombuffer(f.read(), dtype=PCD_DTYPE)
    labels = np.frombuffer((folder/'lidar_labels.bin').read_bytes(), dtype=LABEL_DTYPE)
    if len(labels) != count or np.max(labels['semantic'], initial=0) > 6:
        raise ValueError('PCD/point label count or semantic mismatch')
    xyz = np.column_stack([points[n] for n in ('x', 'y', 'z')])
    # PCD intensity is the existing uint16 modeled response, not a calibrated reflectance.
    return np.column_stack((xyz, points['intensity'].astype(np.float32)/65535.)).astype('<f4'), labels


def cancelled(marker):
    if marker and Path(marker).exists():
        raise ValueError('export cancelled')


@contextlib.contextmanager
def destination(source, output):
    """Publish a new output atomically. Failed attempts remove only their own staging directory."""
    source, output = Path(source).resolve(), Path(output).resolve()
    if source == output or source in output.parents or output in source.parents:
        raise ValueError('input/output must be disjoint; source is immutable')
    if output.exists():
        raise ValueError('output already exists; choose a new directory')
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=output.name+'.pending-', dir=output.parent)).resolve()
    try:
        yield staging
        if output.exists():
            raise ValueError('output appeared during export')
        staging.rename(output)
    finally:
        if staging.exists() and staging.parent == output.parent and staging.name.startswith(output.name+'.pending-'):
            shutil.rmtree(staging)


def source_hash(root):
    h = hashlib.sha256()
    for p in sorted(Path(root).rglob('*')):
        if p.is_symlink():
            raise ValueError('dataset symlinks are unsupported')
        if p.is_file():
            h.update(p.relative_to(root).as_posix().encode())
            with p.open('rb') as f:
                for block in iter(lambda: f.read(1024*1024), b''):
                    h.update(block)
    return h.hexdigest()
