"""Independently verify a TRAIN-BATCH-01 archive with the Python standard library.

Usage:
    python verify_archive.py <archive.zip|archive.tar.gz> <checksums.sha256> [--prefix job-1/]

Every entry listed in the checksum file must exist in the archive with the same SHA-256,
no extra regular files may be present, and no entry may escape the archive root.
The archive is streamed; nothing is extracted to disk.
"""
import argparse
import hashlib
import posixpath
import sys
import tarfile
import zipfile

CHUNK = 1024 * 1024


def safe_name(name: str) -> bool:
    if not name or name.startswith("/") or "\\" in name or ":" in name:
        return False
    parts = name.split("/")
    return all(p not in ("", ".", "..") for p in parts) and posixpath.normpath(name) == name


def read_checksums(path: str, prefix: str) -> dict:
    expected = {}
    with open(path, "r", encoding="utf-8", newline="\n") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            digest, name = line.split("  ", 1)
            if len(digest) != 64:
                raise ValueError(f"bad digest line: {line!r}")
            expected[prefix + name if not name.startswith(prefix) else name] = digest
    return expected


def hash_stream(stream) -> str:
    h = hashlib.sha256()
    for block in iter(lambda: stream.read(CHUNK), b""):
        h.update(block)
    return h.hexdigest()


def actual_digests(archive: str) -> dict:
    found = {}
    if archive.endswith(".zip"):
        with zipfile.ZipFile(archive) as z:
            bad = z.testzip()
            if bad is not None:
                raise ValueError(f"CRC mismatch: {bad}")
            for info in z.infolist():
                if info.is_dir():
                    continue
                with z.open(info) as s:
                    found[info.filename] = hash_stream(s)
    elif archive.endswith(".tar.gz"):
        with tarfile.open(archive, "r:gz") as t:
            for member in t:
                if member.isdir():
                    continue
                if not member.isfile():
                    raise ValueError(f"non-regular entry: {member.name}")
                found[member.name] = hash_stream(t.extractfile(member))
    else:
        raise ValueError("unsupported archive type (expected .zip or .tar.gz)")
    return found


def verify(archive: str, checksums: str, prefix: str) -> list:
    errors = []
    expected = read_checksums(checksums, prefix)
    found = actual_digests(archive)
    for name in found:
        if not safe_name(name):
            errors.append(f"unsafe entry name: {name}")
    for name, digest in expected.items():
        if name not in found:
            errors.append(f"missing: {name}")
        elif found[name] != digest:
            errors.append(f"sha256 mismatch: {name}")
    for name in sorted(set(found) - set(expected)):
        errors.append(f"unexpected entry: {name}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("archive")
    parser.add_argument("checksums")
    parser.add_argument("--prefix", default="")
    args = parser.parse_args()
    errors = verify(args.archive, args.checksums, args.prefix)
    for e in errors:
        print(e)
    print("OK" if not errors else f"FAILED ({len(errors)})")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
