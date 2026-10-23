"""Independent bounded receiver checks; no broker, credentials or Unreal required."""
import hashlib
import random
import struct
import unittest

from pcd_loopback_probe import FIELD_TYPES, decode_lzf, validate_pcd


class PcdLoopbackTests(unittest.TestCase):
    def test_profiles_and_encodings(self):
        for names in (list(FIELD_TYPES), "x y z intensity ring time_offset_ns".split(),
                      "x y z".split(), "x y z confidence".split()):
            for compressed in (False, True):
                with self.subTest(names=names, compressed=compressed):
                    sizes, types = zip(*(FIELD_TYPES[n] for n in names))
                    raw = bytes(sum(sizes))
                    literal = b"".join(bytes([len(raw[i:i+32])-1])+raw[i:i+32]
                                       for i in range(0, len(raw), 32))
                    body = ("# .PCD v0.7\nVERSION 0.7\nFIELDS "+" ".join(names)+
                            "\nSIZE "+" ".join(map(str, sizes))+"\nTYPE "+" ".join(types)+
                            "\nCOUNT "+" ".join("1" for _ in names)+
                            "\nWIDTH 1\nHEIGHT 1\nPOINTS 1\nDATA "+
                            ("binary_compressed" if compressed else "binary")+"\n").encode()
                    body += struct.pack("<II", len(literal), len(raw))+literal if compressed else raw
                    headers = {"point-count": "1", "checksum": hashlib.sha1(body).hexdigest()}
                    self.assertEqual(validate_pcd(body, headers), 1)
                    headers["checksum"] = "0"*40
                    with self.assertRaises(ValueError):
                        validate_pcd(body, headers)

    def test_overlapping_reference_and_literal(self):
        self.assertEqual(decode_lzf(bytes([0, 65, 224, 0, 0]), 10), b"A"*10)
        self.assertEqual(decode_lzf(bytes([2, 11, 22, 33]), 3), bytes([11, 22, 33]))
        # Non-overlapping distance much larger than match: copy only match bytes.
        raw = bytes(range(32))
        self.assertEqual(decode_lzf(bytes([31])+raw+bytes([32, 31]), 35), raw+raw[:3])

    def test_validation_acceptance_is_identical_without_materializing(self):
        rng = random.Random(3301)
        corpus = [(b"", 0), (bytes([0, 65, 224, 0, 0]), 10), (bytes([2, 11, 22, 33]), 3)]
        for _ in range(3000):
            data = bytes(rng.randrange(256) for _ in range(rng.randrange(40)))
            corpus.append((data, rng.randrange(80)))
        for data, expected in corpus:
            accepted = []
            for validate_only in (False, True):
                try:
                    decode_lzf(data, expected, validate_only=validate_only)
                    accepted.append(True)
                except ValueError:
                    accepted.append(False)
            self.assertEqual(accepted[0], accepted[1])


if __name__ == "__main__":
    unittest.main()
