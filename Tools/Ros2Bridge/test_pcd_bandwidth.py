"""New wire profiles, compression, defaults and CLI inspector; golden inputs unchanged."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

import wire
from pcd_codec import FIELD_NAMES, FIELD_SIZES, FIELD_TYPES, decode_pcd, lzf_decode
from test_wire import GOLDEN, ROOT, headers


def profile_body(original, names, compressed):
    head, raw = original.split(b'DATA binary\n', 1)
    offsets, at = {}, 0
    for name, size in zip(FIELD_NAMES, FIELD_SIZES):
        offsets[name] = at
        at += size
    selected = [(n, FIELD_SIZES[FIELD_NAMES.index(n)], offsets[n]) for n in names]
    data = b''.join(raw[p*33+off:p*33+off+size] for p in range(2) for _,size,off in selected)
    lines = head.decode().splitlines()
    replacements = {'FIELDS': ' '.join(names), 'SIZE': ' '.join(str(s) for _,s,_ in selected),
                    'TYPE': ' '.join(FIELD_TYPES[FIELD_NAMES.index(n)] for n in names),
                    'COUNT': ' '.join('1' for _ in names)}
    lines = [line.split(' ',1)[0]+' '+replacements[line.split(' ',1)[0]]
             if line.split(' ',1)[0] in replacements else line for line in lines]
    mode = 'binary_compressed' if compressed else 'binary'
    if compressed:
        soa = b''.join(raw[p*33+off:p*33+off+size] for _,size,off in selected for p in range(2))
        packed = b''.join(bytes([len(soa[i:i+32])-1])+soa[i:i+32] for i in range(0,len(soa),32))
        data = struct.pack('<II',len(packed),len(soa))+packed
    return ('\n'.join(lines)+'\nDATA '+mode+'\n').encode()+data


class PcdBandwidthWireTests(unittest.TestCase):
    def test_all_profiles_encodings_and_existing_ros_layout(self):
        original=(GOLDEN/'with-context.pcd').read_bytes()
        source=wire.pcd(original,headers('with-context.pcd.headers'))
        for names in (FIELD_NAMES, 'x y z intensity ring time_offset_ns'.split(),
                      ['x','y','z'], ['confidence','z','x','y','ring']):
            for compressed in (False,True):
                with self.subTest(names=names,compressed=compressed):
                    body=profile_body(original,names,compressed)
                    h=headers('with-context.pcd.headers')
                    for key in ('checksum','x-checksum-sha1'):
                        if key in h:h[key]=hashlib.sha1(body).hexdigest()
                    h['content-length']=str(len(body))
                    cloud=wire.pcd(body,h)
                    self.assertEqual((cloud.count,cloud.point_step,cloud.fields),(2,33,wire.PCD_FIELDS))
                    self.assertEqual(cloud.pose,source.pose)
                    for p in range(2):
                        offset=0
                        for name,size in zip(FIELD_NAMES,FIELD_SIZES):
                            actual=cloud.data[p*33+offset:p*33+offset+size]
                            if name in names:expected=source.data[p*33+offset:p*33+offset+size]
                            elif name=='confidence':expected=struct.pack('<f',1.)
                            elif name=='return_count':expected=b'\x01'
                            else:expected=bytes(size)
                            self.assertEqual(actual,expected)
                            offset+=size
                    with tempfile.TemporaryDirectory(prefix='pcd-bandwidth-') as folder:
                        path=Path(folder)/'received.pcd';path.write_bytes(body)
                        spec=importlib.util.spec_from_file_location('pcd_inspector',ROOT/'Scripts/inspect_binary_pcd.py')
                        inspector=importlib.util.module_from_spec(spec);spec.loader.exec_module(inspector)
                        meta,step,rows=inspector.read_pcd(path)
                        self.assertEqual(step,sum(FIELD_SIZES[FIELD_NAMES.index(n)] for n in names))
                        self.assertEqual(rows[0][0],list(struct.unpack_from('<fff',source.data)))

    def test_fixed_lzf_vector_and_corruptions(self):
        self.assertEqual(lzf_decode(bytes([0,65,224,0,0]),10),b'A'*10)
        for data,n in ((bytes([224,0,0]),10),(bytes([2,11]),3),(bytes([0,1]),0)):
            with self.assertRaises(ValueError):lzf_decode(data,n)
        original=(GOLDEN/'with-context.pcd').read_bytes()
        packed=profile_body(original,['x','y','z'],True)
        self.assertEqual(decode_pcd(packed).step,12)
        for malformed in (packed[:-1],packed.replace(b'SIZE 4 4 4',b'SIZE 8 4 4'),packed.replace(b'FIELDS x y z',b'FIELDS x x z')):
            with self.assertRaises((ValueError,KeyError)):decode_pcd(malformed)


if __name__=='__main__':unittest.main()
