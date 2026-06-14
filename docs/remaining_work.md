# Remaining Work

This document tracks what is still open after the current LiDAR/virtual sensor
branch work. It is intentionally practical: each item should either name the
decision needed, the implementation task, or the evidence required to call the
work complete.

## Current Baseline

Implemented in DT-Project:

- Virtual LiDAR scan/replay path with separated server payload and UI preview
  policies.
- Virtual camera payload export and monitor status text.
- Slab angle analysis v1 from semantic LiDAR points.
- CSV/JSONL point-cloud replay sources.
- Sensor manager point-cloud-only view policy.
- Native monitor widget fallback and monitor host actor.
- Local smoke runner and project readiness wrapper.
- Local asset decision report with generated-output, category, and
  unclassified-untracked gates.

Out of scope for the current DT-Project-only work:

- DTCore source changes.
- Real SDK/bridge implementation inside DTCore.

## Local Project Decisions

The local project currently contains intentional untracked decision points. Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

Open decisions:

- `Content/M7AT10/UI/WBP_VirtualSensorMonitor.uasset`
  - Decide whether this binary WBP is the intended production monitor asset.
  - Current local state: binary Designer widget decision point, approximately
    40 KB in the local project.
  - Evidence needed: open in Unreal Editor, verify bindings, run PIE smoke test,
    then commit only if accepted.
- `Config/Game.ini`
  - Decide whether the local config change is intentional project config.
  - Current local state: empty `[DTCoreRuntimeOverride]` values.
  - Recommended default: keep untracked as local-only runtime override unless
    shared endpoint defaults are explicitly required.
  - Evidence needed for commit: inspect diff, confirm there are no endpoint or
    credential values, and document why the setting belongs in repo.
- `Content/ChemicalPlantEnv/`
  - Large environment pack.
  - Evidence needed: explicit asset/vendor decision, size acceptance, and map
    dependency confirmation.
- `Content/Mega_Crane/`
  - Large crane asset pack.
  - Evidence needed: explicit asset/vendor decision and whether crane example
    remains part of this branch.
- `Content/Materials/`, `Content/Meshes/`, `Content/Textures/`
  - Shared content folders.
  - Evidence needed: map/WBP dependency check and asset ownership decision.
- `Samples/PixelStreaming/`
  - Sample or third-party content.
  - Current local state: tracked as a sample/third-party decision point with
    extension counts and largest-file details in
    `Scripts/report_local_project_status.ps1`.
  - Evidence needed: explicit decision that Pixel Streaming samples are part of
    this repository.

Generated/local-output items are ignored by Git but still reported:

- `Windows/`
- `Windows.zip`
- `launcher.config.json`

These should stay out of commits unless a separate packaging workflow explicitly
requires a different policy.

## Implementation Gaps

### Real Sensor Adapters

Current state:

- `URos2SensorBridgeSourceComp`, `ULivoxLidarSourceComp`, and
  `URealSenseCameraSourceComp` are placeholders.
- They expose configuration/state but do not connect to real SDKs or bridges.
- `Scripts/validate_real_sensor_adapter_plan.ps1` checks that replay adapters,
  placeholder adapters, sample files, handoff API text, automation test names,
  and the adapter plan remain in sync.

Next implementation steps:

- Define the final normalized frame contract for LiDAR and camera input.
- Implement a ROS2 bridge adapter that converts incoming messages to
  `FVirtualLidarPoint` frames or camera payloads.
- Implement a Livox SDK adapter that normalizes packet streams into the same
  frame handoff path.
- Implement a RealSense adapter for camera/depth frames if required.

Completion evidence:

- Adapter starts and stops without editor crashes.
- Recorded real or replayed SDK sample data reaches the target LiDAR/camera
  component.
- Server payload/export path is shared with virtual/replay sources.
- Automation or editor smoke coverage exists for connection failure and at least
  one successful frame handoff path.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_plan.ps1"`.

### Server Payload Schema Approval

Current state:

- LiDAR schema is documented as `virtual-lidar.v1`.
- Camera schema is documented as `virtual-camera.v1`.
- Reference fixtures exist under `Samples/payload_fixtures/` and can be checked
  with `Scripts/validate_payload_fixtures.ps1`.
- A local mock contract validator checks the fixtures against acceptance rules
  that approximate likely judging-server rejections before the final server
  contract is approved.
- `Scripts/export_payload_contract_report.ps1` exports JSON and Markdown review
  artifacts under `Saved/PayloadContractReports/` for judging-server handoff.
- The readiness wrapper runs both fixture validation and mock contract
  validation before smoke tests unless those gates are explicitly skipped.
- The schema is implemented enough for local export and smoke tests, but the
  final judging server contract is not confirmed.

Next implementation steps:

- Confirm required field names, units, coordinate frame, timestamp format, and
  compression/transport requirements with the judging server.
- Add schema compatibility examples to `docs/lidar_payload_schema.md` and
  `docs/camera_payload_schema.md`.
- Add a fixture-based payload contract test if the server contract becomes
  stable.

Completion evidence:

- Approved example payloads exist.
- Contract tests validate required fields.
- Transport mode can produce payloads accepted by the server or the local mock
  contract validator.

### True LAZ Compression

Current state:

- `ExportLastPointCloudLaz()` is intentionally a placeholder.
- It writes a LAS-compatible `*_laz_source_*.las` file and logs a warning.
- `Scripts/validate_laz_placeholder_policy.ps1` checks that code, docs, monitor
  settings, and automation continue to describe this as a placeholder until true
  LAZ compression is integrated.

Next implementation steps:

- Decide whether true LAZ is required for this project.
- Choose an integration path such as a supported native library, external tool,
  or post-processing workflow.
- Keep current warning behavior until a real compressor is integrated.

Completion evidence:

- `.laz` output is actually compressed and readable by a known point-cloud tool.
- Automation verifies `.laz` creation separately from LAS source export.
- Static placeholder readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_laz_placeholder_policy.ps1"`.

### Large Point Cloud Renderer

Current state:

- Preview is CPU/instance oriented and protected by preview stride/max point
  policies.
- This is enough for editor feedback but not ideal for very large point clouds.
- `Scripts/validate_point_cloud_preview_policy.ps1` checks that the current
  server/preview split, runtime warnings, point-cloud-only preview clamps, and
  automation test names remain in sync while the GPU renderer decision is open.

Next implementation steps:

- Decide whether to use Niagara, GPU buffers, or a custom renderer.
- Preserve the current split: server payload can stay dense while preview is
  downsampled or GPU-rendered.
- Add pixel/screenshot smoke checks if a GPU renderer is added.

Completion evidence:

- High-density scan can be previewed without editor stalls.
- Point-cloud-only mode still preserves collision/trace behavior.
- Dense server payload count is independent from preview point count.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_point_cloud_preview_policy.ps1"`.

### Production Monitor WBP

Current state:

- Native fallback exists and is covered by automation.
- Designer-authored `WBP_VirtualSensorMonitor` is still a local binary decision.

Next implementation steps:

- Open the WBP in Unreal Editor and verify optional bindings.
- Confirm camera/LiDAR switching, preview budget controls, hit-only toggle,
  server payload export, and slab analysis text.
- Commit the WBP only after manual editor verification.

Completion evidence:

- PIE smoke test demonstrates the WBP in a real map.
- No missing binding crashes.
- Operator-facing status text exposes selected sensor, frame/ray counts, payload
  counts, preview counts, warnings, and slab angle result.

## Routine Verification

Fast local readiness check:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
```

Pre-commit progress and remaining-work summary:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -Json
```

The percentages are planning snapshots derived from the current remaining-work
areas. They are not a substitute for build, smoke, editor, or server acceptance
evidence.

Full local readiness check:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
```

Strict asset gates for clean-content review:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnStagedDecisionPoints
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```
