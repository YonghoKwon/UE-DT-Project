"""Export DATASET-01 to COCO instances. python export_coco.py DATASET OUT"""
from __future__ import annotations
import shutil
import numpy as np
from pycocotools import mask as masks
from export_common import Dataset, cancelled, destination, write_json


def annotations(semantic, instance, image_id):
    """One annotation per Actor/class pair; preserve holes and disconnected visible regions."""
    result = []
    for actor in np.unique(instance):
        if not actor:
            continue
        for category in np.unique(semantic[instance == actor]):
            if not category:
                continue
            binary = np.asfortranarray((instance == actor) & (semantic == category), dtype=np.uint8)
            rle = masks.encode(binary)
            area, bbox = int(masks.area(rle)), masks.toBbox(rle).tolist()
            result.append({'image_id': image_id, 'category_id': int(category), 'bbox': bbox,
                           'area': area, 'iscrowd': 0, 'source_instance_id': int(actor),
                           'segmentation': {'size': rle['size'], 'counts': rle['counts'].decode('ascii')}})
    return result


def write_coco(data, out, cancel=None):
    images, anns = [], []
    (out/'images').mkdir(parents=True)
    (out/'annotations').mkdir()
    for i in range(data.count):
        cancelled(cancel)
        folder, label, semantic, instance = data.scene(i)
        name = f'{i:06d}.png'
        shutil.copyfile(folder/'rgb.png', out/'images'/name)
        w, h = label['camera_resolution']
        images.append({'id': i+1, 'file_name': 'images/'+name, 'width': int(w), 'height': int(h)})
        for a in annotations(semantic, instance, i+1):
            a['id'] = len(anns)+1
            anns.append(a)
    doc = {'info': {'description': 'MA0T10 visible Actor/class masks; synthetic categories', 'version': '1.0'},
           'licenses': [], 'images': images, 'annotations': anns,
           'categories': [{'id': c['id'], 'name': c['name'], 'supercategory': 'industrial'}
                          for c in data.manifest['classes'] if c['id'] != 0]}
    write_json(out/'annotations/instances.json', doc)
    return {'images': len(images), 'annotations': len(anns), 'mask_encoding': 'COCO compressed column-major RLE'}


def export_coco(dataset, output, cancel=None):
    from validate_export import validate_coco
    data = Dataset(dataset)
    with destination(data.root, output) as out:
        report = write_coco(data, out, cancel)
        report['reload'] = validate_coco(out, data.root)
        write_json(out/'export-report.json', report)
    return report


if __name__ == '__main__':
    from export_dataset import cli
    cli('coco')
