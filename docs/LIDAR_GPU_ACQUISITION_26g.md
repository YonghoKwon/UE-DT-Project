# LiDAR GPU acquisition — 26g

Status: implementation and validation in progress on `feat/lidar-gpu-acq-rewrite-26g`, based on `c9e6e40`. No merge or final acceptance is implied. Current evidence is in [RESULT.md](../Saved/Reports/LIDAR-GPU-REWRITE-26g/RESULT.md).

## Current supported fallback

The default now uses the supported engine raygen path with persistent project-owned RHI TLAS/scratch/instance/upload/address buffers. Engine `SingleFrame` scene metadata is recreated in each render frame and never reused across frames. A cache record is borrowed for storage reuse only when it has no scan owner and belongs to an older render frame. Every scan retains the record through its terminal GPU readback fence. Geometry-count/capacity changes recreate the required storage; ordinary idle frames reuse it. Existing BLAS ownership, residency updates, GPU mask handling and deferred RHI deletion remain in the engine path.

This fallback separates frame-local metadata from reusable storage as allowed in the original plan. It does **not** claim the primary goal of a fully persistent engine-independent TLAS was achieved. The native prototype is explicitly diagnostic-only (`-SensorRtNative`) after repeated device-removal failures. Its earlier results remain below.

Installed UE 5.3 Windows shader-platform data declares `bSupportsInlineRayTracing=false` for PCD3D_SM5 and PCD3D_SM6. The RHI inline variant is capability-gated and explicit-only (`-SensorRtRhiInline`). No engine capability flag, shader-platform INI or project shader-model default was changed. The inline capability qualification failed; it is not a pass.

The supported storage fallback passed two full byte oracles and the six-test actual-D3D12 collision/rigid/byte/dig/cancel/scheduled group. Steady tests require TLAS storage creation to stop increasing, observe actual storage reuse, preserve whole result bytes and refresh SingleFrame metadata. Package and long-memory gates remain pending.

## Diagnostic native prototype resource contract

The Windows DXR 1.1 backend owns committed D3D12 buffers, a root signature, one fixed compute PSO, command allocators/lists, and a monotonically increasing queue fence. Its independent direct command list is submitted to the existing graphics queue through the public UE 5.3 D3D12 bridge. It never records into an engine command list or modifies its state cache/descriptor heaps. It does not retain an engine `FRHIRayTracingScene`, raw engine residency handle, pooled engine BLAS address, or opaque engine PSO pointer. Existing Scheduler admissions and immutable requests remain the only acquisition entry point.

| Resource | Owner and reuse condition |
|---|---|
| BLAS, vertices, surface records and build scratch | Immutable mesh revision; at most eight cached entries. In-flight scenes retain strong references after eviction. No compaction or relocation. |
| TLAS, instance descriptors and normal matrices | Exact ordered revisions and poses; at most four cached entries. Every GPU job retains its scene until its terminal queue fence. |
| Ray upload, result UAV, readback, dispatch constants and command allocator/list | Fenced job. A job returns to the free pool only after its queue fence completes and CPU readback has copied the result. At most four free jobs and 64 MiB of free buffers. |
| Live, cached, free and retired native buffers | One 512 MiB allocation ledger, including 64 KiB committed-allocation rounding. This ledger excludes opaque driver/PSO/allocator bookkeeping; RSS and process Commit still measure that cost. |
| Cancellation | Suppresses publication. The batch retains cancelled members and all GPU ownership until the same terminal fence; siblings keep their original output offsets. |
| Teardown | Stops new admissions, retains pending native jobs, waits for the last native queue fence once, then releases native resources. It does not wait for later engine work or add a GPU idle wait to acquisition. |

Committed allocations are initially resident; this backend never evicts them and never hands their storage to the engine allocator. GPU node mask 1 and single-GPU operation are required. Unsupported platforms/DXR tiers retain the engine backend. If the native buffer budget cannot admit a request before submission, the supported engine `SingleFrame` path preserves the previous geometry/ray limits and records a fallback; it does not lower point count or choose a cheaper quality mode. GPU/validation failures are reported as failures.

The buffer residency rule follows [Microsoft's committed-resource contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommittedresource). Inline queries use the [DXR 1.1 query contract](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html). Installed UE 5.3 source is authoritative for the RHI bridge and `RTSL_SingleFrame` restriction; later online UE signatures are not assumed compatible.

## Shader and compatibility

The standalone `MainInlineCS` entry and existing engine raygen/hit entries share `Shaders/Private/SensorOwnedRayTracing.usf`. Native hits keep the same 32-byte record, nearest opaque triangle, miss sentinel, primitive/mesh/surface identity and normal calculation. The engine `TraceRay` path remains an independent byte oracle. There is no new UE module, sensor plugin, scheduler, wire format or Blueprint signature.

`Scripts/generate_sensor_native_shader.ps1` compiles the standalone entry with Windows SDK 10.0.22621 DXC (`cs_6_5`, `-O3 -Ges`) into a checked-in C++ DXIL array. Shipping requires no runtime compiler or shader source deployment. `-Check` verifies reproducibility; quick's `-CheckSourceOnly` checks a line-ending-independent source digest without requiring DXC.

`-SensorRtEngine` selects the existing engine path for diagnosis; `-SensorRtLegacy` retains the older unbatched diagnostic. `-SensorNativeLedger` logs allocation/release GPU addresses only on resource changes. `-SensorNativeComputeQueue` retains the initial separate-compute-queue prototype for diagnosis only; that prototype had device-removal failures. None changes quality, requested Hz, engine GPU-stat/RDG defaults, FIFO limits or default PCD/preview policy.

The graphics-queue fallback removes the additional queue/engine-render overlap. It uses self-contained native lists and a separate fence, retaining the same buffers and shader. Microsoft documents [cross-thread queue submission and ordered executions](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists). This is a mitigation candidate, not proof that the original page fault came from queue overlap.

Native dispatches, BLAS/TLAS builds, buffer bytes/peak, budget fallbacks and optional D3D12 validation availability/errors are sampled by the existing five-second memory probe. An unavailable validation interface is not reported as an observed zero error count. Native buffer bytes are shown separately rather than relabelled as engine RHIMisc.

Depth/semantic render-target/readback reuse and the UE 5.3 semantic proxy uniform-lifetime workaround are preserved. The current work does not remove the workaround without a separate semantic byte/identity regression.

## Validation status and limits

Initial actual-D3D12 tests passed full 65,536-ray × three-sensor byte parity, twelve pose/revision/unequal-count/cancellation oracle epochs, rigid-instance checks, fifty production digs with twenty cancellations, and the three-sensor scheduled OS1 test. These are preliminary evidence, not final memory/performance acceptance.

The separate-compute-queue prototype had one combined Editor process fail with `DXGI_ERROR_DEVICE_HUNG` / GPU page fault when opening the next scheduled-test map after the oracle and dig tests. Its Development package also failed during the 30-second qualification's preparation, before any memory curve. DRED identified the engine graphics queue; the exact allocation/cause is unconfirmed. Repeating the same six Editor tests with the native allocation ledger passed, which does not establish a fix. A GPU-validation scheduled run completed without a device crash but failed its unchanged Hz assertions under diagnostic overhead. These failures remain in the report.

The graphics-queue fallback passed the same six actual-D3D12 tests, including byte oracles, fifty digs/twenty cancellations and ScheduledOS1Three. Final release, package memory, original five-pair 8 MiB, ABAB and Shipping gates remain required; earlier queue results are not carried to this implementation.

Subsequent independent graphics-queue package runs also failed during preparation; queue selection alone did not fix the issue. Address/fence logging found a fault address in a recycled native vertex/scene-metadata allocation range. That vertex allocation's recorded fence was 4 and completion was 56 at release, with device status S_OK; this does not support a simple release-before-recorded-fence conclusion. The engine-path control package completed its 30-second qualification.

The next trial pins each recorded buffer directly in the GPU job, records per-buffer use/completion/device status, closes CPU upload mappings before submission, and keeps immutable normal matrices in ordinary CPU memory. It removes the unused GPU allocation for scene CPU matrices and avoids reading from write-combined upload memory. Upload buffers retain their GPU allocation and are remapped only after their job fence. This follows Microsoft's [simple mapping model and CPU-write ordering guidance](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map); it is an explicit visibility/ownership boundary, not a confirmed root-cause attribution. The updated four-test byte/dig/scheduled regression passed; package validation is still required.

That upload-boundary package also failed its first independent qualification; the remaining two planned qualifications were not run. A preceding pins-only Editor regression failed during production digging. Its ledger showed an old vertex range released at last-use fence 4 / completed 56 / device S_OK, reused for CPU matrix storage, then reported as the GPU fault range. GPU error reporting delay prevents treating that chronology as proof of an early release. The surface-buffer attribution initially considered during investigation was corrected to a recycled vertex/metadata range. Native lifetime/interop remains unresolved, and no native long-memory result is claimed.
