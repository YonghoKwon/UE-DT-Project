"""Bounded, ROS-independent PCD binary/LZF reader. No optional packages required."""
from dataclasses import dataclass
import json
import struct

FIELD_NAMES = "x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence".split()
FIELD_SIZES = [4, 4, 4, 2, 2, 2, 1, 1, 8, 1, 4]
FIELD_TYPES = "F F F U U U U U I U F".split()


def lzf_decode(data, expected, limit=64*1024*1024):
    if not 0 <= expected <= limit:
        raise ValueError("pcd_lzf_size")
    out = bytearray()
    i = 0
    while i < len(data):
        control = data[i]
        i += 1
        if control < 32:
            n = control+1
            if i+n > len(data) or len(out)+n > expected:
                raise ValueError("pcd_lzf_literal")
            out.extend(data[i:i+n])
            i += n
        else:
            n = control >> 5
            if n == 7:
                if i >= len(data):
                    raise ValueError("pcd_lzf_length")
                n += data[i]
                i += 1
            if i >= len(data):
                raise ValueError("pcd_lzf_distance")
            distance = ((control & 31) << 8)+data[i]+1
            i += 1
            n += 2
            if distance > len(out) or len(out)+n > expected:
                raise ValueError("pcd_lzf_reference")
            start = len(out)-distance
            block = bytes(out[start:start+min(distance, n)])
            out.extend((block*((n+len(block)-1)//len(block)))[:n])
    if len(out) != expected:
        raise ValueError("pcd_lzf_size")
    return bytes(out)


@dataclass
class Pcd:
    header: dict
    meta: dict
    fields: list
    sizes: list
    types: list
    counts: list
    offsets: list
    count: int
    step: int
    data: bytes

    def full_records(self, limit=64*1024*1024):
        """Normalize missing optional fields to explicit defaults; retain XYZ bits."""
        if self.fields == FIELD_NAMES:
            return self.data
        if self.count*33 > limit:
            raise ValueError("pcd_normalized_size")
        default = bytearray(33)
        default[19] = 1  # return_count
        struct.pack_into('<f', default, 29, 1.)  # confidence
        out = bytearray(default*self.count)
        target = memoryview(out)
        source = memoryview(self.data)
        offset = 0
        for name, size in zip(FIELD_NAMES, FIELD_SIZES):
            if name in self.fields:
                incoming = self.offsets[self.fields.index(name)]
                for byte in range(size):
                    target[offset+byte::33] = source[incoming+byte::self.step]
            offset += size
        return bytes(out)


def decode_pcd(body, limit=64*1024*1024, *, strict_fields=True):
    if len(body) > limit:
        raise ValueError("pcd_body_size")
    header, meta = {}, None
    start = 0
    for line in body[:65536].splitlines(keepends=True):
        start += len(line)
        if not line.endswith(b'\n'):
            raise ValueError("pcd_header")
        if line.startswith(b'# MA0T10_META '):
            if meta is not None:
                raise ValueError("duplicate_metadata")
            meta = json.loads(line[14:])
        elif not line.startswith(b'#'):
            tokens = line.decode('ascii').split()
            if not tokens:
                continue
            key = tokens[0]
            if key in header:
                raise ValueError("duplicate_header")
            header[key] = tokens[1:]
            if key == 'DATA':
                break
    else:
        raise ValueError("pcd_header_size")
    if header.get('VERSION') != ['0.7'] or header.get('DATA') not in (['binary'], ['binary_compressed']):
        raise ValueError("pcd_encoding")
    fields = header['FIELDS']
    sizes = list(map(int, header['SIZE']))
    types = header['TYPE']
    counts = list(map(int, header['COUNT'] if strict_fields else header.get('COUNT', ['1']*len(fields))))
    if not 3 <= len(fields) <= 64 or len(set(fields)) != len(fields) or not len(fields) == len(sizes) == len(types) == len(counts):
        raise ValueError("pcd_fields")
    if not {'x', 'y', 'z'} <= set(fields):
        raise ValueError("pcd_xyz")
    offsets, step = [], 0
    for name, size, kind, count in zip(fields, sizes, types, counts):
        if size not in (1, 2, 4, 8) or kind not in ('F', 'I', 'U') or not 1 <= count <= 1024:
            raise ValueError("pcd_field_type")
        if strict_fields and name in FIELD_NAMES:
            i = FIELD_NAMES.index(name)
            if (size, kind, count) != (FIELD_SIZES[i], FIELD_TYPES[i], 1):
                raise ValueError("pcd_layout")
        offsets.append(step)
        step += size*count
    if any(len(header.get(k, [])) != 1 for k in ('POINTS', 'WIDTH', 'HEIGHT')):
        raise ValueError("pcd_dimensions")
    count, width, height = (int(header[k][0]) for k in ('POINTS', 'WIDTH', 'HEIGHT'))
    if count < 0 or width < 0 or height < 1 or width*height != count or count*step > limit:
        raise ValueError("pcd_dimensions")
    data = body[start:]
    if header['DATA'] == ['binary_compressed']:
        if len(data) < 8:
            raise ValueError("pcd_lzf_prefix")
        packed, unpacked = struct.unpack_from('<II', data)
        if packed != len(data)-8 or unpacked != count*step:
            raise ValueError("pcd_lzf_prefix")
        soa = lzf_decode(data[8:], unpacked, limit)
        data = bytearray(unpacked)
        target = memoryview(data)
        plane = 0
        for offset, size, elements in zip(offsets, sizes, counts):
            field_bytes = size*elements
            for byte in range(field_bytes):
                target[offset+byte::step] = soa[plane+byte:plane+count*field_bytes:field_bytes]
            plane += count*field_bytes
        data = bytes(data)
    elif len(data) != count*step:
        raise ValueError("pcd_bytes")
    return Pcd(header, meta or {}, fields, sizes, types, counts, offsets, count, step, data)
