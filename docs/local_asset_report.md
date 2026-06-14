# Local Asset Report

`Scripts/report_local_project_status.ps1` is a local decision-aid for the Unreal
project copy, especially `C:\Unreal Projects\m7at10_dt`.

The script separates known local decision points from unexpected untracked files.
This keeps generated outputs, vendor/sample content, binary WBP edits, and local
settings out of code commits until each item has an explicit project decision.

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

## Recommended Workflow

1. Run the report before staging.
2. Resolve or explicitly accept each decision point.
3. Run with `-FailOnUnclassifiedUntracked` to catch newly created paths that the
   inventory does not know about yet.
4. Stage only intended source, docs, config, or content files.
