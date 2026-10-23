"""Independent PCD reader. No numpy/PCL required; respects SIZE/TYPE/COUNT without padding.

python Scripts/inspect_binary_pcd.py received.pcd --plate-z-m 0.0
The matrix consumes a column vector [local_x, local_y, local_z, 1] in metres.
"""
import argparse
import json
import math
import struct
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"Tools"/"Ros2Bridge"))
from pcd_codec import decode_pcd


def read_pcd(path):
    parsed = decode_pcd(Path(path).read_bytes(), strict_fields=False)
    header, meta = parsed.header, parsed.meta
    fields, sizes, counts, types = parsed.fields, parsed.sizes, parsed.counts, parsed.types
    codes = {('F',4):'f', ('F',8):'d', ('U',1):'B', ('U',2):'H', ('U',4):'I', ('U',8):'Q', ('I',1):'b', ('I',2):'h', ('I',4):'i', ('I',8):'q'}
    record = struct.Struct('<' + ''.join(codes[t, s] * n for t,s,n in zip(types,sizes,counts)))
    body = parsed.data
    offsets, offset = {}, 0
    for field, n in zip(fields, counts):
        offsets[field] = offset
        offset += n
    matrix = meta.get('sensor_to_world_m')
    if matrix is not None and (len(matrix) != 16 or not all(math.isfinite(v) for v in matrix)):
        raise ValueError('Invalid sensor_to_world_m matrix')
    rows = []
    for row in record.iter_unpack(body):
        xyz = [row[offsets[k]] for k in ('x','y','z')]
        if not all(math.isfinite(v) for v in xyz):
            raise ValueError('Non-finite XYZ')
        world = [sum(matrix[r*4+c] * (xyz + [1])[c] for c in range(4)) for r in range(3)] if matrix is not None else None
        rows.append((xyz, world))
    return meta, record.size, rows

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pcd')
    parser.add_argument('--plate-z-m', type=float)
    args = parser.parse_args()
    meta, stride, rows = read_pcd(args.pcd)
    local_z = [p[0][2] for p in rows]
    world_z = [p[1][2] for p in rows if p[1] is not None]
    bounds = lambda values: [min(values), max(values)] if values else None
    print(json.dumps({'metadata':meta, 'points':len(rows), 'record_bytes':stride,
        'sensor_local_z_m':bounds(local_z), 'world_z_m':bounds(world_z),
        'height_above_plate_m':bounds([z - args.plate_z_m for z in world_z]) if args.plate_z_m is not None else None,
        'warning':None if meta.get('sensor_to_world_m') else 'No acquisition transform: do not interpret local Z as world height'}, ensure_ascii=False, indent=2))
