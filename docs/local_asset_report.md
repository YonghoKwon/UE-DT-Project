# Local Asset Report

`Scripts/report_local_project_status.ps1` is a local decision-aid for the Unreal
project copy, especially `C:\Unreal Projects\m7at10_dt`.

The script separates known local decision points from unexpected untracked files.
This keeps generated outputs, vendor/sample content, binary WBP edits, and local
settings out of code commits until each item has an explicit project decision.
The active decision list and completion evidence are tracked in
`docs/remaining_work.md`.

## Basic Usage

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

Useful machine-readable output:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -Json
```

## Gate Options

Fail when generated or local-output items are present:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnGeneratedOutput
```

Fail when specific decision categories are present:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```

Fail when Git sees an untracked file that is not covered by the known decision
point inventory:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnUnclassifiedUntracked
```

## Current Categories

- `ReviewCandidate`: small project or binary edits that need manual review.
- `LargeContentCandidate`: Unreal content folders that may be valid production
  assets, but should be committed only after an explicit asset/vendor decision.
- `SampleOrThirdParty`: sample or third-party content that should not enter the
  project accidentally.
- `GeneratedOutput`: packaged outputs such as `Windows/` and `Windows.zip`.
- `GeneratedOrLocalConfig`: local runtime configuration.

`Config/Game.ini` receives an additional detected note when it contains
`[DTCoreRuntimeOverride]`. If every override value is blank, treat it as a
local-only runtime override unless shared endpoint defaults are explicitly
required. If any endpoint or credential value is populated, review it for
environment or secret leakage before staging.

`Content/M7AT10/UI/WBP_VirtualSensorMonitor.uasset` receives an additional
detected note because it is a binary Designer widget. Keep it untracked until it
has been opened in Unreal Editor, optional bindings have been checked against
`docs/widget_designer_setup.md`, and a PIE smoke pass confirms that the widget
does not crash or show stale sensor status.

Large content candidates and sample/third-party folders include extension counts
and the largest files in each folder. Use this to spot built-data-heavy map
packages, oversized textures, vendor packs, and copied sample projects before
deciding whether the repository should own them.

## Recommended Workflow

1. Run the report before staging.
2. Resolve or explicitly accept each decision point.
3. Run with `-FailOnUnclassifiedUntracked` to catch newly created paths that the
   inventory does not know about yet.
4. Stage only intended source, docs, config, or content files.

## Readiness Wrapper

For regular local checks, use `Scripts/check_project_readiness.ps1`. It runs the
local asset report with `-FailOnUnclassifiedUntracked`, then runs the smoke test
script unless `-SkipSmoke` is passed.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipBuild
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
```

Strict local-output gates can be enabled when a clean packaging state is needed:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```
