"""Read frame context from a saved PCD, without STOMP headers or point decoding."""
import argparse
import json
from pathlib import Path


def read_context(stream):
    context = None
    consumed = 0
    limit = 1024 * 1024
    while consumed < limit:
        line = stream.readline(limit - consumed + 1)
        consumed += len(line)
        if not line or consumed > limit:
            raise ValueError("Missing or oversized PCD header")
        if line.startswith(b"# MA0T10_META "):
            if context is not None:
                raise ValueError("Duplicate PCD context")
            context = json.loads(line[len(b"# MA0T10_META "):].decode("utf-8"))
            if context.get("schema") != "virtual-pointcloud.context.v1":
                raise ValueError("Unsupported PCD context schema")
        if line.strip() in (b"DATA binary", b"DATA ascii", b"DATA binary_compressed"):
            return context
    raise ValueError("Oversized PCD header")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcd", type=Path)
    args = parser.parse_args()
    with args.pcd.open("rb") as source:
        result = read_context(source)
    if result is None:
        raise SystemExit("This PCD has no embedded MA0T10 context (legacy file).")
    print(json.dumps(result, ensure_ascii=False, indent=2))
