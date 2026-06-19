# Pixel Streaming Setup Notes

This project does not commit copied Pixel Streaming sample projects by default.
The local `Samples/PixelStreaming/` folder is treated as third-party/sample
content and must remain untracked unless project ownership and redistribution
approval are recorded.

## Current Decision

- Local path: `Samples/PixelStreaming/`
- Category: `SampleOrThirdParty`
- Default decision: `KeepLocalUnlessOwned`
- Preferred repository path: document setup steps instead of committing copied
  sample files.
- Commit boundary: do not stage `Samples/PixelStreaming/` while ownership,
  license/redistribution approval, and setup-documentation alternative evidence
  are incomplete.

## Recreate Locally

Use the Unreal Engine 5.3 Pixel Streaming plugin and official sample/setup path
for the local machine instead of relying on copied files in this repository.
The exact command or launcher workflow can vary by workstation and engine
installation, so record the local source and version in the sample decision
evidence if the project later decides to own those files.

Minimum local notes to capture before sharing a setup:

- Unreal Engine version and install path.
- Pixel Streaming plugin state in the project.
- Source of any copied sample files.
- Whether the sample is required for DT runtime, editor smoke, packaging, or
  only local experimentation.
- License or redistribution note for every copied file group.

## Acceptance Evidence

`Samples/PixelStreaming/` can move toward repository staging only after all of
the following are complete:

- Project ownership decision says DT-Project intentionally owns these sample
  files.
- License and redistribution review accepts committing them.
- Setup documentation alternative is rejected with a reason, or the repository
  only stages docs/config needed to recreate the setup.
- `Scripts/export_sample_content_decision_report.ps1` reports
  `ReadyToStageCount > 0` for the reviewed path.
- `Scripts/report_local_project_status.ps1 -FailOnStagedDecisionPoints` passes
  after the intended staged paths are accepted by evidence.

## Routine Checks

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_sample_content_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1" -ProjectRoot "." -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
```

These checks are read-only. They do not copy sample files, modify assets, or
stage `Samples/PixelStreaming/`.
