"""ROS-independent readers for the unchanged ma0t10 wire contracts. No body logging."""
import base64
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import io
import json
import math
import re
import struct

try:
    import numpy as np  # Installed by ROS base; optional for standalone Windows wire tests.
except ImportError:
    np = None

MAX_BODY = 64 * 1024 * 1024
PCD_SCHEMA = 'virtual-pointcloud.pcd.v1'
PCD_LAYOUT = {
    'FIELDS': 'x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence',
    'SIZE': '4 4 4 2 2 2 1 1 8 1 4',
    'TYPE': 'F F F U U U U U I U F',
    'COUNT': '1 1 1 1 1 1 1 1 1 1 1', 'DATA': 'binary', 'VERSION': '0.7',
}
# PointField has no int64 datatype in Jazzy. Preserve the original signed ns bits
# as low uint32/high int32 at offsets 20/24. All other fields retain wire offsets.
PCD_FIELDS = [('x', 0, 7), ('y', 4, 7), ('z', 8, 7), ('intensity', 12, 4),
              ('ring', 14, 4), ('horizontal_index', 16, 4), ('return_index', 18, 2),
              ('return_count', 19, 2), ('time_offset_ns_lo', 20, 6),
              ('time_offset_ns_hi', 24, 5), ('validity', 28, 2), ('confidence', 29, 7)]
RECORD = struct.Struct('<fffHHHBBqBf')


class WireError(ValueError):
    """The message cannot be mapped without inventing or changing information."""


def finite_vector(value, count):
    if not isinstance(value, (list, tuple)) or len(value) != count:
        raise WireError('vector_length')
    if any(isinstance(x, bool) or not isinstance(x, (int, float)) or not math.isfinite(x) for x in value):
        raise WireError('vector_number')
    return list(value)


def integer(value, low=0, high=2**63 - 1):
    if isinstance(value, bool):
        raise WireError('integer')
    try:
        n = int(value)
        if str(n) != str(value) and n != value:
            raise ValueError()
    except (TypeError, ValueError, OverflowError):
        raise WireError('integer') from None
    if not low <= n <= high:
        raise WireError('integer_range')
    return n


def stamp(value):
    # ROS Time.sec is int32. Retain up to nanoseconds, rather than float epoch conversion.
    if not isinstance(value, str) or not re.fullmatch(r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d{1,9})?Z', value):
        raise WireError('timestamp')
    whole, _, fraction = value[:-1].partition('.')
    try:
        seconds = int((datetime.strptime(whole, '%Y-%m-%dT%H:%M:%S').replace(tzinfo=timezone.utc)
                       - datetime(1970, 1, 1, tzinfo=timezone.utc)).total_seconds())
    except ValueError:
        raise WireError('timestamp') from None
    return integer(seconds, 0, 2**31 - 1), int(fraction.ljust(9, '0') or '0')


def sensor_key(sensor_id):
    # Hex UTF-8 is reversible and collision-free; even leading digits/punctuation are legal.
    if not isinstance(sensor_id, str) or not sensor_id or len(sensor_id.encode('utf8')) > 128:
        raise WireError('sensor_id')
    return 's_' + sensor_id.encode('utf8').hex()


def checked_body(body, headers):
    if not isinstance(body, bytes) or len(body) > MAX_BODY:
        raise WireError('body_size')
    if 'content-length' in headers and integer(headers['content-length']) != len(body):
        raise WireError('content_length')
    digest = hashlib.sha1(body).hexdigest() if any(name in headers for name in ('checksum','x-checksum-sha1')) else None
    for name in ('checksum', 'x-checksum-sha1'):
        if name in headers and headers[name].lower() != digest:
            raise WireError('checksum')


def identity(headers, meta=None):
    meta = meta or {}
    def pick(header, metadata):
        left, right = headers.get(header), meta.get(metadata)
        if left is not None and right is not None and str(left) != str(right):
            raise WireError('identity_mismatch')
        return left if left is not None else right
    sid = pick('sensor-id', 'sensor_id')
    sensor_key(sid)
    frame = integer(pick('frame-id', 'sensor_frame_id'))
    utc = pick('timestamp-utc', 'timestamp_utc')
    return sid, frame, stamp(utc)


def json_identity(headers, obj):
    meta = {'sensor_id':obj.get('sensorId'), 'sensor_frame_id':obj.get('frameId'), 'timestamp_utc':obj.get('timestampUtc')}
    frame = meta['sensor_frame_id']
    # Existing UE JSON encoders use double. ARCH-06 golden intentionally has
    # JSON 9007199254740992 alongside the exact STOMP string 9007199254740993.
    # Accept only that representable-double agreement; use the exact wire header.
    if 'frame-id' in headers and isinstance(frame,(int,float)) and not isinstance(frame,bool) and abs(frame) >= 2**53:
        exact = integer(headers['frame-id'])
        if float(exact) == float(frame):
            meta['sensor_frame_id'] = headers['frame-id']
    return identity(headers, meta)


def quaternion(matrix):
    """Unit quaternion xyzw from a proper orthonormal 3x3 row-major rotation."""
    m = matrix
    for r in range(3):
        for c in range(3):
            dot = sum(m[r][k] * m[c][k] for k in range(3))
            if abs(dot - (1 if r == c else 0)) > 1e-4:
                raise WireError('non_rigid_pose')
    det = sum(m[0][i] * (m[1][(i+1)%3]*m[2][(i+2)%3] - m[1][(i+2)%3]*m[2][(i+1)%3]) for i in range(3))
    if abs(det - 1) > 1e-4:
        raise WireError('pose_handedness')
    # Largest diagonal formulation also handles rotations near 180 degrees.
    terms = [1+m[0][0]-m[1][1]-m[2][2], 1-m[0][0]+m[1][1]-m[2][2],
             1-m[0][0]-m[1][1]+m[2][2], 1+m[0][0]+m[1][1]+m[2][2]]
    i = max(range(4), key=terms.__getitem__)
    q = [0.0]*4
    q[i] = math.sqrt(max(0, terms[i])) / 2
    d = 4*q[i]
    if i == 3:
        q[:3] = [(m[2][1]-m[1][2])/d, (m[0][2]-m[2][0])/d, (m[1][0]-m[0][1])/d]
    else:
        j, k = (i+1)%3, (i+2)%3
        q[j], q[k] = (m[j][i]+m[i][j])/d, (m[k][i]+m[i][k])/d
        q[3] = (m[k][j]-m[j][k])/d
    return [-x for x in q] if q[3] < 0 else q


def pcd_pose(meta):
    if 'sensor_to_world_m' not in meta:
        return None
    expected = {'coordinate_frame': 'sensor_local', 'coordinate_units': 'm',
                'coordinate_axes': 'x_forward_y_left_z_up',
                'world_coordinate_axes': 'ue_x_forward_y_right_z_up',
                'transform_convention': 'row_major_4x4_column_vector_local_m_to_ue_world_m'}
    if any(meta.get(k) != v for k, v in expected.items()):
        raise WireError('pose_convention')
    m = finite_vector(meta['sensor_to_world_m'], 16)
    if m[12:] != [0, 0, 0, 1]:
        raise WireError('pose_affine')
    # Local PCD is already right handed; reflect only the output UE world rows.
    r = [[m[i*4+j] * (1, -1, 1)[i] for j in range(3)] for i in range(3)]
    return ([m[3], -m[7], m[11]], quaternion(r))


@dataclass
class Cloud:
    sensor_id: str
    frame: int
    stamp: tuple
    data: bytes
    count: int
    fields: list
    point_step: int
    pose: object = None
    world: bool = False


def pcd(body, headers):
    checked_body(body, headers)
    if headers.get('schema') != PCD_SCHEMA:
        raise WireError('unknown_schema')
    stream = io.BytesIO(body)
    values, meta = {}, None
    while stream.tell() < 65536:
        line = stream.readline(65537)
        if not line or len(line) > 65536:
            raise WireError('pcd_header')
        if line.startswith(b'# MA0T10_META '):
            if meta is not None:
                raise WireError('duplicate_metadata')
            meta = json.loads(line[14:])
        elif not line.startswith(b'#'):
            parts = line.decode('ascii').split()
            if parts:
                if parts[0] in values:
                    raise WireError('duplicate_header')
                values[parts[0]] = ' '.join(parts[1:])
                if parts[0] == 'DATA':
                    break
    else:
        raise WireError('pcd_header_size')
    if any(values.get(k) != v for k, v in PCD_LAYOUT.items()):
        raise WireError('pcd_layout')
    meta = meta or {}
    if meta and meta.get('schema') != 'virtual-pointcloud.context.v1':
        raise WireError('unknown_context_schema')
    sid, frame, utc = identity(headers, meta)
    count = integer(values.get('POINTS'), high=MAX_BODY//33)
    if integer(values.get('WIDTH')) * integer(values.get('HEIGHT'), low=1) != count:
        raise WireError('pcd_dimensions')
    data = stream.read()
    if len(data) != count * 33:
        raise WireError('pcd_bytes')
    if 'point-count' in headers and integer(headers['point-count']) != count:
        raise WireError('pcd_point_count')
    if np is not None:
        # Strided views retain the 33-byte unaligned record without per-point Python allocation.
        for offset in (0, 4, 8, 29):
            values = np.ndarray((count,), dtype='<f4', buffer=data, offset=offset if count else 0, strides=(33,))
            if not np.isfinite(values).all():
                raise WireError('pcd_nonfinite')
    else:
        for row in RECORD.iter_unpack(data):
            if not all(math.isfinite(x) for x in (*row[:3], row[-1])):
                raise WireError('pcd_nonfinite')
    return Cloud(sid, frame, utc, data, count, PCD_FIELDS, 33, pcd_pose(meta))


def json_payload(body):
    if len(body) > MAX_BODY:
        raise WireError('body_size')
    obj = json.loads(body)
    if not isinstance(obj, dict):
        raise WireError('json_object')
    return obj


def lidar_json(body, headers):
    checked_body(body, headers)
    obj = json_payload(body)
    if obj.get('schemaVersion') != 'virtual-lidar.v1' or obj.get('sensorType') != 'virtual_lidar':
        raise WireError('unknown_schema')
    sid, frame, utc = json_identity(headers, obj)
    points = obj.get('points')
    if not isinstance(points, list) or integer(obj.get('payloadPointCount')) != len(points):
        raise WireError('lidar_count')
    out = bytearray()
    for point in points:
        if not isinstance(point.get('hit'), bool):
            raise WireError('lidar_hit')
        if point['hit']:
            x, y, z = finite_vector(point['worldLocation'], 3)
            out.extend(struct.pack('<fff', x*.01, -y*.01, z*.01))
    # Legacy points are UE world cm; do not label them as sensor-local.
    return Cloud(sid, frame, utc, bytes(out), len(out)//12, PCD_FIELDS[:3], 12, world=True)


def camera(body, headers):
    checked_body(body, headers)
    schema = headers.get('schema')
    pose, fov = None, None
    if schema == 'virtual-camera.jpeg.v1':
        sid, frame, utc = identity(headers)
        width, height = integer(headers.get('width'), 1, 8192), integer(headers.get('height'), 1, 8192)
        image = body
    else:
        obj = json_payload(body)
        if obj.get('schemaVersion') != 'virtual-camera.v1' or obj.get('sensorType') != 'virtual_camera' or obj.get('encoding') != 'jpeg/base64':
            raise WireError('unknown_schema')
        sid, frame, utc = json_identity(headers, obj)
        width, height = integer(obj.get('width'), 1, 8192), integer(obj.get('height'), 1, 8192)
        image = base64.b64decode(obj['image'], validate=True)
        if len(image) != integer(obj.get('byteSize')):
            raise WireError('camera_bytes')
        fov = finite_vector([obj.get('horizontalFov'), obj.get('verticalFov')], 2)
        if any(not 0 < x < 180 for x in fov):
            raise WireError('camera_fov')
        pose = camera_pose(obj['sensorTransform'])
    if not image.startswith(b'\xff\xd8') or width * height > 16*1024*1024:
        raise WireError('camera_jpeg')
    return {'sensor_id': sid, 'frame': frame, 'stamp': utc, 'width': width, 'height': height,
            'jpeg': image, 'pose': pose, 'fov': fov}


def camera_pose(transform):
    # Use snapshot forward/up rather than guessing UE Euler order. ROS body basis.
    def unit(v):
        n = math.sqrt(sum(x*x for x in v))
        if n < 1e-6:
            raise WireError('camera_basis')
        return [x/n for x in v]
    def reflect(v):
        x, y, z = finite_vector(v, 3)
        return [x, -y, z]
    x = unit(reflect(transform['forward']))
    z0 = reflect(transform['up'])
    z = unit([z0[i]-sum(x[j]*z0[j] for j in range(3))*x[i] for i in range(3)])
    y = [z[1]*x[2]-z[2]*x[1], z[2]*x[0]-z[0]*x[2], z[0]*x[1]-z[1]*x[0]]
    t = [v*.01 for v in reflect(transform['location'])]
    return t, quaternion([[x[i], y[i], z[i]] for i in range(3)])


def decode_rgb(camera_frame):
    # Optional approved dependency, kept out of Windows wire-only tests.
    from PIL import Image
    with Image.open(io.BytesIO(camera_frame['jpeg'])) as image:
        if image.format != 'JPEG' or image.size != (camera_frame['width'], camera_frame['height']):
            raise WireError('camera_dimensions')
        return image.convert('RGB').tobytes()


def jpeg_dimensions(data):
    """Bounded JPEG marker validation without a decoder; not a codec correctness proof."""
    if not data.startswith(b'\xff\xd8') or not data.endswith(b'\xff\xd9'):
        raise WireError('camera_jpeg_markers')
    i, dimensions, scan = 2, None, False
    while i < len(data):
        if data[i] != 255:
            raise WireError('camera_jpeg_segment')
        while i < len(data) and data[i] == 255:
            i += 1
        if i >= len(data):
            raise WireError('camera_jpeg_segment')
        marker = data[i]
        i += 1
        if marker == 0xd9:
            if not scan or dimensions is None or i != len(data):
                raise WireError('camera_jpeg_incomplete')
            return dimensions
        if marker in (0x00,0xd8) or 0xd0 <= marker <= 0xd7:
            raise WireError('camera_jpeg_segment')
        if i+2 > len(data):
            raise WireError('camera_jpeg_segment')
        size = int.from_bytes(data[i:i+2], 'big')
        if size < 2 or i+size > len(data):
            raise WireError('camera_jpeg_segment')
        if marker in (0xc0,0xc1,0xc2):
            if size < 8 or dimensions is not None or data[i+2] != 8:
                raise WireError('camera_jpeg_sof')
            h, w = struct.unpack_from('>HH',data,i+3)
            if not 0 < w <= 8192 or not 0 < h <= 8192 or w*h > 16*1024*1024:
                raise WireError('camera_dimensions')
            dimensions = w,h
        i += size
        if marker == 0xda:
            scan = True
            # Find the next unstuffed, non-restart marker (supports progressive scans).
            while True:
                i = data.find(b'\xff',i)
                if i < 0 or i+1 >= len(data):
                    raise WireError('camera_jpeg_entropy')
                if data[i+1] == 0 or 0xd0 <= data[i+1] <= 0xd7:
                    i += 2
                    continue
                break
    raise WireError('camera_jpeg_incomplete')
