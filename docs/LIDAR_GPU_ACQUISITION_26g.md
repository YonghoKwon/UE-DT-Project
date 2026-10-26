# LiDAR GPU acquisition — 26g

Status: implementation and validation in progress on `feat/lidar-gpu-acq-rewrite-26g`, based on `c9e6e40`. No merge or final acceptance is implied. Current evidence is in [RESULT.md](../Saved/Reports/LIDAR-GPU-REWRITE-26g/RESULT.md).

## Resource and completion contract

The Windows DXR 1.1 backend owns committed D3D12 buffers, a compute queue, a root signature, one fixed compute PSO, command allocators/lists, and a monotonically increasing queue fence. It does not retain an engine `FRHIRayTracingScene`, raw engine residency handle, pooled engine BLAS address, or opaque engine PSO pointer. Existing Scheduler admissions and immutable requests remain the only acquisition entry point.

| Resource | Owner and reuse condition |
|---|---|
| BLAS, vertices, surface records and build scratch | Immutable mesh revision; at most eight cached entries. In-flight scenes retain strong references after eviction. No compaction or relocation. |
| TLAS, instance descriptors and normal matrices | Exact ordered revisions and poses; at most four cached entries. Every GPU job retains its scene until its terminal queue fence. |
| Ray upload, result UAV, readback, dispatch constants and command allocator/list | Fenced job. A job returns to the free pool only after its queue fence completes and CPU readback has copied the result. At most four free jobs and 64 MiB of free buffers. |
| Live, cached, free and retired native buffers | One 512 MiB allocation ledger, including 64 KiB committed-allocation rounding. This ledger excludes opaque driver/PSO/allocator bookkeeping; RSS and process Commit still measure that cost. |
| Cancellation | Suppresses publication. The batch retains cancelled members and all GPU ownership until the same terminal fence; siblings keep their original output offsets. |
| Teardown | Stops new admissions, retains pending native jobs, drains the owned queue once, then releases native resources. No GPU idle wait is added to acquisition. |

Committed allocations are initially resident; this backend never evicts them and never hands their storage to the engine allocator. GPU node mask 1 and single-GPU operation are required. Unsupported platforms/DXR tiers retain the engine backend. If the native buffer budget cannot admit a request before submission, the supported engine `SingleFrame` path preserves the previous geometry/ray limits and records a fallback; it does not lower point count or choose a cheaper quality mode. GPU/validation failures are reported as failures.

The buffer residency rule follows [Microsoft's committed-resource contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommittedresource). Inline queries use the [DXR 1.1 query contract](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html). Installed UE 5.3 source is authoritative for the RHI bridge and `RTSL_SingleFrame` restriction; later online UE signatures are not assumed compatible.

## Shader and compatibility

The standalone `MainInlineCS` entry and existing engine raygen/hit entries share `Shaders/Private/SensorOwnedRayTracing.usf`. Native hits keep the same 32-byte record, nearest opaque triangle, miss sentinel, primitive/mesh/surface identity and normal calculation. The engine `TraceRay` path remains an independent byte oracle. There is no new UE module, sensor plugin, scheduler, wire format or Blueprint signature.

`Scripts/generate_sensor_native_shader.ps1` compiles the standalone entry with Windows SDK 10.0.22621 DXC (`cs_6_5`, `-O3 -Ges`) into a checked-in C++ DXIL array. Shipping requires no runtime compiler or shader source deployment. `-Check` verifies reproducibility; quick's `-CheckSourceOnly` checks a line-ending-independent source digest without requiring DXC.

`-SensorRtEngine` selects the existing engine path for diagnosis; `-SensorRtLegacy` retains the older unbatched diagnostic. `-SensorNativeLedger` logs allocation/release GPU addresses only on resource changes. None changes quality, requested Hz, engine GPU-stat/RDG defaults, FIFO limits or default PCD/preview policy.

Native dispatches, BLAS/TLAS builds, buffer bytes/peak, budget fallbacks and optional D3D12 validation availability/errors are sampled by the existing five-second memory probe. An unavailable validation interface is not reported as an observed zero error count. Native buffer bytes are shown separately rather than relabelled as engine RHIMisc.

Depth/semantic render-target/readback reuse and the UE 5.3 semantic proxy uniform-lifetime workaround are preserved. The current work does not remove the workaround without a separate semantic byte/identity regression.

## Validation status and limits

Initial actual-D3D12 tests passed full 65,536-ray × three-sensor byte parity, twelve pose/revision/unequal-count/cancellation oracle epochs, rigid-instance checks, fifty production digs with twenty cancellations, and the three-sensor scheduled OS1 test. These are preliminary evidence, not final memory/performance acceptance.

One subsequent combined Editor process suffered `DXGI_ERROR_DEVICE_HUNG` / GPU page fault when opening the next scheduled-test map after the oracle and dig tests. DRED identified the engine graphics queue; the exact allocation/cause is unconfirmed. Repeating the same six tests with the native allocation ledger passed, which does not establish a fix. A GPU-validation scheduled run completed without a device crash but failed its unchanged Hz assertions under diagnostic overhead. Both failures remain in the report. Final release, package memory, original five-pair 8 MiB, ABAB and Shipping gates remain required.
