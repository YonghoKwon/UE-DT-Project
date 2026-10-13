ARCH-06 golden bytes, captured with the pre-neutralization runtime at a28b4a1.

`MA0T10.Architecture.WireGolden` compares 14 files without regenerating them.
The fixed input lives in ArchitectureWireGoldenTests.cpp: sensor-fixed-007,
sensor FrameId 9007199254740993, Slab frame_no 9007199254740995, distinct
scenario/run UUIDs, material with leading zeroes, UTC, acquisition pose,
two 33-byte PCD points, and context present/absent cases.

PCD files include header, MA0T10_META comment and binary body. JSON files
come from the existing compatibility encoder and publisher telemetry queue.
Header files use the existing RawPcd/EnginePcd/RawFrame builders and the
worker's ordered traversal and STOMP escaping. Camera encoded input is an
opaque fixed six-byte body; this fixture tests metadata/header preservation,
not JPEG codec or RHI rendering. Its checksum is SHA1 of those exact bytes.
Camera metadata is exercised through the publisher's actual prepared queue.
Compatibility LiDAR JSON and telemetry have no Slab fields in their body;
their linkage headers are checked separately, preserving the existing contract.

Fixtures are byte files. Do not normalize line endings, JSON or header order.
